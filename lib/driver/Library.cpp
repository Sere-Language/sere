/// @file Library.cpp
/// Packs Sere sources and native objects into a drop-in .slib file.

#include "sere/driver/Library.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Compression.h>
#include <llvm/Support/Error.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <map>
#include <string_view>
#include <system_error>

namespace sere {
namespace {

constexpr std::string_view kMagicV1 = "SERELIB/1";
constexpr std::string_view kMagicV2 = "SERELIB/2";
/// Version 3 adds a SHA-256 line per member and writes members in path order, so
/// two packs of the same content produce the same bytes.
constexpr std::string_view kMagicV3 = "SERELIB/3";
constexpr std::string_view kExtractStamp = ".extracted";

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), used for `.slib` member integrity checks.
// ---------------------------------------------------------------------------

struct Sha256 {
  std::uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  std::uint64_t bitCount = 0;
  std::uint8_t buffer[64] = {};
  std::size_t buffered = 0;

  void absorbBlock(const std::uint8_t* block) {
    static constexpr std::uint32_t kRound[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
        0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
        0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
        0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
        0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
        0xc67178f2u};
    const auto rotate = [](std::uint32_t value, unsigned bits) {
      return (value >> bits) | (value << (32u - bits));
    };
    std::uint32_t words[64] = {};
    for (std::size_t index = 0; index < 16; ++index) {
      words[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24) |
                     (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16) |
                     (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8) |
                     static_cast<std::uint32_t>(block[index * 4 + 3]);
    }
    for (std::size_t index = 16; index < 64; ++index) {
      const std::uint32_t s0 = rotate(words[index - 15], 7) ^ rotate(words[index - 15], 18) ^
                               (words[index - 15] >> 3);
      const std::uint32_t s1 = rotate(words[index - 2], 17) ^ rotate(words[index - 2], 19) ^
                               (words[index - 2] >> 10);
      words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];
    for (std::size_t index = 0; index < 64; ++index) {
      const std::uint32_t s1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
      const std::uint32_t choose = (e & f) ^ (~e & g);
      const std::uint32_t temp1 = h + s1 + choose + kRound[index] + words[index];
      const std::uint32_t s0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = s0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
  }

  void update(const char* data, std::size_t size) {
    bitCount += static_cast<std::uint64_t>(size) * 8u;
    for (std::size_t index = 0; index < size; ++index) {
      buffer[buffered++] = static_cast<std::uint8_t>(data[index]);
      if (buffered == 64) {
        absorbBlock(buffer);
        buffered = 0;
      }
    }
  }

  [[nodiscard]] std::string finish() {
    const std::uint64_t bits = bitCount;
    const std::uint8_t pad = 0x80;
    update(reinterpret_cast<const char*>(&pad), 1);
    const std::uint8_t zero = 0;
    while (buffered != 56) {
      update(reinterpret_cast<const char*>(&zero), 1);
    }
    std::uint8_t length[8] = {};
    for (int index = 0; index < 8; ++index) {
      length[7 - index] = static_cast<std::uint8_t>((bits >> (index * 8)) & 0xffu);
    }
    update(reinterpret_cast<const char*>(length), 8);
    static const char kHex[] = "0123456789abcdef";
    std::string text;
    text.reserve(64);
    for (int index = 0; index < 8; ++index) {
      for (int shift = 28; shift >= 0; shift -= 4) {
        text.push_back(kHex[(state[index] >> shift) & 0xfu]);
      }
    }
    return text;
  }
};

[[nodiscard]] std::string lowerHex(std::string_view text) {
  std::string out(text);
  for (char& ch : out) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

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

bool isNativeHeaderFile(const std::filesystem::path& path) {
  const std::string ext = path.extension().string();
  return ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx" || ext == ".inc";
}

std::string sha256Hex(std::string_view data) {
  Sha256 hash;
  hash.update(data.data(), data.size());
  return hash.finish();
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
  const std::filesystem::path init = directory / "__init__.sere";
  const std::filesystem::path named = directory / (name + ".sere");
  const std::filesystem::path lib = directory / "lib.sere";
  if (std::filesystem::is_regular_file(init, error)) {
    return std::filesystem::weakly_canonical(init, error);
  }
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
  if (stem == "__init__" || stem == "lib" || stem == dirName) {
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
  // Members are written in path order and each one records its hash, so packing
  // the same content twice produces the same archive bytes.
  std::vector<const LibraryMember*> members;
  members.reserve(library.files.size());
  for (const LibraryMember& file : library.files) {
    members.push_back(&file);
  }
  std::sort(members.begin(), members.end(), [](const LibraryMember* left,
                                               const LibraryMember* right) {
    return left->relativePath < right->relativePath;
  });
  const bool canCompress = zlibAvailable();
  output << kMagicV3 << '\n';
  output << "name=" << library.name << '\n';
  output << "version=" << library.version << '\n';
  output << "entry=" << library.entry << '\n';
  if (canCompress) {
    output << "encoding=zlib\n";
  }
  output << "hashes=sha256\n";
  for (const LibraryMember* file : members) {
    output << "HASH " << file->relativePath << ' ' << sha256Hex(file->bytes) << '\n';
  }
  output << '\n';
  for (const LibraryMember* file : members) {
    std::string packed;
    const bool compressed = canCompress && zlibCompress(file->bytes, packed);
    const std::string& payload = compressed ? packed : file->bytes;
    if (canCompress) {
      output << "FILE " << file->relativePath << ' ' << file->bytes.size() << ' '
             << payload.size() << '\n';
    } else {
      output << "FILE " << file->relativePath << ' ' << file->bytes.size() << '\n';
    }
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!output) {
      error = "cannot write member '" + file->relativePath + "'";
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
  if (magic != kMagicV1 && magic != kMagicV2 && magic != kMagicV3) {
    if (magic.starts_with("SERELIB/")) {
      error = "'" + slibPath.string() +
              "' uses a newer .slib format than this compiler understands; update sere";
      return false;
    }
    error = "'" + slibPath.string() + "' is not a Sere library (.slib)";
    return false;
  }
  library = PackedLibrary{};
  std::string encoding = "raw";
  bool hashesRecorded = false;
  while (std::getline(input, line)) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty()) {
      break;
    }
    if (trimmed == "hashes=sha256") {
      hashesRecorded = true;
      continue;
    }
    if (!parseMetaLine(trimmed, library, encoding)) {
      error = "invalid library header in '" + slibPath.string() + "'";
      return false;
    }
  }
  std::map<std::string, std::string> recorded;
  bool firstMember = true;
  std::string pending;
  const bool zlibMembers = encoding == "zlib";
  while (true) {
    if (firstMember) {
      firstMember = false;
    } else if (!std::getline(input, pending)) {
      break;
    }
    if (trimCopy(pending).empty()) {
      continue;
    }
    std::string current = trimCopy(pending);
    while (hashesRecorded && current.starts_with("HASH ")) {
      const std::string_view rest = std::string_view(current).substr(5);
      const std::size_t split = rest.find_last_of(" \t");
      if (split == std::string_view::npos) {
        error = "invalid HASH line in '" + slibPath.string() + "'";
        return false;
      }
      recorded[trimCopy(rest.substr(0, split))] = lowerHex(trimCopy(rest.substr(split + 1)));
      if (!std::getline(input, pending)) {
        error = "library is missing its entry module";
        return false;
      }
      current = trimCopy(pending);
      if (current.empty()) {
        error = "library is missing its entry module";
        return false;
      }
    }
    std::string relativePath;
    std::uint64_t rawSize = 0;
    std::uint64_t packedSize = 0;
    if (!parseFileHeader(current, relativePath, rawSize, packedSize)) {
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
    if (hashesRecorded) {
      const auto found = recorded.find(member.relativePath);
      if (found == recorded.end()) {
        error = "library member '" + member.relativePath + "' has no recorded hash";
        return false;
      }
      if (sha256Hex(member.bytes) != found->second) {
        error = "library member '" + member.relativePath +
                "' failed its integrity check (the archive is damaged or was rebuilt)";
        return false;
      }
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
