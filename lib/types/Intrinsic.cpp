/// @file Intrinsic.cpp
/// Single declarative registry for compiler intrinsics.
///
/// This is the only file that maps an IntrinsicKind to its source name,
/// parameter list, and builtin-symbol visibility. Everything else iterates
/// allIntrinsics() or reads intrinsicInfo(kind), so adding an intrinsic never
/// requires touching sema, codegen, or LSP name tables again.

#include "sere/types/Intrinsic.h"

#include <array>
#include <cstddef>

namespace sere {
namespace {

/// One row per IntrinsicKind, in enumerator order (index == enum value).
constexpr std::array<IntrinsicInfo, 27> IntrinsicTable = {{
    {IntrinsicKind::None, "", "", false},
    {IntrinsicKind::UniqueNew, "unique", "value", true},
    {IntrinsicKind::SharedNew, "shared", "value", true},
    {IntrinsicKind::Alloc, "alloc", "value", true},
    {IntrinsicKind::Free, "free", "value", true},
    {IntrinsicKind::Load, "load", "value", true},
    {IntrinsicKind::Store, "store", "pointer value", true},
    {IntrinsicKind::Len, "len", "items", true},
    {IntrinsicKind::Print, "print", "*values", true},
    {IntrinsicKind::Str, "str", "value", true},
    {IntrinsicKind::Repr, "repr", "", true},
    {IntrinsicKind::Append, "append", "value", true},
    {IntrinsicKind::BuiltinMethod, "", "", false},
    {IntrinsicKind::ListNew, "list", "", true},
    {IntrinsicKind::ArrayNew, "array", "", true},
    {IntrinsicKind::DictNew, "dict", "", true},
    {IntrinsicKind::Range, "range", "start stop step", true},
    {IntrinsicKind::TypeOf, "typeof", "value", true},
    {IntrinsicKind::IsInstance, "isinstance", "value type", true},
    {IntrinsicKind::Dir, "dir", "value", true},
    {IntrinsicKind::Inspect, "inspect", "value", true},
    {IntrinsicKind::SizeOf, "sizeof", "value", true},
    {IntrinsicKind::AlignOf, "alignof", "value", true},
    {IntrinsicKind::Panic, "panic", "value", true},
    {IntrinsicKind::Super, "super", "", false},
    {IntrinsicKind::Parse, "parse", "text", true},
    {IntrinsicKind::TryParse, "try_parse", "text", true},
}};

// Compile-time guards: the table must never be empty and must begin with the
// None sentinel row so intrinsicInfo() always has a valid fallback to return.
static_assert(!IntrinsicTable.empty());
static_assert(IntrinsicTable.front().kind == IntrinsicKind::None);

constexpr IntrinsicInfo FallbackIntrinsic = {IntrinsicKind::None, "", "", false};

} // namespace

std::span<const IntrinsicInfo> allIntrinsics() {
  return IntrinsicTable;
}

const IntrinsicInfo& intrinsicInfo(IntrinsicKind kind) {
  const std::size_t kindIndex = static_cast<std::size_t>(kind);
  if (kindIndex >= IntrinsicTable.size()) {
    // Bounds check: a kind added without a registry row must not index OOB.
    return FallbackIntrinsic;
  }
  return IntrinsicTable.at(kindIndex);
}

std::string_view intrinsicName(IntrinsicKind kind) {
  return intrinsicInfo(kind).name;
}

IntrinsicKind intrinsicByName(std::string_view name) {
  for (const IntrinsicInfo& info : IntrinsicTable) {
    if (!info.name.empty() && info.name == name) {
      return info.kind;
    }
  }
  return IntrinsicKind::None;
}

} // namespace sere
