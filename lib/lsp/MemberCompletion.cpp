/// @file MemberCompletion.cpp
/// Line-based member access detection and type-driven completion items.

#include "sere/lsp/MemberCompletion.h"

#include "sere/ast/Query.h"
#include "sere/driver/Frontend.h"
#include "sere/sema/TypeChecker.h"
#include "sere/types/BuiltinMembers.h"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace sere {
namespace {

constexpr int kCompletionMethod = 2;
constexpr int kCompletionFunction = 3;
constexpr int kCompletionConstructor = 4;
constexpr int kCompletionField = 5;
constexpr int kCompletionProperty = 10;
constexpr int kCompletionEnumMember = 20;

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
  return type->isGenericCtor("Iterator") || type->isRecord() || type->isModule() ||
         type->isList() || type->isDict() || type->isStrLayout();
}

/// Type whose members complete, or nullptr. Type objects stay as they are so
/// `Name.` can offer statics, variants and constructors.
[[nodiscard]] const Type* normalizeCompletionType(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }
  type = type->canonical();
  const bool structural = type->isTypeObject() || type->isRecord() || type->isModule() ||
                          type->isList() || type->isDict() || type->isStrLayout() ||
                          type->isGenericCtor("Iterator");
  if (!structural) {
    // Subclasses of a builtin carry their payload in a "$value" field; member
    // access falls through to the value type (mirrors TypeChecker::checkMember).
    if (const Type* value = type->valueType()) {
      type = value->canonical();
    }
  }
  return isCompletionType(type) ? type : nullptr;
}

/// `Result[T, E]` rather than `Result` for a still-generic record.
[[nodiscard]] std::string displayWithTypeParams(const Type* type) {
  if (type == nullptr) {
    return "?";
  }
  std::string text = type->display();
  if (type->kind() == TypeKind::Record && type->args().empty() && !type->typeParams().empty() &&
      text == type->name() && text.find('[') == std::string::npos) {
    text += "[";
    for (std::size_t index = 0; index < type->typeParams().size(); ++index) {
      if (index != 0) {
        text += ", ";
      }
      text += type->typeParams()[index];
    }
    text += "]";
  }
  return text;
}

/// `(value: T, code: i32)` — the payload list of an enum variant.
[[nodiscard]] std::string variantParams(const RecordField& variant) {
  std::string text = "(";
  for (std::size_t index = 0; index < variant.payloadTypes.size(); ++index) {
    if (index != 0) {
      text += ", ";
    }
    if (index < variant.paramNames.size()) {
      text += variant.paramNames[index] + ": ";
    }
    const Type* payload = variant.payloadTypes[index];
    text += payload == nullptr ? "?" : payload->display();
  }
  text += ")";
  return text;
}

/// `Ok(${1:value})` — inserting a variant fills in its payload slots.
[[nodiscard]] std::string variantSnippet(const RecordField& variant) {
  std::string snippet = variant.name + "(";
  for (std::size_t index = 0; index < variant.payloadTypes.size(); ++index) {
    if (index != 0) {
      snippet += ", ";
    }
    const std::string name =
        index < variant.paramNames.size() ? variant.paramNames[index] : std::string("value");
    snippet += "${" + std::to_string(index + 1) + ":" + name + "}";
  }
  snippet += ")";
  return snippet;
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
  std::string text = method.name;
  if (!method.typeParams.empty()) {
    text += "[";
    for (std::size_t index = 0; index < method.typeParams.size(); ++index) {
      if (index != 0) {
        text += ", ";
      }
      text += method.typeParams[index];
    }
    text += "]";
  }
  text += "(";
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
  MemberAccessQuery query = detectMemberAccessLine(line, line.size());
  query.offset = offset;
  return query;
}

AstMemberAccess resolveMemberAccessFromAst(Frontend* frontend,
                                           std::string_view text,
                                           std::uint32_t offset) {
  AstMemberAccess access;
  if (frontend == nullptr || frontend->module() == nullptr) {
    return access;
  }
  const MemberExpr* member = findMemberAccessAt(*frontend->module(), offset);
  if (member == nullptr) {
    return access;
  }
  const std::uint32_t size = static_cast<std::uint32_t>(member->field().size());
  if (member->range().end.offset < size) {
    return access;
  }
  const std::uint32_t dotEnd = member->range().end.offset - size;
  if (offset < dotEnd) {
    return access;
  }
  const std::uint32_t stop =
      offset > member->range().end.offset ? member->range().end.offset : offset;
  const std::string typed(text.substr(dotEnd, stop - dotEnd));
  // The field token is an identifier, so anything else means the offset drifted.
  for (const char ch : typed) {
    if (!isIdentContinue(ch)) {
      return access;
    }
  }
  access.prefix = typed;
  access.receiverType = member->object().resolvedType();
  access.active = true;
  return access;
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
  if (frontend->checker() == nullptr) {
    return nullptr;
  }
  return normalizeCompletionType(frontend->checker()->typeOfPathAt(query.receiver, query.offset));
}

std::vector<MemberCompletionItem> collectMemberCompletions(const Type* type) {
  std::vector<MemberCompletionItem> items;
  type = normalizeCompletionType(type);
  if (type == nullptr) {
    return items;
  }
  const bool onTypeObject = type->isTypeObject() && type->typeObjectInstance() != nullptr;
  type = completionType(type);
  if (type == nullptr) {
    return items;
  }
  auto addItem = [&](MemberCompletionItem item) { items.push_back(std::move(item)); };
  auto addMethod = [&](std::string_view label, std::string_view detail, int kind) {
    MemberCompletionItem item;
    item.label = std::string(label);
    item.detail = std::string(detail);
    item.insertText = methodSnippet(label, detail);
    item.kind = kind;
    item.sortText = "0" + item.label;
    addItem(std::move(item));
  };
  // Built-in members come from the compiler's table, so the completion list is
  // exactly what the checker resolves.
  if (!onTypeObject) {
    BuiltinReceiver receiver = BuiltinReceiver::List;
    if (builtinReceiverOf(type, receiver)) {
      for (const BuiltinMember& member : builtinMembers()) {
        if (member.receiver != receiver) {
          continue;
        }
        addMethod(member.name, member.signature, kCompletionMethod);
      }
      return items;
    }
  }
  if (!(type->isRecord() || type->isModule())) {
    return items;
  }
  const std::string owner = displayWithTypeParams(type);
  if (onTypeObject && type->isEnum()) {
    // `Result.Ok(...)`: variants are constructors of the enum, not values.
    for (const RecordField& variant : type->fields()) {
      if (!variant.isStatic || variant.name.empty() || variant.name[0] == '_') {
        continue;
      }
      MemberCompletionItem item;
      item.label = variant.name;
      item.kind = kCompletionEnumMember;
      item.sortText = "0" + item.label;
      item.detail = owner + "." + variant.name;
      item.insertText = variant.name;
      if (!variant.payloadTypes.empty()) {
        item.detail += variantParams(variant) + " -> " + owner;
        item.insertText = variantSnippet(variant);
      }
      addItem(std::move(item));
    }
    addMethod("variants", "variants() -> list[str]", kCompletionMethod);
  } else if (onTypeObject && type->isRecord()) {
    const int initIndex = type->methodIndex("__init__");
    if (initIndex >= 0) {
      const RecordMethod& init = type->methods()[static_cast<std::size_t>(initIndex)];
      MemberCompletionItem item;
      item.label = type->name();
      item.detail = formatMethod(init);
      const std::size_t arrow = item.detail.find(" -> ");
      if (arrow != std::string::npos) {
        item.detail = item.detail.substr(0, arrow);
      }
      item.insertText = methodSnippet(item.label, item.detail);
      item.detail += " -> " + owner;
      item.kind = kCompletionConstructor;
      item.sortText = "0" + item.label;
      addItem(std::move(item));
    }
  }
  for (const RecordField& field : type->fields()) {
    if (!fieldVisible(field) || field.name.empty() || field.name[0] == '_') {
      continue;
    }
    // Enum variants are constructors, only reachable through the enum's name.
    if (field.isStatic && type->isEnum()) {
      continue;
    }
    // `Account.owner` is rejected by the checker: instance fields need a value.
    if (onTypeObject && !field.isStatic && !type->isModule()) {
      continue;
    }
    MemberCompletionItem item;
    item.label = field.name;
    item.detail = field.type == nullptr ? "" : field.type->display();
    const bool isFn = field.type != nullptr && field.type->kind() == TypeKind::Function;
    if (isFn) {
      // Module exports and callable fields read better with parameter names.
      item.detail = field.name + field.type->display();
      item.insertText = methodSnippet(field.name, item.detail);
      item.kind = kCompletionFunction;
    } else {
      if (field.isStatic) {
        item.detail = "static " + item.detail;
      }
      item.kind = field.getterLlvm.empty() ? kCompletionField : kCompletionProperty;
    }
    item.sortText = "0" + field.name;
    addItem(std::move(item));
  }
  if (type->isEnum() && !onTypeObject) {
    // Enum values expose their variant name and discriminant.
    MemberCompletionItem name;
    name.label = "name";
    name.detail = "str";
    name.kind = kCompletionProperty;
    name.sortText = "1name";
    addItem(std::move(name));
    MemberCompletionItem value;
    value.label = "value";
    value.detail = "i32";
    value.kind = kCompletionProperty;
    value.sortText = "1value";
    addItem(std::move(value));
  }
  for (const RecordMethod& method : type->methods()) {
    if (!methodVisible(method) || method.name.empty() || method.name[0] == '_') {
      continue;
    }
    MemberCompletionItem item;
    item.label = method.name;
    item.detail = formatMethod(method);
    item.insertText = methodSnippet(method.name, item.detail);
    item.kind = kCompletionMethod;
    item.sortText = "0" + method.name;
    addItem(std::move(item));
  }
  return items;
}

} // namespace sere
