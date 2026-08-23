/// @file Token.cpp
/// Token accessors and keyword / kind name tables.

#include "sere/lex/Token.h"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace sere {
namespace {

using KeywordEntry = std::pair<std::string_view, TokenKind>;

constexpr KeywordEntry kKeywords[] = {
    {"False", TokenKind::KeywordFalse},
    {"None", TokenKind::KeywordNone},
    {"True", TokenKind::KeywordTrue},
    {"and", TokenKind::KeywordAnd},
    {"as", TokenKind::KeywordAs},
    {"assert", TokenKind::KeywordAssert},
    {"break", TokenKind::KeywordBreak},
    {"case", TokenKind::KeywordCase},
    {"class", TokenKind::KeywordClass},
    {"const", TokenKind::KeywordConst},
    {"continue", TokenKind::KeywordContinue},
    {"def", TokenKind::KeywordDef},
    {"defer", TokenKind::KeywordDefer},
    {"del", TokenKind::KeywordDel},
    {"elif", TokenKind::KeywordElif},
    {"else", TokenKind::KeywordElse},
    {"enum", TokenKind::KeywordEnum},
    {"except", TokenKind::KeywordExcept},
    {"extern", TokenKind::KeywordExtern},
    {"finally", TokenKind::KeywordFinally},
    {"for", TokenKind::KeywordFor},
    {"from", TokenKind::KeywordFrom},
    {"if", TokenKind::KeywordIf},
    {"import", TokenKind::KeywordImport},
    {"in", TokenKind::KeywordIn},
    {"is", TokenKind::KeywordIs},
    {"lambda", TokenKind::KeywordLambda},
    {"macro", TokenKind::KeywordMacro},
    {"match", TokenKind::KeywordMatch},
    {"not", TokenKind::KeywordNot},
    {"or", TokenKind::KeywordOr},
    {"pass", TokenKind::KeywordPass},
    {"raise", TokenKind::KeywordRaise},
    {"return", TokenKind::KeywordReturn},
    {"static", TokenKind::KeywordStatic},
    {"struct", TokenKind::KeywordStruct},
    {"super", TokenKind::KeywordSuper},
    {"try", TokenKind::KeywordTry},
    {"type", TokenKind::KeywordType},
    {"while", TokenKind::KeywordWhile},
    {"with", TokenKind::KeywordWith},
};

}  // namespace

Token::Token(TokenKind kind, SourceRange range, std::string spelling)
    : kind_(kind), range_(range), spelling_(std::move(spelling)) {}

TokenKind Token::kind() const { return kind_; }

SourceRange Token::range() const { return range_; }

SourceLocation Token::location() const { return range_.start; }

std::string_view Token::spelling() const { return spelling_; }

TokenKind keywordKind(std::string_view spelling) {
  for (const KeywordEntry& entry : kKeywords) {
    if (entry.first == spelling) {
      return entry.second;
    }
  }
  return TokenKind::Identifier;
}

std::string_view tokenKindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::EndOfFile:
    return "EndOfFile";
  case TokenKind::Newline:
    return "Newline";
  case TokenKind::Indent:
    return "Indent";
  case TokenKind::Dedent:
    return "Dedent";
  case TokenKind::Identifier:
    return "Identifier";
  case TokenKind::Integer:
    return "Integer";
  case TokenKind::Float:
    return "Float";
  case TokenKind::String:
    return "String";
  case TokenKind::FString:
    return "FString";
  case TokenKind::Regex:
    return "Regex";
  case TokenKind::Comment:
    return "Comment";
  case TokenKind::Plus:
    return "Plus";
  case TokenKind::PlusPlus:
    return "PlusPlus";
  case TokenKind::PlusEqual:
    return "PlusEqual";
  case TokenKind::Minus:
    return "Minus";
  case TokenKind::MinusMinus:
    return "MinusMinus";
  case TokenKind::MinusEqual:
    return "MinusEqual";
  case TokenKind::Star:
    return "Star";
  case TokenKind::StarStar:
    return "StarStar";
  case TokenKind::StarEqual:
    return "StarEqual";
  case TokenKind::StarStarEqual:
    return "StarStarEqual";
  case TokenKind::Slash:
    return "Slash";
  case TokenKind::SlashSlash:
    return "SlashSlash";
  case TokenKind::SlashEqual:
    return "SlashEqual";
  case TokenKind::SlashSlashEqual:
    return "SlashSlashEqual";
  case TokenKind::Percent:
    return "Percent";
  case TokenKind::PercentEqual:
    return "PercentEqual";
  case TokenKind::Equal:
    return "Equal";
  case TokenKind::EqualEqual:
    return "EqualEqual";
  case TokenKind::Bang:
    return "Bang";
  case TokenKind::BangEqual:
    return "BangEqual";
  case TokenKind::Dollar:
    return "Dollar";
  case TokenKind::FatArrow:
    return "FatArrow";
  case TokenKind::Less:
    return "Less";
  case TokenKind::LessLess:
    return "LessLess";
  case TokenKind::LessEqual:
    return "LessEqual";
  case TokenKind::LessLessEqual:
    return "LessLessEqual";
  case TokenKind::Greater:
    return "Greater";
  case TokenKind::GreaterGreater:
    return "GreaterGreater";
  case TokenKind::GreaterEqual:
    return "GreaterEqual";
  case TokenKind::GreaterGreaterEqual:
    return "GreaterGreaterEqual";
  case TokenKind::Amp:
    return "Amp";
  case TokenKind::AmpEqual:
    return "AmpEqual";
  case TokenKind::Caret:
    return "Caret";
  case TokenKind::CaretEqual:
    return "CaretEqual";
  case TokenKind::Tilde:
    return "Tilde";
  case TokenKind::PipeEqual:
    return "PipeEqual";
  case TokenKind::Question:
    return "Question";
  case TokenKind::ColonEqual:
    return "ColonEqual";
  case TokenKind::LParen:
    return "LParen";
  case TokenKind::RParen:
    return "RParen";
  case TokenKind::LBracket:
    return "LBracket";
  case TokenKind::RBracket:
    return "RBracket";
  case TokenKind::LBrace:
    return "LBrace";
  case TokenKind::RBrace:
    return "RBrace";
  case TokenKind::Colon:
    return "Colon";
  case TokenKind::Comma:
    return "Comma";
  case TokenKind::Dot:
    return "Dot";
  case TokenKind::DotDotDot:
    return "DotDotDot";
  case TokenKind::Pipe:
    return "Pipe";
  case TokenKind::Arrow:
    return "Arrow";
  case TokenKind::At:
    return "At";
  default:
    break;
  }
  switch (kind) {
  case TokenKind::KeywordFalse:
    return "KeywordFalse";
  case TokenKind::KeywordNone:
    return "KeywordNone";
  case TokenKind::KeywordTrue:
    return "KeywordTrue";
  case TokenKind::KeywordAnd:
    return "KeywordAnd";
  case TokenKind::KeywordAs:
    return "KeywordAs";
  case TokenKind::KeywordAssert:
    return "KeywordAssert";
  case TokenKind::KeywordBreak:
    return "KeywordBreak";
  case TokenKind::KeywordCase:
    return "KeywordCase";
  case TokenKind::KeywordClass:
    return "KeywordClass";
  case TokenKind::KeywordConst:
    return "KeywordConst";
  case TokenKind::KeywordContinue:
    return "KeywordContinue";
  case TokenKind::KeywordDef:
    return "KeywordDef";
  case TokenKind::KeywordDefer:
    return "KeywordDefer";
  case TokenKind::KeywordDel:
    return "KeywordDel";
  case TokenKind::KeywordElif:
    return "KeywordElif";
  case TokenKind::KeywordElse:
    return "KeywordElse";
  case TokenKind::KeywordEnum:
    return "KeywordEnum";
  case TokenKind::KeywordExcept:
    return "KeywordExcept";
  case TokenKind::KeywordExtern:
    return "KeywordExtern";
  case TokenKind::KeywordFinally:
    return "KeywordFinally";
  case TokenKind::KeywordFor:
    return "KeywordFor";
  case TokenKind::KeywordFrom:
    return "KeywordFrom";
  case TokenKind::KeywordIf:
    return "KeywordIf";
  case TokenKind::KeywordImport:
    return "KeywordImport";
  case TokenKind::KeywordIn:
    return "KeywordIn";
  case TokenKind::KeywordIs:
    return "KeywordIs";
  case TokenKind::KeywordLambda:
    return "KeywordLambda";
  case TokenKind::KeywordMacro:
    return "KeywordMacro";
  case TokenKind::KeywordMatch:
    return "KeywordMatch";
  case TokenKind::KeywordNot:
    return "KeywordNot";
  case TokenKind::KeywordOr:
    return "KeywordOr";
  case TokenKind::KeywordPass:
    return "KeywordPass";
  case TokenKind::KeywordRaise:
    return "KeywordRaise";
  case TokenKind::KeywordReturn:
    return "KeywordReturn";
  case TokenKind::KeywordStatic:
    return "KeywordStatic";
  case TokenKind::KeywordStruct:
    return "KeywordStruct";
  case TokenKind::KeywordSuper:
    return "KeywordSuper";
  case TokenKind::KeywordTry:
    return "KeywordTry";
  case TokenKind::KeywordType:
    return "KeywordType";
  case TokenKind::KeywordWhile:
    return "KeywordWhile";
  case TokenKind::KeywordWith:
    return "KeywordWith";
  case TokenKind::Unknown:
    return "Unknown";
  default:
    return "Unknown";
  }
}

namespace {

std::string unescapeStringBody(std::string_view body, bool regex) {
  std::string out;
  out.reserve(body.size());
  for (std::size_t index = 0; index < body.size(); ++index) {
    if (body[index] != '\\' || index + 1 >= body.size()) {
      out.push_back(body[index]);
      continue;
    }
    const char next = body[index + 1];
    ++index;
    if (regex) {
      if (next == '`' || next == '\\') {
        out.push_back(next);
      } else {
        out.push_back('\\');
        out.push_back(next);
      }
      continue;
    }
    switch (next) {
    case 'n':
      out.push_back('\n');
      break;
    case 't':
      out.push_back('\t');
      break;
    case 'r':
      out.push_back('\r');
      break;
    case '0':
      out.push_back('\0');
      break;
    default:
      out.push_back(next);
      break;
    }
  }
  return out;
}

}  // namespace

std::string_view stringLiteralInner(std::string_view spelling) {
  if (spelling.empty()) {
    return {};
  }
  std::size_t start = 0;
  if (spelling[0] == 'f' || spelling[0] == 'F') {
    start = 1;
  }
  if (start >= spelling.size()) {
    return {};
  }
  const char quote = spelling[start];
  if (quote == '`') {
    if (spelling.size() < start + 2) {
      return {};
    }
    return spelling.substr(start + 1, spelling.size() - start - 2);
  }
  std::size_t open = 1;
  while (start + open < spelling.size() && spelling[start + open] == quote && open < 3) {
    ++open;
  }
  if (open == 2) {
    open = 1;
  }
  if (spelling.size() < start + open * 2) {
    return {};
  }
  return spelling.substr(start + open, spelling.size() - start - open * 2);
}

DecodedString decodeStringToken(std::string_view spelling) {
  DecodedString decoded;
  decoded.regex = !spelling.empty() && spelling[0] == '`';
  decoded.value = unescapeStringBody(stringLiteralInner(spelling), decoded.regex);
  return decoded;
}

namespace {

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

[[nodiscard]] int integerBase(std::string_view spelling) {
  if (spelling.size() >= 2 && spelling[0] == '0') {
    const char prefix = spelling[1];
    if (prefix == 'x' || prefix == 'X') {
      return 16;
    }
    if (prefix == 'b' || prefix == 'B') {
      return 2;
    }
    if (prefix == 'o' || prefix == 'O') {
      return 8;
    }
  }
  return 10;
}

}  // namespace

ParsedInteger parseIntegerToken(std::string_view spelling) {
  ParsedInteger parsed;
  if (spelling.empty()) {
    return parsed;
  }
  const int base = integerBase(spelling);
  const std::size_t start = base == 10 ? 0 : 2;
  bool sawDigit = false;
  bool lastWasUnderscore = false;
  std::uint64_t value = 0;
  const std::uint64_t maxSigned =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  for (std::size_t index = start; index < spelling.size(); ++index) {
    const char character = spelling[index];
    if (character == '_') {
      if (!sawDigit || lastWasUnderscore) {
        return parsed;
      }
      lastWasUnderscore = true;
      continue;
    }
    const int digit = numericDigitValue(character);
    if (digit < 0 || digit >= base) {
      return parsed;
    }
    if (value > (std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(digit)) /
                    static_cast<std::uint64_t>(base)) {
      parsed.status = IntegerParseStatus::Overflow;
      return parsed;
    }
    value = value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
    sawDigit = true;
    lastWasUnderscore = false;
  }
  if (!sawDigit || lastWasUnderscore) {
    return parsed;
  }
  if (value > maxSigned) {
    parsed.status = IntegerParseStatus::Overflow;
    return parsed;
  }
  parsed.value = static_cast<std::int64_t>(value);
  parsed.status = IntegerParseStatus::Ok;
  return parsed;
}

}  // namespace sere
