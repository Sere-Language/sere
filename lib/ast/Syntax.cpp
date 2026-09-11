/// @file Syntax.cpp
/// AST node constructors and accessors.

#include "sere/ast/Syntax.h"

#include "sere/source/SourceLocation.h"
#include "sere/types/Intrinsic.h"
#include "sere/types/Type.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sere {

bool isReservedDecoratorName(std::string_view name) {
  return name == "public" || name == "private" || name == "abstract" || name == "override" ||
         name == "frozen" || name == "flags" || name == "static";
}

std::string decoratorExprName(const Expr& expr) {
  const Expr* current = &expr;
  if (current->kind() == NodeKind::CallExpr) {
    current = &static_cast<const CallExpr*>(current)->callee();
  }
  if (current->kind() == NodeKind::MemberExpr) {
    return static_cast<const MemberExpr*>(current)->field();
  }
  if (current->kind() == NodeKind::NameExpr) {
    return static_cast<const NameExpr*>(current)->name();
  }
  return {};
}

bool isReservedDecoratorExpr(const Expr& expr) {
  if (expr.kind() != NodeKind::NameExpr) {
    return false;
  }
  return isReservedDecoratorName(static_cast<const NameExpr&>(expr).name());
}

std::vector<std::string> decoratorExprNames(const std::vector<std::unique_ptr<Expr>>& exprs) {
  std::vector<std::string> names;
  names.reserve(exprs.size());
  for (const std::unique_ptr<Expr>& expr : exprs) {
    names.push_back(expr == nullptr ? std::string{} : decoratorExprName(*expr));
  }
  return names;
}

bool hasRuntimeDecorators(const std::vector<std::unique_ptr<Expr>>& exprs) {
  for (const std::unique_ptr<Expr>& expr : exprs) {
    if (expr != nullptr && !isReservedDecoratorExpr(*expr)) {
      return true;
    }
  }
  return false;
}

Node::Node(NodeKind kind, SourceRange range) : kind_(kind), range_(range) {
}

NodeKind Node::kind() const {
  return kind_;
}

SourceRange Node::range() const {
  return range_;
}

void Node::setRange(SourceRange range) {
  range_ = range;
}

const Type* Node::resolvedType() const {
  return resolvedType_;
}

void Node::setResolvedType(const Type* type) {
  resolvedType_ = type;
}

bool Node::fromPrelude() const {
  return fromPrelude_;
}

void Node::setFromPrelude(bool value) {
  fromPrelude_ = value;
}

bool Node::isPrivate() const {
  return isPrivate_;
}

void Node::setPrivate(bool value) {
  isPrivate_ = value;
}

TypeExpr::TypeExpr(SourceRange range, std::string name, std::vector<std::unique_ptr<TypeExpr>> args)
    : Node(NodeKind::TypeExpr, range), name_(std::move(name)), args_(std::move(args)) {
}

const std::string& TypeExpr::name() const {
  return name_;
}

const std::vector<std::unique_ptr<TypeExpr>>& TypeExpr::args() const {
  return args_;
}

IntegerLiteral::IntegerLiteral(SourceRange range, std::int64_t value, bool isByte)
    : Expr(NodeKind::IntegerLiteral, range), value_(value), isByte_(isByte) {
}

std::int64_t IntegerLiteral::value() const {
  return value_;
}

bool IntegerLiteral::isByte() const {
  return isByte_;
}

FloatLiteral::FloatLiteral(SourceRange range, double value, bool isF32)
    : Expr(NodeKind::FloatLiteral, range), value_(value), isF32_(isF32) {
}

double FloatLiteral::value() const {
  return value_;
}

bool FloatLiteral::isF32() const {
  return isF32_;
}

StringLiteral::StringLiteral(SourceRange range, std::string value, bool regex)
    : Expr(NodeKind::StringLiteral, range), value_(std::move(value)), regex_(regex) {
}

const std::string& StringLiteral::value() const {
  return value_;
}

bool StringLiteral::isRegex() const {
  return regex_;
}

InterpolatedStringExpr::InterpolatedStringExpr(SourceRange range, std::vector<StringPart> parts)
    : Expr(NodeKind::InterpolatedStringExpr, range), parts_(std::move(parts)) {
}

const std::vector<StringPart>& InterpolatedStringExpr::parts() const {
  return parts_;
}

std::vector<StringPart>& InterpolatedStringExpr::parts() {
  return parts_;
}

BooleanLiteral::BooleanLiteral(SourceRange range, bool value)
    : Expr(NodeKind::BooleanLiteral, range), value_(value) {
}

bool BooleanLiteral::value() const {
  return value_;
}

NoneLiteral::NoneLiteral(SourceRange range) : Expr(NodeKind::NoneLiteral, range) {
}

NameExpr::NameExpr(SourceRange range, std::string name)
    : Expr(NodeKind::NameExpr, range), name_(std::move(name)) {
}

const std::string& NameExpr::name() const {
  return name_;
}

void NameExpr::setName(std::string name) {
  name_ = std::move(name);
}

void NameExpr::setCompileTimeText(std::string text) {
  compileTimeText_ = std::move(text);
}

const std::string& NameExpr::compileTimeText() const {
  return compileTimeText_;
}

void NameExpr::setCompileTimeBool(bool value) {
  hasCompileTimeBool_ = true;
  compileTimeBool_ = value;
}

bool NameExpr::hasCompileTimeBool() const {
  return hasCompileTimeBool_;
}

bool NameExpr::compileTimeBool() const {
  return compileTimeBool_;
}

CallExpr::CallExpr(SourceRange range,
                   std::unique_ptr<Expr> callee,
                   std::vector<std::unique_ptr<TypeExpr>> typeArgs,
                   std::vector<std::unique_ptr<Expr>> arguments,
                   std::vector<NamedArgument> keywordArguments)
    : Expr(NodeKind::CallExpr, range), callee_(std::move(callee)), typeArgs_(std::move(typeArgs)),
      arguments_(std::move(arguments)), keywordArguments_(std::move(keywordArguments)) {
}

const Expr& CallExpr::callee() const {
  return *callee_;
}

Expr& CallExpr::callee() {
  return *callee_;
}

const std::vector<std::unique_ptr<TypeExpr>>& CallExpr::typeArgs() const {
  return typeArgs_;
}

const std::vector<std::unique_ptr<Expr>>& CallExpr::arguments() const {
  return arguments_;
}

const std::vector<NamedArgument>& CallExpr::keywordArguments() const {
  return keywordArguments_;
}

const std::vector<const Expr*>& CallExpr::boundArguments() const {
  return boundArguments_;
}

void CallExpr::setBoundArguments(std::vector<const Expr*> arguments,
                                 std::vector<std::unique_ptr<Expr>> owned) {
  boundArguments_ = std::move(arguments);
  boundStorage_ = std::move(owned);
}

IntrinsicKind CallExpr::intrinsic() const {
  return intrinsic_;
}

void CallExpr::setIntrinsic(IntrinsicKind kind) {
  intrinsic_ = kind;
}

const std::string& CallExpr::loweredName() const {
  return loweredName_;
}

void CallExpr::setLoweredName(std::string name) {
  loweredName_ = std::move(name);
}

bool CallExpr::isConstructor() const {
  return isConstructor_;
}

void CallExpr::setConstructor(bool value) {
  isConstructor_ = value;
}

bool CallExpr::isMethod() const {
  return isMethod_;
}

void CallExpr::setMethod(bool value) {
  isMethod_ = value;
}

bool CallExpr::isCast() const {
  return isCast_;
}

void CallExpr::setCast(bool value) {
  isCast_ = value;
}

void CallExpr::setUnboundMethodCall(bool value) {
  isUnboundMethodCall_ = value;
}

bool CallExpr::isUnboundMethodCall() const {
  return isUnboundMethodCall_;
}

const std::vector<std::string>& CallExpr::paramNames() const {
  return paramNames_;
}

void CallExpr::setParamNames(std::vector<std::string> names) {
  paramNames_ = std::move(names);
}

void CallExpr::setCompileTimeNames(std::vector<std::string> names) {
  compileTimeNames_ = std::move(names);
}

const std::vector<std::string>& CallExpr::compileTimeNames() const {
  return compileTimeNames_;
}

MemberExpr::MemberExpr(SourceRange range, std::unique_ptr<Expr> object, std::string field)
    : Expr(NodeKind::MemberExpr, range), object_(std::move(object)), field_(std::move(field)) {
}

const Expr& MemberExpr::object() const {
  return *object_;
}

Expr& MemberExpr::object() {
  return *object_;
}

const std::string& MemberExpr::field() const {
  return field_;
}

void MemberExpr::setCompileTimeText(std::string text) {
  compileTimeText_ = std::move(text);
}

const std::string& MemberExpr::compileTimeText() const {
  return compileTimeText_;
}

void MemberExpr::setPropertyGet(std::string name) {
  propertyGet_ = std::move(name);
}

const std::string& MemberExpr::propertyGet() const {
  return propertyGet_;
}

void MemberExpr::setPropertySet(std::string name) {
  propertySet_ = std::move(name);
}

const std::string& MemberExpr::propertySet() const {
  return propertySet_;
}

void MemberExpr::setBackingField(bool value) {
  backingField_ = value;
}

bool MemberExpr::usesBackingField() const {
  return backingField_;
}

void MemberExpr::setBoundMethod(std::string llvmName) {
  boundMethodLlvm_ = std::move(llvmName);
  unboundMethod_ = false;
}

void MemberExpr::setUnboundMethod(std::string llvmName) {
  boundMethodLlvm_ = std::move(llvmName);
  unboundMethod_ = true;
}

bool MemberExpr::isBoundMethod() const {
  return !boundMethodLlvm_.empty() && !unboundMethod_;
}

bool MemberExpr::isUnboundMethod() const {
  return unboundMethod_ && !boundMethodLlvm_.empty();
}

const std::string& MemberExpr::boundMethodLlvm() const {
  return boundMethodLlvm_;
}

BinaryExpr::BinaryExpr(SourceRange range,
                       BinaryOp op,
                       std::unique_ptr<Expr> left,
                       std::unique_ptr<Expr> right)
    : Expr(NodeKind::BinaryExpr, range), op_(op), left_(std::move(left)), right_(std::move(right)) {
}

BinaryOp BinaryExpr::op() const {
  return op_;
}

const Expr& BinaryExpr::left() const {
  return *left_;
}

Expr& BinaryExpr::left() {
  return *left_;
}

const Expr& BinaryExpr::right() const {
  return *right_;
}

Expr& BinaryExpr::right() {
  return *right_;
}

UnaryExpr::UnaryExpr(SourceRange range, UnaryOp op, std::unique_ptr<Expr> operand)
    : Expr(NodeKind::UnaryExpr, range), op_(op), operand_(std::move(operand)) {
}

UnaryOp UnaryExpr::op() const {
  return op_;
}

const Expr& UnaryExpr::operand() const {
  return *operand_;
}

Expr& UnaryExpr::operand() {
  return *operand_;
}

AwaitExpr::AwaitExpr(SourceRange range, std::unique_ptr<Expr> operand)
    : Expr(NodeKind::AwaitExpr, range), operand_(std::move(operand)) {
}

const Expr& AwaitExpr::operand() const {
  return *operand_;
}

Expr& AwaitExpr::operand() {
  return *operand_;
}

CastExpr::CastExpr(SourceRange range, std::unique_ptr<Expr> value, std::unique_ptr<TypeExpr> target)
    : Expr(NodeKind::CastExpr, range), value_(std::move(value)), target_(std::move(target)) {
}

const Expr& CastExpr::value() const {
  return *value_;
}

Expr& CastExpr::value() {
  return *value_;
}

const TypeExpr& CastExpr::target() const {
  return *target_;
}

TypeExpr& CastExpr::target() {
  return *target_;
}

IndexExpr::IndexExpr(SourceRange range,
                     std::unique_ptr<Expr> object,
                     std::unique_ptr<Expr> index,
                     std::unique_ptr<Expr> end,
                     bool isSlice)
    : Expr(NodeKind::IndexExpr, range), object_(std::move(object)), index_(std::move(index)),
      end_(std::move(end)), isSlice_(isSlice) {
}

const Expr& IndexExpr::object() const {
  return *object_;
}

Expr& IndexExpr::object() {
  return *object_;
}

const Expr& IndexExpr::index() const {
  return *index_;
}

Expr& IndexExpr::index() {
  return *index_;
}

bool IndexExpr::isSlice() const {
  return isSlice_;
}

bool IndexExpr::hasStart() const {
  return index_ != nullptr;
}

bool IndexExpr::hasStop() const {
  return end_ != nullptr;
}

const Expr* IndexExpr::start() const {
  return index_.get();
}

Expr* IndexExpr::start() {
  return index_.get();
}

const Expr* IndexExpr::stop() const {
  return end_.get();
}

Expr* IndexExpr::stop() {
  return end_.get();
}

ListLiteral::ListLiteral(SourceRange range, std::vector<std::unique_ptr<Expr>> elements)
    : Expr(NodeKind::ListLiteral, range), elements_(std::move(elements)) {
}

const std::vector<std::unique_ptr<Expr>>& ListLiteral::elements() const {
  return elements_;
}

std::vector<std::unique_ptr<Expr>>& ListLiteral::elements() {
  return elements_;
}

DictLiteral::DictLiteral(SourceRange range,
                         std::vector<std::unique_ptr<Expr>> keys,
                         std::vector<std::unique_ptr<Expr>> values)
    : Expr(NodeKind::DictLiteral, range), keys_(std::move(keys)), values_(std::move(values)) {
}

const std::vector<std::unique_ptr<Expr>>& DictLiteral::keys() const {
  return keys_;
}

const std::vector<std::unique_ptr<Expr>>& DictLiteral::values() const {
  return values_;
}

std::vector<std::unique_ptr<Expr>>& DictLiteral::keys() {
  return keys_;
}

std::vector<std::unique_ptr<Expr>>& DictLiteral::values() {
  return values_;
}

ComprehensionExpr::ComprehensionExpr(SourceRange range,
                                     std::unique_ptr<Expr> element,
                                     std::string name,
                                     std::unique_ptr<Expr> iterable)
    : Expr(NodeKind::ComprehensionExpr, range), element_(std::move(element)),
      name_(std::move(name)), iterable_(std::move(iterable)) {
}

const Expr& ComprehensionExpr::element() const {
  return *element_;
}

Expr& ComprehensionExpr::element() {
  return *element_;
}

const std::string& ComprehensionExpr::name() const {
  return name_;
}

const Expr& ComprehensionExpr::iterable() const {
  return *iterable_;
}

Expr& ComprehensionExpr::iterable() {
  return *iterable_;
}

TernaryExpr::TernaryExpr(SourceRange range,
                         std::unique_ptr<Expr> thenValue,
                         std::unique_ptr<Expr> condition,
                         std::unique_ptr<Expr> elseValue)
    : Expr(NodeKind::TernaryExpr, range), thenValue_(std::move(thenValue)),
      condition_(std::move(condition)), elseValue_(std::move(elseValue)) {
}

const Expr& TernaryExpr::thenValue() const {
  return *thenValue_;
}

Expr& TernaryExpr::thenValue() {
  return *thenValue_;
}

const Expr& TernaryExpr::condition() const {
  return *condition_;
}

Expr& TernaryExpr::condition() {
  return *condition_;
}

const Expr& TernaryExpr::elseValue() const {
  return *elseValue_;
}

Expr& TernaryExpr::elseValue() {
  return *elseValue_;
}

TupleExpr::TupleExpr(SourceRange range, std::vector<std::unique_ptr<Expr>> elements)
    : Expr(NodeKind::TupleExpr, range), elements_(std::move(elements)) {
}

const std::vector<std::unique_ptr<Expr>>& TupleExpr::elements() const {
  return elements_;
}

std::vector<std::unique_ptr<Expr>>& TupleExpr::elements() {
  return elements_;
}

WalrusExpr::WalrusExpr(SourceRange range, std::string name, std::unique_ptr<Expr> value)
    : Expr(NodeKind::WalrusExpr, range), name_(std::move(name)), value_(std::move(value)) {
}

const std::string& WalrusExpr::name() const {
  return name_;
}

const Expr& WalrusExpr::value() const {
  return *value_;
}

Expr& WalrusExpr::value() {
  return *value_;
}

LambdaExpr::LambdaExpr(SourceRange range,
                       std::vector<ParamDecl> params,
                       std::unique_ptr<Expr> body,
                       std::unique_ptr<TypeExpr> returnType)
    : Expr(NodeKind::LambdaExpr, range), params_(std::move(params)), body_(std::move(body)),
      returnType_(std::move(returnType)) {
}

const std::vector<ParamDecl>& LambdaExpr::params() const {
  return params_;
}

std::vector<ParamDecl>& LambdaExpr::params() {
  return params_;
}

const Expr& LambdaExpr::body() const {
  return *body_;
}

Expr& LambdaExpr::body() {
  return *body_;
}

const TypeExpr* LambdaExpr::returnType() const {
  return returnType_.get();
}

void LambdaExpr::setLlvmName(std::string name) {
  llvmName_ = std::move(name);
}

const std::string& LambdaExpr::llvmName() const {
  return llvmName_;
}

IfStmt::IfStmt(SourceRange range, std::vector<IfBranch> branches)
    : Stmt(NodeKind::IfStmt, range), branches_(std::move(branches)) {
}

const std::vector<IfBranch>& IfStmt::branches() const {
  return branches_;
}

std::vector<IfBranch>& IfStmt::branches() {
  return branches_;
}

WhileStmt::WhileStmt(SourceRange range,
                     std::unique_ptr<Expr> condition,
                     std::vector<std::unique_ptr<Stmt>> body)
    : Stmt(NodeKind::WhileStmt, range), condition_(std::move(condition)), body_(std::move(body)) {
}

const Expr& WhileStmt::condition() const {
  return *condition_;
}

Expr& WhileStmt::condition() {
  return *condition_;
}

const std::vector<std::unique_ptr<Stmt>>& WhileStmt::body() const {
  return body_;
}

std::vector<std::unique_ptr<Stmt>>& WhileStmt::body() {
  return body_;
}

ForStmt::ForStmt(SourceRange range,
                 std::string name,
                 std::unique_ptr<Expr> iterable,
                 std::vector<std::unique_ptr<Stmt>> body)
    : Stmt(NodeKind::ForStmt, range), name_(std::move(name)), iterable_(std::move(iterable)),
      body_(std::move(body)) {
}

const std::string& ForStmt::name() const {
  return name_;
}

const Expr& ForStmt::iterable() const {
  return *iterable_;
}

Expr& ForStmt::iterable() {
  return *iterable_;
}

const std::vector<std::unique_ptr<Stmt>>& ForStmt::body() const {
  return body_;
}

std::vector<std::unique_ptr<Stmt>>& ForStmt::body() {
  return body_;
}

AssertStmt::AssertStmt(SourceRange range,
                       std::unique_ptr<Expr> condition,
                       std::unique_ptr<Expr> message)
    : Stmt(NodeKind::AssertStmt, range), condition_(std::move(condition)),
      message_(std::move(message)) {
}

const Expr& AssertStmt::condition() const {
  return *condition_;
}

Expr& AssertStmt::condition() {
  return *condition_;
}

const Expr* AssertStmt::message() const {
  return message_.get();
}

RaiseStmt::RaiseStmt(SourceRange range, std::unique_ptr<Expr> value)
    : Stmt(NodeKind::RaiseStmt, range), value_(std::move(value)) {
}

const Expr* RaiseStmt::value() const {
  return value_.get();
}

TryStmt::TryStmt(SourceRange range,
                 std::vector<std::unique_ptr<Stmt>> body,
                 std::vector<ExceptHandler> handlers,
                 std::vector<std::unique_ptr<Stmt>> elseBody,
                 std::vector<std::unique_ptr<Stmt>> finallyBody)
    : Stmt(NodeKind::TryStmt, range), body_(std::move(body)), handlers_(std::move(handlers)),
      elseBody_(std::move(elseBody)), finallyBody_(std::move(finallyBody)) {
}

const std::vector<std::unique_ptr<Stmt>>& TryStmt::body() const {
  return body_;
}

std::vector<std::unique_ptr<Stmt>>& TryStmt::body() {
  return body_;
}

const std::vector<ExceptHandler>& TryStmt::handlers() const {
  return handlers_;
}

std::vector<ExceptHandler>& TryStmt::handlers() {
  return handlers_;
}

const std::vector<std::unique_ptr<Stmt>>& TryStmt::elseBody() const {
  return elseBody_;
}

std::vector<std::unique_ptr<Stmt>>& TryStmt::elseBody() {
  return elseBody_;
}

const std::vector<std::unique_ptr<Stmt>>& TryStmt::finallyBody() const {
  return finallyBody_;
}

std::vector<std::unique_ptr<Stmt>>& TryStmt::finallyBody() {
  return finallyBody_;
}

MatchStmt::MatchStmt(SourceRange range, std::unique_ptr<Expr> subject, std::vector<MatchArm> arms)
    : Stmt(NodeKind::MatchStmt, range), subject_(std::move(subject)), arms_(std::move(arms)) {
}

const Expr& MatchStmt::subject() const {
  return *subject_;
}

Expr& MatchStmt::subject() {
  return *subject_;
}

void MatchStmt::setSubject(std::unique_ptr<Expr> subject) {
  subject_ = std::move(subject);
}

const std::vector<MatchArm>& MatchStmt::arms() const {
  return arms_;
}

std::vector<MatchArm>& MatchStmt::arms() {
  return arms_;
}

DelStmt::DelStmt(SourceRange range, std::unique_ptr<Expr> target)
    : Stmt(NodeKind::DelStmt, range), target_(std::move(target)) {
}

const Expr& DelStmt::target() const {
  return *target_;
}

Expr& DelStmt::target() {
  return *target_;
}

DeferStmt::DeferStmt(SourceRange range, std::vector<std::unique_ptr<Stmt>> body)
    : Stmt(NodeKind::DeferStmt, range), body_(std::move(body)) {
}

const std::vector<std::unique_ptr<Stmt>>& DeferStmt::body() const {
  return body_;
}

std::vector<std::unique_ptr<Stmt>>& DeferStmt::body() {
  return body_;
}

WithStmt::WithStmt(SourceRange range,
                   std::unique_ptr<Expr> context,
                   std::string name,
                   std::vector<std::unique_ptr<Stmt>> body)
    : Stmt(NodeKind::WithStmt, range), context_(std::move(context)), name_(std::move(name)),
      body_(std::move(body)) {
}

const Expr& WithStmt::context() const {
  return *context_;
}

Expr& WithStmt::context() {
  return *context_;
}

const std::string& WithStmt::name() const {
  return name_;
}

const std::vector<std::unique_ptr<Stmt>>& WithStmt::body() const {
  return body_;
}

std::vector<std::unique_ptr<Stmt>>& WithStmt::body() {
  return body_;
}

EnumDef::EnumDef(SourceRange range,
                 std::string name,
                 std::vector<std::string> typeParams,
                 std::vector<EnumVariant> variants)
    : Stmt(NodeKind::EnumDef, range), name_(std::move(name)), typeParams_(std::move(typeParams)),
      variants_(std::move(variants)) {
}

const std::string& EnumDef::name() const {
  return name_;
}

void EnumDef::setTypeConstraints(std::vector<std::unique_ptr<TypeExpr>> constraints) {
  typeConstraints_ = std::move(constraints);
}

const std::vector<std::unique_ptr<TypeExpr>>& EnumDef::typeConstraints() const {
  return typeConstraints_;
}

const std::vector<std::string>& EnumDef::typeParams() const {
  return typeParams_;
}

const std::vector<EnumVariant>& EnumDef::variants() const {
  return variants_;
}

std::vector<EnumVariant>& EnumDef::variants() {
  return variants_;
}

const std::vector<std::unique_ptr<FunctionDef>>& EnumDef::methods() const {
  return methods_;
}

std::vector<std::unique_ptr<FunctionDef>>& EnumDef::methods() {
  return methods_;
}

void EnumDef::setFlags(bool value) {
  isFlags_ = value;
}

bool EnumDef::isFlags() const {
  return isFlags_;
}

void EnumDef::setDecorators(std::vector<std::string> decorators) {
  decorators_ = std::move(decorators);
}

const std::vector<std::string>& EnumDef::decorators() const {
  return decorators_;
}

void EnumDef::setDecoratorExprs(std::vector<std::unique_ptr<Expr>> exprs) {
  decoratorExprs_ = std::move(exprs);
}

const std::vector<std::unique_ptr<Expr>>& EnumDef::decoratorExprs() const {
  return decoratorExprs_;
}

std::vector<std::unique_ptr<Expr>>& EnumDef::decoratorExprs() {
  return decoratorExprs_;
}

VarDecl::VarDecl(SourceRange range,
                 std::string name,
                 std::unique_ptr<TypeExpr> type,
                 std::unique_ptr<Expr> init,
                 bool isStatic,
                 bool isConst)
    : Stmt(NodeKind::VarDecl, range), name_(std::move(name)), type_(std::move(type)),
      init_(std::move(init)), isStatic_(isStatic), isConst_(isConst) {
}

const std::string& VarDecl::name() const {
  return name_;
}

void VarDecl::setName(std::string name) {
  name_ = std::move(name);
}

bool VarDecl::hasType() const {
  return type_ != nullptr;
}

const TypeExpr& VarDecl::type() const {
  return *type_;
}

TypeExpr& VarDecl::type() {
  return *type_;
}

const Expr* VarDecl::init() const {
  return init_.get();
}

bool VarDecl::isStatic() const {
  return isStatic_;
}

bool VarDecl::isConst() const {
  return isConst_;
}

void VarDecl::setConst(bool value) {
  isConst_ = value;
}

AssignStmt::AssignStmt(SourceRange range,
                       std::unique_ptr<Expr> target,
                       std::unique_ptr<Expr> value,
                       AssignOp op)
    : Stmt(NodeKind::AssignStmt, range), target_(std::move(target)), value_(std::move(value)),
      op_(op) {
}

const Expr& AssignStmt::target() const {
  return *target_;
}

const Expr& AssignStmt::value() const {
  return *value_;
}

AssignOp AssignStmt::op() const {
  return op_;
}

void AssignStmt::setNameAlias(bool value) {
  isNameAlias_ = value;
}

bool AssignStmt::isNameAlias() const {
  return isNameAlias_;
}

ReturnStmt::ReturnStmt(SourceRange range, std::unique_ptr<Expr> value)
    : ReturnStmt(NodeKind::ReturnStmt, range, std::move(value)) {
}

ReturnStmt::ReturnStmt(NodeKind kind, SourceRange range, std::unique_ptr<Expr> value)
    : Stmt(kind, range), value_(std::move(value)) {
}

YieldStmt::YieldStmt(SourceRange range, std::unique_ptr<Expr> value)
    : ReturnStmt(NodeKind::YieldStmt, range, std::move(value)) {
}

const Expr* ReturnStmt::value() const {
  return value_.get();
}

ExprStmt::ExprStmt(SourceRange range, std::unique_ptr<Expr> expression)
    : Stmt(NodeKind::ExprStmt, range), expression_(std::move(expression)) {
}

const Expr& ExprStmt::expression() const {
  return *expression_;
}

PassStmt::PassStmt(SourceRange range) : Stmt(NodeKind::PassStmt, range) {
}

BreakStmt::BreakStmt(SourceRange range) : Stmt(NodeKind::BreakStmt, range) {
}

ContinueStmt::ContinueStmt(SourceRange range) : Stmt(NodeKind::ContinueStmt, range) {
}

FunctionDef::FunctionDef(SourceRange range,
                         std::string name,
                         std::vector<ParamDecl> params,
                         std::unique_ptr<TypeExpr> returnType,
                         std::vector<std::unique_ptr<Stmt>> body,
                         std::string externName)
    : Stmt(NodeKind::FunctionDef, range), name_(std::move(name)), params_(std::move(params)),
      returnType_(std::move(returnType)), body_(std::move(body)),
      externName_(std::move(externName)) {
}

const std::string& FunctionDef::name() const {
  return name_;
}

const std::vector<ParamDecl>& FunctionDef::params() const {
  return params_;
}

std::vector<ParamDecl>& FunctionDef::params() {
  return params_;
}

const TypeExpr& FunctionDef::returnType() const {
  return *returnType_;
}

TypeExpr& FunctionDef::returnType() {
  return *returnType_;
}

const std::vector<std::unique_ptr<Stmt>>& FunctionDef::body() const {
  return body_;
}

std::vector<std::unique_ptr<Stmt>>& FunctionDef::body() {
  return body_;
}

const std::string& FunctionDef::externName() const {
  return externName_;
}

bool FunctionDef::isExtern() const {
  return !externName_.empty();
}

const std::string& FunctionDef::ownerClass() const {
  return ownerClass_;
}

bool FunctionDef::isMethod() const {
  return !ownerClass_.empty();
}

void FunctionDef::setOwnerClass(std::string ownerClass) {
  ownerClass_ = std::move(ownerClass);
}

bool FunctionDef::isAbstract() const {
  return isAbstract_;
}

bool FunctionDef::isOverride() const {
  return isOverride_;
}

void FunctionDef::setAbstract(bool value) {
  isAbstract_ = value;
}

void FunctionDef::setOverride(bool value) {
  isOverride_ = value;
}

bool containsYield(const std::vector<std::unique_ptr<Stmt>>& body) {
  for (const auto& stmt : body) {
    if (stmt == nullptr)
      continue;
    switch (stmt->kind()) {
    case NodeKind::YieldStmt:
      return true;
    case NodeKind::IfStmt:
      for (const auto& branch : static_cast<const IfStmt&>(*stmt).branches())
        if (containsYield(branch.body))
          return true;
      break;
    case NodeKind::ForStmt:
      if (containsYield(static_cast<const ForStmt&>(*stmt).body()))
        return true;
      break;
    case NodeKind::WhileStmt:
      if (containsYield(static_cast<const WhileStmt&>(*stmt).body()))
        return true;
      break;
    case NodeKind::WithStmt:
      if (containsYield(static_cast<const WithStmt&>(*stmt).body()))
        return true;
      break;
    case NodeKind::DeferStmt:
      if (containsYield(static_cast<const DeferStmt&>(*stmt).body()))
        return true;
      break;
    case NodeKind::TryStmt: {
      const auto& t = static_cast<const TryStmt&>(*stmt);
      if (containsYield(t.body()) || containsYield(t.elseBody()) || containsYield(t.finallyBody()))
        return true;
      for (const auto& handler : t.handlers())
        if (containsYield(handler.body))
          return true;
      break;
    }
    case NodeKind::MatchStmt:
      for (const auto& arm : static_cast<const MatchStmt&>(*stmt).arms())
        if (containsYield(arm.body))
          return true;
      break;
    default:
      break; // Nested functions have their own generator status.
    }
  }
  return false;
}

bool FunctionDef::isGenerator() const {
  return containsYield(body());
}

bool FunctionDef::isAsync() const {
  return isAsync_;
}

void FunctionDef::setAsync(bool value) {
  isAsync_ = value;
}

void FunctionDef::setDecorators(std::vector<std::string> decorators) {
  decorators_ = std::move(decorators);
}

const std::vector<std::string>& FunctionDef::decorators() const {
  return decorators_;
}

void FunctionDef::setDecoratorExprs(std::vector<std::unique_ptr<Expr>> exprs) {
  decoratorExprs_ = std::move(exprs);
}

const std::vector<std::unique_ptr<Expr>>& FunctionDef::decoratorExprs() const {
  return decoratorExprs_;
}

std::vector<std::unique_ptr<Expr>>& FunctionDef::decoratorExprs() {
  return decoratorExprs_;
}

void FunctionDef::setDecoratedType(const Type* type) {
  decoratedType_ = type;
}

const Type* FunctionDef::decoratedType() const {
  return decoratedType_;
}

void FunctionDef::setModulePrefix(std::string prefix) {
  modulePrefix_ = std::move(prefix);
}

const std::string& FunctionDef::modulePrefix() const {
  return modulePrefix_;
}

void FunctionDef::setTypeParams(std::vector<std::string> typeParams) {
  typeParams_ = std::move(typeParams);
}

void FunctionDef::setTypeConstraints(std::vector<std::unique_ptr<TypeExpr>> constraints) {
  typeConstraints_ = std::move(constraints);
}

const std::vector<std::unique_ptr<TypeExpr>>& FunctionDef::typeConstraints() const {
  return typeConstraints_;
}

const std::vector<std::string>& FunctionDef::typeParams() const {
  return typeParams_;
}

void FunctionDef::setInferredReturn(bool value) {
  inferredReturn_ = value;
}

bool FunctionDef::hasInferredReturn() const {
  return inferredReturn_;
}

void FunctionDef::setProperty(PropertyKind kind, std::string name) {
  propertyKind_ = kind;
  propertyName_ = std::move(name);
}

PropertyKind FunctionDef::propertyKind() const {
  return propertyKind_;
}

const std::string& FunctionDef::propertyName() const {
  return propertyName_;
}

void FunctionDef::addCapture(std::string name, const Type* type) {
  for (const Capture& capture : captures_) {
    if (capture.name == name) {
      return;
    }
  }
  captures_.push_back({std::move(name), type});
}

const std::vector<FunctionDef::Capture>& FunctionDef::captures() const {
  return captures_;
}

ClassDef::ClassDef(SourceRange range,
                   std::string name,
                   std::vector<FieldDecl> fields,
                   std::vector<std::unique_ptr<FunctionDef>> methods,
                   std::vector<std::string> bases,
                   std::vector<std::string> typeParams)
    : Stmt(NodeKind::ClassDef, range), name_(std::move(name)), fields_(std::move(fields)),
      methods_(std::move(methods)), bases_(std::move(bases)), typeParams_(std::move(typeParams)) {
}

const std::string& ClassDef::name() const {
  return name_;
}

const std::vector<FieldDecl>& ClassDef::fields() const {
  return fields_;
}

const std::vector<std::unique_ptr<FunctionDef>>& ClassDef::methods() const {
  return methods_;
}

std::vector<std::unique_ptr<FunctionDef>>& ClassDef::methods() {
  return methods_;
}

const std::vector<std::string>& ClassDef::bases() const {
  return bases_;
}

const std::vector<std::unique_ptr<TypeExpr>>& ClassDef::baseTypes() const {
  return baseTypes_;
}

std::vector<std::unique_ptr<TypeExpr>>& ClassDef::baseTypes() {
  return baseTypes_;
}

void ClassDef::setBaseTypes(std::vector<std::unique_ptr<TypeExpr>> types) {
  baseTypes_ = std::move(types);
}

void ClassDef::setTypeConstraints(std::vector<std::unique_ptr<TypeExpr>> constraints) {
  typeConstraints_ = std::move(constraints);
}

const std::vector<std::unique_ptr<TypeExpr>>& ClassDef::typeConstraints() const {
  return typeConstraints_;
}

const std::vector<std::string>& ClassDef::typeParams() const {
  return typeParams_;
}

void ClassDef::setDecorators(std::vector<std::string> decorators) {
  decorators_ = std::move(decorators);
}

const std::vector<std::string>& ClassDef::decorators() const {
  return decorators_;
}

void ClassDef::setDecoratorExprs(std::vector<std::unique_ptr<Expr>> exprs) {
  decoratorExprs_ = std::move(exprs);
}

const std::vector<std::unique_ptr<Expr>>& ClassDef::decoratorExprs() const {
  return decoratorExprs_;
}

std::vector<std::unique_ptr<Expr>>& ClassDef::decoratorExprs() {
  return decoratorExprs_;
}

void ClassDef::setDecoratedType(const Type* type) {
  decoratedType_ = type;
}

const Type* ClassDef::decoratedType() const {
  return decoratedType_;
}

void ClassDef::setStruct(bool value) {
  isStruct_ = value;
}

bool ClassDef::isStruct() const {
  return isStruct_;
}

void ClassDef::setFrozen(bool value) {
  isFrozen_ = value;
}

bool ClassDef::isFrozen() const {
  return isFrozen_;
}

ImportStmt::ImportStmt(SourceRange range,
                       std::vector<std::string> modulePath,
                       std::string alias,
                       std::vector<std::string> names,
                       bool star,
                       std::vector<std::string> nameAliases)
    : Stmt(NodeKind::ImportStmt, range), modulePath_(std::move(modulePath)),
      alias_(std::move(alias)), names_(std::move(names)), nameAliases_(std::move(nameAliases)),
      star_(star) {
}

const std::vector<std::string>& ImportStmt::modulePath() const {
  return modulePath_;
}

const std::string& ImportStmt::alias() const {
  return alias_;
}

const std::vector<std::string>& ImportStmt::names() const {
  return names_;
}

const std::vector<std::string>& ImportStmt::nameAliases() const {
  return nameAliases_;
}

std::string ImportStmt::boundName(std::size_t index) const {
  if (index < nameAliases_.size() && !nameAliases_[index].empty()) {
    return nameAliases_[index];
  }
  if (index < names_.size()) {
    return names_[index];
  }
  return {};
}

bool ImportStmt::star() const {
  return star_;
}

bool ImportStmt::isFrom() const {
  return star_ || !names_.empty();
}

TypeAlias::TypeAlias(SourceRange range,
                     std::string name,
                     std::unique_ptr<TypeExpr> type,
                     std::vector<std::string> typeParams,
                     std::vector<std::unique_ptr<TypeExpr>> typeConstraints)
    : Stmt(NodeKind::TypeAlias, range),
      name_(std::move(name)),
      type_(std::move(type)),
      typeParams_(std::move(typeParams)),
      typeConstraints_(std::move(typeConstraints)) {
}

const std::string& TypeAlias::name() const {
  return name_;
}

const TypeExpr& TypeAlias::type() const {
  return *type_;
}

TypeExpr& TypeAlias::type() {
  return *type_;
}

const std::vector<std::string>& TypeAlias::typeParams() const {
  return typeParams_;
}

const std::vector<std::unique_ptr<TypeExpr>>& TypeAlias::typeConstraints() const {
  return typeConstraints_;
}

Module::Module(SourceRange range, std::vector<std::unique_ptr<Stmt>> statements)
    : Node(NodeKind::Module, range), statements_(std::move(statements)) {
}

const std::vector<std::unique_ptr<Stmt>>& Module::statements() const {
  return statements_;
}

std::vector<std::unique_ptr<Stmt>>& Module::statements() {
  return statements_;
}

void Module::insertFront(std::vector<std::unique_ptr<Stmt>> extra) {
  for (std::unique_ptr<Stmt>& statement : extra) {
    statement->setFromPrelude(true);
  }
  extra.insert(extra.end(),
               std::make_move_iterator(statements_.begin()),
               std::make_move_iterator(statements_.end()));
  statements_ = std::move(extra);
}

void Module::setExportList(ModuleExportMode mode, std::vector<std::string> names) {
  exportMode_ = mode;
  exportNames_ = std::move(names);
}

ModuleExportMode Module::exportMode() const {
  return exportMode_;
}

const std::vector<std::string>& Module::exportNames() const {
  return exportNames_;
}

void Module::addExportAlias(RecordField field) {
  exportAliases_.push_back(std::move(field));
}

const std::vector<RecordField>& Module::exportAliases() const {
  return exportAliases_;
}

SpliceExpr::SpliceExpr(SourceRange range, std::string name, bool repeat, bool commaSeparated)
    : Expr(NodeKind::SpliceExpr, range), name_(std::move(name)), repeat_(repeat),
      commaSeparated_(commaSeparated) {
}

const std::string& SpliceExpr::name() const {
  return name_;
}

bool SpliceExpr::isRepeat() const {
  return repeat_;
}

bool SpliceExpr::commaSeparated() const {
  return commaSeparated_;
}

void SpliceExpr::setRepeat(bool repeat, bool commaSeparated) {
  repeat_ = repeat;
  commaSeparated_ = commaSeparated;
}

MacroDef::MacroDef(SourceRange range,
                   std::string name,
                   std::vector<std::string> params,
                   SourceRange nameRange)
    : Stmt(NodeKind::MacroDef, range), name_(std::move(name)), nameRange_(nameRange),
      params_(std::move(params)) {
}

const std::string& MacroDef::name() const {
  return name_;
}

SourceRange MacroDef::nameRange() const {
  return nameRange_;
}

const std::vector<std::string>& MacroDef::params() const {
  return params_;
}

bool MacroDef::variadic() const {
  return variadic_;
}

void MacroDef::setVariadic(bool value) {
  variadic_ = value;
}

MacroSyntaxMode MacroDef::syntaxMode() const {
  return syntaxMode_;
}

void MacroDef::setSyntaxMode(MacroSyntaxMode mode) {
  syntaxMode_ = mode;
}

MacroInterpolate MacroDef::interpolate() const {
  return interpolate_;
}

void MacroDef::setInterpolate(MacroInterpolate mode) {
  interpolate_ = mode;
}

bool MacroDef::typed() const {
  return typed_;
}

void MacroDef::setTyped(bool value) {
  typed_ = value;
}

const std::string& MacroDef::wrapper() const {
  return wrapper_;
}

void MacroDef::setWrapper(std::string wrapper) {
  wrapper_ = std::move(wrapper);
}

const std::vector<MacroMatchArm>& MacroDef::matchArms() const {
  return matchArms_;
}

std::vector<MacroMatchArm>& MacroDef::matchArms() {
  return matchArms_;
}

const std::vector<std::unique_ptr<Stmt>>& MacroDef::quoteBody() const {
  return quoteBody_;
}

std::vector<std::unique_ptr<Stmt>>& MacroDef::quoteBody() {
  return quoteBody_;
}

MacroInvokeExpr::MacroInvokeExpr(SourceRange range,
                                 std::string name,
                                 MacroDelimiter delimiter,
                                 std::string rawText,
                                 SourceRange rawRange,
                                 std::vector<Token> tokens)
    : Expr(NodeKind::MacroInvokeExpr, range), name_(std::move(name)), delimiter_(delimiter),
      rawText_(std::move(rawText)), rawRange_(rawRange), tokens_(std::move(tokens)) {
}

const std::string& MacroInvokeExpr::name() const {
  return name_;
}

MacroDelimiter MacroInvokeExpr::delimiter() const {
  return delimiter_;
}

const std::string& MacroInvokeExpr::rawText() const {
  return rawText_;
}

SourceRange MacroInvokeExpr::rawRange() const {
  return rawRange_;
}

const std::vector<Token>& MacroInvokeExpr::tokens() const {
  return tokens_;
}

MacroInvokeStmt::MacroInvokeStmt(SourceRange range,
                                 std::string name,
                                 MacroDelimiter delimiter,
                                 std::string rawText,
                                 SourceRange rawRange,
                                 std::vector<Token> tokens)
    : Stmt(NodeKind::MacroInvokeStmt, range), name_(std::move(name)), delimiter_(delimiter),
      rawText_(std::move(rawText)), rawRange_(rawRange), tokens_(std::move(tokens)) {
}

const std::string& MacroInvokeStmt::name() const {
  return name_;
}

MacroDelimiter MacroInvokeStmt::delimiter() const {
  return delimiter_;
}

const std::string& MacroInvokeStmt::rawText() const {
  return rawText_;
}

SourceRange MacroInvokeStmt::rawRange() const {
  return rawRange_;
}

const std::vector<Token>& MacroInvokeStmt::tokens() const {
  return tokens_;
}

} // namespace sere
