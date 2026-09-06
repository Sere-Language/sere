/// @file Parse.cpp
/// Parses captured macro source with the real Sere parser.

#include "sere/macro/Parse.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <utility>

namespace sere {
namespace {

[[nodiscard]] SourceLocation shiftLocation(SourceLocation local, SourceLocation base) {
  SourceLocation out = base;
  out.offset = base.offset + local.offset;
  out.line = base.line + local.line - 1;
  out.column = local.line == 1 ? base.column + local.column - 1 : local.column;
  return out;
}

[[nodiscard]] std::vector<Token> shiftTokens(const std::vector<Token>& tokens,
                                             SourceLocation base) {
  std::vector<Token> shifted;
  shifted.reserve(tokens.size());
  for (const Token& token : tokens) {
    SourceRange range{shiftLocation(token.range().start, base),
                      shiftLocation(token.range().end, base)};
    shifted.emplace_back(token.kind(), range, std::string(token.spelling()));
  }
  return shifted;
}

} // namespace

std::unique_ptr<Expr>
parseSereExpr(DiagnosticEngine& diagnostics, std::string_view text, SourceLocation base) {
  DiagnosticEngine nested;
  SourceManager source("<macro>", std::string(text));
  Lexer lexer(source, nested, true);
  std::vector<Token> tokens = lexer.tokenizeAll();
  if (nested.hasErrors()) {
    diagnostics.error(base, "invalid macro expression");
    return nullptr;
  }
  Parser parser(diagnostics, shiftTokens(tokens, base), &source);
  return parser.parseTopExpr();
}

std::vector<std::unique_ptr<Expr>>
parseSereExprList(DiagnosticEngine& diagnostics, std::string_view text, SourceLocation base) {
  DiagnosticEngine nested;
  SourceManager source("<macro>", std::string(text));
  Lexer lexer(source, nested, true);
  std::vector<Token> tokens = lexer.tokenizeAll();
  if (nested.hasErrors()) {
    diagnostics.error(base, "invalid macro argument list");
    return {};
  }
  Parser parser(diagnostics, shiftTokens(tokens, base), &source);
  return parser.parseTopExprList();
}

std::unique_ptr<Expr> parseSereExprFromTokens(DiagnosticEngine& diagnostics,
                                              std::vector<Token> tokens) {
  if (tokens.empty() || tokens.back().kind() != TokenKind::EndOfFile) {
    SourceRange range{};
    if (!tokens.empty()) {
      range = tokens.back().range();
    }
    tokens.emplace_back(TokenKind::EndOfFile, range, "");
  }
  Parser parser(diagnostics, std::move(tokens), nullptr);
  return parser.parseTopExpr();
}

} // namespace sere
