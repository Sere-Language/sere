/// @file parse_fn.cpp
/// Checks that the parser accepts typed functions, including multiline signatures.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

bool fail(const char* message) {
  std::cerr << "parse_fn: " << message << '\n';
  return false;
}

// Parses `text` and checks that it yields a single `def foo(...) -> <return>` whose
// parameter names match `expectedParams` in order.
bool checkFunction(const std::string& text,
                   const std::vector<std::string>& expectedParams,
                   const std::string& expectedReturn) {
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("parse_fn.sere", text);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll());
  const std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return fail("parse failed");
  }
  if (module->statements().size() != 1 ||
      module->statements().front()->kind() != sere::NodeKind::FunctionDef) {
    return fail("expected one function definition");
  }
  const auto& function = static_cast<const sere::FunctionDef&>(*module->statements().front());
  if (function.name() != "foo") {
    return fail("expected function named 'foo'");
  }
  if (function.returnType().name() != expectedReturn) {
    return fail("unexpected return type");
  }
  const std::vector<sere::ParamDecl>& params = function.params();
  if (params.size() != expectedParams.size()) {
    return fail("unexpected parameter count");
  }
  for (std::size_t index = 0; index < params.size(); ++index) {
    if (params[index].name != expectedParams[index]) {
      return fail("unexpected parameter name");
    }
  }
  return true;
}

}  // namespace

int main() {
  int failures = 0;

  // Single-line signature.
  if (!checkFunction("def foo(a: str) -> None:\n    return\n", {"a"}, "None")) {
    ++failures;
  }

  // Parameters each on their own line, closing paren and return type on the last line.
  if (!checkFunction("def foo(\n"
                     "    arg1: str,\n"
                     "    arg2: str\n"
                     ") -> None:\n"
                     "    return\n",
                     {"arg1", "arg2"},
                     "None")) {
    ++failures;
  }

  // Leading newline after '(' and a trailing comma before ')'.
  if (!checkFunction("def foo(\n"
                     "    arg1: str,\n"
                     "    arg2: str,\n"
                     ") -> None:\n"
                     "    return\n",
                     {"arg1", "arg2"},
                     "None")) {
    ++failures;
  }

  // Empty parameter list split across lines.
  if (!checkFunction("def foo(\n"
                     ") -> None:\n"
                     "    return\n",
                     {},
                     "None")) {
    ++failures;
  }

  // Multiline type parameter list together with multiline parameters.
  if (!checkFunction("def foo[\n"
                     "    T\n"
                     "](\n"
                     "    value: T,\n"
                     "    other: str\n"
                     ") -> T:\n"
                     "    return value\n",
                     {"value", "other"},
                     "T")) {
    ++failures;
  }

  // Default values with the signature split across lines.
  if (!checkFunction("def foo(\n"
                     "    a: i32 = 1,\n"
                     "    b: str = \"x\"\n"
                     ") -> i32:\n"
                     "    return a\n",
                     {"a", "b"},
                     "i32")) {
    ++failures;
  }

  if (failures != 0) {
    return 1;
  }
  return 0;
}
