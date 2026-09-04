/// @file Token.h
/// A single lexer token with source range and spelling.

#pragma once

#include "sere/lex/TokenKind.h"
#include "sere/source/SourceLocation.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace sere {

class Token {
public:
  Token(TokenKind kind, SourceRange range, std::string spelling);

  [[nodiscard]] TokenKind kind() const;
  [[nodiscard]] SourceRange range() const;
  [[nodiscard]] SourceLocation location() const;
  [[nodiscard]] std::string_view spelling() const;

private:
  TokenKind kind_;
  SourceRange range_;
  std::string spelling_;
};

struct DecodedString {
  std::string value;
  bool regex = false;
};

[[nodiscard]] std::string_view stringLiteralInner(std::string_view spelling);
// Decode escapes in literal text after separating interpolation expressions.
[[nodiscard]] std::string unescapeStringBody(std::string_view body, bool regex = false);
[[nodiscard]] DecodedString decodeStringToken(std::string_view spelling);

/// True for `'...'` but not `'''...'''` or `"..."`.
[[nodiscard]] bool isSingleQuotedLiteral(std::string_view spelling);

enum class IntegerParseStatus {
  Ok,
  Invalid,
  Overflow,
};

struct ParsedInteger {
  std::int64_t value = 0;
  IntegerParseStatus status = IntegerParseStatus::Invalid;
};

/// Parses a decimal, `0x`/`0X`, `0b`/`0B`, or `0o`/`0O` integer spelling.
/// Underscores are allowed between digits.
[[nodiscard]] ParsedInteger parseIntegerToken(std::string_view spelling);

}  // namespace sere
