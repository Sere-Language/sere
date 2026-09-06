/// @file Intrinsic.h
/// Compiler primitives for allocation and pointer operations.
///
/// Every intrinsic is described by a single declarative registry (see
/// Intrinsic.cpp) rather than by scattered switch/if chains. Adding a primitive
/// is a one-row change in that table; semantic analysis, name lookup, and LSP
/// highlighting all derive from it, so there are no duplicated name or
/// parameter lists to keep in sync.

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace sere {

/// Identifies a compiler primitive. Append new primitives here AND as a row in
/// the registry table in Intrinsic.cpp (rows are laid out in enum order). The
/// explicit narrow underlying type keeps the value compact and documented.
enum class IntrinsicKind : std::uint8_t {
  None,          // Sentinel: not an intrinsic call.
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
  BuiltinMethod, // Synthetic kind for lowered method calls; never a named symbol.
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

/// Declarative description of one intrinsic. All per-intrinsic metadata lives
/// in this single row; consumers must not keep their own name/parameter lists.
struct IntrinsicInfo {
  IntrinsicKind kind = IntrinsicKind::None;
  /// Source name as written in .sere source. Empty for kinds without a
  /// user-facing name (IntrinsicKind::None, IntrinsicKind::BuiltinMethod).
  std::string_view name;
  /// Space-separated parameter names used for signatures and hover text; empty
  /// when the intrinsic declares no named parameters.
  std::string_view params;
  /// Whether the intrinsic is registered as a top-level builtin symbol.
  bool declared = false;
};

/// The full registry table, laid out in IntrinsicKind order.
[[nodiscard]] std::span<const IntrinsicInfo> allIntrinsics();

/// Descriptor for a kind. Out-of-range values (e.g. an enum member added
/// without a matching table row) return the IntrinsicKind::None sentinel row
/// instead of reading out of bounds, so a forgotten row degrades safely.
[[nodiscard]] const IntrinsicInfo& intrinsicInfo(IntrinsicKind kind);

/// Source name for a kind ("" for kinds without a user-facing name).
[[nodiscard]] std::string_view intrinsicName(IntrinsicKind kind);

/// Kind for a source name, or IntrinsicKind::None when the name is unknown.
[[nodiscard]] IntrinsicKind intrinsicByName(std::string_view name);

} // namespace sere
