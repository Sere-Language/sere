/// @file Lexer.h
/// Python-style tokenizer with INDENT / DEDENT / NEWLINE production.

#pragma once

#include "sere/lex/Token.h"

#include <cstddef>
#include <vector>

namespace sere {

class DiagnosticEngine;
class SourceManager;

class Lexer {
public:
  Lexer(const SourceManager& source, DiagnosticEngine& diagnostics, bool embedded = false);

  [[nodiscard]] Token nextToken();
  [[nodiscard]] std::vector<Token> tokenizeAll();

private:
  [[nodiscard]] bool isAtEnd() const;
  [[nodiscard]] char peek(std::size_t ahead = 0) const;
  char advance();
  void skipHorizontalWhitespace();
  Token makeToken(TokenKind kind, std::size_t start, std::size_t end) const;
  Token lexIdentifier(std::size_t start);
  Token lexNumber(std::size_t start);
  Token lexRadixInteger(std::size_t start, int base);
  Token lexDecimalNumber(std::size_t start);
  [[nodiscard]] bool consumeSeparatedDigits(int base);
  Token lexQuoted(std::size_t start, char quote, TokenKind kind);
  Token lexFromLineStart();
  void queueDedents(std::size_t indent);

  const SourceManager* source_;
  DiagnosticEngine* diagnostics_;
  std::size_t offset_ = 0;
  std::size_t parenDepth_ = 0;
  bool atLineStart_ = true;
  std::vector<std::size_t> indents_{0};
  std::vector<Token> pending_{};
};

}  // namespace sere
