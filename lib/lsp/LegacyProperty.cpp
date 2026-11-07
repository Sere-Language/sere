#include "sere/lsp/LegacyProperty.h"

#include "sere/ast/Syntax.h"

#include <memory>

namespace sere {
namespace {

/// The accessor node spans its whole body; the deprecation squiggle belongs on
/// the member name written before `.get` / `.set`.
[[nodiscard]] SourceRange accessorNameRange(const FunctionDef& accessor) {
  SourceRange range = accessor.range();
  const std::uint32_t length = static_cast<std::uint32_t>(accessor.propertyName().size());
  range.end = range.start;
  range.end.offset += length;
  range.end.column += length;
  return range;
}

} // namespace

std::vector<LegacyPropertyAccessor> legacyPropertyAccessors(const Module& module) {
  std::vector<LegacyPropertyAccessor> accessors;
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr || statement->kind() != NodeKind::ClassDef ||
        statement->fromPrelude()) {
      continue;
    }
    const auto& owner = static_cast<const ClassDef&>(*statement);
    for (const std::unique_ptr<FunctionDef>& method : owner.methods()) {
      if (method == nullptr || method->propertyKind() == PropertyKind::None) {
        continue;
      }
      LegacyPropertyAccessor accessor;
      accessor.member = method->propertyName();
      accessor.isGetter = method->propertyKind() == PropertyKind::Get;
      accessor.range = accessorNameRange(*method);
      accessor.owner = owner.name();
      accessors.push_back(std::move(accessor));
    }
  }
  return accessors;
}

std::string legacyPropertyHelp(const std::string& owner, const std::string& member,
                               bool isGetter) {
  const std::string accessor = std::string(isGetter ? "self._get_" : "self._set_") + member;
  return "declare the accessor with a custom access modifier instead:\n"
         "  type @access:\n"
         "      get = @param or @public\n"
         "      set = @param or @private\n"
         "  class " +
         owner + ":\n"
         "      @access[" + (isGetter ? "get" : "set") + " = " + accessor + "] " + member +
         ": T";
}

}  // namespace sere
