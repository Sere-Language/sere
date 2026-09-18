/// @file Packages.cpp
/// `sere add`: resolves a published package, verifies its checksum, and installs
/// it under the project's `libs/`.
///
/// Layout follows https://sere-lang.com/docs/installing-packages:
///   libs/<name>/        folder library unpacked from a .tar.gz / .zip archive
///   libs/<name>.slib    packed library
///   libs/<name>.sere    single-file library
///
/// Downloads are public, so this path never needs a token. Everything the
/// registry reports (`checksumSha256`, `X-Checksum-Sha256`) is verified before a
/// single byte is written into the project.

#include "sere/driver/Packages.h"

#include "sere/driver/Library.h"
#include "sere/driver/Project.h"
#include "sere/driver/Registry.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/SHA256.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace sere {
namespace {

/// The registry rejects archives above this size; refuse to buffer more.
constexpr std::uintmax_t MaxArchiveBytes = 25u * 1024u * 1024u;
constexpr std::string_view SlibMagic = "SERELIB/";

// ---------------------------------------------------------------- text helpers

[[nodiscard]] bool isSpace(char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; }

[[nodiscard]] std::string trimCopy(std::string text) {
  text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
  text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
  return text;
}

[[nodiscard]] std::string lowerCopy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

[[nodiscard]] bool writeFileBytes(const std::filesystem::path& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(output);
}

[[nodiscard]] std::string uniqueSuffix() {
  const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  return std::to_string(static_cast<long long>(ticks));
}

[[nodiscard]] std::string sha256Hex(const std::string& bytes) {
  const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
  const std::array<std::uint8_t, 32> digest =
      llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(data, bytes.size()));
  constexpr char Hex[] = "0123456789abcdef";
  std::string text;
  text.reserve(digest.size() * 2);
  for (const std::uint8_t byte : digest) {
    text.push_back(Hex[byte >> 4]);
    text.push_back(Hex[byte & 0x0f]);
  }
  return text;
}

// -------------------------------------------------------------- package spec

struct PackageSpec {
  std::string name;
  std::string version;
};

[[nodiscard]] bool isNameChar(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) != 0 || ch == '.' || ch == '-' || ch == '_';
}

[[nodiscard]] bool isVersionChar(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) != 0 || ch == '.' || ch == '-' || ch == '+' || ch == '_';
}

/// `mathlib`, `mathlib@0.1.0`, `mathlib@0.1`.
[[nodiscard]] bool parsePackageSpec(std::string_view spec, PackageSpec& parsed, std::string& error) {
  const std::string trimmed = trimCopy(std::string(spec));
  if (trimmed.empty()) {
    error = "missing package name";
    return false;
  }
  const std::size_t at = trimmed.find('@');
  if (trimmed.find('@', at == std::string::npos ? trimmed.size() : at + 1) != std::string::npos) {
    error = "invalid package '" + trimmed + "'; use <name>[@<version>]";
    return false;
  }
  parsed.name = lowerCopy(trimmed.substr(0, at));
  parsed.version =
      at == std::string::npos ? std::string() : trimCopy(trimmed.substr(at + 1));
  if (parsed.version == "latest" || parsed.version == "*") {
    parsed.version.clear();
  }
  if (parsed.name.empty() || parsed.name.size() > 64 ||
      std::isalnum(static_cast<unsigned char>(parsed.name.front())) == 0) {
    error = "invalid package name '" + parsed.name + "'";
    return false;
  }
  for (const char ch : parsed.name) {
    if (!isNameChar(ch)) {
      error = "invalid package name '" + parsed.name + "'";
      return false;
    }
  }
  if (!parsed.version.empty()) {
    if (std::isdigit(static_cast<unsigned char>(parsed.version.front())) == 0) {
      error = "invalid version '" + parsed.version + "'; expected 0.1.0, 0.1, or latest";
      return false;
    }
    for (const char ch : parsed.version) {
      if (!isVersionChar(ch)) {
        error = "invalid version '" + parsed.version + "'";
        return false;
      }
    }
  }
  return true;
}

// ------------------------------------------------------------ version compare

struct VersionParts {
  std::vector<long long> numbers;
  std::string prerelease;
};

[[nodiscard]] VersionParts splitVersion(std::string_view version) {
  VersionParts parts;
  std::string_view text = version;
  if (const std::size_t plus = text.find('+'); plus != std::string_view::npos) {
    text = text.substr(0, plus);
  }
  if (const std::size_t dash = text.find('-'); dash != std::string_view::npos) {
    parts.prerelease = std::string(text.substr(dash + 1));
    text = text.substr(0, dash);
  }
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t dot = text.find('.', start);
    const std::string_view chunk =
        text.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
    long long value = 0;
    bool numeric = !chunk.empty();
    for (const char ch : chunk) {
      if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
        numeric = false;
        break;
      }
      value = value * 10 + (ch - '0');
    }
    parts.numbers.push_back(numeric ? value : 0);
    if (dot == std::string_view::npos) {
      break;
    }
    start = dot + 1;
  }
  return parts;
}

/// Ordering used to pick the newest release matching a version prefix.
[[nodiscard]] bool versionLess(std::string_view left, std::string_view right) {
  const VersionParts a = splitVersion(left);
  const VersionParts b = splitVersion(right);
  const std::size_t count = std::max(a.numbers.size(), b.numbers.size());
  for (std::size_t index = 0; index < count; ++index) {
    const long long av = index < a.numbers.size() ? a.numbers[index] : 0;
    const long long bv = index < b.numbers.size() ? b.numbers[index] : 0;
    if (av != bv) {
      return av < bv;
    }
  }
  if (a.prerelease.empty() != b.prerelease.empty()) {
    return !a.prerelease.empty();
  }
  return a.prerelease < b.prerelease;
}

// --------------------------------------------------------------- json replies

[[nodiscard]] std::string jsonString(const llvm::json::Object& object, std::string_view key) {
  if (const std::optional<llvm::StringRef> value = object.getString(key)) {
    return value->str();
  }
  return {};
}

[[nodiscard]] std::uintmax_t jsonBytes(const llvm::json::Object& object, std::string_view key) {
  if (const std::optional<std::int64_t> value = object.getInteger(key)) {
    return static_cast<std::uintmax_t>(*value < 0 ? 0 : *value);
  }
  return 0;
}

/// Server-provided `error` / `message` / `detail`, else the raw body.
[[nodiscard]] std::string serverMessage(const std::string& body, const std::string& fallback) {
  llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(body);
  if (parsed) {
    if (const llvm::json::Object* object = parsed->getAsObject()) {
      for (const char* key : {"error", "message", "detail"}) {
        const std::string value = jsonString(*object, key);
        if (!value.empty()) {
          return value;
        }
      }
    }
  }
  const std::string trimmed = trimCopy(body);
  return trimmed.empty() ? fallback : trimmed;
}

[[nodiscard]] std::string rateLimitNote(const RegistryResponse& response) {
  if (const std::string* retry = registryHeader(response, "retry-after")) {
    if (!retry->empty()) {
      return " (retry after " + *retry + " seconds)";
    }
  }
  return {};
}

[[nodiscard]] std::string httpFailure(const RegistryResponse& response) {
  const int status = response.status;
  if (status == 404) {
    return serverMessage(response.body, "not found");
  }
  if (status == 429) {
    return "the registry rate limited this network" + rateLimitNote(response);
  }
  if (status == 503) {
    return "the registry cannot reach package storage right now; try again later";
  }
  return "the registry returned HTTP " + std::to_string(status) +
         (trimCopy(response.body).empty() ? std::string() : ": " + trimCopy(response.body));
}

struct PackageInfo {
  std::string name;
  std::string version;
  std::string entry;
  std::string checksum;
  std::string install;
  std::string download;
  std::uintmax_t bytes = 0;
};

void applyVersionObject(const llvm::json::Object& object, PackageInfo& info) {
  const std::string name = jsonString(object, "name");
  const std::string version = jsonString(object, "version");
  const std::string entry = jsonString(object, "entry");
  const std::string checksum = jsonString(object, "checksumSha256");
  const std::string install = jsonString(object, "install");
  const std::string download = jsonString(object, "download");
  if (!name.empty()) {
    info.name = name;
  }
  if (!version.empty()) {
    info.version = version;
  }
  if (!entry.empty()) {
    info.entry = entry;
  }
  if (!checksum.empty()) {
    info.checksum = lowerCopy(checksum);
  }
  if (!install.empty()) {
    info.install = install;
  }
  if (!download.empty()) {
    info.download = download;
  }
  if (const std::uintmax_t bytes = jsonBytes(object, "bytes"); bytes != 0) {
    info.bytes = bytes;
  }
}

/// Reads `latestVersion` and every entry of a `versions` array.
void applyListing(const llvm::json::Value& value, std::string& latestVersion,
                  std::vector<std::string>& versions, std::vector<PackageInfo>& entries) {
  const llvm::json::Object* object = value.getAsObject();
  if (object == nullptr) {
    return;
  }
  latestVersion = jsonString(*object, "latestVersion");
  const llvm::json::Value* list = object->get("versions");
  if (list == nullptr) {
    return;
  }
  const llvm::json::Array* array = list->getAsArray();
  if (array == nullptr) {
    return;
  }
  for (const llvm::json::Value& item : *array) {
    if (const std::optional<llvm::StringRef> text = item.getAsString()) {
      versions.push_back(lowerCopy(text->str()));
      continue;
    }
    if (const llvm::json::Object* versionObject = item.getAsObject()) {
      PackageInfo info;
      applyVersionObject(*versionObject, info);
      if (!info.version.empty()) {
        versions.push_back(info.version);
        entries.push_back(info);
      }
    }
  }
}

/// Highest version that starts with `prefix`, skipping prereleases unless the
/// prefix itself names one.
[[nodiscard]] std::string bestPrefixMatch(const std::vector<std::string>& versions,
                                         const std::string& prefix) {
  const bool wantPrerelease = prefix.find('-') != std::string::npos;
  std::string best;
  for (const std::string& version : versions) {
    if (!version.starts_with(prefix)) {
      continue;
    }
    if (!wantPrerelease && version.find('-') != std::string::npos) {
      continue;
    }
    if (best.empty() || versionLess(best, version)) {
      best = version;
    }
  }
  return best;
}

[[nodiscard]] std::string joinVersions(const std::vector<std::string>& versions) {
  std::string text;
  for (const std::string& version : versions) {
    if (!text.empty()) {
      text += ", ";
    }
    text += version;
  }
  return text;
}

// ---------------------------------------------------------------- resolution

[[nodiscard]] std::string absoluteUrl(const std::string& base, const std::string& link) {
  if (link.starts_with("http://") || link.starts_with("https://")) {
    return link;
  }
  return link.starts_with('/') ? base + link : base + "/" + link;
}

[[nodiscard]] bool resolvePackage(const std::string& base,
                                  const PackageSpec& spec,
                                  PackageInfo& info,
                                  std::string& error) {
  const std::string packageUrl = base + "/api/packages/" + spec.name;
  const RegistryResponse listing = registryHttpGet(packageUrl);
  if (!listing.reached) {
    error = listing.error;
    return false;
  }
  if (listing.status == 404) {
    error = "no package named '" + spec.name + "'";
    return false;
  }
  if (listing.status != 200) {
    error = httpFailure(listing);
    return false;
  }
  std::string latestVersion;
  std::vector<std::string> versions;
  std::vector<PackageInfo> entries;
  if (llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(listing.body)) {
    applyListing(*parsed, latestVersion, versions, entries);
  } else {
    llvm::consumeError(parsed.takeError());
  }
  std::string wanted = spec.version;
  if (wanted.empty()) {
    wanted = latestVersion;
    if (wanted.empty()) {
      for (const std::string& version : versions) {
        if (version.find('-') == std::string::npos && (wanted.empty() || versionLess(wanted, version))) {
          wanted = version;
        }
      }
    }
    if (wanted.empty()) {
      error = "'" + spec.name + "' has no published versions yet";
      return false;
    }
  }
  for (const PackageInfo& entry : entries) {
    if (entry.version == wanted) {
      info = entry;
      break;
    }
  }
  if (info.version != wanted || info.checksum.empty()) {
    RegistryResponse detail = registryHttpGet(packageUrl + "/" + wanted);
    if (!detail.reached) {
      error = detail.error;
      return false;
    }
    if (detail.status == 404 && !spec.version.empty()) {
      // The version was written as `0.1` (or the listing knows more than the
      // exact path does): widen the selector to the newest matching release.
      const std::string matched = bestPrefixMatch(versions, spec.version);
      if (matched.empty() || matched == wanted) {
        error = "'" + spec.name + "' has no version matching '" + spec.version + "'";
        if (!versions.empty()) {
          error += "; available: " + joinVersions(versions);
        }
        return false;
      }
      wanted = matched;
      detail = registryHttpGet(packageUrl + "/" + wanted);
      if (!detail.reached) {
        error = detail.error;
        return false;
      }
    }
    if (detail.status == 404) {
      error = "no package named '" + spec.name + "' has version '" + wanted + "'";
      if (!versions.empty()) {
        error += "; available: " + joinVersions(versions);
      }
      return false;
    }
    if (detail.status != 200) {
      error = httpFailure(detail);
      return false;
    }
    llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(detail.body);
    if (!parsed) {
      llvm::consumeError(parsed.takeError());
      error = "the registry returned malformed metadata for '" + spec.name + "'";
      return false;
    }
    const llvm::json::Object* object = parsed->getAsObject();
    if (object == nullptr) {
      error = "the registry returned malformed metadata for '" + spec.name + "'";
      return false;
    }
    info = PackageInfo{};
    applyVersionObject(*object, info);
  }
  if (info.name.empty()) {
    info.name = spec.name;
  }
  if (info.version.empty()) {
    info.version = wanted;
  }
  if (info.download.empty()) {
    info.download = "/api/packages/" + spec.name + "/" + wanted + "/download";
  }
  return true;
}

// ------------------------------------------------------------------ download

struct DownloadResult {
  std::string bytes;
  std::string checksum;
};

[[nodiscard]] bool downloadPackage(const std::string& base,
                                   const PackageInfo& info,
                                   DownloadResult& result,
                                   std::string& error) {
  const std::string url = absoluteUrl(base, info.download);
  // `stream=1` keeps the archive on the API host so the checksum headers arrive
  // with the bytes instead of being lost behind a redirect.
  const std::string streamed =
      url + (url.find('?') == std::string::npos ? "?stream=1" : "&stream=1");
  RegistryResponse response = registryHttpGet(streamed, static_cast<std::size_t>(MaxArchiveBytes));
  if (!response.reached) {
    error = response.error;
    return false;
  }
  if (response.status >= 300 && response.status < 400) {
    const std::string* location = registryHeader(response, "location");
    if (location == nullptr) {
      error = "the registry redirected the download without a location";
      return false;
    }
    response = registryHttpGet(absoluteUrl(base, *location),
                               static_cast<std::size_t>(MaxArchiveBytes));
    if (!response.reached) {
      error = response.error;
      return false;
    }
  }
  if (response.status != 200) {
    if (response.status == 404) {
      error = "no download available for '" + info.name + " " + info.version + "'";
    } else {
      error = httpFailure(response);
    }
    return false;
  }
  if (response.body.empty()) {
    error = "the registry returned an empty archive for '" + info.name + "'";
    return false;
  }
  if (response.body.size() > MaxArchiveBytes) {
    error = "the archive for '" + info.name + "' is larger than the registry limit";
    return false;
  }
  if (const std::string* checksum = registryHeader(response, "x-checksum-sha256")) {
    result.checksum = lowerCopy(trimCopy(*checksum));
  }
  result.bytes = std::move(response.body);
  return true;
}

// ------------------------------------------------------------------- install

enum class PayloadKind {
  Gzip,
  Zip,
  PackedLibrary,
  Source,
};

[[nodiscard]] PayloadKind sniffPayload(const std::string& bytes) {
  const auto byteAt = [&bytes](std::size_t index) -> unsigned char {
    return index < bytes.size() ? static_cast<unsigned char>(bytes[index]) : 0;
  };
  if (byteAt(0) == 0x1f && byteAt(1) == 0x8b) {
    return PayloadKind::Gzip;
  }
  if (byteAt(0) == 'P' && byteAt(1) == 'K') {
    return PayloadKind::Zip;
  }
  if (bytes.compare(0, SlibMagic.size(), SlibMagic) == 0) {
    return PayloadKind::PackedLibrary;
  }
  return PayloadKind::Source;
}

/// Unpacks `archive` into `destination` with the first tool that succeeds.
[[nodiscard]] bool runExtractTool(const std::filesystem::path& archive,
                                 const std::filesystem::path& destination,
                                 PayloadKind kind,
                                 std::string& error) {
  struct Candidate {
    const char* program;
    std::vector<std::string> args;
  };
  std::vector<Candidate> candidates;
  if (kind == PayloadKind::Zip) {
    candidates.push_back({"unzip",
                          {"-o", "-q", archive.string(), "-d", destination.string()}});
  }
  candidates.push_back({"tar", {"-xf", archive.string(), "-C", destination.string()}});
  std::string lastError;
  for (const Candidate& candidate : candidates) {
    const llvm::ErrorOr<std::string> program = llvm::sys::findProgramByName(candidate.program);
    if (!program) {
      lastError = std::string(candidate.program) + " is not available";
      continue;
    }
    std::vector<std::string> owned;
    owned.push_back(*program);
    owned.insert(owned.end(), candidate.args.begin(), candidate.args.end());
    std::vector<llvm::StringRef> args;
    args.reserve(owned.size());
    for (const std::string& item : owned) {
      args.push_back(item);
    }
    std::string execError;
    if (llvm::sys::ExecuteAndWait(*program, args, std::nullopt, {}, 0, 0, &execError) == 0) {
      return true;
    }
    lastError = execError.empty() ? std::string(candidate.program) + " could not unpack the archive"
                                  : execError;
  }
  error = lastError.empty() ? "no tool could unpack the archive" : lastError;
  return false;
}

/// Copies an unpacked tree into `destination`, ignoring entries that would
/// escape it.
[[nodiscard]] std::size_t copyTree(const std::filesystem::path& source,
                                   const std::filesystem::path& destination,
                                   std::string& error) {
  std::error_code fsError;
  std::size_t copied = 0;
  std::filesystem::recursive_directory_iterator iterator(source, fsError);
  const std::filesystem::recursive_directory_iterator end;
  for (; iterator != end; iterator.increment(fsError)) {
    if (fsError) {
      error = "cannot read '" + source.string() + "'";
      return 0;
    }
    const std::filesystem::directory_entry& entry = *iterator;
    if (!entry.is_regular_file(fsError)) {
      continue;
    }
    const std::filesystem::path relative =
        std::filesystem::relative(entry.path(), source, fsError);
    if (fsError) {
      error = "cannot resolve '" + entry.path().string() + "'";
      return 0;
    }
    const std::string relativeText = relative.generic_string();
    if (!isSafeLibraryPath(relativeText)) {
      continue;
    }
    const std::filesystem::path target = destination / relative;
    if (!target.parent_path().empty()) {
      std::filesystem::create_directories(target.parent_path(), fsError);
    }
    std::filesystem::copy_file(entry.path(), target,
                               std::filesystem::copy_options::overwrite_existing, fsError);
    if (fsError) {
      error = "cannot write '" + target.string() + "'";
      return 0;
    }
    ++copied;
  }
  return copied;
}

[[nodiscard]] bool unpackArchive(const std::string& bytes,
                                const std::filesystem::path& destination,
                                PayloadKind kind,
                                std::string& error) {
  std::error_code fsError;
  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path(fsError) / ("sere-add-" + uniqueSuffix());
  std::filesystem::create_directories(tempRoot, fsError);
  if (fsError) {
    error = "cannot create a temporary directory";
    return false;
  }
  const std::filesystem::path archivePath = tempRoot / "package.bin";
  const std::filesystem::path unpacked = tempRoot / "unpacked";
  std::filesystem::create_directories(unpacked, fsError);
  if (!writeFileBytes(archivePath, bytes)) {
    std::filesystem::remove_all(tempRoot, fsError);
    error = "cannot stage the downloaded archive";
    return false;
  }
  if (!runExtractTool(archivePath, unpacked, kind, error)) {
    std::filesystem::remove_all(tempRoot, fsError);
    return false;
  }
  // Archives usually wrap everything in a single version directory.
  std::filesystem::path source = unpacked;
  std::vector<std::filesystem::path> children;
  std::filesystem::directory_iterator children_it(unpacked, fsError);
  const std::filesystem::directory_iterator end;
  for (; !fsError && children_it != end; children_it.increment(fsError)) {
    children.push_back(children_it->path());
  }
  if (!fsError && children.size() == 1 && std::filesystem::is_directory(children.front(), fsError)) {
    source = children.front();
  }
  if (std::filesystem::exists(destination, fsError)) {
    std::filesystem::remove_all(destination, fsError);
  }
  const std::size_t copied = copyTree(source, destination, error);
  std::filesystem::remove_all(tempRoot, fsError);
  if (copied == 0) {
    if (error.empty()) {
      error = "the archive did not contain any files";
    }
    std::filesystem::remove_all(destination, fsError);
    return false;
  }
  return true;
}

/// Writes the payload under `libs/` and returns the installed path.
[[nodiscard]] bool installPayload(const std::string& bytes,
                                  const std::filesystem::path& libs,
                                  const std::string& name,
                                  bool force,
                                  std::filesystem::path& installed,
                                  std::string& error) {
  std::error_code fsError;
  const PayloadKind kind = sniffPayload(bytes);
  std::filesystem::create_directories(libs, fsError);
  if (fsError) {
    error = "cannot create '" + libs.string() + "'";
    return false;
  }
  if (kind == PayloadKind::Gzip || kind == PayloadKind::Zip) {
    installed = libs / name;
    if (std::filesystem::exists(installed, fsError) && !force) {
      error = "'" + installed.string() + "' already exists; pass --force to replace it";
      return false;
    }
    return unpackArchive(bytes, installed, kind, error);
  }
  installed = libs / (name + (kind == PayloadKind::PackedLibrary ? ".slib" : ".sere"));
  if (std::filesystem::exists(installed, fsError) && !force) {
    error = "'" + installed.string() + "' already exists; pass --force to replace it";
    return false;
  }
  if (!writeFileBytes(installed, bytes)) {
    error = "cannot write '" + installed.string() + "'";
    return false;
  }
  return true;
}

} // namespace

int addCommand(const CompilerOptions& options) {
  std::string error;
  PackageSpec spec;
  if (!parsePackageSpec(options.packageSpec, spec, error)) {
    llvm::errs() << "error: " << error << '\n';
    llvm::errs() << "usage: sere add <name>[@<version>]\n";
    return 1;
  }
  std::error_code fsError;
  const std::optional<std::filesystem::path> root =
      findProjectRoot(std::filesystem::current_path(fsError));
  if (!root.has_value()) {
    llvm::errs() << "error: no sere.toml found; run this from a Sere project "
                    "(sere init <name>)\n";
    return 1;
  }
  ProjectManifest manifest;
  if (!loadProjectManifest(*root, manifest, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  const std::string base = registryBaseUrl();
  PackageInfo info;
  if (!resolvePackage(base, spec, info, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  DownloadResult download;
  if (!downloadPackage(base, info, download, error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  const std::string digest = sha256Hex(download.bytes);
  const std::string expected = !info.checksum.empty() ? info.checksum : download.checksum;
  if (!expected.empty() && lowerCopy(trimCopy(expected)) != digest) {
    llvm::errs() << "error: checksum mismatch for " << info.name << " " << info.version << '\n';
    llvm::errs() << "  expected: " << expected << '\n';
    llvm::errs() << "  got:      " << digest << '\n';
    llvm::errs() << "note: the download was truncated or altered; run the command again\n";
    return 1;
  }
  if (options.dryRun) {
    std::cout << "sere add --dry-run " << info.name << " " << info.version << " -> "
              << (manifest.libs / info.name).lexically_relative(*root).generic_string() << '\n';
    if (!info.entry.empty()) {
      std::cout << "  entry:  " << info.entry << '\n';
    }
    std::cout << "  bytes:  " << download.bytes.size() << '\n';
    std::cout << "  sha256: " << digest
              << (expected.empty() ? " (registry reported no checksum)" : " (verified)") << '\n';
    std::cout << "nothing installed; drop --dry-run to install\n";
    return 0;
  }
  std::filesystem::path installed;
  if (!installPayload(download.bytes, manifest.libs, info.name, options.force, installed,
                      error)) {
    llvm::errs() << "error: " << error << '\n';
    return 1;
  }
  std::cout << "installed " << info.name << " " << info.version << " -> "
            << installed.lexically_relative(*root).generic_string() << '\n';
  if (!info.entry.empty()) {
    std::cout << "  entry:  " << info.entry << '\n';
  }
  std::cout << "  sha256: " << digest
            << (expected.empty() ? " (registry reported no checksum)" : " (verified)") << '\n';
  std::cout << "  import: import " << info.name << '\n';
  return 0;
}

} // namespace sere
