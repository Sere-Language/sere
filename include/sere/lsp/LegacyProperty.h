/// @file LegacyProperty.h
/// Locates members declared with the deprecated `name.get:` / `name.set(value)`
/// accessor syntax. Tooling reports them as deprecated and points at the
/// replacement, a custom access modifier with `get` / `set` accessor slots.

#pragma once

#include "sere/source/SourceLocation.h"

#include <string>
#include <vector>

namespace sere {

class Module;

struct LegacyPropertyAccessor {
  /// Member the accessor reads or writes (`x` for `x.get:`).
  std::string member;
  bool isGetter = false;
  /// The member name in the accessor declaration, used for the warning squiggle
  /// and the deprecated semantic highlight.
  SourceRange range{};
  /// The owning type, so the warning can say which member to migrate.
  std::string owner;
};

/// Every legacy property accessor declared in `module`, in source order.
[[nodiscard]] std::vector<LegacyPropertyAccessor> legacyPropertyAccessors(const Module& module);

/// Help text handed to the editor with each deprecation warning: how the same
/// accessors are declared with a custom access modifier.
[[nodiscard]] std::string legacyPropertyHelp(const std::string& owner, const std::string& member,
                                             bool isGetter);

}  // namespace sere
