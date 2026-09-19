/// @file SeremGenerator.cpp
/// Typed Sere AST to Serem lowering for the initial supported language slice.

#include "sere/codegen/SeremGenerator.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/types/Type.h"
#include "sere/types/TypeContext.h"

#include <utility>

namespace sere {
namespace {

[[nodiscard]] serem::IRType integerType(const Type* type) {
  if (type->isNamed("i8")) return serem::IRType::i8();
  if (type->isNamed("i16")) return serem::IRType::i16();
  if (type->isNamed("i32")) return serem::IRType::i32();
  if (type->isNamed("u8")) return serem::IRType::u8();
  if (type->isNamed("u16")) return serem::IRType::u16();
  if (type->isNamed("u32")) return serem::IRType::u32();
  if (type->isNamed("u64")) return serem::IRType::u64();
  return serem::IRType::i64();
}

[[nodiscard]] const Type* functionType(const FunctionDef& function) {
  return function.resolvedType();
}

} // namespace

SeremGenerator::SeremGenerator(DiagnosticEngine& diagnostics, TypeContext& types)
    : diagnostics_(&diagnostics) {
  (void)types;
}

serem::IRType SeremGenerator::lowerType(const Type* type) const {
  if (type == nullptr || type->isVoidLike()) return serem::IRType::voidType();
  type = type->canonical();
  if (type->isNamed("bool")) return serem::IRType::boolType();
  if (type->isInteger()) return integerType(type);
  if (type->isNamed("f32")) return serem::IRType::f32();
  if (type->isNamed("f64")) return serem::IRType::f64();
  if (type->isNamed("str")) return serem::IRType::stringType();
  if (type->kind() == TypeKind::Function) {
    std::vector<serem::IRType> params;
    for (const Type* param : type->paramTypes()) params.push_back(lowerType(param));
    return serem::IRType::function(lowerType(type->returnType()), std::move(params));
  }
  if (type->isRecord() && !type->isEnum()) {
    std::vector<serem::IRType> fields;
    for (const RecordField& field : type->fields()) {
      if (!field.isStatic && field.stored) fields.push_back(lowerType(field.type));
    }
    return serem::IRType::structType(type->name(), std::move(fields));
  }
  if (type->isUnion()) return serem::IRType::structType("union", {});
  return serem::IRType::ptr(serem::IRType::i8());
}

std::string SeremGenerator::functionName(const FunctionDef& function) const {
  if (!function.modulePrefix().empty()) return function.modulePrefix() + "." + function.name();
  if (function.isMethod() && !function.ownerClass().empty()) {
    return function.ownerClass() + "." + function.name();
  }
  return function.name();
}

std::unique_ptr<serem::IRModule> SeremGenerator::emit(const Module& module,
                                                       std::string moduleName) {
  module_ = std::make_unique<serem::IRModule>(std::move(moduleName));
  functions_.clear();
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement != nullptr && statement->kind() == NodeKind::FunctionDef) {
      const auto& function = static_cast<const FunctionDef&>(*statement);
      const Type* type = functionType(function);
      if (type != nullptr) functions_.insert_or_assign(functionName(function), lowerType(type));
    }
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr || statement->kind() != NodeKind::FunctionDef) continue;
    if (!emitFunction(static_cast<const FunctionDef&>(*statement))) return nullptr;
  }
  return std::move(module_);
}

bool SeremGenerator::emitFunction(const FunctionDef& function) {
  const Type* type = functionType(function);
  if (type == nullptr) return unsupported(function, "function without a resolved type");
  std::vector<serem::IRType> params;
  for (const Type* param : type->paramTypes()) params.push_back(lowerType(param));
  auto irFunction = std::make_unique<serem::IRFunction>(functionName(function), std::move(params),
                                                        lowerType(type->returnType()));
  function_ = &module_->addFunction(std::move(irFunction));
  builder_ = std::make_unique<serem::IRBuilder>(*function_);
  locals_.clear();
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    if (index < function_->arguments().size()) {
      locals_[function.params()[index].name] = function_->argument(index);
    }
  }
  for (const std::unique_ptr<Stmt>& statement : function.body()) {
    if (statement != nullptr && !emitStatement(*statement)) return false;
  }
  if (!builder_->currentBlock().isTerminated()) {
    if (type->returnType() != nullptr && type->returnType()->isVoidLike()) {
      builder_->retVoid();
    } else {
      (void)builder_->unreachable();
    }
  }
  builder_.reset();
  function_ = nullptr;
  locals_.clear();
  return true;
}

bool SeremGenerator::emitStatement(const Stmt& statement) {
  switch (statement.kind()) {
  case NodeKind::VarDecl: {
    const auto& declaration = static_cast<const VarDecl&>(statement);
    const Type* type = declaration.resolvedType();
    const serem::IRType irType = lowerType(type);
    auto slot = builder_->alloca(irType);
    locals_[declaration.name()] = slot;
    if (declaration.init() != nullptr) builder_->store(emitExpression(*declaration.init()), slot);
    return true;
  }
  case NodeKind::AssignStmt: {
    const auto& assign = static_cast<const AssignStmt&>(statement);
    if (assign.target().kind() != NodeKind::NameExpr) {
      return unsupported(statement, "non-name assignment target");
    }
    const auto& name = static_cast<const NameExpr&>(assign.target());
    serem::ValuePtr slot = local(name.name());
    if (slot == nullptr) return unsupported(statement, "assignment to unknown local");
    builder_->store(emitExpression(assign.value()), slot);
    return true;
  }
  case NodeKind::ReturnStmt: {
    const auto& result = static_cast<const ReturnStmt&>(statement);
    if (result.value() == nullptr) builder_->retVoid();
    else builder_->ret(emitExpression(*result.value()));
    return true;
  }
  case NodeKind::ExprStmt:
    (void)emitExpression(static_cast<const ExprStmt&>(statement).expression());
    return true;
  case NodeKind::PassStmt:
    return true;
  default:
    return unsupported(statement, "statement kind");
  }
}

serem::ValuePtr SeremGenerator::emitExpression(const Expr& expression) {
  switch (expression.kind()) {
  case NodeKind::IntegerLiteral: {
    const auto& literal = static_cast<const IntegerLiteral&>(expression);
    return std::make_shared<serem::ConstantInt>(literal.value(), lowerType(expression.resolvedType()));
  }
  case NodeKind::FloatLiteral: {
    const auto& literal = static_cast<const FloatLiteral&>(expression);
    return std::make_shared<serem::ConstantFloat>(literal.value(), lowerType(expression.resolvedType()));
  }
  case NodeKind::StringLiteral:
    return std::make_shared<serem::ConstantString>(static_cast<const StringLiteral&>(expression).value());
  case NodeKind::BooleanLiteral:
    return std::make_shared<serem::ConstantInt>(static_cast<const BooleanLiteral&>(expression).value() ? 1 : 0,
                                                serem::IRType::boolType());
  case NodeKind::NoneLiteral:
    return std::make_shared<serem::ConstantInt>(0, serem::IRType::i8());
  case NodeKind::NameExpr:
    return emitName(static_cast<const NameExpr&>(expression));
  case NodeKind::BinaryExpr:
    return emitBinary(static_cast<const BinaryExpr&>(expression));
  case NodeKind::UnaryExpr:
    return emitUnary(static_cast<const UnaryExpr&>(expression));
  case NodeKind::CallExpr:
    return emitCall(static_cast<const CallExpr&>(expression));
  case NodeKind::CastExpr: {
    const auto& cast = static_cast<const CastExpr&>(expression);
    return builder_->cast("value", emitExpression(cast.value()), lowerType(expression.resolvedType()));
  }
  default:
    (void)unsupported(expression, "expression kind");
    return builder_->operation("sere.invalid", lowerType(expression.resolvedType()));
  }
}

serem::ValuePtr SeremGenerator::emitName(const NameExpr& expression) {
  if (serem::ValuePtr value = local(expression.name())) return builder_->load(value, lowerType(expression.resolvedType()));
  const auto found = functions_.find(expression.name());
  if (found != functions_.end()) return std::make_shared<serem::FunctionRef>(expression.name(), found->second);
  (void)unsupported(expression, "unknown name");
  return builder_->operation("sere.invalid", lowerType(expression.resolvedType()));
}

serem::ValuePtr SeremGenerator::emitBinary(const BinaryExpr& expression) {
  serem::ValuePtr left = emitExpression(expression.left());
  serem::ValuePtr right = emitExpression(expression.right());
  const serem::IRType type = lowerType(expression.resolvedType());
  switch (expression.op()) {
  case BinaryOp::Add: return builder_->add(left, right, type);
  case BinaryOp::Sub: return builder_->sub(left, right, type);
  case BinaryOp::Mul: return builder_->mul(left, right, type);
  case BinaryOp::Div:
  case BinaryOp::FloorDiv: return builder_->div(left, right, type);
  case BinaryOp::Mod: return builder_->rem(left, right, type);
  case BinaryOp::BitAnd: return builder_->bitAnd(left, right, type);
  case BinaryOp::BitOr: return builder_->bitOr(left, right, type);
  case BinaryOp::BitXor: return builder_->bitXor(left, right, type);
  case BinaryOp::Shl: return builder_->shiftLeft(left, right, type);
  case BinaryOp::Shr: return builder_->shiftRight(left, right, type);
  case BinaryOp::Eq: return builder_->compare("eq", left, right);
  case BinaryOp::Ne: return builder_->compare("ne", left, right);
  case BinaryOp::Lt: return builder_->compare("lt", left, right);
  case BinaryOp::Le: return builder_->compare("le", left, right);
  case BinaryOp::Gt: return builder_->compare("gt", left, right);
  case BinaryOp::Ge: return builder_->compare("ge", left, right);
  default:
    (void)unsupported(expression, "binary operator");
    return builder_->operation("sere.invalid", type);
  }
}

serem::ValuePtr SeremGenerator::emitCall(const CallExpr& expression) {
  serem::ValuePtr callee = emitExpression(expression.callee());
  std::vector<serem::ValuePtr> args;
  for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
    args.push_back(emitExpression(*argument));
  }
  return builder_->call(std::move(callee), std::move(args), lowerType(expression.resolvedType()));
}

serem::ValuePtr SeremGenerator::emitUnary(const UnaryExpr& expression) {
  const serem::ValuePtr operand = emitExpression(expression.operand());
  const serem::IRType type = lowerType(expression.resolvedType());
  switch (expression.op()) {
  case UnaryOp::Neg: return builder_->operation("neg", type, {operand});
  case UnaryOp::Pos: return operand;
  case UnaryOp::Not: return builder_->operation("not", type, {operand});
  case UnaryOp::Invert: return builder_->operation("invert", type, {operand});
  default:
    (void)unsupported(expression, "unary operator");
    return builder_->operation("sere.invalid", type);
  }
}

bool SeremGenerator::bindLocal(const std::string& name, serem::ValuePtr value) {
  locals_[name] = std::move(value);
  return true;
}

serem::ValuePtr SeremGenerator::local(const std::string& name) const {
  const auto found = locals_.find(name);
  return found == locals_.end() ? nullptr : found->second;
}

bool SeremGenerator::unsupported(const Node& node, std::string_view feature) {
  diagnostics_->error(node.range(), "Serem lowering does not support " + std::string(feature));
  return false;
}

} // namespace sere
