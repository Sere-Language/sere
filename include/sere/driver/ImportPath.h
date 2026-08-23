/// @file ImportPath.h
/// Shared import search and module discovery for the compiler and language server.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

struct ImportModuleEntry {
  std::string dottedName;
  std::string lastSegment;
  std::string detail;
  bool isPackage = false;
};

[[nodiscard]] std::vector<std::string> splitImportPath(std::string_view dotted);

[[nodiscard]] std::string joinImportPath(const std::vector<std::string>& parts);

[[nodiscard]] std::vector<std::filesystem::path> importSearchDirs(
    const std::filesystem::path& originDir, const std::filesystem::path& stdlibDir);

void appendImportSearchDir(std::vector<std::filesystem::path>& dirs,
                           const std::filesystem::path& directory);

void appendWorkspaceImportDirs(std::vector<std::filesystem::path>& dirs,
                               const std::filesystem::path& workspaceRoot);

[[nodiscard]] std::filesystem::path resolveImportFile(
    const std::vector<std::filesystem::path>& searchDirs, const std::vector<std::string>& parts,
    const std::filesystem::path& skipFile = {});

[[nodiscard]] std::vector<ImportModuleEntry> listImportModules(
    const std::vector<std::filesystem::path>& searchDirs,
    const std::filesystem::path& stdlibDir,
    std::string_view typedPath,
    const std::filesystem::path& skipFile = {});

}  // namespace sere
