/// @file DiagnosticCode.cpp
/// Catalog, parsing, and message classification for diagnostic exceptions.

#include "sere/diag/DiagnosticCode.h"

#include <cctype>
#include <string>

namespace sere {
namespace {

[[nodiscard]] bool contains(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] std::string normalizeCodeName(std::string_view name) {
  std::string out;
  out.reserve(name.size());
  for (const char ch : name) {
    if (ch == '_' || ch == '-') {
      continue;
    }
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return out;
}

}  // namespace

const std::vector<DiagnosticCodeInfo>& diagnosticCodeCatalog() {
  static const std::vector<DiagnosticCodeInfo> kCatalog = {
      {DiagnosticCode::Exception, "Exception",
       "Base ignore name. `# type[Exception]: ignore` suppresses every diagnostic."},
      {DiagnosticCode::SyntaxError, "SyntaxError",
       "Parse errors: unexpected tokens, missing punctuation, invalid syntax."},
      {DiagnosticCode::IndentationError, "IndentationError",
       "Indent levels that do not match a previous indent."},
      {DiagnosticCode::NameError, "NameError",
       "Unknown names, types, functions, macros, modules, or exports."},
      {DiagnosticCode::AttributeError, "AttributeError",
       "Unknown fields or methods, or using a method as a field."},
      {DiagnosticCode::TypeError, "TypeError",
       "Type mismatches, invalid operands, wrong arguments, and invalid casts."},
      {DiagnosticCode::IndexError, "IndexError",
       "Invalid indexing or slicing of lists, arrays, dicts, or strings."},
      {DiagnosticCode::ImportError, "ImportError",
       "Missing modules or a prelude that cannot be loaded."},
      {DiagnosticCode::ValueError, "ValueError",
       "Values the type checker cannot infer or that are not valid in context."},
      {DiagnosticCode::AssertionError, "AssertionError",
       "Invalid assert statements."},
      {DiagnosticCode::PermissionError, "PermissionError",
       "Access to private fields, methods, or module exports from outside their owner."},
      {DiagnosticCode::RuntimeError, "RuntimeError",
       "Control-flow errors, codegen failures, and internal compiler stops."},
      {DiagnosticCode::RecursionError, "RecursionError",
       "Macro expansion that exceeded the recursion limit."},
      {DiagnosticCode::NotImplementedError, "NotImplementedError",
       "Unsupported constructs or macros that were not expanded."},
  };
  return kCatalog;
}

std::string_view diagnosticCodeName(DiagnosticCode code) {
  for (const DiagnosticCodeInfo& info : diagnosticCodeCatalog()) {
    if (info.code == code) {
      return info.name;
    }
  }
  return "TypeError";
}

std::string_view diagnosticCodeDescription(DiagnosticCode code) {
  for (const DiagnosticCodeInfo& info : diagnosticCodeCatalog()) {
    if (info.code == code) {
      return info.description;
    }
  }
  return "Type mismatch or invalid operation.";
}

std::optional<DiagnosticCode> parseDiagnosticCode(std::string_view name) {
  const std::string normalized = normalizeCodeName(name);
  for (const DiagnosticCodeInfo& info : diagnosticCodeCatalog()) {
    if (normalizeCodeName(info.name) == normalized) {
      return info.code;
    }
  }
  return std::nullopt;
}

std::string diagnosticCodeCatalogText() {
  std::string text;
  for (const DiagnosticCodeInfo& info : diagnosticCodeCatalog()) {
    if (!text.empty()) {
      text += ", ";
    }
    text += info.name;
  }
  return text;
}

DiagnosticCode inferDiagnosticCode(std::string_view message) {
  if (contains(message, "inconsistent indentation")) {
    return DiagnosticCode::IndentationError;
  }
  if (contains(message, "cannot find module") || contains(message, "cannot load standard library") ||
      contains(message, "cannot import name")) {
    return DiagnosticCode::ImportError;
  }
  if (contains(message, "recursion limit")) {
    return DiagnosticCode::RecursionError;
  }
  if (contains(message, "assert ")) {
    return DiagnosticCode::AssertionError;
  }
  if (contains(message, " is private")) {
    return DiagnosticCode::PermissionError;
  }
  if (contains(message, "not indexable") || contains(message, "index must") ||
      contains(message, "slice ") || contains(message, "expected index")) {
    return DiagnosticCode::IndexError;
  }
  if (message.starts_with("unknown ") || contains(message, "unknown name") ||
      contains(message, "unknown export") || contains(message, "unknown macro") ||
      contains(message, "unknown local") || contains(message, "unknown base") ||
      contains(message, "unknown intrinsic") || contains(message, "redeclaration of")) {
    return DiagnosticCode::NameError;
  }
  if (contains(message, "must be called") || contains(message, "unknown field") ||
      contains(message, "not assignable")) {
    return DiagnosticCode::AttributeError;
  }
  if (contains(message, "outside loop") || contains(message, "too many errors") ||
      contains(message, "LLVM IR") || contains(message, "cannot open")) {
    return DiagnosticCode::RuntimeError;
  }
  if (contains(message, "unsupported") || contains(message, "not expanded")) {
    return DiagnosticCode::NotImplementedError;
  }
  if (contains(message, "cannot initialize") || contains(message, "type mismatch") ||
      contains(message, "cannot cast") || contains(message, "cannot append") ||
      contains(message, "cannot compare") || contains(message, "cannot construct") ||
      contains(message, "is not a function") || contains(message, "is not generic") ||
      contains(message, "requires numeric") || contains(message, "requires a bool") ||
      contains(message, "requires an integer") || contains(message, "requires a number") ||
      contains(message, "requires a list") || contains(message, "does not support") ||
      contains(message, "must share")) {
    return DiagnosticCode::TypeError;
  }
  if (contains(message, "cannot infer") || contains(message, "must be an integer") ||
      contains(message, "must be integer")) {
    return DiagnosticCode::ValueError;
  }
  if (contains(message, "expected expression") || contains(message, "expected type") ||
      contains(message, "unexpected") || contains(message, "unterminated") ||
      contains(message, "invalid interpolation") || contains(message, "unmatched") ||
      contains(message, "needs at least") || contains(message, "needs except") ||
      contains(message, "expected quote") || contains(message, "expected splice") ||
      contains(message, "expected macro") || contains(message, "expected method") ||
      contains(message, "expected '") || contains(message, "cannot be both")) {
    return DiagnosticCode::SyntaxError;
  }
  return DiagnosticCode::TypeError;
}

}  // namespace sere
