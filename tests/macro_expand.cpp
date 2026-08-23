/// @file macro_expand.cpp
/// Declarative, quote, indent, raw, and pipeline macros expand before type checking.

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/macro/Expander.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* message) {
  std::cerr << "macro_expand: " << message << '\n';
  return 1;
}

[[nodiscard]] std::unique_ptr<sere::Module> expandSource(sere::DiagnosticEngine& diagnostics,
                                                         sere::SourceManager& source) {
  sere::Lexer lexer(source, diagnostics);
  sere::Parser parser(diagnostics, lexer.tokenizeAll(), &source);
  std::unique_ptr<sere::Module> module = parser.parseModule();
  if (module == nullptr || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return nullptr;
  }
  sere::MacroEnv env;
  sere::MacroExpander expander(diagnostics, env);
  if (!expander.expandModule(*module) || diagnostics.hasErrors()) {
    diagnostics.printAll(source);
    return nullptr;
  }
  return module;
}

[[nodiscard]] const sere::FunctionDef* mainFunction(const sere::Module& module) {
  for (const std::unique_ptr<sere::Stmt>& statement : module.statements()) {
    if (statement->kind() != sere::NodeKind::FunctionDef) {
      continue;
    }
    const auto& function = static_cast<const sere::FunctionDef&>(*statement);
    if (function.name() == "main") {
      return &function;
    }
  }
  return nullptr;
}

[[nodiscard]] bool hasInvoke(const sere::Expr& expr) {
  return expr.kind() == sere::NodeKind::MacroInvokeExpr;
}

}  // namespace

int main() {
  {
    const std::string text =
        "macro twice(x):\n"
        "    quote:\n"
        "        ($x) + ($x)\n"
        "\n"
        "def main() -> i32:\n"
        "    return twice!(3)\n";
    sere::DiagnosticEngine diagnostics;
    sere::SourceManager source("macro_expand.sere", text);
    std::unique_ptr<sere::Module> module = expandSource(diagnostics, source);
    if (module == nullptr) {
      return fail("quote parse/expand failed");
    }
    const sere::FunctionDef* function = mainFunction(*module);
    if (function == nullptr || function->body().empty() ||
        function->body().front()->kind() != sere::NodeKind::ReturnStmt) {
      return fail("expected return in twice example");
    }
    const sere::Expr* value =
        static_cast<const sere::ReturnStmt&>(*function->body().front()).value();
    if (value == nullptr || hasInvoke(*value) || value->kind() != sere::NodeKind::BinaryExpr) {
      return fail("expected expanded addition");
    }
  }

  {
    const std::string text =
        "macro html:\n"
        "    syntax: raw\n"
        "    interpolate: brace\n"
        "    wrapper: Html\n"
        "\n"
        "def main() -> i32:\n"
        "    node: i32 = html:\n"
        "        <h1>{title}</h1>\n"
        "    return 0\n";
    sere::DiagnosticEngine diagnostics;
    sere::SourceManager source("macro_html.sere", text);
    std::unique_ptr<sere::Module> module = expandSource(diagnostics, source);
    if (module == nullptr) {
      return fail("indent html parse/expand failed");
    }
    const sere::FunctionDef* function = mainFunction(*module);
    if (function == nullptr) {
      return fail("missing main in html example");
    }
    bool sawCall = false;
    for (const std::unique_ptr<sere::Stmt>& body : function->body()) {
      if (body->kind() != sere::NodeKind::VarDecl) {
        continue;
      }
      const sere::Expr* init = static_cast<const sere::VarDecl&>(*body).init();
      if (init == nullptr || hasInvoke(*init) || init->kind() != sere::NodeKind::CallExpr) {
        return fail("expected Html wrapper call");
      }
      const auto& call = static_cast<const sere::CallExpr&>(*init);
      if (call.arguments().size() != 1 ||
          call.arguments().front()->kind() != sere::NodeKind::InterpolatedStringExpr) {
        return fail("expected interpolated raw body");
      }
      sawCall = true;
    }
    if (!sawCall) {
      return fail("html indent invoke was not expanded");
    }
  }

  {
    const std::string text =
        "macro pipeline:\n"
        "    syntax: pipeline\n"
        "\n"
        "def main() -> i32:\n"
        "    n: i32 = pipeline:\n"
        "        1\n"
        "        |> add2\n"
        "    return n\n";
    sere::DiagnosticEngine diagnostics;
    sere::SourceManager source("macro_pipeline.sere", text);
    std::unique_ptr<sere::Module> module = expandSource(diagnostics, source);
    if (module == nullptr) {
      return fail("pipeline parse/expand failed");
    }
    const sere::FunctionDef* function = mainFunction(*module);
    if (function == nullptr) {
      return fail("missing main in pipeline example");
    }
    bool sawCall = false;
    for (const std::unique_ptr<sere::Stmt>& body : function->body()) {
      if (body->kind() != sere::NodeKind::VarDecl) {
        continue;
      }
      const sere::Expr* init = static_cast<const sere::VarDecl&>(*body).init();
      if (init == nullptr || hasInvoke(*init) || init->kind() != sere::NodeKind::CallExpr) {
        return fail("expected piped call");
      }
      sawCall = true;
    }
    if (!sawCall) {
      return fail("pipeline indent invoke was not expanded");
    }
  }

  {
    const std::string text =
        "macro classify(x):\n"
        "    quote:\n"
        "        match $x:\n"
        "            case _:\n"
        "                $x\n"
        "\n"
        "def main() -> i32:\n"
        "    n: i32 = 3\n"
        "    classify!(n)\n"
        "    return n\n";
    sere::DiagnosticEngine diagnostics;
    sere::SourceManager source("macro_match.sere", text);
    std::unique_ptr<sere::Module> module = expandSource(diagnostics, source);
    if (module == nullptr) {
      return fail("match quote parse/expand failed");
    }
    const sere::FunctionDef* function = mainFunction(*module);
    if (function == nullptr) {
      return fail("missing main in classify example");
    }
    bool sawMatch = false;
    for (const std::unique_ptr<sere::Stmt>& body : function->body()) {
      if (body->kind() == sere::NodeKind::MatchStmt) {
        sawMatch = true;
      }
      if (body->kind() == sere::NodeKind::PassStmt) {
        return fail("match quote was cloned as pass");
      }
    }
    if (!sawMatch) {
      return fail("expected expanded match statement");
    }
  }

  return 0;
}
