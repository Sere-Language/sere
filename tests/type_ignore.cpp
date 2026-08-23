/// @file type_ignore.cpp
/// Checks `# type: ignore` suppression and diagnostic exception codes.

#include "sere/diag/DiagnosticCode.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/diag/IgnoreDirective.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/TypeContext.h"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::cerr << "type_ignore: " << message << '\n';
  return 1;
}

[[nodiscard]] bool hasCode(const sere::DiagnosticEngine& diagnostics,
                           sere::DiagnosticCode code,
                           std::string_view needle = {}) {
  for (const sere::Diagnostic& diagnostic : diagnostics.diagnostics()) {
    if (diagnostic.code != code) {
      continue;
    }
    if (needle.empty() || diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] int checkSource(const std::string& path,
                              const std::string& text,
                              sere::DiagnosticEngine& diagnostics) {
  sere::SourceManager source(path, text);
  diagnostics.setSource(&source);
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll(), &source);
  std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr) {
    return fail("parse returned null");
  }
  sere::TypeContext types;
  sere::TypeChecker checker(types, diagnostics);
  (void)checker.check(*module);
  return 0;
}

int testCatalog() {
  if (!sere::parseDiagnosticCode("name-error").has_value() ||
      sere::parseDiagnosticCode("NameError") != sere::DiagnosticCode::NameError) {
    return fail("NameError aliases must parse");
  }
  const std::string catalog = sere::diagnosticCodeCatalogText();
  const char* required[] = {"Exception",         "SyntaxError",
                            "IndentationError",  "NameError",
                            "AttributeError",    "TypeError",
                            "IndexError",        "ImportError",
                            "ValueError",        "AssertionError",
                            "PermissionError",   "RuntimeError",
                            "RecursionError",    "NotImplementedError"};
  for (const char* name : required) {
    if (catalog.find(name) == std::string::npos) {
      return fail("catalog missing exception name");
    }
  }
  return 0;
}

int testLineIgnore() {
  sere::DiagnosticEngine diagnostics;
  if (checkSource("line_ignore.sere",
                  "def main() -> i32:\n    return foo  # type: ignore\n",
                  diagnostics) != 0) {
    return 1;
  }
  if (diagnostics.hasErrors()) {
    diagnostics.printAll();
    return fail("end-of-line # type: ignore should suppress NameError");
  }
  return 0;
}

int testPreviousLineIgnore() {
  sere::DiagnosticEngine diagnostics;
  if (checkSource("prev_ignore.sere",
                  "def main() -> i32:\n    # type: ignore\n    return foo\n",
                  diagnostics) != 0) {
    return 1;
  }
  if (diagnostics.hasErrors()) {
    diagnostics.printAll();
    return fail("comment-only # type: ignore should cover the next line");
  }
  return 0;
}

int testFileIgnore() {
  sere::DiagnosticEngine diagnostics;
  if (checkSource("file_ignore.sere",
                  "# type[NameError]: ignore\n"
                  "def main() -> i32:\n"
                  "    n: i32 = \"nope\"\n"
                  "    return foo\n",
                  diagnostics) != 0) {
    return 1;
  }
  if (hasCode(diagnostics, sere::DiagnosticCode::NameError)) {
    diagnostics.printAll();
    return fail("file-level NameError ignore should hide unknown names");
  }
  if (!hasCode(diagnostics, sere::DiagnosticCode::TypeError, "cannot initialize")) {
    diagnostics.printAll();
    return fail("file-level NameError ignore must not hide TypeError");
  }
  return 0;
}

int testExceptionIgnore() {
  sere::DiagnosticEngine diagnostics;
  if (checkSource("all_ignore.sere",
                  "# type[Exception]: ignore\n"
                  "def main() -> i32:\n"
                  "    n: i32 = \"nope\"\n"
                  "    return foo\n",
                  diagnostics) != 0) {
    return 1;
  }
  if (diagnostics.hasErrors()) {
    diagnostics.printAll();
    return fail("# type[Exception]: ignore should hide every diagnostic");
  }
  return 0;
}

int testUnknownException() {
  sere::DiagnosticEngine diagnostics;
  sere::SourceManager source("bad_ignore.sere", "# type[Bogus]: ignore\ndef main() -> i32:\n    return 0\n");
  diagnostics.setSource(&source);
  if (!hasCode(diagnostics, sere::DiagnosticCode::ValueError, "Bogus")) {
    diagnostics.printAll();
    return fail("unknown ignore exception should be reported");
  }
  bool listed = false;
  for (const sere::Diagnostic& diagnostic : diagnostics.diagnostics()) {
    if (diagnostic.help.find("NameError") != std::string::npos &&
        diagnostic.help.find("TypeError") != std::string::npos) {
      listed = true;
    }
  }
  if (!listed) {
    return fail("unknown ignore diagnostic must list available exceptions");
  }
  return 0;
}

}  // namespace

int main() {
  if (const int status = testCatalog(); status != 0) {
    return status;
  }
  if (const int status = testLineIgnore(); status != 0) {
    return status;
  }
  if (const int status = testPreviousLineIgnore(); status != 0) {
    return status;
  }
  if (const int status = testFileIgnore(); status != 0) {
    return status;
  }
  if (const int status = testExceptionIgnore(); status != 0) {
    return status;
  }
  return testUnknownException();
}
