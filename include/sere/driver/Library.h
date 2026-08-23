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

[[nodiscard]] bool isNativeRuntimeFile(const std::filesystem::path& path);

[[nodiscard]] bool isNativeSourceFile(const std::filesystem::path& path);

[[nodiscard]] bool isExtractedLibraryPath(const std::filesystem::path& path);

/// Entry file for a folder library: `name/name.sere` or `name/lib.sere`.
[[nodiscard]] std::filesystem::path folderLibraryEntry(const std::filesystem::path& directory);

/// Directory that owns native objects for an imported module, if any.
[[nodiscard]] std::filesystem::path libraryNativeRoot(const std::filesystem::path& importedPath);

[[nodiscard]] std::filesystem::path libraryExtractDir(const std::filesystem::path& slibPath);

void collectNativeLinkFiles(const std::filesystem::path& directory,
                            std::vector<std::filesystem::path>& libraries);

void collectNativeRuntimeFiles(const std::filesystem::path& directory,
                               std::vector<std::filesystem::path>& files);

void appendExtractedLibraryLinks(const std::vector<std::filesystem::path>& importedPaths,
                                 std::vector<std::filesystem::path>& libraries);

void appendExtractedLibraryRuntimes(const std::vector<std::filesystem::path>& importedPaths,
                                    std::vector<std::filesystem::path>& files);

[[nodiscard]] bool writePackedLibrary(const std::filesystem::path& slibPath,
                                      const PackedLibrary& library, std::string& error);

[[nodiscard]] bool readPackedLibrary(const std::filesystem::path& slibPath, PackedLibrary& library,
                                     std::string& error);

/// Extracts `slibPath` next to itself under `.sere-lib/<stem>/` when stale.
[[nodiscard]] std::filesystem::path ensureLibraryExtracted(const std::filesystem::path& slibPath,
                                                           std::string& error);

}  // namespace sere
