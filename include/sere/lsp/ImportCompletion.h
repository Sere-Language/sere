/// @file ImportCompletion.h
/// Context detection and candidates for `import` / `from` autocomplete.

#pragma once

#include "sere/driver/ImportPath.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

enum class ImportCompletionKind {
  None,
  ModulePath,
  FromImportKeyword,
  ImportAsKeyword,
  FromNames,
};

enum class ImportItemKind {
  Module,
  Package,
  Function,
  Class,
  Struct,
  Enum,
  Type,
  Macro,
  Variable,
  Keyword,
};

struct ImportCompletionQuery {
  ImportCompletionKind kind = ImportCompletionKind::None;
  std::string typedPath;
  std::string modulePath;
  std::string prefix;
};

struct ImportCompletionItem {
  std::string label;
  std::string detail;
  std::string sortText;
  std::string insertText;
  ImportItemKind kind = ImportItemKind::Module;
};

[[nodiscard]] ImportCompletionQuery detectImportCompletion(std::string_view text,
                                                           std::uint32_t offset);

[[nodiscard]] std::vector<ImportCompletionItem> importModuleCompletions(
    const std::vector<std::filesystem::path>& searchDirs,
    const std::filesystem::path& stdlibDir,
    std::string_view typedPath,
    const std::filesystem::path& skipFile = {});

[[nodiscard]] std::vector<ImportCompletionItem> importExportCompletions(
    const std::filesystem::path& moduleFile,
    std::string_view prefix,
    const std::optional<std::string>& overlayText = std::nullopt);

}  // namespace sere
