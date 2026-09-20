/// @file MemberCompletion.h
/// Resolves `obj.` / `mod.Name.` autocomplete from the type checker.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

class Frontend;
class Type;

struct MemberAccessQuery {
  bool active = false;
  std::vector<std::string> receiver;
  std::string receiverType;
  std::string prefix;
  /// Offset of the cursor, so a receiver type narrowed by a type test resolves
  /// to its narrowed type inside the branch.
  std::uint32_t offset = 0;
};

struct MemberCompletionItem {
  std::string label;
  std::string detail;
  std::string insertText;
  int kind = 0;
  std::string sortText;
};

/// A member access read from the typed AST. This is what resolves receivers the
/// textual scan cannot know the type of, such as the result of a call
/// (`add(5, 4).`) or an index (`xs[0].`).
struct AstMemberAccess {
  bool active = false;
  /// Type of the receiver expression; may be a type object for `Name.` access.
  const Type* receiverType = nullptr;
  /// What the cursor already typed after the dot.
  std::string prefix;
};

[[nodiscard]] MemberAccessQuery detectMemberAccess(std::string_view text, std::uint32_t offset);

[[nodiscard]] MemberAccessQuery detectMemberAccessLine(std::string_view line, std::size_t cursor);

/// Reads the member access at `offset` from the analyzed module, if any.
[[nodiscard]] AstMemberAccess resolveMemberAccessFromAst(Frontend* frontend,
                                                         std::string_view text,
                                                         std::uint32_t offset);

[[nodiscard]] const Type* resolveMemberType(Frontend* frontend, const MemberAccessQuery& query);

[[nodiscard]] std::vector<MemberCompletionItem> collectMemberCompletions(const Type* type);

} // namespace sere
