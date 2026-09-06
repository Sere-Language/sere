/// @file Intrinsic.cpp
/// Name table for compiler memory/collection primitives.

#include "sere/types/Intrinsic.h"

namespace sere {

std::string_view intrinsicName(IntrinsicKind kind) {
  switch (kind) {
  case IntrinsicKind::UniqueNew:
    return "unique";
  case IntrinsicKind::SharedNew:
    return "shared";
  case IntrinsicKind::Alloc:
    return "alloc";
  case IntrinsicKind::Free:
    return "free";
  case IntrinsicKind::Load:
    return "load";
  case IntrinsicKind::Store:
    return "store";
  case IntrinsicKind::Len:
    return "len";
  case IntrinsicKind::Print:
    return "print";
  case IntrinsicKind::Str:
    return "str";
  case IntrinsicKind::Repr:
    return "repr";
  case IntrinsicKind::Append:
    return "append";
  case IntrinsicKind::BuiltinMethod:
    return "";
  case IntrinsicKind::ListNew:
    return "list";
  case IntrinsicKind::ArrayNew:
    return "array";
  case IntrinsicKind::DictNew:
    return "dict";
  case IntrinsicKind::Range:
    return "range";
  case IntrinsicKind::TypeOf:
    return "typeof";
  case IntrinsicKind::IsInstance:
    return "isinstance";
  case IntrinsicKind::Dir:
    return "dir";
  case IntrinsicKind::Inspect:
    return "inspect";
  case IntrinsicKind::SizeOf:
    return "sizeof";
  case IntrinsicKind::AlignOf:
    return "alignof";
  case IntrinsicKind::Panic:
    return "panic";
  case IntrinsicKind::Super:
    return "super";
  case IntrinsicKind::Parse:
    return "parse";
  case IntrinsicKind::TryParse:
    return "try_parse";
  case IntrinsicKind::None:
    return "";
  }
  return "";
}

IntrinsicKind intrinsicByName(std::string_view name) {
  if (name == "unique") {
    return IntrinsicKind::UniqueNew;
  }
  if (name == "shared") {
    return IntrinsicKind::SharedNew;
  }
  if (name == "alloc") {
    return IntrinsicKind::Alloc;
  }
  if (name == "free") {
    return IntrinsicKind::Free;
  }
  if (name == "load") {
    return IntrinsicKind::Load;
  }
  if (name == "store") {
    return IntrinsicKind::Store;
  }
  if (name == "len") {
    return IntrinsicKind::Len;
  }
  if (name == "print") {
    return IntrinsicKind::Print;
  }
  if (name == "str") {
    return IntrinsicKind::Str;
  }
  if (name == "repr") {
    return IntrinsicKind::Repr;
  }
  if (name == "append") {
    return IntrinsicKind::Append;
  }
  if (name == "list") {
    return IntrinsicKind::ListNew;
  }
  if (name == "array") {
    return IntrinsicKind::ArrayNew;
  }
  if (name == "dict") {
    return IntrinsicKind::DictNew;
  }
  if (name == "range") {
    return IntrinsicKind::Range;
  }
  if (name == "typeof") {
    return IntrinsicKind::TypeOf;
  }
  if (name == "isinstance") {
    return IntrinsicKind::IsInstance;
  }
  if (name == "dir") {
    return IntrinsicKind::Dir;
  }
  if (name == "inspect") {
    return IntrinsicKind::Inspect;
  }
  if (name == "sizeof") {
    return IntrinsicKind::SizeOf;
  }
  if (name == "alignof") {
    return IntrinsicKind::AlignOf;
  }
  if (name == "panic") {
    return IntrinsicKind::Panic;
  }
  if (name == "super") {
    return IntrinsicKind::Super;
  }
  if (name == "parse") {
    return IntrinsicKind::Parse;
  }
  if (name == "try_parse") {
    return IntrinsicKind::TryParse;
  }
  return IntrinsicKind::None;
}

} // namespace sere
