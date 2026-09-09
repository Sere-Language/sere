#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/TypeContext.h"

#include <iostream>
#include <string>

int main() {
  const char* invalid[] = {
      "yield 1\n",
      "def bad() -> Iterator[i32]:\n    defer:\n        yield 1\n",
      "def bad() -> Iterator[i32]:\n    try:\n        pass\n    finally:\n        yield 1\n",
      "def bad() -> i32:\n    yield 1\n",
      "def bad() -> Iterator[i32]:\n    yield \"oops\"\n",
      "def bad() -> Iterator[i32]:\n    yield\n",
      "def bad() -> Iterator[i32]:\n    yield 1\n    return 2\n",
      "def bad() -> Iterator[i32, str]:\n    yield 1\n",
      "def bad() -> Iterator[void]:\n    yield 1\n",
      "async def bad() -> Iterator[i32]:\n    yield 1\n",
      ("class Bad:\n    def __iter__(self) -> i32:\n        return 1\n"
       "def main() -> void:\n    for n in Bad():\n        pass\n"),
      "def outer() -> Iterator[i32]:\n    def inner() -> void:\n        yield 1\n    yield 2\n",
  };
  for (const char* text : invalid) {
    sere::DiagnosticEngine diagnostics;
    sere::SourceManager source("invalid_iterator.sere", text);
    sere::Lexer lexer(source, diagnostics);
    sere::Parser parser(diagnostics, lexer.tokenizeAll());
    auto module = parser.parseModule();
    sere::TypeContext types;
    sere::TypeChecker checker(types, diagnostics);
    if (module != nullptr && !diagnostics.hasErrors() && checker.check(*module)) {
      std::cerr << "accepted invalid iterator program: " << text;
      return 1;
    }
  }
  return 0;
}
