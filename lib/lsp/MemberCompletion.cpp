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

[[nodiscard]] const Type* completionType(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }
  type = type->canonical();
  if (type->isTypeObject() && type->typeObjectInstance() != nullptr) {
    type = type->typeObjectInstance()->canonical();
  }
  return type;
}

[[nodiscard]] bool isCompletionType(const Type* type) {
  type = completionType(type);
  if (type == nullptr) {
    return false;
  }
  return type->isRecord() || type->isModule() || type->isList() || type->isDict() ||
         type->isStrLayout();
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
  const std::size_t start = !method.paramNames.empty() && method.paramNames[0] == "self" ? 1 : 0;
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

[[nodiscard]] std::string methodSnippet(std::string_view label, std::string_view detail) {
  std::string snippet(label);
  snippet += '(';
  const std::size_t open = detail.find('(');
  const std::size_t close =
      open == std::string_view::npos ? std::string_view::npos : detail.find(')', open + 1);
  if (open != std::string_view::npos && close != std::string_view::npos) {
    std::string_view params = detail.substr(open + 1, close - open - 1);
    std::size_t index = 0;
    unsigned placeholder = 1;
    while (index < params.size()) {
      const std::size_t comma = params.find(',', index);
      std::string_view param = params.substr(
          index, comma == std::string_view::npos ? params.size() - index : comma - index);
      while (!param.empty() && param.front() == ' ') {
        param.remove_prefix(1);
      }
      const std::size_t colon = param.find(':');
      if (colon != std::string_view::npos) {
        param = param.substr(0, colon);
      }
      if (!param.empty() && param.back() == '?') {
        param.remove_suffix(1);
      }
      if (!param.empty()) {
        if (placeholder > 1) {
          snippet += ", ";
        }
        snippet += "${" + std::to_string(placeholder++) + ":" + std::string(param) + "}";
      }
      if (comma == std::string_view::npos) {
        break;
      }
      index = comma + 1;
    }
  }
  snippet += ')';
  return snippet;
}

} // namespace

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
  const std::size_t receiverEnd = cursor;
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
  if (query.receiver.empty() && receiverEnd > 0) {
    const char last = line[receiverEnd - 1];
    if (last == '\'' || last == '"') {
      const char quote = last;
      std::size_t start = receiverEnd - 1;
      while (start > 0) {
        --start;
        if (line[start] != quote) {
          continue;
        }
        std::size_t escapes = 0;
        for (std::size_t scan = start; scan > 0 && line[scan - 1] == '\\'; --scan) {
          ++escapes;
        }
        if ((escapes % 2) == 0) {
          query.receiverType = "str";
          break;
        }
      }
    } else if (last == ']') {
      query.receiverType = "list";
    } else if (last == '}') {
      query.receiverType = "dict";
    }
  }
  query.active = !query.receiver.empty() || !query.receiverType.empty();
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
  if (frontend == nullptr || !query.active) {
    return nullptr;
  }
  if (!query.receiverType.empty() && frontend->types() != nullptr) {
    if (query.receiverType == "str") {
      return frontend->types()->strType();
    }
    if (query.receiverType == "list") {
      return frontend->types()->listType(frontend->types()->anyType());
    }
    if (query.receiverType == "dict") {
      return frontend->types()->dictType(frontend->types()->anyType(),
                                         frontend->types()->anyType());
    }
  }
  if (frontend->checker() != nullptr) {
    if (const Type* fromChecker = frontend->checker()->typeOfPath(query.receiver)) {
      fromChecker = completionType(fromChecker);
      if (isCompletionType(fromChecker)) {
        return fromChecker;
      }
    }
  }
  return nullptr;
}

std::vector<MemberCompletionItem> collectMemberCompletions(const Type* type) {
  std::vector<MemberCompletionItem> items;
  if (type == nullptr) {
    return items;
  }
  type = completionType(type);
  if (type == nullptr) {
    return items;
  }
  auto addMethod = [&](std::string_view label, std::string_view detail) {
    MemberCompletionItem item;
    item.label = std::string(label);
    item.detail = std::string(detail);
    item.insertText = methodSnippet(label, detail);
    item.kind = kCompletionMethod;
    item.sortText = "0" + item.label;
    items.push_back(std::move(item));
  };
  if (type->isList()) {
    addMethod("append", "append(value)");
    addMethod("push", "push(value)");
    addMethod("insert", "insert(index, value)");
    addMethod("pop", "pop(index?)");
    addMethod("remove", "remove(value) -> bool");
    addMethod("find", "find(value) -> i64");
    addMethod("index", "index(value) -> i64");
    addMethod("count", "count(value) -> i64");
    addMethod("contains", "contains(value) -> bool");
    addMethod("clear", "clear()");
    addMethod("reverse", "reverse()");
    addMethod("copy", "copy()");
    addMethod("extend", "extend(items)");
    return items;
  }
  if (type->isDict()) {
    addMethod("get", "get(key)");
    addMethod("set", "set(key, value)");
    addMethod("pop", "pop(key)");
    addMethod("remove", "remove(key) -> bool");
    addMethod("contains", "contains(key) -> bool");
    addMethod("keys", "keys()");
    addMethod("values", "values()");
    addMethod("clear", "clear()");
    addMethod("copy", "copy()");
    return items;
  }
  if (type->isStrLayout()) {
    addMethod("join", "join(parts: list[str]) -> str");
    addMethod("split", "split(sep) -> list[str]");
    addMethod("replace", "replace(old, new) -> str");
    addMethod("find", "find(text) -> i64");
    addMethod("rfind", "rfind(text) -> i64");
    addMethod("count", "count(text) -> i64");
    addMethod("upper", "upper() -> str");
    addMethod("lower", "lower() -> str");
    addMethod("strip", "strip() -> str");
    addMethod("lstrip", "lstrip() -> str");
    addMethod("rstrip", "rstrip() -> str");
    addMethod("capitalize", "capitalize() -> str");
    addMethod("title", "title() -> str");
    addMethod("starts_with", "starts_with(text) -> bool");
    addMethod("ends_with", "ends_with(text) -> bool");
    addMethod("contains", "contains(text) -> bool");
    addMethod("repeat", "repeat(count) -> str");
    addMethod("is_empty", "is_empty() -> bool");
    addMethod("is_digit", "is_digit() -> bool");
    addMethod("is_alpha", "is_alpha() -> bool");
    addMethod("is_space", "is_space() -> bool");
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
    item.kind = isFn ? kCompletionFunction
                     : (field.getterLlvm.empty() ? kCompletionField : kCompletionProperty);
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
    item.insertText = methodSnippet(method.name, item.detail);
    item.kind = kCompletionMethod;
    item.sortText = "0" + method.name;
    items.push_back(std::move(item));
  }
  return items;
}

} // namespace sere
