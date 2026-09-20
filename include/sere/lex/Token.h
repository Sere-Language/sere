/// @file Token.h
/// A single lexer token with source range and spelling.

#pragma once

#include "sere/lex/TokenKind.h"
#include "sere/source/SourceLocation.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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
/// Offset of that inner text inside the literal's spelling, so a caller can map
/// an offset within the text back to the source file.
[[nodiscard]] std::size_t stringLiteralInnerOffset(std::string_view spelling);
// Decode escapes in literal text after separating interpolation expressions.
[[nodiscard]] std::string unescapeStringBody(std::string_view body, bool regex = false);
[[nodiscard]] DecodedString decodeStringToken(std::string_view spelling);

/// One piece of an interpolated string's inner text.
struct FStringSegment {
  /// Offsets inside the inner text: `start`..`end` is a literal run, or for an
  /// expression the code between `{` and its matching `}`.
  std::size_t start = 0;
  std::size_t end = 0;
  /// Offset of the format-spec `:` of an expression, if it has one.
  std::size_t spec = std::string_view::npos;
  /// Offset of the `}` that closes an expression.
  std::size_t close = std::string_view::npos;
  bool expression = false;
};

/// Why splitting an interpolated string failed.
enum class FStringSplitStatus {
  Ok,
  UnterminatedInterpolation,
  UnmatchedBrace,
};

/// Splits the inner text of an interpolated string (without its prefix and
/// quotes) into literal runs and the expressions they embed. The parser and the
/// language server both rely on this, so embedded code is found in one place.
[[nodiscard]] FStringSplitStatus splitFStringSegments(std::string_view inner,
                                                      std::vector<FStringSegment>& out);

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

} // namespace sere
