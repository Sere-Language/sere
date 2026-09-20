/// @file BuiltinMembers.cpp
/// The single definition of the members built-in types carry.

#include "sere/types/BuiltinMembers.h"

#include "sere/types/Type.h"

#include <string>

namespace sere {
namespace {

/// Walks the space-separated parameter names of a member.
[[nodiscard]] std::vector<std::string> splitParams(std::string_view text) {
  std::vector<std::string> names;
  std::size_t index = 0;
  while (index < text.size()) {
    const std::size_t space = text.find(' ', index);
    const std::size_t stop = space == std::string_view::npos ? text.size() : space;
    if (stop > index) {
      names.emplace_back(text.substr(index, stop - index));
    }
    index = stop == text.size() ? text.size() : stop + 1;
  }
  return names;
}

/// The table both the checker and the language server read. A member the
/// checker does not implement must not appear here, and vice versa.
const std::vector<BuiltinMember>& table() {
  static const std::vector<BuiltinMember> members = {
      // Iterators are what `for` loops and explicit iterators hand back.
      {BuiltinReceiver::Iterator, "close", "close()", "", -1},

      {BuiltinReceiver::List, "append", "append(value)", "value", -1},
      {BuiltinReceiver::List, "push", "push(value)", "value", -1},
      {BuiltinReceiver::List, "insert", "insert(index, value)", "index value", -1},
      {BuiltinReceiver::List, "pop", "pop(index?) -> T", "index", 0},
      {BuiltinReceiver::List, "remove", "remove(value) -> bool", "value", -1},
      {BuiltinReceiver::List, "find", "find(value) -> i64", "value", -1},
      {BuiltinReceiver::List, "index", "index(value) -> i64", "value", -1},
      {BuiltinReceiver::List, "count", "count(value) -> i64", "value", -1},
      {BuiltinReceiver::List, "contains", "contains(value) -> bool", "value", -1},
      {BuiltinReceiver::List, "has", "has(value) -> bool", "value", -1},
      {BuiltinReceiver::List, "clear", "clear()", "", -1},
      {BuiltinReceiver::List, "reverse", "reverse()", "", -1},
      {BuiltinReceiver::List, "copy", "copy() -> list[T]", "", -1},
      {BuiltinReceiver::List, "clone", "clone() -> list[T]", "", -1},
      {BuiltinReceiver::List, "extend", "extend(items)", "items", -1},

      {BuiltinReceiver::Dict, "get", "get(key) -> V", "key", -1},
      {BuiltinReceiver::Dict, "pop", "pop(key) -> V", "key", -1},
      {BuiltinReceiver::Dict, "set", "set(key, value)", "key value", -1},
      {BuiltinReceiver::Dict, "remove", "remove(key) -> bool", "key", -1},
      {BuiltinReceiver::Dict, "delete", "delete(key) -> bool", "key", -1},
      {BuiltinReceiver::Dict, "contains", "contains(key) -> bool", "key", -1},
      {BuiltinReceiver::Dict, "has", "has(key) -> bool", "key", -1},
      {BuiltinReceiver::Dict, "keys", "keys() -> list[K]", "", -1},
      {BuiltinReceiver::Dict, "values", "values() -> list[V]", "", -1},
      {BuiltinReceiver::Dict, "clear", "clear()", "", -1},
      {BuiltinReceiver::Dict, "copy", "copy() -> dict[K, V]", "", -1},
      {BuiltinReceiver::Dict, "clone", "clone() -> dict[K, V]", "", -1},

      {BuiltinReceiver::Str, "upper", "upper() -> str", "", -1},
      {BuiltinReceiver::Str, "lower", "lower() -> str", "", -1},
      {BuiltinReceiver::Str, "strip", "strip() -> str", "", -1},
      {BuiltinReceiver::Str, "lstrip", "lstrip() -> str", "", -1},
      {BuiltinReceiver::Str, "rstrip", "rstrip() -> str", "", -1},
      {BuiltinReceiver::Str, "capitalize", "capitalize() -> str", "", -1},
      {BuiltinReceiver::Str, "title", "title() -> str", "", -1},
      {BuiltinReceiver::Str, "starts_with", "starts_with(text) -> bool", "text", -1},
      {BuiltinReceiver::Str, "startswith", "startswith(text) -> bool", "text", -1},
      {BuiltinReceiver::Str, "ends_with", "ends_with(text) -> bool", "text", -1},
      {BuiltinReceiver::Str, "endswith", "endswith(text) -> bool", "text", -1},
      {BuiltinReceiver::Str, "contains", "contains(text) -> bool", "text", -1},
      {BuiltinReceiver::Str, "has", "has(text) -> bool", "text", -1},
      {BuiltinReceiver::Str, "find", "find(text) -> i64", "text", -1},
      {BuiltinReceiver::Str, "rfind", "rfind(text) -> i64", "text", -1},
      {BuiltinReceiver::Str, "count", "count(text) -> i64", "text", -1},
      {BuiltinReceiver::Str, "replace", "replace(old, new) -> str", "old new", -1},
      {BuiltinReceiver::Str, "split", "split(sep) -> list[str]", "sep", -1},
      {BuiltinReceiver::Str, "join", "join(parts) -> str", "parts", -1},
      {BuiltinReceiver::Str, "repeat", "repeat(count) -> str", "count", -1},
      {BuiltinReceiver::Str, "is_empty", "is_empty() -> bool", "", -1},
      {BuiltinReceiver::Str, "is_digit", "is_digit() -> bool", "", -1},
      {BuiltinReceiver::Str, "is_alpha", "is_alpha() -> bool", "", -1},
      {BuiltinReceiver::Str, "is_space", "is_space() -> bool", "", -1},
  };
  return members;
}

} // namespace

std::vector<std::string> BuiltinMember::parameterNames(std::size_t count) const {
  std::vector<std::string> names = splitParams(params);
  if (optionalFrom >= 0 && count < names.size()) {
    names.resize(static_cast<std::size_t>(optionalFrom) + count);
  }
  return names;
}

const std::vector<BuiltinMember>& builtinMembers() { return table(); }

const BuiltinMember* builtinMember(BuiltinReceiver receiver, std::string_view name) {
  for (const BuiltinMember& member : table()) {
    if (member.receiver == receiver && member.name == name) {
      return &member;
    }
  }
  return nullptr;
}

bool builtinReceiverOf(const Type* type, BuiltinReceiver& out) {
  if (type == nullptr) {
    return false;
  }
  type = type->canonical();
  if (type->isGenericCtor("Iterator")) {
    out = BuiltinReceiver::Iterator;
    return true;
  }
  if (type->isList()) {
    out = BuiltinReceiver::List;
    return true;
  }
  if (type->isDict()) {
    out = BuiltinReceiver::Dict;
    return true;
  }
  if (type->isStrLayout()) {
    out = BuiltinReceiver::Str;
    return true;
  }
  return false;
}

std::string_view builtinPrefix(BuiltinReceiver receiver) {
  switch (receiver) {
  case BuiltinReceiver::Iterator:
    return "iterator.";
  case BuiltinReceiver::List:
    return "list.";
  case BuiltinReceiver::Dict:
    return "dict.";
  case BuiltinReceiver::Str:
    return "str.";
  }
  return "";
}

} // namespace sere
