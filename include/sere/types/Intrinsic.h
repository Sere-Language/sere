/// @file Intrinsic.h
/// Compiler primitives for allocation and pointer operations.

#pragma once

#include <string_view>

namespace sere {

enum class IntrinsicKind {
  None,
  UniqueNew,
  SharedNew,
  Alloc,
  Free,
  Load,
  Store,
  Len,
  Print,
  Str,
  Repr,
  Append,
  BuiltinMethod,
  ListNew,
  ArrayNew,
  DictNew,
  Range,
  TypeOf,
  IsInstance,
  Dir,
  Inspect,
  SizeOf,
  AlignOf,
  Panic,
  Super,
  Parse,
  TryParse,
};

[[nodiscard]] std::string_view intrinsicName(IntrinsicKind kind);
[[nodiscard]] IntrinsicKind intrinsicByName(std::string_view name);

}  // namespace sere
