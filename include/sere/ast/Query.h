/// @file Query.h
/// AST lookup helpers for hover and completion.

#pragma once

#include "sere/ast/Syntax.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

struct MacroUse {
  std::string name;
  SourceRange nameRange{};
  SourceRange range{};
  MacroDelimiter delimiter = MacroDelimiter::BangParen;
};

[[nodiscard]] bool rangeContains(SourceRange range, std::uint32_t offset);
[[nodiscard]] SourceRange identifierRange(SourceRange start, std::string_view name);
[[nodiscard]] const Node* findNodeAt(const Node& root, std::uint32_t offset);
[[nodiscard]] const CallExpr* findCallAt(const Node& root, std::uint32_t offset);
[[nodiscard]] const MacroUse* findMacroUseAt(const std::vector<MacroUse>& uses,
                                             std::uint32_t offset);
[[nodiscard]] const MacroUse* findMacroNameAt(const std::vector<MacroUse>& uses,
                                              std::uint32_t offset);
void collectCalls(const Node& root, std::vector<const CallExpr*>& out);
void collectNameRefs(const Node& root, std::string_view name, std::vector<SourceRange>& out);
void collectMacroUses(const Node& root, std::vector<MacroUse>& out);
[[nodiscard]] std::string formatMacro(const MacroDef& def);
[[nodiscard]] std::string macroSnippet(const MacroDef& def);
[[nodiscard]] SourceRange macroNameRange(const MacroDef& def);

} // namespace sere
