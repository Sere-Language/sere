/// @file Parser.cpp
/// Parses typed functions, classes, declarations, and expressions.

#include "sere/parse/Parser.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/source/SourceManager.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace sere {
namespace {

void markPrivateFromDecorators(Node& node, const std::vector<std::string>& decorators) {
  for (const std::string& name : decorators) {
    if (name == "private") {
      node.setPrivate(true);
      return;
    }
  }
}

void attachFunctionDecorators(FunctionDef& function, std::vector<std::unique_ptr<Expr>> exprs) {
  function.setDecorators(decoratorExprNames(exprs));
  function.setDecoratorExprs(std::move(exprs));
}

[[nodiscard]] bool isLineEnd(TokenKind kind) {
  return kind == TokenKind::Newline || kind == TokenKind::Dedent || kind == TokenKind::EndOfFile;
}

[[nodiscard]] std::string describeToken(const Token& token) {
  switch (token.kind()) {
  case TokenKind::EndOfFile:
    return "end of file";
  case TokenKind::Newline:
    return "newline";
  case TokenKind::Indent:
    return "indent";
  case TokenKind::Dedent:
    return "dedent";
  case TokenKind::String:
    return "string literal";
  case TokenKind::FString:
    return "f-string";
  case TokenKind::Regex:
    return "regex literal";
  case TokenKind::Float:
    return "float literal";
  case TokenKind::Integer:
    return "integer literal";
  default:
    break;
  }
  if (token.spelling().empty()) {
    return std::string(tokenKindName(token.kind()));
  }
  return "'" + std::string(token.spelling()) + "'";
}

class BoolScope {
public:
  BoolScope(bool& flag, bool value) : flag_(flag), previous_(flag) { flag_ = value; }
  ~BoolScope() { flag_ = previous_; }

private:
  bool& flag_;
  bool previous_;
};

[[nodiscard]] std::int64_t parseIntegerOrZero(DiagnosticEngine& diagnostics, const Token& token) {
  const ParsedInteger parsed = parseIntegerToken(token.spelling());
  if (parsed.status == IntegerParseStatus::Ok) {
    return parsed.value;
  }
  if (parsed.status == IntegerParseStatus::Overflow) {
    diagnostics.error(token.range(), "integer literal does not fit in i64");
    diagnostics.help("use a value between -9223372036854775808 and 9223372036854775807");
  } else {
    diagnostics.error(token.range(), "invalid integer literal");
    diagnostics.help("use 0x / 0b / 0o prefixes or decimal digits, with '_' between digits");
  }
  return 0;
}

struct ParsedFloat {
  double value = 0.0;
  bool isF32 = false;
};

[[nodiscard]] ParsedFloat parseFloat(std::string_view spelling) {
  ParsedFloat parsed;
  if (spelling.empty()) {
    return parsed;
  }
  std::string cleaned;
  cleaned.reserve(spelling.size());
  for (const char character : spelling) {
    if (character != '_') {
      cleaned.push_back(character);
    }
  }
  if (cleaned.empty()) {
    return parsed;
  }
  if (cleaned.back() == 'f' || cleaned.back() == 'F') {
    parsed.isF32 = true;
    cleaned.pop_back();
  }
  char* end = nullptr;
  parsed.value = std::strtod(cleaned.c_str(), &end);
  return parsed;
}

[[nodiscard]] SourceLocation shiftLocation(SourceLocation local, SourceLocation base) {
  SourceLocation out = base;
  out.offset = base.offset + local.offset;
  out.line = base.line + local.line - 1;
  out.column = local.line == 1 ? base.column + local.column - 1 : local.column;
  return out;
}

[[nodiscard]] UnaryOp prefixOp(TokenKind kind) {
  switch (kind) {
  case TokenKind::KeywordNot:
    return UnaryOp::Not;
  case TokenKind::Plus:
    return UnaryOp::Pos;
  case TokenKind::Tilde:
    return UnaryOp::Invert;
  case TokenKind::PlusPlus:
    return UnaryOp::PreInc;
  case TokenKind::MinusMinus:
    return UnaryOp::PreDec;
  case TokenKind::Star:
    return UnaryOp::Deref;
  case TokenKind::Amp:
    return UnaryOp::AddrOf;
  default:
    return UnaryOp::Neg;
  }
}

} // namespace

Parser::Parser(DiagnosticEngine& diagnostics,
               std::vector<Token> tokens,
               const SourceManager* source)
    : diagnostics_(&diagnostics), source_(source), tokens_(std::move(tokens)) {
}

bool Parser::isAtEnd() const {
  return peek().kind() == TokenKind::EndOfFile;
}

const Token& Parser::peek() const {
  return tokens_.at(current_);
}

const Token& Parser::peekNth(std::size_t ahead) const {
  const std::size_t index = current_ + ahead;
  if (index >= tokens_.size()) {
    return tokens_.back();
  }
  return tokens_[index];
}

const Token& Parser::previous() const {
  return tokens_.at(current_ - 1);
}

const Token& Parser::advance() {
  if (!isAtEnd()) {
    ++current_;
  }
  return previous();
}

bool Parser::check(TokenKind kind) const {
  return peek().kind() == kind;
}

bool Parser::match(TokenKind kind) {
  if (!check(kind)) {
    return false;
  }
  advance();
  return true;
}

bool Parser::matchDot() {
  if (match(TokenKind::Dot)) {
    return true;
  }
  if (check(TokenKind::Newline) && peekNth(1).kind() == TokenKind::Dot) {
    advance();
    advance();
    return true;
  }
  return false;
}

bool Parser::consume(TokenKind kind, const char* errorMessage) {
  if (match(kind)) {
    return true;
  }
  diagnostics_->error(peek().range(),
                      std::string(errorMessage) + ", found " + describeToken(peek()));
  return false;
}

void Parser::synchronize() {
  while (!isAtEnd()) {
    if (match(TokenKind::Newline)) {
      return;
    }
    if (check(TokenKind::Dedent) || check(TokenKind::KeywordDef) ||
        check(TokenKind::KeywordClass) || check(TokenKind::KeywordIf) ||
        check(TokenKind::KeywordElif) || check(TokenKind::KeywordElse) ||
        check(TokenKind::KeywordWhile) || check(TokenKind::KeywordReturn) ||
        check(TokenKind::EndOfFile)) {
      return;
    }
    // `type Name = ...` starts a statement; `type[...]` is a type constructor.
    if (check(TokenKind::KeywordType) && peekNth(1).kind() == TokenKind::Identifier) {
      return;
    }
    advance();
  }
}

void Parser::recoverStatement() {
  const std::size_t before = current_;
  synchronize();
  if (current_ == before && !isAtEnd()) {
    advance();
  }
}

void Parser::skipNewlines() {
  while (match(TokenKind::Newline)) {
  }
}

bool Parser::finishLine() {
  if (isLineEnd(peek().kind())) {
    match(TokenKind::Newline);
    return true;
  }
  diagnostics_->error(peek().range(), "expected end of statement, found " + describeToken(peek()));
  synchronize();
  return false;
}

bool Parser::finishExprLine(const Expr* expr) {
  if (expr != nullptr && expr->kind() == NodeKind::MacroInvokeExpr &&
      static_cast<const MacroInvokeExpr&>(*expr).delimiter() == MacroDelimiter::Indent) {
    return true;
  }
  return finishLine();
}

std::unique_ptr<Expr> Parser::parseEqualsValue() {
  if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Colon &&
      peekNth(2).kind() == TokenKind::Newline) {
    std::string name = std::string(peek().spelling());
    SourceRange nameRange = peek().range();
    advance();
    return parseMacroInvokeIndentExpr(std::move(name), nameRange);
  }
  std::unique_ptr<Expr> first = parseExpr();
  if (first == nullptr || !check(TokenKind::Comma)) {
    return first;
  }
  std::vector<std::unique_ptr<Expr>> elements;
  const SourceLocation start = first->range().start;
  elements.push_back(std::move(first));
  while (match(TokenKind::Comma)) {
    if (isLineEnd(peek().kind())) {
      break;
    }
    std::unique_ptr<Expr> next = parseExpr();
    if (next == nullptr) {
      return nullptr;
    }
    elements.push_back(std::move(next));
  }
  return std::make_unique<TupleExpr>(SourceRange{start, elements.back()->range().end},
                                     std::move(elements));
}

std::string Parser::parseIdentifier(const char* errorMessage) {
  if (!check(TokenKind::Identifier)) {
    diagnostics_->error(peek().range(),
                        std::string(errorMessage) + ", found " + describeToken(peek()));
    return {};
  }
  return std::string(advance().spelling());
}

std::string Parser::parseStringValue() {
  return decodeStringToken(previous().spelling()).value;
}

std::unique_ptr<TypeExpr> Parser::parseTypeAtom() {
  if (match(TokenKind::KeywordNone)) {
    return std::make_unique<TypeExpr>(
        previous().range(), "None", std::vector<std::unique_ptr<TypeExpr>>{});
  }
  if (check(TokenKind::Integer)) {
    const Token& number = advance();
    return std::make_unique<TypeExpr>(
        number.range(), std::string(number.spelling()), std::vector<std::unique_ptr<TypeExpr>>{});
  }
  if (match(TokenKind::KeywordType)) {
    const Token& name = previous();
    const SourceLocation start = name.range().start;
    SourceLocation end = name.range().end;
    std::vector<std::unique_ptr<TypeExpr>> args;
    if (match(TokenKind::LBracket)) {
      args = parseTypeArgList();
      if (!consume(TokenKind::RBracket, "expected ']' after type arguments")) {
        return nullptr;
      }
      end = previous().range().end;
    }
    return std::make_unique<TypeExpr>(SourceRange{start, end}, "type", std::move(args));
  }
  if (match(TokenKind::DotDotDot)) {
    return std::make_unique<TypeExpr>(
        previous().range(), "...", std::vector<std::unique_ptr<TypeExpr>>{});
  }
  if (match(TokenKind::LBracket)) {
    const SourceLocation start = previous().range().start;
    std::vector<std::unique_ptr<TypeExpr>> args = parseTypeArgList();
    if (!consume(TokenKind::RBracket, "expected ']' after type list")) {
      return nullptr;
    }
    return std::make_unique<TypeExpr>(
        SourceRange{start, previous().range().end}, "[]", std::move(args));
  }
  if (!check(TokenKind::Identifier)) {
    diagnostics_->error(peek().range(), "expected type name, found " + describeToken(peek()));
    return nullptr;
  }
  const Token& name = advance();
  std::string spelling(name.spelling());
  const SourceLocation start = name.range().start;
  SourceLocation end = name.range().end;
  while (matchDot()) {
    skipNewlines();
    if (!check(TokenKind::Identifier)) {
      end = previous().range().end;
      break;
    }
    spelling += '.';
    spelling += advance().spelling();
    end = previous().range().end;
  }
  std::vector<std::unique_ptr<TypeExpr>> args;
  if (match(TokenKind::LBracket)) {
    args = parseTypeArgList();
    if (!consume(TokenKind::RBracket, "expected ']' after type arguments")) {
      return nullptr;
    }
    end = previous().range().end;
  }
  return std::make_unique<TypeExpr>(SourceRange{start, end}, std::move(spelling), std::move(args));
}

std::unique_ptr<TypeExpr> Parser::parseTypeExpr() {
  std::unique_ptr<TypeExpr> left = parseTypeAtom();
  if (left == nullptr || !check(TokenKind::Pipe)) {
    return left;
  }
  std::vector<std::unique_ptr<TypeExpr>> members;
  const SourceLocation start = left->range().start;
  members.push_back(std::move(left));
  while (match(TokenKind::Pipe)) {
    std::unique_ptr<TypeExpr> next = parseTypeAtom();
    if (next == nullptr) {
      return nullptr;
    }
    members.push_back(std::move(next));
  }
  return std::make_unique<TypeExpr>(
      SourceRange{start, previous().range().end}, "|", std::move(members));
}

std::vector<std::unique_ptr<TypeExpr>> Parser::parseTypeArgList() {
  std::vector<std::unique_ptr<TypeExpr>> args;
  skipNewlines();
  if (check(TokenKind::RBracket)) {
    return args;
  }
  while (true) {
    skipNewlines();
    if (check(TokenKind::RBracket)) {
      break;
    }
    std::unique_ptr<TypeExpr> arg = parseTypeExpr();
    if (arg == nullptr) {
      return {};
    }
    args.push_back(std::move(arg));
    skipNewlines();
    if (!match(TokenKind::Comma)) {
      break;
    }
  }
  skipNewlines();
  return args;
}

std::vector<std::string> Parser::parseTypeParamList() {
  std::vector<std::string> typeParams;
  if (!match(TokenKind::LBracket)) {
    return typeParams;
  }
  if (!check(TokenKind::RBracket)) {
    while (true) {
      typeParams.push_back(parseIdentifier("expected type parameter"));
      if (!match(TokenKind::Comma)) {
        break;
      }
    }
  }
  if (!consume(TokenKind::RBracket, "expected ']' after type parameters")) {
    return {};
  }
  return typeParams;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenKind::Integer)) {
    return std::make_unique<IntegerLiteral>(previous().range(),
                                            parseIntegerOrZero(*diagnostics_, previous()));
  }
  if (match(TokenKind::Float)) {
    const ParsedFloat parsed = parseFloat(previous().spelling());
    return std::make_unique<FloatLiteral>(previous().range(), parsed.value, parsed.isF32);
  }
  if (match(TokenKind::String)) {
    const Token& token = previous();
    const DecodedString decoded = decodeStringToken(token.spelling());
    if (isSingleQuotedLiteral(token.spelling()) && decoded.value.size() == 1) {
      const auto byte = static_cast<unsigned char>(decoded.value[0]);
      return std::make_unique<IntegerLiteral>(token.range(), static_cast<std::int64_t>(byte), true);
    }
    return std::make_unique<StringLiteral>(token.range(), decoded.value, false);
  }
  if (match(TokenKind::Regex)) {
    const DecodedString decoded = decodeStringToken(previous().spelling());
    return std::make_unique<StringLiteral>(previous().range(), decoded.value, true);
  }
  if (match(TokenKind::FString)) {
    return parseFString();
  }
  if (match(TokenKind::KeywordTrue)) {
    return std::make_unique<BooleanLiteral>(previous().range(), true);
  }
  if (match(TokenKind::KeywordFalse)) {
    return std::make_unique<BooleanLiteral>(previous().range(), false);
  }
  if (match(TokenKind::KeywordNone)) {
    return std::make_unique<NoneLiteral>(previous().range());
  }
  if (match(TokenKind::KeywordLambda)) {
    return parseLambda();
  }
  if (match(TokenKind::KeywordSuper)) {
    return std::make_unique<NameExpr>(previous().range(), "super");
  }
  if (match(TokenKind::LParen)) {
    const SourceLocation start = previous().range().start;
    skipNewlines();
    if (match(TokenKind::RParen)) {
      return std::make_unique<TupleExpr>(SourceRange{start, previous().range().end},
                                         std::vector<std::unique_ptr<Expr>>{});
    }
    const BoolScope enableCasts(allowAsCast_, true);
    std::unique_ptr<Expr> inner = parseExpr();
    if (inner == nullptr) {
      return nullptr;
    }
    if (check(TokenKind::KeywordFor)) {
      std::unique_ptr<Expr> comprehension = parseComprehension(std::move(inner));
      if (comprehension == nullptr || !consume(TokenKind::RParen, "expected ')'")) {
        return nullptr;
      }
      return comprehension;
    }
    skipNewlines();
    if (check(TokenKind::Comma)) {
      return parseTupleTail(std::move(inner), start);
    }
    if (!consume(TokenKind::RParen, "expected ')'")) {
      return nullptr;
    }
    return inner;
  }
  if (match(TokenKind::Identifier)) {
    const Token& name = previous();
    if (match(TokenKind::ColonEqual)) {
      std::unique_ptr<Expr> value = parseExpr();
      if (value == nullptr) {
        return nullptr;
      }
      return std::make_unique<WalrusExpr>(SourceRange{name.range().start, value->range().end},
                                          std::string(name.spelling()),
                                          std::move(value));
    }
    if (check(TokenKind::Bang)) {
      return parseMacroInvokeExpr(std::string(name.spelling()), name.range());
    }
    return std::make_unique<NameExpr>(name.range(), std::string(name.spelling()));
  }
  if (inQuote_ && match(TokenKind::Dollar)) {
    return parseSplice();
  }
  if (check(TokenKind::Unknown)) {
    diagnostics_->error(peek().range(),
                        "unexpected character '" + std::string(peek().spelling()) + "'");
    return nullptr;
  }
  if (check(TokenKind::LBracket)) {
    return parseListLiteral();
  }
  if (check(TokenKind::LBrace)) {
    return parseDictLiteral();
  }
  diagnostics_->error(peek().range(), "expected expression, found " + describeToken(peek()));
  return nullptr;
}

bool Parser::looksLikeGenericCall() const {
  std::size_t index = current_;
  int depth = 1;
  while (index < tokens_.size() && depth > 0) {
    const TokenKind kind = tokens_[index].kind();
    if (kind == TokenKind::EndOfFile) {
      return false;
    }
    if (kind == TokenKind::LBracket) {
      ++depth;
    } else if (kind == TokenKind::RBracket) {
      --depth;
    }
    ++index;
  }
  return depth == 0 && index < tokens_.size() && tokens_[index].kind() == TokenKind::LParen;
}

bool Parser::looksLikeTypeApplication() const {
  std::size_t index = current_;
  int depth = 1;
  bool colonAtTop = false;
  while (index < tokens_.size() && depth > 0) {
    const TokenKind kind = tokens_[index].kind();
    if (kind == TokenKind::EndOfFile) {
      return false;
    }
    if (kind == TokenKind::LBracket) {
      ++depth;
    } else if (kind == TokenKind::RBracket) {
      --depth;
    } else if (kind == TokenKind::Colon && depth == 1) {
      colonAtTop = true;
    }
    ++index;
  }
  if (depth != 0 || colonAtTop || current_ >= tokens_.size()) {
    return false;
  }
  const TokenKind first = tokens_[current_].kind();
  return first == TokenKind::Identifier || first == TokenKind::Integer ||
         first == TokenKind::KeywordNone;
}

std::unique_ptr<Expr> Parser::parseComprehension(std::unique_ptr<Expr> element) {
  if (!consume(TokenKind::KeywordFor, "expected 'for' in comprehension")) {
    return nullptr;
  }
  const std::string name = parseIdentifier("expected comprehension variable");
  if (name.empty() ||
      !consume(TokenKind::KeywordIn, "expected 'in' after comprehension variable")) {
    return nullptr;
  }
  std::unique_ptr<Expr> iterable = parseExpr();
  if (iterable == nullptr || element == nullptr) {
    return nullptr;
  }
  return std::make_unique<ComprehensionExpr>(
      SourceRange{element->range().start, iterable->range().end},
      std::move(element),
      name,
      std::move(iterable));
}

std::unique_ptr<Expr> Parser::parseTupleTail(std::unique_ptr<Expr> first, SourceLocation start) {
  std::vector<std::unique_ptr<Expr>> elements;
  elements.push_back(std::move(first));
  while (match(TokenKind::Comma)) {
    skipNewlines();
    if (check(TokenKind::RParen)) {
      break;
    }
    const BoolScope enableCasts(allowAsCast_, true);
    std::unique_ptr<Expr> next = parseExpr();
    if (next == nullptr) {
      return nullptr;
    }
    elements.push_back(std::move(next));
    skipNewlines();
  }
  if (!consume(TokenKind::RParen, "expected ')' after tuple")) {
    return nullptr;
  }
  return std::make_unique<TupleExpr>(SourceRange{start, previous().range().end},
                                     std::move(elements));
}

std::unique_ptr<Expr> Parser::parseLambda() {
  const SourceLocation start = previous().range().start;
  std::vector<ParamDecl> params;
  std::unique_ptr<TypeExpr> returnType;
  if (match(TokenKind::LParen)) {
    params = parseParams();
    if (!consume(TokenKind::RParen, "expected ')' after lambda parameters")) {
      return nullptr;
    }
    if (match(TokenKind::Arrow)) {
      returnType = parseTypeExpr();
      if (returnType == nullptr) {
        return nullptr;
      }
    }
  } else if (!check(TokenKind::Colon)) {
    while (true) {
      ParamDecl param;
      param.range = peek().range();
      param.name = parseIdentifier("expected lambda parameter");
      if (param.name.empty()) {
        return nullptr;
      }
      params.push_back(std::move(param));
      if (!match(TokenKind::Comma)) {
        break;
      }
    }
  }
  if (!consume(TokenKind::Colon, "expected ':' after lambda parameters")) {
    return nullptr;
  }
  std::unique_ptr<Expr> body = parseExpr();
  if (body == nullptr) {
    return nullptr;
  }
  return std::make_unique<LambdaExpr>(SourceRange{start, body->range().end},
                                      std::move(params),
                                      std::move(body),
                                      std::move(returnType));
}

std::unique_ptr<Expr> Parser::parseListLiteral() {
  const SourceLocation start = peek().range().start;
  if (!consume(TokenKind::LBracket, "expected '['")) {
    return nullptr;
  }
  const BoolScope enableCasts(allowAsCast_, true);
  skipNewlines();
  std::vector<std::unique_ptr<Expr>> elements;
  if (!check(TokenKind::RBracket)) {
    std::unique_ptr<Expr> element = parseExpr();
    if (element == nullptr) {
      return nullptr;
    }
    if (check(TokenKind::KeywordFor)) {
      std::unique_ptr<Expr> comprehension = parseComprehension(std::move(element));
      if (comprehension == nullptr ||
          !consume(TokenKind::RBracket, "expected ']' after comprehension")) {
        return nullptr;
      }
      return comprehension;
    }
    elements.push_back(std::move(element));
    skipNewlines();
    while (match(TokenKind::Comma)) {
      skipNewlines();
      if (check(TokenKind::RBracket)) {
        break;
      }
      std::unique_ptr<Expr> next = parseExpr();
      if (next == nullptr) {
        return nullptr;
      }
      elements.push_back(std::move(next));
      skipNewlines();
    }
  }
  skipNewlines();
  if (!consume(TokenKind::RBracket, "expected ']' after list")) {
    return nullptr;
  }
  return std::make_unique<ListLiteral>(SourceRange{start, previous().range().end},
                                       std::move(elements));
}

std::unique_ptr<Expr> Parser::parseDictLiteral() {
  const SourceLocation start = peek().range().start;
  if (!consume(TokenKind::LBrace, "expected '{'")) {
    return nullptr;
  }
  const BoolScope enableCasts(allowAsCast_, true);
  skipNewlines();
  std::vector<std::unique_ptr<Expr>> keys;
  std::vector<std::unique_ptr<Expr>> values;
  if (!check(TokenKind::RBrace)) {
    while (true) {
      skipNewlines();
      if (check(TokenKind::RBrace)) {
        break;
      }
      std::unique_ptr<Expr> key = parseExpr();
      if (key == nullptr || !consume(TokenKind::Colon, "expected ':' after dict key")) {
        return nullptr;
      }
      skipNewlines();
      std::unique_ptr<Expr> value = parseExpr();
      if (value == nullptr) {
        return nullptr;
      }
      keys.push_back(std::move(key));
      values.push_back(std::move(value));
      skipNewlines();
      if (!match(TokenKind::Comma)) {
        break;
      }
    }
  }
  skipNewlines();
  if (!consume(TokenKind::RBrace, "expected '}' after dict")) {
    return nullptr;
  }
  return std::make_unique<DictLiteral>(
      SourceRange{start, previous().range().end}, std::move(keys), std::move(values));
}

ParsedCallArguments Parser::parseCallArguments() {
  ParsedCallArguments parsed;
  const BoolScope enableCasts(allowAsCast_, true);
  bool seenKeyword = false;
  skipNewlines();
  if (check(TokenKind::RParen)) {
    return parsed;
  }
  while (true) {
    skipNewlines();
    if (check(TokenKind::RParen)) {
      break;
    }
    if (match(TokenKind::DotDotDot)) {
      match(TokenKind::Comma);
      skipNewlines();
      if (check(TokenKind::RParen)) {
        break;
      }
      continue;
    }
    if (match(TokenKind::StarStar)) {
      seenKeyword = true;
      NamedArgument keyword;
      keyword.splat = true;
      keyword.value = parseExpr();
      if (keyword.value == nullptr) {
        return {};
      }
      parsed.keyword.push_back(std::move(keyword));
    } else if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Equal) {
      seenKeyword = true;
      NamedArgument keyword;
      keyword.name = advance().spelling();
      (void)advance();
      keyword.value = parseExpr();
      if (keyword.value == nullptr) {
        return {};
      }
      parsed.keyword.push_back(std::move(keyword));
    } else {
      if (seenKeyword) {
        diagnostics_->error(peek().range(), "positional argument follows keyword argument");
        return {};
      }
      std::unique_ptr<Expr> arg = parseExpr();
      if (arg == nullptr) {
        break;
      }
      if (check(TokenKind::KeywordFor)) {
        arg = parseComprehension(std::move(arg));
        if (arg == nullptr) {
          return {};
        }
      }
      parsed.positional.push_back(std::move(arg));
    }
    skipNewlines();
    if (!match(TokenKind::Comma)) {
      break;
    }
    skipNewlines();
  }
  return parsed;
}

std::unique_ptr<Expr> Parser::parsePostfix() {
  std::unique_ptr<Expr> expr = parsePrimary();
  if (expr == nullptr) {
    return nullptr;
  }
  while (true) {
    if (match(TokenKind::LBracket)) {
      if (looksLikeGenericCall()) {
        std::vector<std::unique_ptr<TypeExpr>> typeArgs = parseTypeArgList();
        if (!consume(TokenKind::RBracket, "expected ']'") ||
            !consume(TokenKind::LParen, "expected '(' after type arguments")) {
          return nullptr;
        }
        ParsedCallArguments callArgs = parseCallArguments();
        (void)consume(TokenKind::RParen, "expected ')'");
        expr = std::make_unique<CallExpr>(SourceRange{expr->range().start, previous().range().end},
                                          std::move(expr),
                                          std::move(typeArgs),
                                          std::move(callArgs.positional),
                                          std::move(callArgs.keyword));
        continue;
      }
      if (expr->kind() == NodeKind::NameExpr) {
        const std::string& ctor = static_cast<const NameExpr&>(*expr).name();
        if ((ctor == "list" || ctor == "array" || ctor == "dict" || ctor == "Unique" ||
             ctor == "Shared" || ctor == "Ptr") &&
            looksLikeTypeApplication()) {
          std::vector<std::unique_ptr<TypeExpr>> typeArgs = parseTypeArgList();
          if (!consume(TokenKind::RBracket, "expected ']' after type arguments")) {
            return nullptr;
          }
          expr =
              std::make_unique<CallExpr>(SourceRange{expr->range().start, previous().range().end},
                                         std::move(expr),
                                         std::move(typeArgs),
                                         std::vector<std::unique_ptr<Expr>>{},
                                         std::vector<NamedArgument>{});
          continue;
        }
      }
      std::unique_ptr<Expr> start;
      std::unique_ptr<Expr> stop;
      bool slice = false;
      const BoolScope enableCasts(allowAsCast_, true);
      if (match(TokenKind::Colon)) {
        slice = true;
        if (!check(TokenKind::RBracket)) {
          stop = parseExpr();
          if (stop == nullptr) {
            return nullptr;
          }
        }
      } else {
        start = parseExpr();
        if (start == nullptr) {
          return nullptr;
        }
        if (match(TokenKind::Colon)) {
          slice = true;
          if (!check(TokenKind::RBracket)) {
            stop = parseExpr();
            if (stop == nullptr) {
              return nullptr;
            }
          }
        }
      }
      if (!consume(TokenKind::RBracket, "expected ']' after index")) {
        return nullptr;
      }
      expr = std::make_unique<IndexExpr>(SourceRange{expr->range().start, previous().range().end},
                                         std::move(expr),
                                         std::move(start),
                                         std::move(stop),
                                         slice);
      continue;
    }
    if (match(TokenKind::LParen)) {
      ParsedCallArguments callArgs = parseCallArguments();
      (void)consume(TokenKind::RParen, "expected ')'");
      expr = std::make_unique<CallExpr>(SourceRange{expr->range().start, previous().range().end},
                                        std::move(expr),
                                        std::vector<std::unique_ptr<TypeExpr>>{},
                                        std::move(callArgs.positional),
                                        std::move(callArgs.keyword));
      continue;
    }
    if (match(TokenKind::Dot)) {
      std::string field;
      if (check(TokenKind::Identifier)) {
        field = parseIdentifier("expected field name");
      }
      expr = std::make_unique<MemberExpr>(SourceRange{expr->range().start, previous().range().end},
                                          std::move(expr),
                                          std::move(field));
      continue;
    }
    if (match(TokenKind::PlusPlus) || match(TokenKind::MinusMinus)) {
      const UnaryOp op =
          previous().kind() == TokenKind::PlusPlus ? UnaryOp::PostInc : UnaryOp::PostDec;
      expr = std::make_unique<UnaryExpr>(
          SourceRange{expr->range().start, previous().range().end}, op, std::move(expr));
      continue;
    }
    break;
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
  if (match(TokenKind::KeywordNot) || match(TokenKind::Minus) || match(TokenKind::Plus) ||
      match(TokenKind::Tilde) || match(TokenKind::PlusPlus) || match(TokenKind::MinusMinus) ||
      match(TokenKind::Star) || match(TokenKind::Amp)) {
    const UnaryOp op = prefixOp(previous().kind());
    const SourceLocation start = previous().location();
    std::unique_ptr<Expr> operand = parseUnary();
    if (operand == nullptr) {
      return nullptr;
    }
    return std::make_unique<UnaryExpr>(
        SourceRange{start, operand->range().end}, op, std::move(operand));
  }
  return parsePostfix();
}

std::unique_ptr<Expr> Parser::parseCast() {
  std::unique_ptr<Expr> expr = parseUnary();
  while (expr != nullptr && allowAsCast_ && match(TokenKind::KeywordAs)) {
    std::unique_ptr<TypeExpr> target = parseTypeExpr();
    if (target == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<CastExpr>(
        SourceRange{expr->range().start, target->range().end}, std::move(expr), std::move(target));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseRange() {
  std::unique_ptr<Expr> expr = parseCast();
  if (expr == nullptr || !match(TokenKind::DotDotDot)) {
    return expr;
  }
  std::unique_ptr<Expr> stop = parseCast();
  if (stop == nullptr) {
    return nullptr;
  }
  const SourceRange range{expr->range().start, stop->range().end};
  std::vector<std::unique_ptr<Expr>> args;
  args.push_back(std::move(expr));
  args.push_back(std::move(stop));
  auto callee = std::make_unique<NameExpr>(range, "range");
  return std::make_unique<CallExpr>(
      range, std::move(callee), std::vector<std::unique_ptr<TypeExpr>>{}, std::move(args));
}

std::unique_ptr<Expr> Parser::parseMul() {
  std::unique_ptr<Expr> expr = parseRange();
  while (expr != nullptr &&
         (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::SlashSlash) ||
          check(TokenKind::Percent) || check(TokenKind::StarStar))) {
    BinaryOp op = BinaryOp::Mul;
    if (peek().kind() == TokenKind::Slash) {
      op = BinaryOp::Div;
    } else if (peek().kind() == TokenKind::SlashSlash) {
      op = BinaryOp::FloorDiv;
    } else if (peek().kind() == TokenKind::Percent) {
      op = BinaryOp::Mod;
    } else if (peek().kind() == TokenKind::StarStar) {
      op = BinaryOp::Pow;
    }
    advance();
    std::unique_ptr<Expr> right = parseRange();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        op,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseAdd() {
  std::unique_ptr<Expr> expr = parseMul();
  while (expr != nullptr && (check(TokenKind::Plus) || check(TokenKind::Minus))) {
    const BinaryOp op = peek().kind() == TokenKind::Plus ? BinaryOp::Add : BinaryOp::Sub;
    advance();
    std::unique_ptr<Expr> right = parseMul();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        op,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseShift() {
  std::unique_ptr<Expr> expr = parseAdd();
  while (expr != nullptr && (check(TokenKind::LessLess) || check(TokenKind::GreaterGreater))) {
    const BinaryOp op = peek().kind() == TokenKind::LessLess ? BinaryOp::Shl : BinaryOp::Shr;
    advance();
    std::unique_ptr<Expr> right = parseAdd();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        op,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseBitAnd() {
  std::unique_ptr<Expr> expr = parseShift();
  while (expr != nullptr && match(TokenKind::Amp)) {
    std::unique_ptr<Expr> right = parseShift();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        BinaryOp::BitAnd,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseBitXor() {
  std::unique_ptr<Expr> expr = parseBitAnd();
  while (expr != nullptr && match(TokenKind::Caret)) {
    std::unique_ptr<Expr> right = parseBitAnd();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        BinaryOp::BitXor,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseBitOr() {
  std::unique_ptr<Expr> expr = parseBitXor();
  while (expr != nullptr && match(TokenKind::Pipe)) {
    std::unique_ptr<Expr> right = parseBitXor();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        BinaryOp::BitOr,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseComparison() {
  std::unique_ptr<Expr> expr = parseBitOr();
  if (expr == nullptr) {
    return nullptr;
  }
  BinaryOp op = BinaryOp::Add;
  bool found = false;
  if (match(TokenKind::EqualEqual)) {
    op = BinaryOp::Eq;
    found = true;
  } else if (match(TokenKind::BangEqual)) {
    op = BinaryOp::Ne;
    found = true;
  } else if (match(TokenKind::LessEqual)) {
    op = BinaryOp::Le;
    found = true;
  } else if (match(TokenKind::GreaterEqual)) {
    op = BinaryOp::Ge;
    found = true;
  } else if (match(TokenKind::Less)) {
    op = BinaryOp::Lt;
    found = true;
  } else if (match(TokenKind::Greater)) {
    op = BinaryOp::Gt;
    found = true;
  } else if (match(TokenKind::KeywordIs)) {
    if (match(TokenKind::KeywordNot)) {
      op = BinaryOp::IsNot;
    } else {
      op = BinaryOp::Is;
    }
    found = true;
  } else if (match(TokenKind::KeywordNot) && match(TokenKind::KeywordIn)) {
    op = BinaryOp::NotIn;
    found = true;
  } else if (match(TokenKind::KeywordIn)) {
    op = BinaryOp::In;
    found = true;
  }
  if (!found) {
    return expr;
  }
  std::unique_ptr<Expr> right = parseBitOr();
  if (right == nullptr) {
    return nullptr;
  }
  return std::make_unique<BinaryExpr>(
      SourceRange{expr->range().start, right->range().end}, op, std::move(expr), std::move(right));
}

std::unique_ptr<Expr> Parser::parseAnd() {
  std::unique_ptr<Expr> expr = parseComparison();
  while (expr != nullptr && match(TokenKind::KeywordAnd)) {
    std::unique_ptr<Expr> right = parseComparison();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        BinaryOp::And,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseOr() {
  std::unique_ptr<Expr> expr = parseAnd();
  while (expr != nullptr && match(TokenKind::KeywordOr)) {
    std::unique_ptr<Expr> right = parseAnd();
    if (right == nullptr) {
      return nullptr;
    }
    expr = std::make_unique<BinaryExpr>(SourceRange{expr->range().start, right->range().end},
                                        BinaryOp::Or,
                                        std::move(expr),
                                        std::move(right));
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseTernary() {
  std::unique_ptr<Expr> expr = parseOr();
  if (expr == nullptr || !match(TokenKind::KeywordIf)) {
    return expr;
  }
  std::unique_ptr<Expr> condition = parseOr();
  if (condition == nullptr || !consume(TokenKind::KeywordElse, "expected 'else' in ternary")) {
    return nullptr;
  }
  std::unique_ptr<Expr> otherwise = parseTernary();
  if (otherwise == nullptr) {
    return nullptr;
  }
  return std::make_unique<TernaryExpr>(SourceRange{expr->range().start, otherwise->range().end},
                                       std::move(expr),
                                       std::move(condition),
                                       std::move(otherwise));
}

std::unique_ptr<Expr> Parser::parseExpr() {
  return parseTernary();
}

std::unique_ptr<Expr> Parser::parseEmbeddedExpr(std::string_view text, SourceLocation base) {
  DiagnosticEngine nested;
  SourceManager source("<fstring>", std::string(text));
  Lexer lexer(source, nested, true);
  std::vector<Token> tokens = lexer.tokenizeAll();
  if (nested.hasErrors()) {
    diagnostics_->error(base, "invalid interpolation");
    return nullptr;
  }
  std::vector<Token> shifted;
  shifted.reserve(tokens.size());
  for (const Token& token : tokens) {
    SourceRange range{shiftLocation(token.range().start, base),
                      shiftLocation(token.range().end, base)};
    shifted.emplace_back(token.kind(), range, std::string(token.spelling()));
  }
  Parser inner(*diagnostics_, std::move(shifted));
  std::unique_ptr<Expr> expr = inner.parseExpr();
  if (expr == nullptr) {
    diagnostics_->error(base, "invalid interpolation expression");
    return nullptr;
  }
  if (!inner.check(TokenKind::EndOfFile) && !inner.check(TokenKind::Newline)) {
    diagnostics_->error(base, "unexpected tokens in interpolation");
    return nullptr;
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseFString() {
  const Token& token = previous();
  const std::string spelling(token.spelling());
  const std::string inner(stringLiteralInner(spelling));
  std::size_t prefixLen = 1;
  if (spelling.size() >= 2) {
    std::size_t start = spelling[0] == 'f' || spelling[0] == 'F' ? 1 : 0;
    const char quote = spelling[start];
    std::size_t open = 1;
    while (start + open < spelling.size() && spelling[start + open] == quote && open < 3) {
      ++open;
    }
    if (open == 2) {
      open = 1;
    }
    prefixLen = start + open;
  }
  SourceLocation innerStart = token.range().start;
  innerStart.offset += static_cast<std::uint32_t>(prefixLen);
  innerStart.column += static_cast<std::uint32_t>(prefixLen);
  std::vector<StringPart> parts;
  std::string literal;
  std::size_t index = 0;
  while (index < inner.size()) {
    if (inner[index] == '{' && index + 1 < inner.size() && inner[index + 1] == '{') {
      literal.push_back('{');
      index += 2;
      continue;
    }
    if (inner[index] == '}' && index + 1 < inner.size() && inner[index + 1] == '}') {
      literal.push_back('}');
      index += 2;
      continue;
    }
    if (inner[index] == '{') {
      if (!literal.empty()) {
        StringPart part;
        part.literal = unescapeStringBody(literal);
        parts.push_back(std::move(part));
        literal.clear();
      }
      ++index;
      const std::size_t exprStart = index;
      int depth = 1;
      while (index < inner.size() && depth > 0) {
        if (inner[index] == '{') {
          ++depth;
        } else if (inner[index] == '}') {
          --depth;
        }
        if (depth > 0) {
          ++index;
        }
      }
      if (depth != 0) {
        diagnostics_->error(token.range().start, "unterminated interpolation");
        return nullptr;
      }
      const std::string exprText = inner.substr(exprStart, index - exprStart);
      SourceLocation base = innerStart;
      base.offset += static_cast<std::uint32_t>(exprStart);
      base.column += static_cast<std::uint32_t>(exprStart);
      std::unique_ptr<Expr> expr = parseEmbeddedExpr(exprText, base);
      if (expr == nullptr) {
        return nullptr;
      }
      StringPart part;
      part.value = std::move(expr);
      parts.push_back(std::move(part));
      ++index;
      continue;
    }
    if (inner[index] == '}') {
      diagnostics_->error(token.range().start, "unmatched '}' in f-string");
      return nullptr;
    }
    literal.push_back(inner[index]);
    ++index;
  }
  if (!literal.empty() || parts.empty()) {
    StringPart part;
    part.literal = unescapeStringBody(literal);
    parts.push_back(std::move(part));
  }
  return std::make_unique<InterpolatedStringExpr>(token.range(), std::move(parts));
}

std::vector<ParamDecl> Parser::parseParams() {
  std::vector<ParamDecl> params;
  if (check(TokenKind::RParen)) {
    return params;
  }
  bool sawVarArg = false;
  bool sawKwArg = false;
  while (true) {
    ParamDecl param;
    param.range = peek().range();
    if (match(TokenKind::StarStar)) {
      if (sawKwArg) {
        diagnostics_->error(previous().range(), "**kwargs may appear only once");
        return {};
      }
      sawKwArg = true;
      param.kind = ParamKind::KwArg;
      param.name = parseIdentifier("expected parameter name after '**'");
      if (param.name.empty()) {
        return {};
      }
    } else if (match(TokenKind::Star)) {
      if (sawVarArg || sawKwArg) {
        diagnostics_->error(previous().range(), "*args may appear only once");
        return {};
      }
      if (check(TokenKind::Comma) || check(TokenKind::RParen)) {
        diagnostics_->error(previous().range(),
                            "bare '*' is not supported; name the vararg parameter");
        return {};
      }
      sawVarArg = true;
      param.kind = ParamKind::VarArg;
      param.name = parseIdentifier("expected parameter name after '*'");
      if (param.name.empty()) {
        return {};
      }
    } else {
      param.name = parseIdentifier("expected parameter name");
      if (param.name.empty()) {
        return {};
      }
    }
    const bool inferredSelf =
        param.name == "self" && (check(TokenKind::Comma) || check(TokenKind::RParen));
    if (!inferredSelf && match(TokenKind::Colon)) {
      param.type = parseTypeExpr();
      if (param.type == nullptr) {
        return {};
      }
    }
    if (match(TokenKind::Equal)) {
      param.defaultValue = parseExpr();
      if (param.defaultValue == nullptr) {
        return {};
      }
    }
    params.push_back(std::move(param));
    if (!match(TokenKind::Comma)) {
      break;
    }
    if (sawKwArg) {
      diagnostics_->error(peek().range(), "parameters may not follow **kwargs");
      return {};
    }
  }
  return params;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseSuite() {
  if (!consume(TokenKind::Colon, "expected ':' before suite") ||
      !consume(TokenKind::Newline, "expected newline after ':'") ||
      !consume(TokenKind::Indent, "expected indented block")) {
    return {};
  }
  std::vector<std::unique_ptr<Stmt>> body;
  while (!check(TokenKind::Dedent) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenKind::Dedent) || isAtEnd()) {
      break;
    }
    std::unique_ptr<Stmt> statement = parseStatement();
    if (statement == nullptr) {
      recoverStatement();
      continue;
    }
    body.push_back(std::move(statement));
  }
  (void)consume(TokenKind::Dedent, "expected dedent to close block");
  return body;
}

std::vector<std::unique_ptr<Expr>> Parser::parseDecoratorExprs() {
  std::vector<std::unique_ptr<Expr>> exprs;
  while (match(TokenKind::At)) {
    std::unique_ptr<Expr> expr;
    if (check(TokenKind::KeywordStatic)) {
      const Token& keyword = advance();
      expr = std::make_unique<NameExpr>(keyword.range(), "static");
    } else {
      expr = parsePostfix();
    }
    if (expr == nullptr) {
      diagnostics_->error(previous().range(), "expected decorator expression after '@'");
      break;
    }
    exprs.push_back(std::move(expr));
    skipNewlines();
  }
  return exprs;
}

void Parser::parseDecorators(bool& isPublic, bool& isPrivate) {
  isPublic = false;
  isPrivate = false;
  const std::vector<std::string> names = decoratorExprNames(parseDecoratorExprs());
  for (const std::string& name : names) {
    if (name == "public") {
      isPublic = true;
    } else if (name == "private") {
      isPrivate = true;
    }
  }
  if (isPublic && isPrivate) {
    diagnostics_->error(previous().range(), "field cannot be both @public and @private");
  }
}

std::vector<std::string> Parser::parseNameList() {
  std::vector<std::string> names;
  names.push_back(parseIdentifier("expected name"));
  while (matchDot()) {
    skipNewlines();
    names.push_back(parseIdentifier("expected name after '.'"));
  }
  return names;
}

std::unique_ptr<VarDecl> Parser::parseVarDecl(bool isStatic) {
  const Token& name = advance();
  if (!consume(TokenKind::Colon, "expected ':' in declaration")) {
    return nullptr;
  }
  std::unique_ptr<TypeExpr> type = parseTypeExpr();
  if (type == nullptr) {
    return nullptr;
  }
  std::unique_ptr<Expr> init;
  if (match(TokenKind::Equal)) {
    if (isLineEnd(peek().kind())) {
      diagnostics_->error(peek().range(), "expected expression after '='");
      return nullptr;
    }
    init = parseEqualsValue();
    if (init == nullptr) {
      return nullptr;
    }
  }
  if (!finishExprLine(init.get())) {
    return nullptr;
  }
  return std::make_unique<VarDecl>(
      name.range(), std::string(name.spelling()), std::move(type), std::move(init), isStatic);
}

std::unique_ptr<ReturnStmt> Parser::parseReturn() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> value;
  if (!isLineEnd(peek().kind())) {
    value = parseExpr();
    if (value == nullptr) {
      return nullptr;
    }
  }
  if (!finishLine()) {
    return nullptr;
  }
  return std::make_unique<ReturnStmt>(keyword.range(), std::move(value));
}

std::unique_ptr<IfStmt> Parser::parseIf() {
  const Token& keyword = advance();
  std::vector<IfBranch> branches;
  while (true) {
    IfBranch branch;
    branch.condition = parseExpr();
    if (branch.condition == nullptr) {
      return nullptr;
    }
    branch.body = parseSuite();
    branches.push_back(std::move(branch));
    skipNewlines();
    if (match(TokenKind::KeywordElif)) {
      continue;
    }
    break;
  }
  skipNewlines();
  if (match(TokenKind::KeywordElse)) {
    IfBranch elseBranch;
    elseBranch.body = parseSuite();
    branches.push_back(std::move(elseBranch));
  }
  return std::make_unique<IfStmt>(keyword.range(), std::move(branches));
}

std::unique_ptr<WhileStmt> Parser::parseWhile() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> condition = parseExpr();
  if (condition == nullptr) {
    return nullptr;
  }
  std::vector<std::unique_ptr<Stmt>> body = parseSuite();
  return std::make_unique<WhileStmt>(keyword.range(), std::move(condition), std::move(body));
}

std::unique_ptr<ForStmt> Parser::parseFor() {
  const Token& keyword = advance();
  std::string name = parseIdentifier("expected loop variable after 'for'");
  if (name.empty() || !consume(TokenKind::KeywordIn, "expected 'in' after loop variable")) {
    return nullptr;
  }
  std::unique_ptr<Expr> iterable = parseExpr();
  if (iterable == nullptr) {
    return nullptr;
  }
  std::vector<std::unique_ptr<Stmt>> body = parseSuite();
  return std::make_unique<ForStmt>(
      keyword.range(), std::move(name), std::move(iterable), std::move(body));
}

std::unique_ptr<AssertStmt> Parser::parseAssert() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> condition = parseExpr();
  if (condition == nullptr) {
    return nullptr;
  }
  std::unique_ptr<Expr> message;
  if (match(TokenKind::Comma)) {
    message = parseExpr();
    if (message == nullptr) {
      return nullptr;
    }
  }
  if (!finishLine()) {
    return nullptr;
  }
  return std::make_unique<AssertStmt>(keyword.range(), std::move(condition), std::move(message));
}

std::unique_ptr<RaiseStmt> Parser::parseRaise() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> value;
  if (!isLineEnd(peek().kind())) {
    value = parseExpr();
    if (value == nullptr) {
      return nullptr;
    }
  }
  if (!finishLine()) {
    return nullptr;
  }
  return std::make_unique<RaiseStmt>(keyword.range(), std::move(value));
}

std::unique_ptr<DelStmt> Parser::parseDel() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> target = parseExpr();
  if (target == nullptr || !finishLine()) {
    return nullptr;
  }
  return std::make_unique<DelStmt>(keyword.range(), std::move(target));
}

std::unique_ptr<DeferStmt> Parser::parseDefer() {
  const Token& keyword = advance();
  std::vector<std::unique_ptr<Stmt>> body;
  if (check(TokenKind::Colon)) {
    body = parseSuite();
  } else {
    std::unique_ptr<Stmt> statement = parseStatement();
    if (statement == nullptr) {
      return nullptr;
    }
    body.push_back(std::move(statement));
  }
  return std::make_unique<DeferStmt>(keyword.range(), std::move(body));
}

std::unique_ptr<Stmt> Parser::parseWith() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> context;
  {
    const BoolScope disableCasts(allowAsCast_, false);
    context = parseExpr();
  }
  if (context == nullptr) {
    return nullptr;
  }
  std::string name;
  if (match(TokenKind::KeywordAs)) {
    name = parseIdentifier("expected name after 'as'");
    if (name.empty()) {
      return nullptr;
    }
  }
  std::vector<std::unique_ptr<Stmt>> body = parseSuite();
  return std::make_unique<WithStmt>(
      keyword.range(), std::move(context), std::move(name), std::move(body));
}

std::unique_ptr<Stmt> Parser::parseConst() {
  const Token& keyword = advance();
  const std::string name = parseIdentifier("expected name after 'const'");
  if (name.empty()) {
    return nullptr;
  }
  std::unique_ptr<TypeExpr> type;
  if (match(TokenKind::Colon)) {
    type = parseTypeExpr();
    if (type == nullptr) {
      return nullptr;
    }
  }
  if (!consume(TokenKind::Equal, "expected '=' after const name")) {
    diagnostics_->help("write `const name = value` or `const name: i32 = value`");
    return nullptr;
  }
  std::unique_ptr<Expr> init = parseEqualsValue();
  if (init == nullptr || !finishExprLine(init.get())) {
    return nullptr;
  }
  return std::make_unique<VarDecl>(
      keyword.range(), name, std::move(type), std::move(init), false, true);
}

std::unique_ptr<TryStmt> Parser::parseTry() {
  const Token& keyword = advance();
  std::vector<std::unique_ptr<Stmt>> body = parseSuite();
  std::vector<ExceptHandler> handlers;
  while (match(TokenKind::KeywordExcept)) {
    ExceptHandler handler;
    handler.range = previous().range();
    if (!check(TokenKind::Colon)) {
      handler.type = parseTypeExpr();
      if (handler.type == nullptr) {
        return nullptr;
      }
      if (match(TokenKind::KeywordAs)) {
        handler.name = parseIdentifier("expected name after 'as'");
      }
    }
    handler.body = parseSuite();
    handlers.push_back(std::move(handler));
  }
  std::vector<std::unique_ptr<Stmt>> elseBody;
  if (match(TokenKind::KeywordElse)) {
    elseBody = parseSuite();
  }
  std::vector<std::unique_ptr<Stmt>> finallyBody;
  if (match(TokenKind::KeywordFinally)) {
    finallyBody = parseSuite();
  }
  if (handlers.empty() && finallyBody.empty()) {
    diagnostics_->error(keyword.range(), "try needs except or finally");
    return nullptr;
  }
  return std::make_unique<TryStmt>(keyword.range(),
                                   std::move(body),
                                   std::move(handlers),
                                   std::move(elseBody),
                                   std::move(finallyBody));
}

std::unique_ptr<MatchStmt> Parser::parseMatch() {
  const Token& keyword = advance();
  std::unique_ptr<Expr> subject = parseExpr();
  if (subject == nullptr || !consume(TokenKind::Colon, "expected ':' after match subject") ||
      !consume(TokenKind::Newline, "expected newline after match header") ||
      !consume(TokenKind::Indent, "expected indented match body")) {
    return nullptr;
  }
  std::vector<MatchArm> arms;
  while (!check(TokenKind::Dedent) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenKind::Dedent)) {
      break;
    }
    if (!consume(TokenKind::KeywordCase, "expected 'case' in match body")) {
      synchronize();
      continue;
    }
    MatchArm arm;
    arm.range = previous().range();
    arm.pattern = parseExpr();
    if (arm.pattern == nullptr) {
      synchronize();
      continue;
    }
    if (match(TokenKind::KeywordIf)) {
      arm.guard = parseExpr();
      if (arm.guard == nullptr) {
        synchronize();
        continue;
      }
    }
    arm.body = parseSuite();
    arm.range.end = previous().range().end;
    arms.push_back(std::move(arm));
  }
  if (!consume(TokenKind::Dedent, "expected dedent after match body") || arms.empty()) {
    if (arms.empty()) {
      diagnostics_->error(keyword.range(), "match needs at least one case");
    }
    return nullptr;
  }
  return std::make_unique<MatchStmt>(SourceRange{keyword.range().start, previous().range().end},
                                     std::move(subject),
                                     std::move(arms));
}

bool Parser::parseEnumVariant(EnumVariant& variant) {
  variant.range = peek().range();
  variant.name = parseIdentifier("expected enum variant");
  if (variant.name.empty()) {
    return false;
  }
  if (match(TokenKind::LParen)) {
    if (!check(TokenKind::RParen)) {
      while (true) {
        EnumPayloadField field;
        if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Colon) {
          field.name = parseIdentifier("expected payload field name");
          if (!consume(TokenKind::Colon, "expected ':' after payload field name")) {
            return false;
          }
        }
        field.type = parseTypeExpr();
        if (field.type == nullptr) {
          return false;
        }
        variant.payload.push_back(std::move(field));
        if (!match(TokenKind::Comma)) {
          break;
        }
      }
    }
    if (!consume(TokenKind::RParen, "expected ')' after enum payload")) {
      return false;
    }
  }
  if (match(TokenKind::Equal)) {
    variant.value = parseExpr();
    if (variant.value == nullptr) {
      return false;
    }
  }
  return true;
}

bool Parser::parseInlineEnumVariants(std::vector<EnumVariant>& variants) {
  while (!isLineEnd(peek().kind()) && !check(TokenKind::RBrace)) {
    EnumVariant variant;
    if (!parseEnumVariant(variant)) {
      return false;
    }
    variants.push_back(std::move(variant));
    if (match(TokenKind::Comma) || match(TokenKind::Slash)) {
      continue;
    }
    break;
  }
  return !variants.empty();
}

std::unique_ptr<EnumDef> Parser::parseEnum() {
  const Token& keyword = advance();
  std::string name = parseIdentifier("expected enum name");
  std::vector<std::string> typeParams = parseTypeParamList();
  if (name.empty()) {
    return nullptr;
  }
  std::vector<EnumVariant> variants;
  if (match(TokenKind::LBrace)) {
    if (!parseInlineEnumVariants(variants) ||
        !consume(TokenKind::RBrace, "expected '}' after enum variants") || !finishLine()) {
      return nullptr;
    }
    return std::make_unique<EnumDef>(
        keyword.range(), std::move(name), std::move(typeParams), std::move(variants));
  }
  if (!consume(TokenKind::Colon, "expected ':' or '{' after enum name")) {
    return nullptr;
  }
  if (!check(TokenKind::Newline) && !check(TokenKind::Indent)) {
    if (!parseInlineEnumVariants(variants) || !finishLine()) {
      return nullptr;
    }
    return std::make_unique<EnumDef>(
        keyword.range(), std::move(name), std::move(typeParams), std::move(variants));
  }
  if (!consume(TokenKind::Newline, "expected newline after enum header") ||
      !consume(TokenKind::Indent, "expected indented enum body")) {
    return nullptr;
  }
  while (!check(TokenKind::Dedent) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenKind::Dedent)) {
      break;
    }
    if (check(TokenKind::KeywordDef) || check(TokenKind::At)) {
      std::vector<std::unique_ptr<Expr>> decoratorExprs = parseDecoratorExprs();
      const std::vector<std::string> decorators = decoratorExprNames(decoratorExprs);
      std::unique_ptr<FunctionDef> method = parseFunction({});
      if (method == nullptr) {
        synchronize();
        continue;
      }
      method->setOwnerClass(name);
      attachFunctionDecorators(*method, std::move(decoratorExprs));
      markPrivateFromDecorators(*method, method->decorators());
      auto enumDef = std::make_unique<EnumDef>(
          keyword.range(), std::move(name), std::move(typeParams), std::move(variants));
      // Collect remaining methods after this one by finishing the loop via a local vector.
      std::vector<std::unique_ptr<FunctionDef>> methods;
      methods.push_back(std::move(method));
      while (!check(TokenKind::Dedent) && !isAtEnd()) {
        skipNewlines();
        if (check(TokenKind::Dedent)) {
          break;
        }
        std::vector<std::unique_ptr<Expr>> moreExprs = parseDecoratorExprs();
        const std::vector<std::string> more = decoratorExprNames(moreExprs);
        if (!check(TokenKind::KeywordDef)) {
          diagnostics_->error(peek().range(), "expected method after enum variants");
          synchronize();
          continue;
        }
        std::unique_ptr<FunctionDef> next = parseFunction({});
        if (next == nullptr) {
          synchronize();
          continue;
        }
        next->setOwnerClass(enumDef->name());
        attachFunctionDecorators(*next, std::move(moreExprs));
        markPrivateFromDecorators(*next, next->decorators());
        methods.push_back(std::move(next));
      }
      if (!consume(TokenKind::Dedent, "expected dedent after enum body")) {
        return nullptr;
      }
      for (std::unique_ptr<FunctionDef>& item : methods) {
        enumDef->methods().push_back(std::move(item));
      }
      return enumDef;
    }
    EnumVariant variant;
    if (!parseEnumVariant(variant)) {
      if (check(TokenKind::KeywordDef) || check(TokenKind::KeywordClass) ||
          check(TokenKind::KeywordType) || check(TokenKind::KeywordEnum) ||
          check(TokenKind::KeywordImport) || check(TokenKind::KeywordFrom)) {
        break;
      }
      const std::size_t before = current_;
      synchronize();
      if (current_ == before && !isAtEnd()) {
        advance();
      }
      continue;
    }
    if (match(TokenKind::Comma) || match(TokenKind::Slash)) {
      variants.push_back(std::move(variant));
      if (!parseInlineEnumVariants(variants) || !finishLine()) {
        synchronize();
        continue;
      }
      continue;
    }
    if (!finishLine()) {
      synchronize();
      continue;
    }
    variants.push_back(std::move(variant));
  }
  if (!consume(TokenKind::Dedent, "expected dedent after enum body")) {
    return nullptr;
  }
  return std::make_unique<EnumDef>(
      keyword.range(), std::move(name), std::move(typeParams), std::move(variants));
}

std::unique_ptr<Stmt> Parser::parseAssignOrExpr() {
  std::unique_ptr<Expr> expr = parseExpr();
  if (expr == nullptr) {
    return nullptr;
  }
  if (check(TokenKind::Comma)) {
    std::vector<std::unique_ptr<Expr>> elements;
    const SourceLocation start = expr->range().start;
    elements.push_back(std::move(expr));
    while (match(TokenKind::Comma)) {
      if (check(TokenKind::Equal) || check(TokenKind::PlusEqual) || isLineEnd(peek().kind())) {
        break;
      }
      std::unique_ptr<Expr> next = parseExpr();
      if (next == nullptr) {
        return nullptr;
      }
      elements.push_back(std::move(next));
    }
    expr = std::make_unique<TupleExpr>(SourceRange{start, elements.back()->range().end},
                                       std::move(elements));
  }
  AssignOp op = AssignOp::Assign;
  bool compound = false;
  if (match(TokenKind::Equal)) {
    compound = true;
  } else if (match(TokenKind::PlusEqual)) {
    op = AssignOp::Add;
    compound = true;
  } else if (match(TokenKind::MinusEqual)) {
    op = AssignOp::Sub;
    compound = true;
  } else if (match(TokenKind::StarEqual)) {
    op = AssignOp::Mul;
    compound = true;
  } else if (match(TokenKind::SlashEqual)) {
    op = AssignOp::Div;
    compound = true;
  } else if (match(TokenKind::PercentEqual)) {
    op = AssignOp::Mod;
    compound = true;
  } else if (match(TokenKind::AmpEqual)) {
    op = AssignOp::BitAnd;
    compound = true;
  } else if (match(TokenKind::PipeEqual)) {
    op = AssignOp::BitOr;
    compound = true;
  } else if (match(TokenKind::CaretEqual)) {
    op = AssignOp::BitXor;
    compound = true;
  } else if (match(TokenKind::LessLessEqual)) {
    op = AssignOp::Shl;
    compound = true;
  } else if (match(TokenKind::GreaterGreaterEqual)) {
    op = AssignOp::Shr;
    compound = true;
  } else if (match(TokenKind::SlashSlashEqual)) {
    op = AssignOp::FloorDiv;
    compound = true;
  } else if (match(TokenKind::StarStarEqual)) {
    op = AssignOp::Pow;
    compound = true;
  }
  if (compound) {
    std::unique_ptr<Expr> value = parseEqualsValue();
    if (value == nullptr || !finishExprLine(value.get())) {
      return nullptr;
    }
    return std::make_unique<AssignStmt>(expr->range(), std::move(expr), std::move(value), op);
  }
  if (!finishExprLine(expr.get())) {
    return nullptr;
  }
  return std::make_unique<ExprStmt>(expr->range(), std::move(expr));
}

std::unique_ptr<FunctionDef> Parser::parseFunction(std::string externName) {
  if (!consume(TokenKind::KeywordDef, "expected 'def'")) {
    return nullptr;
  }
  const SourceLocation start = previous().range().start;
  std::string name = parseIdentifier("expected function name");
  std::vector<std::string> typeParams = parseTypeParamList();
  if (name.empty() || !consume(TokenKind::LParen, "expected '(' after function name")) {
    return nullptr;
  }
  std::vector<ParamDecl> params = parseParams();
  if (!consume(TokenKind::RParen, "expected ')' after parameters")) {
    return nullptr;
  }
  std::unique_ptr<TypeExpr> returnType;
  bool inferredReturn = false;
  if (match(TokenKind::Arrow)) {
    returnType = parseTypeExpr();
    if (returnType == nullptr) {
      return nullptr;
    }
  } else {
    inferredReturn = true;
    const char* inferred = "Any";
    if (name == "main") {
      inferred = "i32";
    } else if (name == "__init__") {
      inferred = "void";
    }
    returnType = std::make_unique<TypeExpr>(
        previous().range(), inferred, std::vector<std::unique_ptr<TypeExpr>>{});
    if (!externName.empty()) {
      diagnostics_->error(previous().range(), "extern function '" + name + "' needs a return type");
      diagnostics_->help("write `-> void` or another type after the parameter list");
      return nullptr;
    }
  }
  std::vector<std::unique_ptr<Stmt>> body;
  const bool isExtern = !externName.empty();
  if (isExtern) {
    if (!finishLine()) {
      return nullptr;
    }
  } else {
    const std::size_t beforeSuite = current_;
    body = parseSuite();
    if (body.empty() && current_ == beforeSuite && diagnostics_->hasErrors()) {
      return nullptr;
    }
  }
  SourceRange range{start, returnType->range().end};
  if (!body.empty()) {
    range.end = body.back()->range().end;
  }
  auto function = std::make_unique<FunctionDef>(range,
                                                std::move(name),
                                                std::move(params),
                                                std::move(returnType),
                                                std::move(body),
                                                std::move(externName));
  function->setTypeParams(std::move(typeParams));
  function->setInferredReturn(inferredReturn);
  return function;
}

std::unique_ptr<FunctionDef> Parser::parsePropertyAccessor(std::string name,
                                                           SourceRange nameRange) {
  if (!consume(TokenKind::Dot, "expected '.' after property name")) {
    return nullptr;
  }
  const std::string kind = parseIdentifier("expected 'get' or 'set' after property name");
  if (kind != "get" && kind != "set") {
    diagnostics_->error(previous().range(), "expected 'get' or 'set' after '" + name + ".'");
    diagnostics_->help("write `" + name + ".get:` or `" + name + ".set(value: T):`");
    return nullptr;
  }
  const bool isGet = kind == "get";
  std::vector<ParamDecl> params;
  ParamDecl self;
  self.name = "self";
  self.range = nameRange;
  params.push_back(std::move(self));
  if (!isGet) {
    if (!consume(TokenKind::LParen, "expected '(' after '.set'")) {
      return nullptr;
    }
    std::vector<ParamDecl> extra = parseParams();
    if (!consume(TokenKind::RParen, "expected ')' after setter parameters")) {
      return nullptr;
    }
    std::size_t start = 0;
    if (!extra.empty() && extra[0].name == "self") {
      start = 1;
    }
    for (std::size_t index = start; index < extra.size(); ++index) {
      params.push_back(std::move(extra[index]));
    }
  } else if (match(TokenKind::LParen)) {
    std::vector<ParamDecl> extra = parseParams();
    if (!consume(TokenKind::RParen, "expected ')' after '.get'")) {
      return nullptr;
    }
    if (!extra.empty() && !(extra.size() == 1 && extra[0].name == "self")) {
      diagnostics_->error(previous().range(), "getter '" + name + ".get' takes only self");
      return nullptr;
    }
  }
  std::unique_ptr<TypeExpr> returnType;
  bool inferredReturn = false;
  if (match(TokenKind::Arrow)) {
    returnType = parseTypeExpr();
    if (returnType == nullptr) {
      return nullptr;
    }
  } else {
    inferredReturn = true;
    returnType = std::make_unique<TypeExpr>(
        previous().range(), isGet ? "Any" : "void", std::vector<std::unique_ptr<TypeExpr>>{});
  }
  const std::size_t beforeSuite = current_;
  std::vector<std::unique_ptr<Stmt>> body = parseSuite();
  if (body.empty() && current_ == beforeSuite && diagnostics_->hasErrors()) {
    return nullptr;
  }
  SourceRange range{nameRange.start, returnType->range().end};
  if (!body.empty()) {
    range.end = body.back()->range().end;
  }
  const std::string methodName = (isGet ? "__get_" : "__set_") + name;
  auto function = std::make_unique<FunctionDef>(
      range, methodName, std::move(params), std::move(returnType), std::move(body), "");
  function->setInferredReturn(inferredReturn);
  function->setProperty(isGet ? PropertyKind::Get : PropertyKind::Set, std::move(name));
  return function;
}

std::unique_ptr<ClassDef> Parser::parseClass() {
  const Token& keyword = advance();
  const bool isStruct = keyword.kind() == TokenKind::KeywordStruct;
  std::string name = parseIdentifier(isStruct ? "expected struct name" : "expected class name");
  std::vector<std::string> typeParams = parseTypeParamList();
  std::vector<std::string> bases;
  std::vector<std::unique_ptr<TypeExpr>> baseTypes;
  if (match(TokenKind::LParen)) {
    if (isStruct) {
      diagnostics_->error(peek().range(), "structs cannot inherit; use class");
      return nullptr;
    }
    skipNewlines();
    if (!check(TokenKind::RParen)) {
      while (true) {
        skipNewlines();
        std::unique_ptr<TypeExpr> baseType = parseTypeExpr();
        if (baseType == nullptr) {
          return nullptr;
        }
        if (!baseType->name().empty()) {
          bases.push_back(baseType->name());
        }
        baseTypes.push_back(std::move(baseType));
        skipNewlines();
        if (!match(TokenKind::Comma)) {
          break;
        }
      }
    }
    skipNewlines();
    if (!consume(TokenKind::RParen, "expected ')' after base classes")) {
      return nullptr;
    }
  }
  if (name.empty() ||
      !consume(TokenKind::Colon,
               isStruct ? "expected ':' after struct name" : "expected ':' after class name") ||
      !consume(TokenKind::Newline, "expected newline after type header") ||
      !consume(TokenKind::Indent, "expected indented type body")) {
    return nullptr;
  }
  std::vector<FieldDecl> fields;
  std::vector<std::unique_ptr<FunctionDef>> methods;
  while (!check(TokenKind::Dedent) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenKind::Dedent)) {
      break;
    }
    std::vector<std::unique_ptr<Expr>> decoratorExprs = parseDecoratorExprs();
    const std::vector<std::string> decorators = decoratorExprNames(decoratorExprs);
    bool markedPrivate = false;
    bool markedAbstract = false;
    bool markedOverride = false;
    bool markedStatic = false;
    for (const std::string& decorator : decorators) {
      markedPrivate = markedPrivate || decorator == "private";
      markedAbstract = markedAbstract || decorator == "abstract";
      markedOverride = markedOverride || decorator == "override";
      markedStatic = markedStatic || decorator == "static";
    }
    if (check(TokenKind::Dedent)) {
      break;
    }
    if (match(TokenKind::KeywordPass)) {
      if (!finishLine()) {
        synchronize();
      }
      continue;
    }
    if (check(TokenKind::KeywordClass) || check(TokenKind::KeywordStruct)) {
      diagnostics_->error(peek().range(), "nested types must be declared at module scope");
      synchronize();
      continue;
    }
    if (check(TokenKind::KeywordDef)) {
      std::unique_ptr<FunctionDef> method = parseFunction({});
      if (method == nullptr) {
        synchronize();
        continue;
      }
      method->setOwnerClass(name);
      attachFunctionDecorators(*method, std::move(decoratorExprs));
      method->setAbstract(markedAbstract);
      method->setOverride(markedOverride);
      markPrivateFromDecorators(*method, method->decorators());
      methods.push_back(std::move(method));
      continue;
    }
    if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Dot &&
        peekNth(2).kind() == TokenKind::Identifier &&
        (peekNth(2).spelling() == "get" || peekNth(2).spelling() == "set")) {
      const SourceRange nameRange = peek().range();
      std::string property = parseIdentifier("expected property name");
      std::unique_ptr<FunctionDef> accessor = parsePropertyAccessor(std::move(property), nameRange);
      if (accessor == nullptr) {
        synchronize();
        continue;
      }
      accessor->setOwnerClass(name);
      attachFunctionDecorators(*accessor, std::move(decoratorExprs));
      markPrivateFromDecorators(*accessor, accessor->decorators());
      methods.push_back(std::move(accessor));
      continue;
    }
    FieldDecl field;
    field.isStatic = markedStatic || match(TokenKind::KeywordStatic);
    field.range = peek().range();
    field.name = parseIdentifier("expected field or method");
    if (field.name.empty() || !consume(TokenKind::Colon, "expected ':' after field name")) {
      synchronize();
      continue;
    }
    field.type = parseTypeExpr();
    if (field.type == nullptr) {
      synchronize();
      continue;
    }
    if (match(TokenKind::Equal)) {
      field.init = parseExpr();
      if (field.init == nullptr) {
        synchronize();
        continue;
      }
    }
    if (!finishLine()) {
      synchronize();
      continue;
    }
    field.isPublic = !markedPrivate;
    fields.push_back(std::move(field));
  }
  if (!consume(TokenKind::Dedent, "expected dedent after type body")) {
    return nullptr;
  }
  auto def = std::make_unique<ClassDef>(keyword.range(),
                                        std::move(name),
                                        std::move(fields),
                                        std::move(methods),
                                        std::move(bases),
                                        std::move(typeParams));
  def->setBaseTypes(std::move(baseTypes));
  def->setStruct(isStruct);
  return def;
}

std::unique_ptr<ImportStmt> Parser::parseImport() {
  const Token& keyword = advance();
  std::vector<std::string> path = parseNameList();
  std::string alias = path.empty() ? std::string() : path.back();
  if (match(TokenKind::KeywordAs)) {
    alias = parseIdentifier("expected alias after 'as'");
  }
  if (path.empty() || !finishLine()) {
    return nullptr;
  }
  return std::make_unique<ImportStmt>(
      keyword.range(), std::move(path), std::move(alias), std::vector<std::string>{}, false);
}

std::unique_ptr<ImportStmt> Parser::parseFromImport() {
  const Token& keyword = advance();
  std::vector<std::string> path = parseNameList();
  if (path.empty() || !consume(TokenKind::KeywordImport, "expected 'import' after module path")) {
    return nullptr;
  }
  bool star = false;
  std::vector<std::string> names;
  std::vector<std::string> nameAliases;
  if (match(TokenKind::Star)) {
    star = true;
  } else {
    while (true) {
      names.push_back(parseIdentifier("expected imported name"));
      if (match(TokenKind::KeywordAs)) {
        nameAliases.push_back(parseIdentifier("expected alias after 'as'"));
      } else {
        nameAliases.push_back(names.back());
      }
      if (!match(TokenKind::Comma)) {
        break;
      }
    }
  }
  if (!finishLine()) {
    return nullptr;
  }
  return std::make_unique<ImportStmt>(
      keyword.range(), std::move(path), "", std::move(names), star, std::move(nameAliases));
}

std::unique_ptr<TypeAlias> Parser::parseTypeAlias() {
  const Token& keyword = advance();
  std::string name = parseIdentifier("expected type name");
  if (name.empty() || !consume(TokenKind::Equal, "expected '=' after type name")) {
    return nullptr;
  }
  std::unique_ptr<TypeExpr> type = parseTypeExpr();
  if (type == nullptr || !finishLine()) {
    return nullptr;
  }
  return std::make_unique<TypeAlias>(keyword.range(), std::move(name), std::move(type));
}

std::unique_ptr<Stmt> Parser::parseStatement() {
  std::vector<std::unique_ptr<Expr>> decoratorExprs = parseDecoratorExprs();
  const std::vector<std::string> decorators = decoratorExprNames(decoratorExprs);
  if (check(TokenKind::KeywordExtern)) {
    advance();
    if (!consume(TokenKind::String, "expected ABI name string after extern")) {
      return nullptr;
    }
    std::string abiName = parseStringValue();
    if ((abiName == "C" || abiName == "c") && check(TokenKind::String)) {
      advance();
      abiName = parseStringValue();
    }
    skipNewlines();
    return parseFunction(std::move(abiName));
  }
  if (check(TokenKind::KeywordImport)) {
    return parseImport();
  }
  if (check(TokenKind::KeywordFrom)) {
    return parseFromImport();
  }
  if (check(TokenKind::KeywordMacro)) {
    std::unique_ptr<MacroDef> macro = parseMacroDef();
    if (macro != nullptr) {
      markPrivateFromDecorators(*macro, decorators);
    }
    return macro;
  }
  if (check(TokenKind::KeywordDef)) {
    std::unique_ptr<FunctionDef> function = parseFunction({});
    if (function != nullptr) {
      attachFunctionDecorators(*function, std::move(decoratorExprs));
      markPrivateFromDecorators(*function, function->decorators());
      for (const std::string& decorator : function->decorators()) {
        if (decorator == "abstract") {
          function->setAbstract(true);
        }
        if (decorator == "override") {
          function->setOverride(true);
        }
      }
    }
    return function;
  }
  if (check(TokenKind::KeywordClass) || check(TokenKind::KeywordStruct)) {
    std::unique_ptr<ClassDef> classDef = parseClass();
    if (classDef != nullptr) {
      classDef->setDecorators(decorators);
      classDef->setDecoratorExprs(std::move(decoratorExprs));
      markPrivateFromDecorators(*classDef, decorators);
      for (const std::string& decorator : classDef->decorators()) {
        if (decorator == "frozen") {
          classDef->setFrozen(true);
        }
      }
    }
    return classDef;
  }
  if (check(TokenKind::KeywordType)) {
    std::unique_ptr<TypeAlias> alias = parseTypeAlias();
    if (alias != nullptr) {
      markPrivateFromDecorators(*alias, decorators);
    }
    return alias;
  }
  if (check(TokenKind::KeywordEnum)) {
    std::unique_ptr<EnumDef> enumDef = parseEnum();
    if (enumDef != nullptr) {
      enumDef->setDecorators(decorators);
      enumDef->setDecoratorExprs(std::move(decoratorExprs));
      markPrivateFromDecorators(*enumDef, decorators);
      for (const std::string& decorator : enumDef->decorators()) {
        if (decorator == "flags") {
          enumDef->setFlags(true);
        }
      }
    }
    return enumDef;
  }
  if (check(TokenKind::KeywordReturn)) {
    return parseReturn();
  }
  if (check(TokenKind::KeywordIf)) {
    return parseIf();
  }
  if (check(TokenKind::KeywordWhile)) {
    return parseWhile();
  }
  if (check(TokenKind::KeywordFor)) {
    return parseFor();
  }
  if (check(TokenKind::KeywordAssert)) {
    return parseAssert();
  }
  if (check(TokenKind::KeywordRaise)) {
    return parseRaise();
  }
  if (check(TokenKind::KeywordTry)) {
    return parseTry();
  }
  if (check(TokenKind::KeywordMatch)) {
    return parseMatch();
  }
  if (check(TokenKind::KeywordDel)) {
    return parseDel();
  }
  if (check(TokenKind::KeywordDefer)) {
    return parseDefer();
  }
  if (check(TokenKind::KeywordWith)) {
    return parseWith();
  }
  if (check(TokenKind::KeywordConst)) {
    return parseConst();
  }
  if (match(TokenKind::KeywordPass)) {
    const Token& token = previous();
    if (!finishLine()) {
      return nullptr;
    }
    return std::make_unique<PassStmt>(token.range());
  }
  if (match(TokenKind::KeywordBreak)) {
    const Token& token = previous();
    if (!finishLine()) {
      return nullptr;
    }
    return std::make_unique<BreakStmt>(token.range());
  }
  if (match(TokenKind::KeywordContinue)) {
    const Token& token = previous();
    if (!finishLine()) {
      return nullptr;
    }
    return std::make_unique<ContinueStmt>(token.range());
  }
  if (match(TokenKind::KeywordStatic)) {
    if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Colon) {
      std::unique_ptr<VarDecl> decl = parseVarDecl(true);
      if (decl != nullptr) {
        markPrivateFromDecorators(*decl, decorators);
      }
      return decl;
    }
    diagnostics_->error(peek().range(), "expected 'name: type' after 'static'");
    return nullptr;
  }
  if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Colon) {
    if (peekNth(2).kind() == TokenKind::Newline) {
      std::string name = std::string(peek().spelling());
      SourceRange nameRange = peek().range();
      advance();
      return parseMacroInvokeStmt(std::move(name), nameRange);
    }
    std::unique_ptr<VarDecl> decl = parseVarDecl();
    if (decl != nullptr) {
      markPrivateFromDecorators(*decl, decorators);
    }
    return decl;
  }
  return parseAssignOrExpr();
}

std::unique_ptr<Module> Parser::parseModule() {
  std::vector<std::unique_ptr<Stmt>> statements;
  skipNewlines();
  while (!isAtEnd()) {
    std::unique_ptr<Stmt> statement = parseStatement();
    if (statement == nullptr) {
      recoverStatement();
      skipNewlines();
      continue;
    }
    statements.push_back(std::move(statement));
    skipNewlines();
  }
  SourceRange range;
  if (!statements.empty()) {
    range.start = statements.front()->range().start;
    range.end = statements.back()->range().end;
  }
  return std::make_unique<Module>(range, std::move(statements));
}

std::unique_ptr<Expr> Parser::parseTopExpr() {
  skipNewlines();
  std::unique_ptr<Expr> expr = parseExpr();
  skipNewlines();
  return expr;
}

std::vector<std::unique_ptr<Expr>> Parser::parseTopExprList() {
  skipNewlines();
  std::vector<std::unique_ptr<Expr>> args;
  if (isAtEnd() || check(TokenKind::EndOfFile)) {
    return args;
  }
  while (true) {
    std::unique_ptr<Expr> expr = parseExpr();
    if (expr == nullptr) {
      return {};
    }
    args.push_back(std::move(expr));
    if (!match(TokenKind::Comma)) {
      break;
    }
    skipNewlines();
  }
  skipNewlines();
  return args;
}

std::string Parser::rawSlice(SourceRange range) const {
  if (source_ != nullptr) {
    return std::string(source_->slice(range));
  }
  return {};
}

std::string Parser::joinTokenSpellings(const std::vector<Token>& tokens) const {
  std::string raw;
  for (const Token& token : tokens) {
    if (!raw.empty()) {
      raw += " ";
    }
    raw += std::string(token.spelling());
  }
  return raw;
}

std::unique_ptr<MacroInvokeExpr> Parser::makeMacroInvokeExpr(SourceRange nameRange,
                                                             std::string name,
                                                             MacroDelimiter delimiter,
                                                             std::string raw,
                                                             SourceRange rawRange,
                                                             std::vector<Token> tokens) {
  if (raw.empty()) {
    raw = joinTokenSpellings(tokens);
  }
  return std::make_unique<MacroInvokeExpr>(SourceRange{nameRange.start, previous().range().end},
                                           std::move(name),
                                           delimiter,
                                           std::move(raw),
                                           rawRange,
                                           std::move(tokens));
}

bool Parser::parseIndentMacroPayload(std::vector<Token>& tokens, SourceRange& rawRange) {
  if (!consume(TokenKind::Colon, "expected ':' after macro name") ||
      !consume(TokenKind::Newline, "expected newline after macro ':'")) {
    return false;
  }
  return captureIndented(tokens, rawRange);
}

bool Parser::captureBalanced(TokenKind closer, std::vector<Token>& tokens, SourceRange& rawRange) {
  const TokenKind opener = peek().kind();
  if (!match(opener)) {
    diagnostics_->error(peek().range(), "expected macro delimiter");
    return false;
  }
  const SourceLocation start = peek().range().start;
  rawRange.start = start;
  int depth = 1;
  while (!isAtEnd() && depth > 0) {
    if (check(opener)) {
      ++depth;
    } else if (check(closer)) {
      --depth;
      if (depth == 0) {
        break;
      }
    }
    tokens.push_back(advance());
  }
  if (!tokens.empty()) {
    rawRange.end = tokens.back().range().end;
  } else {
    rawRange.end = peek().range().start;
  }
  if (!consume(closer, "unmatched delimiter in macro invocation")) {
    return false;
  }
  return true;
}

bool Parser::captureIndented(std::vector<Token>& tokens, SourceRange& rawRange) {
  if (!consume(TokenKind::Indent, "expected indented macro body")) {
    return false;
  }
  rawRange.start = peek().range().start;
  int depth = 1;
  while (!isAtEnd() && depth > 0) {
    if (check(TokenKind::Indent)) {
      ++depth;
    } else if (check(TokenKind::Dedent)) {
      --depth;
      if (depth == 0) {
        break;
      }
    }
    tokens.push_back(advance());
  }
  if (!tokens.empty()) {
    rawRange.end = tokens.back().range().end;
  } else {
    rawRange.end = peek().range().start;
  }
  return consume(TokenKind::Dedent, "expected dedent after macro body");
}

std::unique_ptr<Expr> Parser::parseSplice() {
  if (match(TokenKind::LParen)) {
    std::unique_ptr<Expr> inner;
    if (match(TokenKind::Dollar)) {
      inner = parseSplice();
    } else {
      inner = parseExpr();
    }
    if (inner == nullptr || !consume(TokenKind::RParen, "expected ')' after splice")) {
      return nullptr;
    }
    const bool comma = match(TokenKind::Comma);
    if (match(TokenKind::Star) || match(TokenKind::Plus)) {
      if (inner->kind() == NodeKind::SpliceExpr) {
        static_cast<SpliceExpr&>(*inner).setRepeat(true, comma);
      }
    }
    return inner;
  }
  if (!check(TokenKind::Identifier) && peek().kind() != TokenKind::KeywordType) {
    diagnostics_->error(peek().range(), "expected splice name after '$'");
    return nullptr;
  }
  const Token& name = advance();
  return std::make_unique<SpliceExpr>(name.range(), std::string(name.spelling()));
}

std::unique_ptr<Expr> Parser::parseMacroInvokeExpr(std::string name, SourceRange nameRange) {
  if (!consume(TokenKind::Bang, "expected '!' after macro name")) {
    return nullptr;
  }
  MacroDelimiter delimiter = MacroDelimiter::BangParen;
  TokenKind closer = TokenKind::RParen;
  if (check(TokenKind::LBrace)) {
    delimiter = MacroDelimiter::BangBrace;
    closer = TokenKind::RBrace;
  } else if (check(TokenKind::LBracket)) {
    delimiter = MacroDelimiter::BangBracket;
    closer = TokenKind::RBracket;
  } else if (!check(TokenKind::LParen)) {
    diagnostics_->error(peek().range(), "expected '(', '[' or '{' after macro '!'");
    return nullptr;
  }
  std::vector<Token> tokens;
  SourceRange rawRange;
  if (!captureBalanced(closer, tokens, rawRange)) {
    return nullptr;
  }
  std::string raw = rawSlice(rawRange);
  return makeMacroInvokeExpr(
      nameRange, std::move(name), delimiter, std::move(raw), rawRange, std::move(tokens));
}

std::unique_ptr<Expr> Parser::parseMacroInvokeIndentExpr(std::string name, SourceRange nameRange) {
  std::vector<Token> tokens;
  SourceRange rawRange;
  if (!parseIndentMacroPayload(tokens, rawRange)) {
    return nullptr;
  }
  std::string raw = rawSlice(rawRange);
  return makeMacroInvokeExpr(nameRange,
                             std::move(name),
                             MacroDelimiter::Indent,
                             std::move(raw),
                             rawRange,
                             std::move(tokens));
}

std::unique_ptr<Stmt> Parser::parseMacroInvokeStmt(std::string name, SourceRange nameRange) {
  std::unique_ptr<Expr> expr = parseMacroInvokeIndentExpr(std::move(name), nameRange);
  if (expr == nullptr || expr->kind() != NodeKind::MacroInvokeExpr) {
    return nullptr;
  }
  const auto& invoke = static_cast<const MacroInvokeExpr&>(*expr);
  return std::make_unique<MacroInvokeStmt>(invoke.range(),
                                           invoke.name(),
                                           invoke.delimiter(),
                                           invoke.rawText(),
                                           invoke.rawRange(),
                                           invoke.tokens());
}

std::unique_ptr<MacroDef> Parser::parseMacroDef() {
  if (!consume(TokenKind::KeywordMacro, "expected 'macro'")) {
    return nullptr;
  }
  const SourceLocation start = previous().range().start;
  std::string name = parseIdentifier("expected macro name");
  if (name.empty()) {
    return nullptr;
  }
  const SourceRange nameRange = previous().range();
  std::vector<std::string> params;
  bool variadic = false;
  if (match(TokenKind::LParen)) {
    if (!check(TokenKind::RParen)) {
      while (true) {
        if (match(TokenKind::Star)) {
          variadic = true;
        }
        std::string param = parseIdentifier("expected macro parameter name");
        if (param.empty()) {
          return nullptr;
        }
        params.push_back(std::move(param));
        if (!match(TokenKind::Comma)) {
          break;
        }
      }
    }
    if (!consume(TokenKind::RParen, "expected ')' after macro parameters")) {
      return nullptr;
    }
  }
  if (!consume(TokenKind::Colon, "expected ':' after macro header") ||
      !consume(TokenKind::Newline, "expected newline after macro header") ||
      !consume(TokenKind::Indent, "expected indented macro body")) {
    return nullptr;
  }
  auto def = std::make_unique<MacroDef>(
      SourceRange{start, previous().range().end}, std::move(name), std::move(params), nameRange);
  def->setVariadic(variadic);
  while (!check(TokenKind::Dedent) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenKind::Dedent)) {
      break;
    }
    if (check(TokenKind::Identifier) && peek().spelling() == "quote") {
      advance();
      inQuote_ = true;
      def->quoteBody() = parseSuite();
      inQuote_ = false;
      continue;
    }
    if (check(TokenKind::KeywordMatch)) {
      advance();
      if (!consume(TokenKind::Colon, "expected ':' after match") ||
          !consume(TokenKind::Newline, "expected newline after match:") ||
          !consume(TokenKind::Indent, "expected indented match arms")) {
        return nullptr;
      }
      while (!check(TokenKind::Dedent) && !isAtEnd()) {
        skipNewlines();
        if (check(TokenKind::Dedent)) {
          break;
        }
        MacroMatchArm arm;
        arm.range = peek().range();
        while (!isAtEnd() && !check(TokenKind::FatArrow) && !check(TokenKind::Dedent)) {
          arm.pattern.push_back(advance());
        }
        if (!consume(TokenKind::FatArrow, "expected '=>' in match arm")) {
          return nullptr;
        }
        skipNewlines();
        if (check(TokenKind::Identifier) && peek().spelling() == "quote") {
          advance();
          inQuote_ = true;
          arm.body = parseSuite();
          inQuote_ = false;
        } else {
          arm.body = parseSuite();
        }
        def->matchArms().push_back(std::move(arm));
      }
      if (!consume(TokenKind::Dedent, "expected dedent after match arms")) {
        return nullptr;
      }
      continue;
    }
    if (check(TokenKind::Identifier) && peekNth(1).kind() == TokenKind::Colon) {
      const std::string key = parseIdentifier("expected macro property");
      if (!consume(TokenKind::Colon, "expected ':' after macro property")) {
        return nullptr;
      }
      std::string value;
      if (match(TokenKind::KeywordTrue)) {
        value = "true";
      } else if (match(TokenKind::KeywordFalse)) {
        value = "false";
      } else if (check(TokenKind::Identifier)) {
        value = parseIdentifier("expected property value");
      } else {
        diagnostics_->error(peek().range(), "expected macro property value");
        return nullptr;
      }
      if (!finishLine()) {
        return nullptr;
      }
      if (key == "syntax") {
        if (value == "raw") {
          def->setSyntaxMode(MacroSyntaxMode::Raw);
        } else if (value == "tokens") {
          def->setSyntaxMode(MacroSyntaxMode::Tokens);
        } else if (value == "pipeline") {
          def->setSyntaxMode(MacroSyntaxMode::Pipeline);
        } else {
          def->setSyntaxMode(MacroSyntaxMode::Sere);
        }
      } else if (key == "interpolate") {
        if (value == "brace") {
          def->setInterpolate(MacroInterpolate::Brace);
        } else if (value == "dollar") {
          def->setInterpolate(MacroInterpolate::Dollar);
        }
      } else if (key == "typed") {
        def->setTyped(value == "true");
      } else if (key == "wrapper") {
        def->setWrapper(std::move(value));
      }
      continue;
    }
    diagnostics_->error(peek().range(), "expected quote, match, or macro property");
    recoverStatement();
  }
  if (!consume(TokenKind::Dedent, "expected dedent after macro body")) {
    return nullptr;
  }
  if (!def->quoteBody().empty() || !def->matchArms().empty()) {
    def->setRange(SourceRange{start, previous().range().end});
  }
  return def;
}

} // namespace sere
