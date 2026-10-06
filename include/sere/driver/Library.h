/// @file Library.h
/// Single-file Sere libraries (.slib): pack, extract, and native artifacts.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

struct LibraryMember {
  std::string relativePath;
  std::string bytes;
};

struct PackedLibrary {
  std::string name = "sere-lib";
  std::string version = "0.1.0";
  std::string entry;
  std::vector<LibraryMember> files;
};

[[nodiscard]] bool isSafeLibraryPath(std::string_view relativePath);

[[nodiscard]] bool isSereLibraryFile(const std::filesystem::path& path);

[[nodiscard]] bool isNativeLinkFile(const std::filesystem::path& path);

/// A bare object file (`.o` / `.obj`), which a package ships instead of source.
[[nodiscard]] bool isNativeObjectFile(const std::filesystem::path& path);

/// Metadata a packed library carries for its consumer: one system library name
/// per line, the packages it depends on, and the executables it installs.
inline constexpr std::string_view kNativeDepsFile = "sere-native-deps.txt";
inline constexpr std::string_view kPackageBinsFile = "sere-package-bins.txt";

[[nodiscard]] bool isNativeRuntimeFile(const std::filesystem::path& path);

[[nodiscard]] bool isNativeSourceFile(const std::filesystem::path& path);

/// A native header (`*.h`, `*.hpp`, ...). Headers travel with native sources so
/// a package built from source can include its own declarations.
[[nodiscard]] bool isNativeHeaderFile(const std::filesystem::path& path);

/// Lowercase hex SHA-256 of a byte range, used for `.slib` integrity checks.
[[nodiscard]] std::string sha256Hex(std::string_view data);

[[nodiscard]] bool isExtractedLibraryPath(const std::filesystem::path& path);

/// Entry file for a folder library: `__init__.sere`, `name.sere`, then `lib.sere`.
[[nodiscard]] std::filesystem::path folderLibraryEntry(const std::filesystem::path& directory);

/// Directory that owns native objects for an imported module, if any.
[[nodiscard]] std::filesystem::path libraryNativeRoot(const std::filesystem::path& importedPath);

[[nodiscard]] std::filesystem::path libraryExtractDir(const std::filesystem::path& slibPath);

void collectNativeLinkFiles(const std::filesystem::path& directory,
                            std::vector<std::filesystem::path>& libraries);

void collectNativeRuntimeFiles(const std::filesystem::path& directory,
                               std::vector<std::filesystem::path>& files);

/// Collects native headers under a directory, for packages that ship sources.
void collectNativeHeaderFiles(const std::filesystem::path& directory,
                              std::vector<std::filesystem::path>& files);

/// Collects compiled object files under a directory.
void collectNativeObjectFiles(const std::filesystem::path& directory,
                              std::vector<std::filesystem::path>& files);

void appendExtractedLibraryLinks(const std::vector<std::filesystem::path>& importedPaths,
                                 std::vector<std::filesystem::path>& libraries);

void appendExtractedLibraryRuntimes(const std::vector<std::filesystem::path>& importedPaths,
                                    std::vector<std::filesystem::path>& files);

/// System libraries named by the packages that were imported, in order, without
/// duplicates. The compiler resolves each one for the host before linking.
[[nodiscard]] std::vector<std::string>
packageSystemLibraries(const std::vector<std::filesystem::path>& importedPaths);

/// Executables a package installs, as `source`/`destination` pairs inside the
/// package that was extracted for each imported module.
[[nodiscard]] std::vector<std::filesystem::path>
packageExecutables(const std::vector<std::filesystem::path>& importedPaths);

[[nodiscard]] bool writePackedLibrary(const std::filesystem::path& slibPath,
                                      const PackedLibrary& library,
                                      std::string& error);

[[nodiscard]] bool readPackedLibrary(const std::filesystem::path& slibPath,
                                     PackedLibrary& library,
                                     std::string& error);

/// Extracts `slibPath` next to itself under `.sere-lib/<stem>/` when stale.
[[nodiscard]] std::filesystem::path ensureLibraryExtracted(const std::filesystem::path& slibPath,
                                                           std::string& error);

} // namespace sere
