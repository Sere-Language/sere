/// @file BuiltinMembers.h
/// The members the compiler gives to built-in types.
///
/// Strings, lists, dicts, and iterators get their members from the checker
/// rather than from a class declaration, so both the checker and the language
/// server read this one table: the language server completes exactly what the
/// checker accepts, and a member is added or removed in a single place.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

class Type;

/// A built-in type with compiler-provided members.
enum class BuiltinReceiver : std::uint8_t {
  Iterator,
  List,
  Dict,
  Str,
};

/// One member of a built-in type.
struct BuiltinMember {
  BuiltinReceiver receiver = BuiltinReceiver::List;
  std::string_view name;
  /// Human-readable signature, used for completion details and snippets, such
  /// as `pop(index?) -> T`.
  std::string_view signature;
  /// Parameter names in order; `optionalFrom` is the first parameter a call may
  /// leave out.
  std::string_view params;
  std::int8_t optionalFrom = -1;

  /// Parameter names for a call that provided `count` arguments.
  [[nodiscard]] std::vector<std::string> parameterNames(std::size_t count) const;
};

/// Every member the compiler provides, in completion order.
[[nodiscard]] const std::vector<BuiltinMember>& builtinMembers();

/// Member `name` of `receiver`, or nullptr when the compiler provides none.
[[nodiscard]] const BuiltinMember* builtinMember(BuiltinReceiver receiver, std::string_view name);

/// Receiver whose members `type` has, if any. Class records and modules keep
/// their members on the type itself, so they map to nothing here.
[[nodiscard]] bool builtinReceiverOf(const Type* type, BuiltinReceiver& out);

/// Prefix the checker records in the call's lowered name, such as `list.`.
[[nodiscard]] std::string_view builtinPrefix(BuiltinReceiver receiver);

} // namespace sere
