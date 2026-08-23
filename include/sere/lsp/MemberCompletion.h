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
  std::string prefix;
};

struct MemberCompletionItem {
  std::string label;
  std::string detail;
  int kind = 0;
  std::string sortText;
};

[[nodiscard]] MemberAccessQuery detectMemberAccess(std::string_view text, std::uint32_t offset);

[[nodiscard]] MemberAccessQuery detectMemberAccessLine(std::string_view line, std::size_t cursor);

[[nodiscard]] const Type* resolveMemberType(Frontend* frontend, const MemberAccessQuery& query);

[[nodiscard]] std::vector<MemberCompletionItem> collectMemberCompletions(const Type* type);

}  // namespace sere
