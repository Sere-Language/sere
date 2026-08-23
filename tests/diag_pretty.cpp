/// @file diag_pretty.cpp
/// Checks rustc-style diagnostic snippets, carets, and suggestions.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/TypeContext.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* message) {
  std::cerr << "diag_pretty: " << message << '\n';
  return 1;
}

int failRendered(const char* message, const std::string& rendered) {
  std::cerr << rendered;
  return fail(message);
}

}  // namespace

int main() {
  sere::DiagnosticEngine locationless;
  locationless.error("cannot open input file 'missing.sere'");
  const std::string globalText = locationless.format(false);
  if (globalText.find("error[RuntimeError]: cannot open input file 'missing.sere'") ==
      std::string::npos) {
    return failRendered("missing locationless error text", globalText);
  }
  if (globalText.find("-->") != std::string::npos) {
    return failRendered("locationless error should not have a snippet", globalText);
  }

  sere::DiagnosticEngine parseDiagnostics;
  sere::SourceManager parseSource("parse_pretty.sere", "def main() -> i32:\n    =\n    return 0\n");
  parseDiagnostics.setSource(&parseSource);
  sere::Lexer parseLexer(parseSource, parseDiagnostics);
  sere::Parser parseParser(parseDiagnostics, parseLexer.tokenizeAll());
  (void)parseParser.parseModule();
  if (!parseDiagnostics.hasErrors()) {
    return fail("bare '=' must be a parse error");
  }
  const std::string parseRendered = parseDiagnostics.format(parseSource, false);
  if (parseRendered.find("expected expression") == std::string::npos ||
      parseRendered.find("-->") == std::string::npos || parseRendered.find('^') == std::string::npos) {
    return failRendered("parse error missing snippet", parseRendered);
  }

  const std::string text =
      "def main() -> i32:\n"
      "    n: i32 = \"nope\"\n"
      "    return fooo\n";
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("diag_pretty.sere", text);
  diagnostics.setSource(&source);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll());
  std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr) {
    return fail("parse returned null");
  }
  sere::TypeContext types;
  sere::TypeChecker checker(types, diagnostics);
  (void)checker.check(*module);
  if (!diagnostics.hasErrors()) {
    return fail("expected type errors");
  }
  const std::string rendered = diagnostics.format(source, false);
  if (rendered.find("error[") == std::string::npos) {
    return failRendered("missing error label", rendered);
  }
  if (rendered.find("-->") == std::string::npos ||
      rendered.find("diag_pretty.sere") == std::string::npos) {
    return failRendered("missing file location snippet", rendered);
  }
  if (rendered.find("n: i32 = \"nope\"") == std::string::npos) {
    return failRendered("missing source line", rendered);
  }
  if (rendered.find('^') == std::string::npos) {
    return failRendered("missing caret underline", rendered);
  }
  if (rendered.find("cannot initialize 'n' with") == std::string::npos) {
    return failRendered("missing initialization mismatch", rendered);
  }
  if (rendered.find("did you mean") == std::string::npos &&
      rendered.find("unknown name 'fooo'") == std::string::npos) {
    return failRendered("missing unknown-name diagnostic", rendered);
  }
  return 0;
}
