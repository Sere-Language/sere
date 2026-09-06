/// @file SourceLocation.h
/// Source coordinates used by diagnostics, tokens, and AST nodes.

#pragma once

#include <cstdint>

namespace sere {

struct SourceLocation {
  std::uint32_t offset = 0;
  std::uint32_t line = 1;
  std::uint32_t column = 1;
};

struct SourceRange {
  SourceLocation start{};
  SourceLocation end{};
};

} // namespace sere
