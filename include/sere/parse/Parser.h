/// @file Parser.h
/// Recursive-descent parser for typed Sere (Python-superset).

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/lex/Token.h"
#include "sere/lex/TokenKind.h"

#include <memory>
#include <string>
#include <vector>

namespace sere {

class DiagnosticEngine;
class SourceManager;

class Parser {
public:
  Parser(DiagnosticEngine& diagnostics, std::vector<Token> tokens,
         const SourceManager* source = nullptr);

  [[nodiscard]] std::unique_ptr<Module> parseModule();
  [[nodiscard]] std::unique_ptr<Expr> parseTopExpr();
  [[nodiscard]] std::vector<std::unique_ptr<Expr>> parseTopExprList();

private:
  [[nodiscard]] bool isAtEnd() const;
  [[nodiscard]] const Token& peek() const;
  [[nodiscard]] const Token& peekNth(std::size_t ahead) const;
  [[nodiscard]] const Token& previous() const;
  const Token& advance();
  [[nodiscard]] bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  bool consume(TokenKind kind, const char* errorMessage);
  void skipNewlines();
  bool finishLine();
  bool finishExprLine(const Expr* expr);
  void synchronize();
  [[nodiscard]] bool looksLikeGenericCall() const;

  std::unique_ptr<TypeExpr> parseTypeAtom();
  std::unique_ptr<TypeExpr> parseTypeExpr();
  std::vector<std::unique_ptr<TypeExpr>> parseTypeArgList();
  std::vector<std::string> parseTypeParamList();
  std::unique_ptr<Expr> parseExpr();
  std::unique_ptr<Expr> parseTupleTail(std::unique_ptr<Expr> first, SourceLocation start);
  std::unique_ptr<Expr> parseLambda();
  std::unique_ptr<Stmt> parseConst();
  std::unique_ptr<Expr> parseEqualsValue();
  std::unique_ptr<Expr> parseTernary();
  std::unique_ptr<Expr> parseOr();
  std::unique_ptr<Expr> parseAnd();
  std::unique_ptr<Expr> parseComparison();
  std::unique_ptr<Expr> parseBitOr();
  std::unique_ptr<Expr> parseBitXor();
  std::unique_ptr<Expr> parseBitAnd();
  std::unique_ptr<Expr> parseShift();
  std::unique_ptr<Expr> parseAdd();
  std::unique_ptr<Expr> parseMul();
  std::unique_ptr<Expr> parseUnary();
  std::unique_ptr<Expr> parseCast();
  std::unique_ptr<Expr> parseRange();
  std::unique_ptr<Expr> parsePostfix();
  std::unique_ptr<Expr> parsePrimary();
  std::unique_ptr<Expr> parseListLiteral();
  std::unique_ptr<Expr> parseDictLiteral();
  std::unique_ptr<Expr> parseFString();
  std::unique_ptr<Expr> parseEmbeddedExpr(std::string_view text, SourceLocation base);
  std::vector<std::unique_ptr<Expr>> parseCallArguments();
  std::unique_ptr<Expr> parseComprehension(std::unique_ptr<Expr> element);
  std::unique_ptr<Stmt> parseStatement();
  std::unique_ptr<FunctionDef> parseFunction(std::string externName);
  std::unique_ptr<ClassDef> parseClass();
  std::unique_ptr<TypeAlias> parseTypeAlias();
  std::unique_ptr<ImportStmt> parseImport();
  std::unique_ptr<ImportStmt> parseFromImport();
  std::unique_ptr<VarDecl> parseVarDecl(bool isStatic = false);
  void parseDecorators(bool& isPublic, bool& isPrivate);
  std::vector<std::string> parseDecoratorNames();
  std::vector<std::string> parseNameList();
  std::unique_ptr<ReturnStmt> parseReturn();
  std::unique_ptr<IfStmt> parseIf();
  std::unique_ptr<WhileStmt> parseWhile();
  std::unique_ptr<ForStmt> parseFor();
  std::unique_ptr<AssertStmt> parseAssert();
  std::unique_ptr<RaiseStmt> parseRaise();
  std::unique_ptr<TryStmt> parseTry();
  std::unique_ptr<MatchStmt> parseMatch();
  std::unique_ptr<DelStmt> parseDel();
  std::unique_ptr<DeferStmt> parseDefer();
  std::unique_ptr<Stmt> parseWith();
  std::unique_ptr<EnumDef> parseEnum();
  [[nodiscard]] bool parseEnumVariant(EnumVariant& variant);
  [[nodiscard]] bool parseInlineEnumVariants(std::vector<EnumVariant>& variants);
  std::unique_ptr<Stmt> parseAssignOrExpr();
  std::unique_ptr<MacroDef> parseMacroDef();
  std::unique_ptr<Stmt> parseMacroInvokeStmt(std::string name, SourceRange nameRange);
  std::unique_ptr<Expr> parseMacroInvokeExpr(std::string name, SourceRange nameRange);
  std::unique_ptr<Expr> parseMacroInvokeIndentExpr(std::string name, SourceRange nameRange);
  std::unique_ptr<Expr> parseSplice();
  bool captureBalanced(TokenKind closer, std::vector<Token>& tokens, SourceRange& rawRange);
  bool captureIndented(std::vector<Token>& tokens, SourceRange& rawRange);
  bool parseIndentMacroPayload(std::vector<Token>& tokens, SourceRange& rawRange);
  [[nodiscard]] std::string joinTokenSpellings(const std::vector<Token>& tokens) const;
  [[nodiscard]] std::unique_ptr<MacroInvokeExpr> makeMacroInvokeExpr(
      SourceRange nameRange, std::string name, MacroDelimiter delimiter, std::string raw,
      SourceRange rawRange, std::vector<Token> tokens);
  void recoverStatement();
  [[nodiscard]] std::string rawSlice(SourceRange range) const;
  std::vector<std::unique_ptr<Stmt>> parseSuite();
  std::vector<ParamDecl> parseParams();
  std::string parseIdentifier(const char* errorMessage);
  std::string parseStringValue();

  DiagnosticEngine* diagnostics_;
  const SourceManager* source_ = nullptr;
  std::vector<Token> tokens_;
  std::size_t current_ = 0;
  bool inQuote_ = false;
  bool allowAsCast_ = true;
};

}  // namespace sere
