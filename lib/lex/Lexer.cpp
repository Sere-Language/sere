/// @file Lexer.cpp
/// Python-style tokenizer. Comments are discarded. Indentation emits Indent/Dedent.

#include "sere/lex/Lexer.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/source/SourceManager.h"

#include <string>
#include <utility>

namespace sere {
namespace {

[[nodiscard]] bool isAsciiLetter(char character) {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z') || character == '_';
}

[[nodiscard]] bool isAsciiDigit(char character) {
  return character >= '0' && character <= '9';
}

[[nodiscard]] bool isIdentifierContinue(char character) {
  return isAsciiLetter(character) || isAsciiDigit(character);
}

[[nodiscard]] int numericDigitValue(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

[[nodiscard]] const char* radixName(int base) {
  if (base == 16) {
    return "hex";
  }
  if (base == 2) {
    return "binary";
  }
  if (base == 8) {
    return "octal";
  }
  return "decimal";
}

[[nodiscard]] const char* radixHelp(int base) {
  if (base == 16) {
    return "write hex as 0x0A or 0xFF_FF";
  }
  if (base == 2) {
    return "write binary as 0b1010";
  }
  if (base == 8) {
    return "write octal as 0o17";
  }
  return "separate digits with '_' only between digits";
}

constexpr int kDecimalBase = 10;
constexpr int kBinaryBase = 2;
constexpr int kOctalBase = 8;
constexpr int kHexBase = 16;

}  // namespace

Lexer::Lexer(const SourceManager& source, DiagnosticEngine& diagnostics, bool embedded)
    : source_(&source), diagnostics_(&diagnostics), atLineStart_(!embedded) {}

bool Lexer::isAtEnd() const { return offset_ >= source_->size(); }

char Lexer::peek(std::size_t ahead) const {
  return source_->charAt(offset_ + ahead);
}

char Lexer::advance() {
  const char character = peek();
  if (!isAtEnd()) {
    ++offset_;
  }
  return character;
}

void Lexer::skipHorizontalWhitespace() {
  while (peek() == ' ') {
    advance();
  }
}

Token Lexer::makeToken(TokenKind kind, std::size_t start, std::size_t end) const {
  const SourceRange range{source_->location(start), source_->location(end)};
  const std::string spelling(source_->text().substr(start, end - start));
  return Token(kind, range, spelling);
}

void Lexer::queueDedents(std::size_t indent) {
  while (indents_.size() > 1 && indent < indents_.back()) {
    indents_.pop_back();
    pending_.push_back(makeToken(TokenKind::Dedent, offset_, offset_));
  }
  if (indent != indents_.back()) {
    diagnostics_->error(SourceRange{source_->location(offset_), source_->location(offset_)},
                        "inconsistent indentation");
    diagnostics_->help("each indent level must match a previous one");
  }
}

Token Lexer::lexIdentifier(std::size_t start) {
  while (isIdentifierContinue(peek())) {
    advance();
  }
  const std::string_view spelling = source_->text().substr(start, offset_ - start);
  return makeToken(keywordKind(spelling), start, offset_);
}

bool Lexer::consumeSeparatedDigits(int base) {
  bool sawDigit = false;
  bool lastWasUnderscore = false;
  while (!isAtEnd()) {
    const char character = peek();
    if (character == '_') {
      if (!sawDigit || lastWasUnderscore) {
        break;
      }
      lastWasUnderscore = true;
      advance();
      continue;
    }
    const int digit = numericDigitValue(character);
    if (digit < 0 || digit >= base) {
      break;
    }
    sawDigit = true;
    lastWasUnderscore = false;
    advance();
  }
  if (lastWasUnderscore) {
    diagnostics_->error(SourceRange{source_->location(offset_ - 1), source_->location(offset_)},
                        "digit separator cannot be trailing");
    diagnostics_->help(radixHelp(base));
  }
  return sawDigit;
}

Token Lexer::lexRadixInteger(std::size_t start, int base) {
  const bool sawDigit = consumeSeparatedDigits(base);
  if (isIdentifierContinue(peek())) {
    while (isIdentifierContinue(peek())) {
      advance();
    }
    diagnostics_->error(SourceRange{source_->location(start), source_->location(offset_)},
                        std::string("invalid ") + radixName(base) + " integer literal");
    diagnostics_->help(radixHelp(base));
    return makeToken(TokenKind::Integer, start, offset_);
  }
  if (!sawDigit) {
    diagnostics_->error(SourceRange{source_->location(start), source_->location(offset_)},
                        std::string("expected ") + radixName(base) + " digit after prefix");
    diagnostics_->help(radixHelp(base));
  }
  return makeToken(TokenKind::Integer, start, offset_);
}

Token Lexer::lexDecimalNumber(std::size_t start) {
  bool isFloat = source_->charAt(start) == '.';
  (void)consumeSeparatedDigits(kDecimalBase);
  if (peek() == '.' && peek(1) != '.') {
    isFloat = true;
    advance();
    (void)consumeSeparatedDigits(kDecimalBase);
  }
  if (peek() == 'e' || peek() == 'E') {
    const char signOrDigit = peek(1);
    const char firstDigit =
        (signOrDigit == '+' || signOrDigit == '-') ? peek(2) : signOrDigit;
    if (isAsciiDigit(firstDigit)) {
      isFloat = true;
      advance();
      if (peek() == '+' || peek() == '-') {
        advance();
      }
      (void)consumeSeparatedDigits(kDecimalBase);
    }
  }
  if (peek() == 'f' || peek() == 'F') {
    const char next = peek(1);
    if (!isIdentifierContinue(next)) {
      isFloat = true;
      advance();
    }
  }
  return makeToken(isFloat ? TokenKind::Float : TokenKind::Integer, start, offset_);
}

Token Lexer::lexNumber(std::size_t start) {
  if (source_->charAt(start) == '0') {
    const char prefix = peek();
    if (prefix == 'x' || prefix == 'X') {
      advance();
      return lexRadixInteger(start, kHexBase);
    }
    if (prefix == 'b' || prefix == 'B') {
      advance();
      return lexRadixInteger(start, kBinaryBase);
    }
    if (prefix == 'o' || prefix == 'O') {
      advance();
      return lexRadixInteger(start, kOctalBase);
    }
  }
  return lexDecimalNumber(start);
}

Token Lexer::lexQuoted(std::size_t start, char quote, TokenKind kind) {
  const bool allowTriple = quote == '"' || quote == '\'';
  std::size_t quoteCount = 1;
  if (allowTriple) {
    while (quoteCount < 3 && peek() == quote) {
      advance();
      ++quoteCount;
    }
  }
  if (quoteCount == 2) {
    return makeToken(kind, start, offset_);
  }
  const bool triple = quoteCount == 3;
  while (!isAtEnd()) {
    const char ch = peek();
    if (!triple && quote != '`' && (ch == '\n' || ch == '\r')) {
      break;
    }
    if (ch == '\\') {
      advance();
      if (!isAtEnd()) {
        advance();
      }
      continue;
    }
    if (ch == quote) {
      if (!triple) {
        advance();
        return makeToken(kind, start, offset_);
      }
      if (peek(1) == quote && peek(2) == quote) {
        advance();
        advance();
        advance();
        return makeToken(kind, start, offset_);
      }
    }
    advance();
  }
  diagnostics_->error(SourceRange{source_->location(start), source_->location(offset_)},
                      "unterminated string literal");
  diagnostics_->help("close the string with a matching quote");
  return makeToken(TokenKind::Unknown, start, offset_);
}

Token Lexer::lexFromLineStart() {
  while (!isAtEnd()) {
    std::size_t indent = 0;
    while (peek() == ' ') {
      advance();
      ++indent;
    }
    if (peek() == '\t') {
      diagnostics_->error(SourceRange{source_->location(offset_), source_->location(offset_)},
                          "tabs are not allowed; use spaces");
      diagnostics_->help("indent with spaces only");
      advance();
    }
    const bool blankLine =
        peek() == '#' || peek() == '\n' || peek() == '\r' || isAtEnd();
    if (blankLine) {
      if (peek() == '#') {
        while (!isAtEnd() && peek() != '\n') {
          advance();
        }
      }
      if (peek() == '\r') {
        advance();
      }
      if (peek() == '\n') {
        advance();
      }
      continue;
    }
    atLineStart_ = false;
    if (indent > indents_.back()) {
      indents_.push_back(indent);
      pending_.push_back(makeToken(TokenKind::Indent, offset_, offset_));
    } else if (indent < indents_.back()) {
      queueDedents(indent);
    }
    return nextToken();
  }
  atLineStart_ = false;
  queueDedents(0);
  pending_.push_back(makeToken(TokenKind::EndOfFile, offset_, offset_));
  Token token = std::move(pending_.front());
  pending_.erase(pending_.begin());
  return token;
}

Token Lexer::nextToken() {
  if (!pending_.empty()) {
    Token token = std::move(pending_.front());
    pending_.erase(pending_.begin());
    return token;
  }
  if (atLineStart_ && parenDepth_ == 0) {
    return lexFromLineStart();
  }
  skipHorizontalWhitespace();
  if (peek() == '#') {
    while (!isAtEnd() && peek() != '\n') {
      advance();
    }
  }
  if (isAtEnd()) {
    queueDedents(0);
    pending_.push_back(makeToken(TokenKind::EndOfFile, offset_, offset_));
    Token token = std::move(pending_.front());
    pending_.erase(pending_.begin());
    return token;
  }
  const std::size_t start = offset_;
  const char character = advance();
  switch (character) {
  case '\r':
    if (peek() == '\n') {
      advance();
    }
    atLineStart_ = parenDepth_ == 0;
    return makeToken(TokenKind::Newline, start, offset_);
  case '\n':
    atLineStart_ = parenDepth_ == 0;
    return makeToken(TokenKind::Newline, start, offset_);
  case '(':
    ++parenDepth_;
    return makeToken(TokenKind::LParen, start, offset_);
  case ')':
    if (parenDepth_ > 0) {
      --parenDepth_;
    }
    return makeToken(TokenKind::RParen, start, offset_);
  case '[':
    ++parenDepth_;
    return makeToken(TokenKind::LBracket, start, offset_);
  case ']':
    if (parenDepth_ > 0) {
      --parenDepth_;
    }
    return makeToken(TokenKind::RBracket, start, offset_);
  case '{':
    ++parenDepth_;
    return makeToken(TokenKind::LBrace, start, offset_);
  case '}':
    if (parenDepth_ > 0) {
      --parenDepth_;
    }
    return makeToken(TokenKind::RBrace, start, offset_);
  case ',':
    return makeToken(TokenKind::Comma, start, offset_);
  case ':':
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::ColonEqual, start, offset_);
    }
    return makeToken(TokenKind::Colon, start, offset_);
  case '.':
    if (peek() == '.' && peek(1) == '.') {
      advance();
      advance();
      return makeToken(TokenKind::DotDotDot, start, offset_);
    }
    if (isAsciiDigit(peek())) {
      return lexNumber(start);
    }
    return makeToken(TokenKind::Dot, start, offset_);
  case '|':
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::PipeEqual, start, offset_);
    }
    return makeToken(TokenKind::Pipe, start, offset_);
  case '@':
    return makeToken(TokenKind::At, start, offset_);
  case '&':
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::AmpEqual, start, offset_);
    }
    return makeToken(TokenKind::Amp, start, offset_);
  case '^':
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::CaretEqual, start, offset_);
    }
    return makeToken(TokenKind::Caret, start, offset_);
  case '~':
    return makeToken(TokenKind::Tilde, start, offset_);
  case '?':
    return makeToken(TokenKind::Question, start, offset_);
  case '+':
    if (peek() == '+') {
      advance();
      return makeToken(TokenKind::PlusPlus, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::PlusEqual, start, offset_);
    }
    return makeToken(TokenKind::Plus, start, offset_);
  case '-':
    if (peek() == '>') {
      advance();
      return makeToken(TokenKind::Arrow, start, offset_);
    }
    if (peek() == '-') {
      advance();
      return makeToken(TokenKind::MinusMinus, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::MinusEqual, start, offset_);
    }
    return makeToken(TokenKind::Minus, start, offset_);
  case '*':
    if (peek() == '*') {
      advance();
      if (peek() == '=') {
        advance();
        return makeToken(TokenKind::StarStarEqual, start, offset_);
      }
      return makeToken(TokenKind::StarStar, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::StarEqual, start, offset_);
    }
    return makeToken(TokenKind::Star, start, offset_);
  case '/':
    if (peek() == '/') {
      advance();
      if (peek() == '=') {
        advance();
        return makeToken(TokenKind::SlashSlashEqual, start, offset_);
      }
      return makeToken(TokenKind::SlashSlash, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::SlashEqual, start, offset_);
    }
    return makeToken(TokenKind::Slash, start, offset_);
  case '%':
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::PercentEqual, start, offset_);
    }
    return makeToken(TokenKind::Percent, start, offset_);
  case '"':
    return lexQuoted(start, '"', TokenKind::String);
  case '\'':
    return lexQuoted(start, '\'', TokenKind::String);
  case '`':
    return lexQuoted(start, '`', TokenKind::Regex);
  default:
    break;
  }
  if (character == '=') {
    if (peek() == '>') {
      advance();
      return makeToken(TokenKind::FatArrow, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::EqualEqual, start, offset_);
    }
    return makeToken(TokenKind::Equal, start, offset_);
  }
  if (character == '$') {
    return makeToken(TokenKind::Dollar, start, offset_);
  }
  if (character == '!') {
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::BangEqual, start, offset_);
    }
    return makeToken(TokenKind::Bang, start, offset_);
  }
  if (character == '<') {
    if (peek() == '<') {
      advance();
      if (peek() == '=') {
        advance();
        return makeToken(TokenKind::LessLessEqual, start, offset_);
      }
      return makeToken(TokenKind::LessLess, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::LessEqual, start, offset_);
    }
    return makeToken(TokenKind::Less, start, offset_);
  }
  if (character == '>') {
    if (peek() == '>') {
      advance();
      if (peek() == '=') {
        advance();
        return makeToken(TokenKind::GreaterGreaterEqual, start, offset_);
      }
      return makeToken(TokenKind::GreaterGreater, start, offset_);
    }
    if (peek() == '=') {
      advance();
      return makeToken(TokenKind::GreaterEqual, start, offset_);
    }
    return makeToken(TokenKind::Greater, start, offset_);
  }
  if ((character == 'f' || character == 'F') && (peek() == '"' || peek() == '\'')) {
    const char quote = peek();
    advance();
    return lexQuoted(start, quote, TokenKind::FString);
  }
  if (isAsciiLetter(character)) {
    return lexIdentifier(start);
  }
  if (isAsciiDigit(character)) {
    return lexNumber(start);
  }
  return makeToken(TokenKind::Unknown, start, offset_);
}

std::vector<Token> Lexer::tokenizeAll() {
  std::vector<Token> tokens;
  while (true) {
    Token token = nextToken();
    const bool done = token.kind() == TokenKind::EndOfFile;
    tokens.push_back(std::move(token));
    if (done) {
      break;
    }
  }
  return tokens;
}

}  // namespace sere
