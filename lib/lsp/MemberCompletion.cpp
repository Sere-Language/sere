/// @file MemberCompletion.cpp
/// Line-based member access detection and type-driven completion items.

#include "sere/lsp/MemberCompletion.h"

#include "sere/driver/Frontend.h"
#include "sere/sema/TypeChecker.h"
#include "sere/types/Type.h"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace sere {
namespace {

constexpr int kCompletionMethod = 2;
constexpr int kCompletionFunction = 3;
constexpr int kCompletionField = 5;
constexpr int kCompletionProperty = 10;

[[nodiscard]] bool isIdentStart(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalpha(value) != 0 || ch == '_';
}

[[nodiscard]] bool isIdentContinue(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) != 0 || ch == '_';
}

[[nodiscard]] std::string_view lineToCursor(std::string_view text, std::uint32_t offset) {
  const std::size_t end = offset > text.size() ? text.size() : static_cast<std::size_t>(offset);
  std::size_t start = end;
  while (start > 0 && text[start - 1] != '\n' && text[start - 1] != '\r') {
    --start;
  }
  return text.substr(start, end - start);
}

[[nodiscard]] bool isCompletionType(const Type* type) {
  if (type == nullptr) {
    return false;
  }
  type = type->canonical();
  return type->isRecord() || type->isModule() || type->isList();
}

[[nodiscard]] std::string takeIdentBack(std::string_view line, std::size_t& cursor) {
  std::size_t end = cursor;
  while (cursor > 0 && isIdentContinue(line[cursor - 1])) {
    --cursor;
  }
  if (end == cursor || (cursor < line.size() && !isIdentStart(line[cursor]))) {
    cursor = end;
    return {};
  }
  return std::string(line.substr(cursor, end - cursor));
}

[[nodiscard]] bool fieldVisible(const RecordField& field) {
  if (field.isPublic) {
    return true;
  }
  return !field.getterLlvm.empty() && field.getterPublic;
}

[[nodiscard]] bool methodVisible(const RecordMethod& method) {
  if (!method.isPublic) {
    return false;
  }
  return method.name.rfind("__get_", 0) != 0 && method.name.rfind("__set_", 0) != 0;
}

[[nodiscard]] std::string formatMethod(const RecordMethod& method) {
  std::string text = method.name + "(";
  const Type* fn = method.type;
  const std::size_t start =
      !method.paramNames.empty() && method.paramNames[0] == "self" ? 1 : 0;
  if (fn != nullptr) {
    for (std::size_t index = start; index < fn->paramTypes().size(); ++index) {
      if (index > start) {
        text += ", ";
      }
      if (index < method.paramNames.size()) {
        text += method.paramNames[index] + ": ";
      }
      text += fn->paramTypes()[index] == nullptr ? "?" : fn->paramTypes()[index]->display();
    }
    text += ") -> ";
    text += fn->returnType() == nullptr ? "void" : fn->returnType()->display();
  } else {
    text += ")";
  }
  return text;
}

}  // namespace

MemberAccessQuery detectMemberAccessLine(std::string_view line, std::size_t cursor) {
  MemberAccessQuery query;
  if (cursor > line.size()) {
    cursor = line.size();
  }
  query.prefix = takeIdentBack(line, cursor);
  if (cursor == 0 || line[cursor - 1] != '.') {
    query.prefix.clear();
    return query;
  }
  --cursor;
  while (true) {
    const std::string part = takeIdentBack(line, cursor);
    if (part.empty()) {
      break;
    }
    query.receiver.insert(query.receiver.begin(), part);
    if (cursor == 0 || line[cursor - 1] != '.') {
      break;
    }
    --cursor;
  }
  query.active = !query.receiver.empty();
  if (!query.active) {
    query.prefix.clear();
  }
  return query;
}

MemberAccessQuery detectMemberAccess(std::string_view text, std::uint32_t offset) {
  const std::string_view line = lineToCursor(text, offset);
  return detectMemberAccessLine(line, line.size());
}

const Type* resolveMemberType(Frontend* frontend, const MemberAccessQuery& query) {
  if (frontend == nullptr || !query.active || query.receiver.empty()) {
    return nullptr;
  }
  if (frontend->checker() != nullptr) {
    if (const Type* fromChecker = frontend->checker()->typeOfPath(query.receiver)) {
      if (isCompletionType(fromChecker)) {
        return fromChecker->canonical();
      }
    }
  }
  if (frontend->types() == nullptr) {
    return nullptr;
  }
  const Type* current = frontend->types()->lookupNamed(query.receiver[0]);
  for (std::size_t index = 1; current != nullptr && index < query.receiver.size(); ++index) {
    current = current->canonical();
    const RecordField* field = current->findField(query.receiver[index]);
    current = field == nullptr ? nullptr : field->type;
  }
  if (!isCompletionType(current)) {
    return nullptr;
  }
  return current->canonical();
}

std::vector<MemberCompletionItem> collectMemberCompletions(const Type* type) {
  std::vector<MemberCompletionItem> items;
  if (type == nullptr) {
    return items;
  }
  type = type->canonical();
  if (type->isList()) {
    MemberCompletionItem item;
    item.label = "append";
    item.detail = "append(value)";
    item.kind = kCompletionMethod;
    item.sortText = "0append";
    items.push_back(std::move(item));
    return items;
  }
  if (!(type->isRecord() || type->isModule())) {
    return items;
  }
  for (const RecordField& field : type->fields()) {
    if (!fieldVisible(field) || (!field.name.empty() && field.name[0] == '_')) {
      continue;
    }
    MemberCompletionItem item;
    item.label = field.name;
    item.detail = field.type == nullptr ? "" : field.type->display();
    const bool isFn = field.type != nullptr && field.type->kind() == TypeKind::Function;
    item.kind = isFn ? kCompletionFunction : (field.getterLlvm.empty() ? kCompletionField
                                                                        : kCompletionProperty);
    item.sortText = "0" + field.name;
    items.push_back(std::move(item));
  }
  for (const RecordMethod& method : type->methods()) {
    if (!methodVisible(method) || (!method.name.empty() && method.name[0] == '_')) {
      continue;
    }
    MemberCompletionItem item;
    item.label = method.name;
    item.detail = formatMethod(method);
    item.kind = kCompletionMethod;
    item.sortText = "0" + method.name;
    items.push_back(std::move(item));
  }
  return items;
}

}  // namespace sere
