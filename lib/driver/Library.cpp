/// @file Library.cpp
/// Packs Sere sources and native objects into a drop-in .slib file.

#include "sere/driver/Library.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Compression.h>
#include <llvm/Support/Error.h>

#include <cctype>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <system_error>

namespace sere {
namespace {

constexpr std::string_view kMagicV1 = "SERELIB/1";
constexpr std::string_view kMagicV2 = "SERELIB/2";
constexpr std::string_view kExtractStamp = ".extracted";

[[nodiscard]] bool namesEqual(const std::filesystem::path& left,
                              const std::filesystem::path& right) {
  std::error_code leftError;
  std::error_code rightError;
  const std::filesystem::path a = std::filesystem::weakly_canonical(left, leftError);
  const std::filesystem::path b = std::filesystem::weakly_canonical(right, rightError);
  return !leftError && !rightError && a == b;
}

[[nodiscard]] bool skipCollectDir(const std::filesystem::path& path) {
  const std::string text = path.generic_string();
  return text.find("/CMakeFiles/") != std::string::npos ||
         text.find("\\CMakeFiles\\") != std::string::npos ||
         text.find("/.sere-lib/") != std::string::npos ||
         text.find("\\.sere-lib\\") != std::string::npos;
}

[[nodiscard]] bool pathHasPart(const std::filesystem::path& path, std::string_view name) {
  for (const std::filesystem::path& part : path) {
    if (part.string() == name) {
      return true;
    }
  }
  return false;
}

void addUniquePath(std::vector<std::filesystem::path>& files, const std::filesystem::path& path) {
  for (const std::filesystem::path& existing : files) {
    if (namesEqual(existing, path)) {
      return;
    }
  }
  files.push_back(path);
}

[[nodiscard]] std::string trimCopy(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return std::string(text);
}

[[nodiscard]] bool readExact(std::istream& input, std::size_t size, std::string& out) {
  out.assign(size, '\0');
  if (size == 0) {
    return true;
  }
  input.read(out.data(), static_cast<std::streamsize>(size));
  return static_cast<bool>(input) && input.gcount() == static_cast<std::streamsize>(size);
}

[[nodiscard]] bool parseDecimal(std::string_view text, std::uint64_t& value) {
  if (text.empty()) {
    return false;
  }
  value = 0;
  for (const char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
    if (value > (UINT64_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  return true;
}

[[nodiscard]] bool parseFileHeader(std::string_view line,
                                   std::string& relativePath,
                                   std::uint64_t& rawSize,
                                   std::uint64_t& packedSize) {
  if (!line.starts_with("FILE ")) {
    return false;
  }
  const std::string_view rest = line.substr(5);
  const std::size_t last = rest.find_last_of(" \t");
  if (last == std::string_view::npos) {
    return false;
  }
  const std::string lastText = trimCopy(rest.substr(last + 1));
  const std::string_view head = rest.substr(0, last);
  const std::size_t middle = head.find_last_of(" \t");
  if (middle == std::string_view::npos) {
    relativePath = trimCopy(head);
    if (relativePath.empty() || !parseDecimal(lastText, rawSize)) {
      return false;
    }
    packedSize = rawSize;
    return true;
  }
  relativePath = trimCopy(head.substr(0, middle));
  const std::string rawText = trimCopy(head.substr(middle + 1));
  return !relativePath.empty() && parseDecimal(rawText, rawSize) &&
         parseDecimal(lastText, packedSize);
}

[[nodiscard]] bool
parseMetaLine(std::string_view line, PackedLibrary& library, std::string& encoding) {
  const std::size_t eq = line.find('=');
  if (eq == std::string_view::npos) {
    return false;
  }
  const std::string key = trimCopy(line.substr(0, eq));
  const std::string value = trimCopy(line.substr(eq + 1));
  if (key == "name" && !value.empty()) {
    library.name = value;
    return true;
  }
  if (key == "version" && !value.empty()) {
    library.version = value;
    return true;
  }
  if (key == "entry" && !value.empty()) {
    library.entry = value;
    return true;
  }
  if (key == "encoding") {
    encoding = value;
    return true;
  }
  return false;
}

[[nodiscard]] bool zlibAvailable() {
  return llvm::compression::zlib::isAvailable();
}

[[nodiscard]] bool zlibCompress(const std::string& input, std::string& output) {
  if (!zlibAvailable() || input.empty()) {
    return false;
  }
  llvm::SmallVector<std::uint8_t, 0> compressed;
  llvm::compression::zlib::compress(
      llvm::ArrayRef<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(input.data()),
                                   input.size()),
      compressed,
      llvm::compression::zlib::BestSizeCompression);
  if (compressed.empty() || compressed.size() >= input.size()) {
    return false;
  }
  output.assign(reinterpret_cast<const char*>(compressed.data()), compressed.size());
  return true;
}

[[nodiscard]] bool
zlibDecompress(const std::string& input, std::size_t rawSize, std::string& output) {
  if (!zlibAvailable()) {
    return false;
  }
  llvm::SmallVector<std::uint8_t, 0> raw;
  llvm::Error error = llvm::compression::zlib::decompress(
      llvm::ArrayRef<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(input.data()),
                                   input.size()),
      raw,
      rawSize);
  if (error) {
    llvm::consumeError(std::move(error));
    return false;
  }
  output.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
  return output.size() == rawSize;
}

[[nodiscard]] bool hasEntry(const PackedLibrary& library) {
  for (const LibraryMember& file : library.files) {
    if (file.relativePath == library.entry) {
      return true;
    }
  }
  return false;
}

void collectByPredicate(const std::filesystem::path& directory,
                        bool (*accept)(const std::filesystem::path&),
                        std::vector<std::filesystem::path>& files) {
  std::error_code error;
  if (!std::filesystem::exists(directory, error)) {
    return;
  }
  const std::filesystem::recursive_directory_iterator end;
  for (std::filesystem::recursive_directory_iterator it(directory, error); it != end;
       it.increment(error)) {
    if (error) {
      break;
    }
    const std::filesystem::path path = it->path();
    if (!it->is_regular_file(error) || skipCollectDir(path) || !accept(path)) {
      continue;
    }
    addUniquePath(files, path);
  }
}

[[nodiscard]] std::filesystem::path extractedRoot(const std::filesystem::path& importedPath) {
  std::filesystem::path current = importedPath.parent_path();
  while (!current.empty() && current != current.parent_path()) {
    if (current.parent_path().filename() == ".sere-lib") {
      return current;
    }
    current = current.parent_path();
  }
  return {};
}

[[nodiscard]] bool extractUpToDate(const std::filesystem::path& slibPath,
                                   const std::filesystem::path& dest,
                                   const std::filesystem::path& entry) {
  std::error_code error;
  const std::filesystem::path stamp = dest / kExtractStamp;
  if (!std::filesystem::exists(dest / entry, error) || !std::filesystem::exists(stamp, error)) {
    return false;
  }
  const auto slibTime = std::filesystem::last_write_time(slibPath, error);
  if (error) {
    return false;
  }
  const auto stampTime = std::filesystem::last_write_time(stamp, error);
  return !error && stampTime >= slibTime;
}

} // namespace

bool isSafeLibraryPath(std::string_view relativePath) {
  if (relativePath.empty() || relativePath.starts_with('/') || relativePath.starts_with('\\')) {
    return false;
  }
  if (relativePath.find(':') != std::string_view::npos) {
    return false;
  }
  std::string part;
  for (const char ch : relativePath) {
    if (ch == '/' || ch == '\\') {
      if (part == "..") {
        return false;
      }
      part.clear();
      continue;
    }
    part.push_back(ch);
  }
  return part != "..";
}

bool isSereLibraryFile(const std::filesystem::path& path) {
  return path.extension() == ".slib";
}

bool isNativeLinkFile(const std::filesystem::path& path) {
  const std::string ext = path.extension().string();
  return ext == ".lib" || ext == ".a";
}

bool isNativeRuntimeFile(const std::filesystem::path& path) {
  const std::string ext = path.extension().string();
  return ext == ".dll" || ext == ".so" || ext == ".dylib";
}

bool isNativeSourceFile(const std::filesystem::path& path) {
  const std::string ext = path.extension().string();
  return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx";
}

bool isExtractedLibraryPath(const std::filesystem::path& path) {
  return pathHasPart(path, ".sere-lib");
}

std::filesystem::path folderLibraryEntry(const std::filesystem::path& directory) {
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error) || error) {
    return {};
  }
  const std::string name = directory.filename().string();
  const std::filesystem::path named = directory / (name + ".sere");
  const std::filesystem::path lib = directory / "lib.sere";
  if (std::filesystem::is_regular_file(named, error)) {
    return std::filesystem::weakly_canonical(named, error);
  }
  if (std::filesystem::is_regular_file(lib, error)) {
    return std::filesystem::weakly_canonical(lib, error);
  }
  return {};
}

std::filesystem::path libraryNativeRoot(const std::filesystem::path& importedPath) {
  const std::filesystem::path extracted = extractedRoot(importedPath);
  if (!extracted.empty()) {
    return extracted;
  }
  const std::filesystem::path parent = importedPath.parent_path();
  if (parent.empty()) {
    return {};
  }
  const std::string stem = importedPath.stem().string();
  const std::string dirName = parent.filename().string();
  if (stem == "lib" || stem == dirName) {
    return parent;
  }
  return {};
}

std::filesystem::path libraryExtractDir(const std::filesystem::path& slibPath) {
  return slibPath.parent_path() / ".sere-lib" / slibPath.stem();
}

void collectNativeLinkFiles(const std::filesystem::path& directory,
                            std::vector<std::filesystem::path>& libraries) {
  collectByPredicate(directory, isNativeLinkFile, libraries);
}

void collectNativeRuntimeFiles(const std::filesystem::path& directory,
                               std::vector<std::filesystem::path>& files) {
  collectByPredicate(directory, isNativeRuntimeFile, files);
}

void collectSiblingNative(const std::filesystem::path& importedPath,
                          bool (*accept)(const std::filesystem::path&),
                          std::vector<std::filesystem::path>& files) {
  const std::filesystem::path parent = importedPath.parent_path();
  if (parent.empty()) {
    return;
  }
  const std::string stem = importedPath.stem().string();
  std::error_code error;
  for (const char* ext : {".lib", ".a", ".dll", ".so", ".dylib"}) {
    const std::filesystem::path sibling = parent / (stem + ext);
    if (std::filesystem::is_regular_file(sibling, error) && accept(sibling)) {
      addUniquePath(files, sibling);
    }
  }
  collectByPredicate(parent / "native", accept, files);
  const std::filesystem::path root = libraryNativeRoot(importedPath);
  if (!root.empty()) {
    collectByPredicate(root, accept, files);
  }
}

void appendExtractedLibraryLinks(const std::vector<std::filesystem::path>& importedPaths,
                                 std::vector<std::filesystem::path>& libraries) {
  for (const std::filesystem::path& imported : importedPaths) {
    collectSiblingNative(imported, isNativeLinkFile, libraries);
  }
}

void appendExtractedLibraryRuntimes(const std::vector<std::filesystem::path>& importedPaths,
                                    std::vector<std::filesystem::path>& files) {
  for (const std::filesystem::path& imported : importedPaths) {
    collectSiblingNative(imported, isNativeRuntimeFile, files);
  }
}

bool writePackedLibrary(const std::filesystem::path& slibPath,
                        const PackedLibrary& library,
                        std::string& error) {
  if (library.entry.empty() || library.files.empty()) {
    error = "library has no entry file";
    return false;
  }
  if (!isSafeLibraryPath(library.entry) || !hasEntry(library)) {
    error = "library entry '" + library.entry + "' is missing or unsafe";
    return false;
  }
  for (const LibraryMember& file : library.files) {
    if (!isSafeLibraryPath(file.relativePath)) {
      error = "unsafe library path '" + file.relativePath + "'";
      return false;
    }
  }
  std::error_code fsError;
  std::filesystem::create_directories(slibPath.parent_path(), fsError);
  std::ofstream output(slibPath, std::ios::binary | std::ios::trunc);
  if (!output) {
    error = "cannot write '" + slibPath.string() + "'";
    return false;
  }
  const bool canCompress = zlibAvailable();
  output << (canCompress ? kMagicV2 : kMagicV1) << '\n';
  output << "name=" << library.name << '\n';
  output << "version=" << library.version << '\n';
  output << "entry=" << library.entry << '\n';
  if (canCompress) {
    output << "encoding=zlib\n";
  }
  output << '\n';
  for (const LibraryMember& file : library.files) {
    std::string packed;
    const bool compressed = canCompress && zlibCompress(file.bytes, packed);
    const std::string& payload = compressed ? packed : file.bytes;
    if (canCompress) {
      output << "FILE " << file.relativePath << ' ' << file.bytes.size() << ' ' << payload.size()
             << '\n';
    } else {
      output << "FILE " << file.relativePath << ' ' << file.bytes.size() << '\n';
    }
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!output) {
      error = "cannot write member '" + file.relativePath + "'";
      return false;
    }
  }
  return true;
}

bool readPackedLibrary(const std::filesystem::path& slibPath,
                       PackedLibrary& library,
                       std::string& error) {
  std::ifstream input(slibPath, std::ios::binary);
  if (!input) {
    error = "cannot read '" + slibPath.string() + "'";
    return false;
  }
  std::string line;
  if (!std::getline(input, line)) {
    error = "'" + slibPath.string() + "' is not a Sere library (.slib)";
    return false;
  }
  const std::string magic = trimCopy(line);
  if (magic != kMagicV1 && magic != kMagicV2) {
    error = "'" + slibPath.string() + "' is not a Sere library (.slib)";
    return false;
  }
  library = PackedLibrary{};
  std::string encoding = "raw";
  while (std::getline(input, line)) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
      break;
    }
    if (!parseMetaLine(trimmed, library, encoding)) {
      error = "invalid library header in '" + slibPath.string() + "'";
      return false;
    }
  }
  const bool zlibMembers = encoding == "zlib";
  while (true) {
    if (!std::getline(input, line)) {
      break;
    }
    if (trimCopy(line).empty()) {
      continue;
    }
    std::string relativePath;
    std::uint64_t rawSize = 0;
    std::uint64_t packedSize = 0;
    if (!parseFileHeader(trimCopy(line), relativePath, rawSize, packedSize)) {
      error = "invalid FILE header in '" + slibPath.string() + "'";
      return false;
    }
    if (!isSafeLibraryPath(relativePath)) {
      error = "unsafe path '" + relativePath + "' in '" + slibPath.string() + "'";
      return false;
    }
    if (rawSize > 64ull * 1024ull * 1024ull || packedSize > 64ull * 1024ull * 1024ull) {
      error = "library member '" + relativePath + "' is too large";
      return false;
    }
    LibraryMember member;
    member.relativePath = std::move(relativePath);
    std::string payload;
    if (!readExact(input, static_cast<std::size_t>(packedSize), payload)) {
      error = "truncated library member '" + member.relativePath + "'";
      return false;
    }
    if (zlibMembers && packedSize != rawSize) {
      if (!zlibDecompress(payload, static_cast<std::size_t>(rawSize), member.bytes)) {
        error = "cannot decompress '" + member.relativePath + "'";
        return false;
      }
    } else {
      member.bytes = std::move(payload);
    }
    library.files.push_back(std::move(member));
  }
  if (library.entry.empty() || !hasEntry(library)) {
    error = "library is missing its entry module";
    return false;
  }
  if (library.name.empty()) {
    library.name = slibPath.stem().string();
  }
  return true;
}

std::filesystem::path ensureLibraryExtracted(const std::filesystem::path& slibPath,
                                             std::string& error) {
  PackedLibrary library;
  if (!readPackedLibrary(slibPath, library, error)) {
    return {};
  }
  const std::filesystem::path dest = libraryExtractDir(slibPath);
  if (extractUpToDate(slibPath, dest, library.entry)) {
    std::error_code errorCode;
    return std::filesystem::weakly_canonical(dest / library.entry, errorCode);
  }
  std::error_code fsError;
  std::filesystem::remove_all(dest, fsError);
  std::filesystem::create_directories(dest, fsError);
  if (fsError) {
    error = "cannot extract library to '" + dest.string() + "'";
    return {};
  }
  for (const LibraryMember& file : library.files) {
    const std::filesystem::path out = dest / file.relativePath;
    std::filesystem::create_directories(out.parent_path(), fsError);
    std::ofstream output(out, std::ios::binary | std::ios::trunc);
    if (!output) {
      error = "cannot extract '" + file.relativePath + "'";
      return {};
    }
    output.write(file.bytes.data(), static_cast<std::streamsize>(file.bytes.size()));
  }
  std::ofstream stamp(dest / kExtractStamp, std::ios::binary | std::ios::trunc);
  stamp << library.name << '\n' << library.version << '\n';
  std::error_code errorCode;
  return std::filesystem::weakly_canonical(dest / library.entry, errorCode);
}

} // namespace sere
