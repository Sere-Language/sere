/// @file Syntax.h
/// Typed Python-superset AST. Nodes use kind tags; sema stores resolved types.

#pragma once

#include "sere/lex/Token.h"
#include "sere/source/SourceLocation.h"
#include "sere/types/Intrinsic.h"
#include "sere/types/Type.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sere {

enum class NodeKind {
  TypeExpr,
  IntegerLiteral,
  FloatLiteral,
  StringLiteral,
  InterpolatedStringExpr,
  BooleanLiteral,
  NoneLiteral,
  NameExpr,
  CallExpr,
  MemberExpr,
  BinaryExpr,
  UnaryExpr,
  CastExpr,
  IndexExpr,
  ListLiteral,
  DictLiteral,
  ComprehensionExpr,
  TernaryExpr,
  TupleExpr,
  WalrusExpr,
  LambdaExpr,
  VarDecl,
  AssignStmt,
  ReturnStmt,
  ExprStmt,
  PassStmt,
  BreakStmt,
  ContinueStmt,
  IfStmt,
  WhileStmt,
  ForStmt,
  AssertStmt,
  RaiseStmt,
  TryStmt,
  MatchStmt,
  DelStmt,
  DeferStmt,
  WithStmt,
  FunctionDef,
  ClassDef,
  EnumDef,
  TypeAlias,
  ImportStmt,
  SpliceExpr,
  MacroDef,
  MacroInvokeExpr,
  MacroInvokeStmt,
  Module,
};

class Node {
public:
  Node(NodeKind kind, SourceRange range);
  virtual ~Node() = default;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node(Node&&) = default;
  Node& operator=(Node&&) = default;

  [[nodiscard]] NodeKind kind() const;
  [[nodiscard]] SourceRange range() const;
  void setRange(SourceRange range);
  [[nodiscard]] const Type* resolvedType() const;
  [[nodiscard]] bool fromPrelude() const;
  void setResolvedType(const Type* type);
  void setFromPrelude(bool value);

private:
  NodeKind kind_;
  SourceRange range_;
  const Type* resolvedType_ = nullptr;
  bool fromPrelude_ = false;
};

class TypeExpr final : public Node {
public:
  TypeExpr(SourceRange range, std::string name, std::vector<std::unique_ptr<TypeExpr>> args);

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<std::unique_ptr<TypeExpr>>& args() const;

private:
  std::string name_;
  std::vector<std::unique_ptr<TypeExpr>> args_;
};

class Expr : public Node {
public:
  using Node::Node;
};

class Stmt : public Node {
public:
  using Node::Node;
};

class IntegerLiteral final : public Expr {
public:
  IntegerLiteral(SourceRange range, std::int64_t value);
  [[nodiscard]] std::int64_t value() const;

private:
  std::int64_t value_;
};

class FloatLiteral final : public Expr {
public:
  FloatLiteral(SourceRange range, double value, bool isF32);
  [[nodiscard]] double value() const;
  [[nodiscard]] bool isF32() const;

private:
  double value_;
  bool isF32_;
};

class StringLiteral final : public Expr {
public:
  StringLiteral(SourceRange range, std::string value, bool regex = false);
  [[nodiscard]] const std::string& value() const;
  [[nodiscard]] bool isRegex() const;

private:
  std::string value_;
  bool regex_ = false;
};

struct StringPart {
  std::string literal;
  std::unique_ptr<Expr> value;
};

class InterpolatedStringExpr final : public Expr {
public:
  InterpolatedStringExpr(SourceRange range, std::vector<StringPart> parts);
  [[nodiscard]] const std::vector<StringPart>& parts() const;
  [[nodiscard]] std::vector<StringPart>& parts();

private:
  std::vector<StringPart> parts_;
};

class BooleanLiteral final : public Expr {
public:
  BooleanLiteral(SourceRange range, bool value);
  [[nodiscard]] bool value() const;

private:
  bool value_;
};

class NoneLiteral final : public Expr {
public:
  explicit NoneLiteral(SourceRange range);
};

class NameExpr final : public Expr {
public:
  NameExpr(SourceRange range, std::string name);
  [[nodiscard]] const std::string& name() const;
  void setName(std::string name);
  void setCompileTimeText(std::string text);
  [[nodiscard]] const std::string& compileTimeText() const;
  void setCompileTimeBool(bool value);
  [[nodiscard]] bool hasCompileTimeBool() const;
  [[nodiscard]] bool compileTimeBool() const;

private:
  std::string name_;
  std::string compileTimeText_{};
  bool hasCompileTimeBool_ = false;
  bool compileTimeBool_ = false;
};

class CallExpr final : public Expr {
public:
  CallExpr(SourceRange range,
           std::unique_ptr<Expr> callee,
           std::vector<std::unique_ptr<TypeExpr>> typeArgs,
           std::vector<std::unique_ptr<Expr>> arguments);

  [[nodiscard]] const Expr& callee() const;
  [[nodiscard]] Expr& callee();
  [[nodiscard]] const std::vector<std::unique_ptr<TypeExpr>>& typeArgs() const;
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& arguments() const;
  [[nodiscard]] IntrinsicKind intrinsic() const;
  void setIntrinsic(IntrinsicKind kind);
  [[nodiscard]] const std::string& loweredName() const;
  void setLoweredName(std::string name);
  [[nodiscard]] bool isConstructor() const;
  void setConstructor(bool value);
  [[nodiscard]] bool isMethod() const;
  void setMethod(bool value);
  [[nodiscard]] bool isCast() const;
  void setCast(bool value);
  [[nodiscard]] const std::vector<std::string>& paramNames() const;
  void setParamNames(std::vector<std::string> names);
  void setCompileTimeNames(std::vector<std::string> names);
  [[nodiscard]] const std::vector<std::string>& compileTimeNames() const;

private:
  std::unique_ptr<Expr> callee_;
  std::vector<std::unique_ptr<TypeExpr>> typeArgs_;
  std::vector<std::unique_ptr<Expr>> arguments_;
  IntrinsicKind intrinsic_ = IntrinsicKind::None;
  std::string loweredName_;
  std::vector<std::string> paramNames_{};
  std::vector<std::string> compileTimeNames_{};
  bool isConstructor_ = false;
  bool isMethod_ = false;
  bool isCast_ = false;
};

class MemberExpr final : public Expr {
public:
  MemberExpr(SourceRange range, std::unique_ptr<Expr> object, std::string field);
  [[nodiscard]] const Expr& object() const;
  [[nodiscard]] Expr& object();
  [[nodiscard]] const std::string& field() const;
  void setCompileTimeText(std::string text);
  [[nodiscard]] const std::string& compileTimeText() const;

private:
  std::unique_ptr<Expr> object_;
  std::string field_;
  std::string compileTimeText_{};
};

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  FloorDiv,
  Mod,
  Pow,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  And,
  Or,
  Is,
  IsNot,
  In,
  NotIn,
  BitAnd,
  BitOr,
  BitXor,
  Shl,
  Shr,
};

enum class UnaryOp {
  Neg,
  Pos,
  Not,
  Invert,
  PreInc,
  PreDec,
  PostInc,
  PostDec,
  Deref,
  AddrOf,
};

enum class AssignOp {
  Assign,
  Add,
  Sub,
  Mul,
  Div,
  FloorDiv,
  Pow,
  Mod,
  BitAnd,
  BitOr,
  BitXor,
  Shl,
  Shr,
};

class BinaryExpr final : public Expr {
public:
  BinaryExpr(SourceRange range, BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
  [[nodiscard]] BinaryOp op() const;
  [[nodiscard]] const Expr& left() const;
  [[nodiscard]] Expr& left();
  [[nodiscard]] const Expr& right() const;
  [[nodiscard]] Expr& right();

private:
  BinaryOp op_;
  std::unique_ptr<Expr> left_;
  std::unique_ptr<Expr> right_;
};

class UnaryExpr final : public Expr {
public:
  UnaryExpr(SourceRange range, UnaryOp op, std::unique_ptr<Expr> operand);
  [[nodiscard]] UnaryOp op() const;
  [[nodiscard]] const Expr& operand() const;
  [[nodiscard]] Expr& operand();

private:
  UnaryOp op_;
  std::unique_ptr<Expr> operand_;
};

class CastExpr final : public Expr {
public:
  CastExpr(SourceRange range, std::unique_ptr<Expr> value, std::unique_ptr<TypeExpr> target);
  [[nodiscard]] const Expr& value() const;
  [[nodiscard]] Expr& value();
  [[nodiscard]] const TypeExpr& target() const;
  [[nodiscard]] TypeExpr& target();

private:
  std::unique_ptr<Expr> value_;
  std::unique_ptr<TypeExpr> target_;
};

class IndexExpr final : public Expr {
public:
  IndexExpr(SourceRange range,
            std::unique_ptr<Expr> object,
            std::unique_ptr<Expr> index,
            std::unique_ptr<Expr> end = nullptr,
            bool isSlice = false);
  [[nodiscard]] const Expr& object() const;
  [[nodiscard]] Expr& object();
  [[nodiscard]] const Expr& index() const;
  [[nodiscard]] Expr& index();
  [[nodiscard]] bool isSlice() const;
  [[nodiscard]] bool hasStart() const;
  [[nodiscard]] bool hasStop() const;
  [[nodiscard]] const Expr* start() const;
  [[nodiscard]] Expr* start();
  [[nodiscard]] const Expr* stop() const;
  [[nodiscard]] Expr* stop();

private:
  std::unique_ptr<Expr> object_;
  std::unique_ptr<Expr> index_;
  std::unique_ptr<Expr> end_;
  bool isSlice_ = false;
};

class ListLiteral final : public Expr {
public:
  ListLiteral(SourceRange range, std::vector<std::unique_ptr<Expr>> elements);
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& elements() const;
  [[nodiscard]] std::vector<std::unique_ptr<Expr>>& elements();

private:
  std::vector<std::unique_ptr<Expr>> elements_;
};

class DictLiteral final : public Expr {
public:
  DictLiteral(SourceRange range,
              std::vector<std::unique_ptr<Expr>> keys,
              std::vector<std::unique_ptr<Expr>> values);
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& keys() const;
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& values() const;
  [[nodiscard]] std::vector<std::unique_ptr<Expr>>& keys();
  [[nodiscard]] std::vector<std::unique_ptr<Expr>>& values();

private:
  std::vector<std::unique_ptr<Expr>> keys_;
  std::vector<std::unique_ptr<Expr>> values_;
};

class ComprehensionExpr final : public Expr {
public:
  ComprehensionExpr(SourceRange range,
                    std::unique_ptr<Expr> element,
                    std::string name,
                    std::unique_ptr<Expr> iterable);

  [[nodiscard]] const Expr& element() const;
  [[nodiscard]] Expr& element();
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const Expr& iterable() const;
  [[nodiscard]] Expr& iterable();

private:
  std::unique_ptr<Expr> element_;
  std::string name_;
  std::unique_ptr<Expr> iterable_;
};

class TernaryExpr final : public Expr {
public:
  TernaryExpr(SourceRange range,
              std::unique_ptr<Expr> thenValue,
              std::unique_ptr<Expr> condition,
              std::unique_ptr<Expr> elseValue);
  [[nodiscard]] const Expr& thenValue() const;
  [[nodiscard]] Expr& thenValue();
  [[nodiscard]] const Expr& condition() const;
  [[nodiscard]] Expr& condition();
  [[nodiscard]] const Expr& elseValue() const;
  [[nodiscard]] Expr& elseValue();

private:
  std::unique_ptr<Expr> thenValue_;
  std::unique_ptr<Expr> condition_;
  std::unique_ptr<Expr> elseValue_;
};

class TupleExpr final : public Expr {
public:
  TupleExpr(SourceRange range, std::vector<std::unique_ptr<Expr>> elements);
  [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& elements() const;
  [[nodiscard]] std::vector<std::unique_ptr<Expr>>& elements();

private:
  std::vector<std::unique_ptr<Expr>> elements_;
};

class WalrusExpr final : public Expr {
public:
  WalrusExpr(SourceRange range, std::string name, std::unique_ptr<Expr> value);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const Expr& value() const;
  [[nodiscard]] Expr& value();

private:
  std::string name_;
  std::unique_ptr<Expr> value_;
};

struct ParamDecl {
  std::string name;
  std::unique_ptr<TypeExpr> type;
  std::unique_ptr<Expr> defaultValue;
  SourceRange range{};
};

class LambdaExpr final : public Expr {
public:
  LambdaExpr(SourceRange range, std::vector<ParamDecl> params, std::unique_ptr<Expr> body,
             std::unique_ptr<TypeExpr> returnType = nullptr);
  [[nodiscard]] const std::vector<ParamDecl>& params() const;
  [[nodiscard]] std::vector<ParamDecl>& params();
  [[nodiscard]] const Expr& body() const;
  [[nodiscard]] Expr& body();
  [[nodiscard]] const TypeExpr* returnType() const;
  void setLlvmName(std::string name);
  [[nodiscard]] const std::string& llvmName() const;

private:
  std::vector<ParamDecl> params_;
  std::unique_ptr<Expr> body_;
  std::unique_ptr<TypeExpr> returnType_;
  std::string llvmName_{};
};

struct IfBranch {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> body;
};

class IfStmt final : public Stmt {
public:
  IfStmt(SourceRange range, std::vector<IfBranch> branches);
  [[nodiscard]] const std::vector<IfBranch>& branches() const;
  [[nodiscard]] std::vector<IfBranch>& branches();

private:
  std::vector<IfBranch> branches_;
};

class WhileStmt final : public Stmt {
public:
  WhileStmt(SourceRange range, std::unique_ptr<Expr> condition, std::vector<std::unique_ptr<Stmt>> body);
  [[nodiscard]] const Expr& condition() const;
  [[nodiscard]] Expr& condition();
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& body() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& body();

private:
  std::unique_ptr<Expr> condition_;
  std::vector<std::unique_ptr<Stmt>> body_;
};

class ForStmt final : public Stmt {
public:
  ForStmt(SourceRange range,
          std::string name,
          std::unique_ptr<Expr> iterable,
          std::vector<std::unique_ptr<Stmt>> body);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const Expr& iterable() const;
  [[nodiscard]] Expr& iterable();
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& body() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& body();

private:
  std::string name_;
  std::unique_ptr<Expr> iterable_;
  std::vector<std::unique_ptr<Stmt>> body_;
};

class AssertStmt final : public Stmt {
public:
  AssertStmt(SourceRange range, std::unique_ptr<Expr> condition, std::unique_ptr<Expr> message);
  [[nodiscard]] const Expr& condition() const;
  [[nodiscard]] Expr& condition();
  [[nodiscard]] const Expr* message() const;

private:
  std::unique_ptr<Expr> condition_;
  std::unique_ptr<Expr> message_;
};

class RaiseStmt final : public Stmt {
public:
  RaiseStmt(SourceRange range, std::unique_ptr<Expr> value);
  [[nodiscard]] const Expr* value() const;

private:
  std::unique_ptr<Expr> value_;
};

struct ExceptHandler {
  std::unique_ptr<TypeExpr> type;
  std::string name;
  std::vector<std::unique_ptr<Stmt>> body;
  SourceRange range{};
};

class TryStmt final : public Stmt {
public:
  TryStmt(SourceRange range,
          std::vector<std::unique_ptr<Stmt>> body,
          std::vector<ExceptHandler> handlers,
          std::vector<std::unique_ptr<Stmt>> elseBody,
          std::vector<std::unique_ptr<Stmt>> finallyBody);
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& body() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& body();
  [[nodiscard]] const std::vector<ExceptHandler>& handlers() const;
  [[nodiscard]] std::vector<ExceptHandler>& handlers();
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& elseBody() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& elseBody();
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& finallyBody() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& finallyBody();

private:
  std::vector<std::unique_ptr<Stmt>> body_;
  std::vector<ExceptHandler> handlers_;
  std::vector<std::unique_ptr<Stmt>> elseBody_;
  std::vector<std::unique_ptr<Stmt>> finallyBody_;
};

struct MatchArm {
  std::unique_ptr<Expr> pattern;
  std::unique_ptr<Expr> guard;
  std::vector<std::unique_ptr<Stmt>> body;
  SourceRange range{};
};

class MatchStmt final : public Stmt {
public:
  MatchStmt(SourceRange range, std::unique_ptr<Expr> subject, std::vector<MatchArm> arms);
  [[nodiscard]] const Expr& subject() const;
  [[nodiscard]] Expr& subject();
  void setSubject(std::unique_ptr<Expr> subject);
  [[nodiscard]] const std::vector<MatchArm>& arms() const;
  [[nodiscard]] std::vector<MatchArm>& arms();

private:
  std::unique_ptr<Expr> subject_;
  std::vector<MatchArm> arms_;
};

class DelStmt final : public Stmt {
public:
  DelStmt(SourceRange range, std::unique_ptr<Expr> target);
  [[nodiscard]] const Expr& target() const;
  [[nodiscard]] Expr& target();

private:
  std::unique_ptr<Expr> target_;
};

class DeferStmt final : public Stmt {
public:
  DeferStmt(SourceRange range, std::vector<std::unique_ptr<Stmt>> body);
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& body() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& body();

private:
  std::vector<std::unique_ptr<Stmt>> body_;
};

class WithStmt final : public Stmt {
public:
  WithStmt(SourceRange range, std::unique_ptr<Expr> context, std::string name,
           std::vector<std::unique_ptr<Stmt>> body);
  [[nodiscard]] const Expr& context() const;
  [[nodiscard]] Expr& context();
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& body() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& body();

private:
  std::unique_ptr<Expr> context_;
  std::string name_;
  std::vector<std::unique_ptr<Stmt>> body_;
};

struct FieldDecl {
  std::string name;
  std::unique_ptr<TypeExpr> type;
  std::unique_ptr<Expr> init;
  SourceRange range{};
  bool isPublic = true;
  bool isStatic = false;
};

class VarDecl final : public Stmt {
public:
  VarDecl(SourceRange range,
          std::string name,
          std::unique_ptr<TypeExpr> type,
          std::unique_ptr<Expr> init,
          bool isStatic = false,
          bool isConst = false);

  [[nodiscard]] const std::string& name() const;
  void setName(std::string name);
  [[nodiscard]] bool hasType() const;
  [[nodiscard]] const TypeExpr& type() const;
  [[nodiscard]] TypeExpr& type();
  [[nodiscard]] const Expr* init() const;
  [[nodiscard]] bool isStatic() const;
  [[nodiscard]] bool isConst() const;
  void setConst(bool value);

private:
  std::string name_;
  std::unique_ptr<TypeExpr> type_;
  std::unique_ptr<Expr> init_;
  bool isStatic_ = false;
  bool isConst_ = false;
};

class AssignStmt final : public Stmt {
public:
  AssignStmt(SourceRange range, std::unique_ptr<Expr> target, std::unique_ptr<Expr> value,
             AssignOp op = AssignOp::Assign);
  [[nodiscard]] const Expr& target() const;
  [[nodiscard]] const Expr& value() const;
  [[nodiscard]] AssignOp op() const;
  void setNameAlias(bool value);
  [[nodiscard]] bool isNameAlias() const;

private:
  std::unique_ptr<Expr> target_;
  std::unique_ptr<Expr> value_;
  AssignOp op_ = AssignOp::Assign;
  bool isNameAlias_ = false;
};

class ReturnStmt final : public Stmt {
public:
  ReturnStmt(SourceRange range, std::unique_ptr<Expr> value);
  [[nodiscard]] const Expr* value() const;

private:
  std::unique_ptr<Expr> value_;
};

class ExprStmt final : public Stmt {
public:
  ExprStmt(SourceRange range, std::unique_ptr<Expr> expression);
  [[nodiscard]] const Expr& expression() const;

private:
  std::unique_ptr<Expr> expression_;
};

class PassStmt final : public Stmt {
public:
  explicit PassStmt(SourceRange range);
};

class BreakStmt final : public Stmt {
public:
  explicit BreakStmt(SourceRange range);
};

class ContinueStmt final : public Stmt {
public:
  explicit ContinueStmt(SourceRange range);
};

class FunctionDef;

struct EnumPayloadField {
  std::string name;
  std::unique_ptr<TypeExpr> type;
};

struct EnumVariant {
  std::string name;
  std::unique_ptr<Expr> value;
  std::vector<EnumPayloadField> payload;
  SourceRange range{};
};

class EnumDef final : public Stmt {
public:
  EnumDef(SourceRange range, std::string name, std::vector<EnumVariant> variants);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<EnumVariant>& variants() const;
  [[nodiscard]] std::vector<EnumVariant>& variants();
  [[nodiscard]] const std::vector<std::unique_ptr<FunctionDef>>& methods() const;
  [[nodiscard]] std::vector<std::unique_ptr<FunctionDef>>& methods();
  void setFlags(bool value);
  [[nodiscard]] bool isFlags() const;
  void setDecorators(std::vector<std::string> decorators);
  [[nodiscard]] const std::vector<std::string>& decorators() const;

private:
  std::string name_;
  std::vector<EnumVariant> variants_;
  std::vector<std::unique_ptr<FunctionDef>> methods_{};
  std::vector<std::string> decorators_{};
  bool isFlags_ = false;
};

class FunctionDef final : public Stmt {
public:
  FunctionDef(SourceRange range,
              std::string name,
              std::vector<ParamDecl> params,
              std::unique_ptr<TypeExpr> returnType,
              std::vector<std::unique_ptr<Stmt>> body,
              std::string externName);

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<ParamDecl>& params() const;
  [[nodiscard]] std::vector<ParamDecl>& params();
  [[nodiscard]] const TypeExpr& returnType() const;
  [[nodiscard]] TypeExpr& returnType();
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& body() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& body();
  [[nodiscard]] const std::string& externName() const;
  [[nodiscard]] bool isExtern() const;
  [[nodiscard]] const std::string& ownerClass() const;
  [[nodiscard]] bool isMethod() const;
  void setOwnerClass(std::string ownerClass);
  [[nodiscard]] bool isAbstract() const;
  [[nodiscard]] bool isOverride() const;
  void setAbstract(bool value);
  void setOverride(bool value);
  void setDecorators(std::vector<std::string> decorators);
  [[nodiscard]] const std::vector<std::string>& decorators() const;
  void setModulePrefix(std::string prefix);
  [[nodiscard]] const std::string& modulePrefix() const;
  void setTypeParams(std::vector<std::string> typeParams);
  [[nodiscard]] const std::vector<std::string>& typeParams() const;
  void setInferredReturn(bool value);
  [[nodiscard]] bool hasInferredReturn() const;

private:
  std::string name_;
  std::vector<ParamDecl> params_;
  std::unique_ptr<TypeExpr> returnType_;
  std::vector<std::unique_ptr<Stmt>> body_;
  std::string externName_;
  std::string ownerClass_;
  std::vector<std::string> decorators_{};
  std::string modulePrefix_{};
  std::vector<std::string> typeParams_{};
  bool isAbstract_ = false;
  bool isOverride_ = false;
  bool inferredReturn_ = false;
};

class ClassDef final : public Stmt {
public:
  ClassDef(SourceRange range,
           std::string name,
           std::vector<FieldDecl> fields,
           std::vector<std::unique_ptr<FunctionDef>> methods,
           std::vector<std::string> bases = {},
           std::vector<std::string> typeParams = {});
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const std::vector<FieldDecl>& fields() const;
  [[nodiscard]] const std::vector<std::unique_ptr<FunctionDef>>& methods() const;
  [[nodiscard]] std::vector<std::unique_ptr<FunctionDef>>& methods();
  [[nodiscard]] const std::vector<std::string>& bases() const;
  [[nodiscard]] const std::vector<std::string>& typeParams() const;
  void setDecorators(std::vector<std::string> decorators);
  [[nodiscard]] const std::vector<std::string>& decorators() const;
  void setStruct(bool value);
  [[nodiscard]] bool isStruct() const;
  void setFrozen(bool value);
  [[nodiscard]] bool isFrozen() const;

private:
  std::string name_;
  std::vector<FieldDecl> fields_;
  std::vector<std::unique_ptr<FunctionDef>> methods_;
  std::vector<std::string> bases_{};
  std::vector<std::string> typeParams_{};
  std::vector<std::string> decorators_{};
  bool isStruct_ = false;
  bool isFrozen_ = false;
};

class ImportStmt final : public Stmt {
public:
  ImportStmt(SourceRange range,
             std::vector<std::string> modulePath,
             std::string alias,
             std::vector<std::string> names,
             bool star);

  [[nodiscard]] const std::vector<std::string>& modulePath() const;
  [[nodiscard]] const std::string& alias() const;
  [[nodiscard]] const std::vector<std::string>& names() const;
  [[nodiscard]] bool star() const;
  [[nodiscard]] bool isFrom() const;

private:
  std::vector<std::string> modulePath_;
  std::string alias_;
  std::vector<std::string> names_;
  bool star_ = false;
};

class TypeAlias final : public Stmt {
public:
  TypeAlias(SourceRange range, std::string name, std::unique_ptr<TypeExpr> type);
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] const TypeExpr& type() const;
  [[nodiscard]] TypeExpr& type();

private:
  std::string name_;
  std::unique_ptr<TypeExpr> type_;
};

/// How a macro invocation body is captured.
enum class MacroSyntaxMode {
  Sere,
  Tokens,
  Raw,
  Pipeline,
};

/// Interpolation holes inside a raw/foreign syntax body.
enum class MacroInterpolate {
  None,
  Brace,
  Dollar,
};

/// Delimiter used at a macro invocation site.
enum class MacroDelimiter {
  BangParen,
  BangBrace,
  BangBracket,
  Indent,
};

struct MacroMatchArm {
  std::vector<Token> pattern;
  std::vector<std::unique_ptr<Stmt>> body;
  SourceRange range{};
};

class SpliceExpr final : public Expr {
public:
  SpliceExpr(SourceRange range, std::string name, bool repeat = false, bool commaSeparated = false);

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] bool isRepeat() const;
  [[nodiscard]] bool commaSeparated() const;
  void setRepeat(bool repeat, bool commaSeparated);

private:
  std::string name_;
  bool repeat_ = false;
  bool commaSeparated_ = false;
};

class MacroDef final : public Stmt {
public:
  MacroDef(SourceRange range, std::string name, std::vector<std::string> params,
           SourceRange nameRange = {});

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] SourceRange nameRange() const;
  [[nodiscard]] const std::vector<std::string>& params() const;
  [[nodiscard]] bool variadic() const;
  void setVariadic(bool value);
  [[nodiscard]] MacroSyntaxMode syntaxMode() const;
  void setSyntaxMode(MacroSyntaxMode mode);
  [[nodiscard]] MacroInterpolate interpolate() const;
  void setInterpolate(MacroInterpolate mode);
  [[nodiscard]] bool typed() const;
  void setTyped(bool value);
  [[nodiscard]] const std::string& wrapper() const;
  void setWrapper(std::string wrapper);
  [[nodiscard]] const std::vector<MacroMatchArm>& matchArms() const;
  [[nodiscard]] std::vector<MacroMatchArm>& matchArms();
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& quoteBody() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& quoteBody();

private:
  std::string name_;
  SourceRange nameRange_{};
  std::vector<std::string> params_;
  std::vector<MacroMatchArm> matchArms_{};
  std::vector<std::unique_ptr<Stmt>> quoteBody_{};
  std::string wrapper_{};
  MacroSyntaxMode syntaxMode_ = MacroSyntaxMode::Sere;
  MacroInterpolate interpolate_ = MacroInterpolate::None;
  bool variadic_ = false;
  bool typed_ = false;
};

class MacroInvokeExpr final : public Expr {
public:
  MacroInvokeExpr(SourceRange range,
                  std::string name,
                  MacroDelimiter delimiter,
                  std::string rawText,
                  SourceRange rawRange,
                  std::vector<Token> tokens);

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] MacroDelimiter delimiter() const;
  [[nodiscard]] const std::string& rawText() const;
  [[nodiscard]] SourceRange rawRange() const;
  [[nodiscard]] const std::vector<Token>& tokens() const;

private:
  std::string name_;
  MacroDelimiter delimiter_ = MacroDelimiter::BangParen;
  std::string rawText_;
  SourceRange rawRange_{};
  std::vector<Token> tokens_;
};

class MacroInvokeStmt final : public Stmt {
public:
  MacroInvokeStmt(SourceRange range,
                  std::string name,
                  MacroDelimiter delimiter,
                  std::string rawText,
                  SourceRange rawRange,
                  std::vector<Token> tokens);

  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] MacroDelimiter delimiter() const;
  [[nodiscard]] const std::string& rawText() const;
  [[nodiscard]] SourceRange rawRange() const;
  [[nodiscard]] const std::vector<Token>& tokens() const;

private:
  std::string name_;
  MacroDelimiter delimiter_ = MacroDelimiter::Indent;
  std::string rawText_;
  SourceRange rawRange_{};
  std::vector<Token> tokens_;
};

class Module final : public Node {
public:
  Module(SourceRange range, std::vector<std::unique_ptr<Stmt>> statements);
  [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& statements() const;
  [[nodiscard]] std::vector<std::unique_ptr<Stmt>>& statements();
  void insertFront(std::vector<std::unique_ptr<Stmt>> extra);

private:
  std::vector<std::unique_ptr<Stmt>> statements_;
};

}  // namespace sere
