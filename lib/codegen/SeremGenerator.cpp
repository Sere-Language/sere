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
  declareTypes(module);
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement != nullptr && statement->kind() == NodeKind::FunctionDef) {
      const auto& function = static_cast<const FunctionDef&>(*statement);
      const Type* type = functionType(function);
      if (type != nullptr) functions_.insert_or_assign(functionName(function), lowerType(type));
    } else if (statement != nullptr && statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        const Type* type = functionType(*method);
        if (type != nullptr) functions_.insert_or_assign(functionName(*method), lowerType(type));
      }
    }
  }
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr) continue;
    if (statement->kind() == NodeKind::FunctionDef &&
        !emitFunction(static_cast<const FunctionDef&>(*statement))) {
      return nullptr;
    }
    if (statement->kind() == NodeKind::ClassDef) {
      for (const std::unique_ptr<FunctionDef>& method :
           static_cast<const ClassDef&>(*statement).methods()) {
        if (!emitFunction(*method)) return nullptr;
      }
    }
  }
  return std::move(module_);
}

void SeremGenerator::declareTypes(const Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement == nullptr) continue;
    if (statement->kind() == NodeKind::ClassDef) {
      declareClass(static_cast<const ClassDef&>(*statement));
    } else if (statement->kind() == NodeKind::EnumDef) {
      declareEnum(static_cast<const EnumDef&>(*statement));
    }
  }
}

void SeremGenerator::declareClass(const ClassDef& classDef) {
  std::vector<serem::IRType> fields;
  for (const FieldDecl& field : classDef.fields()) {
    if (!field.isStatic) fields.push_back(lowerType(field.type == nullptr ? nullptr
                                                                  : field.type->resolvedType()));
  }
  std::vector<std::string> attributes;
  attributes.push_back(classDef.isStruct() ? "struct" : "class");
  for (const std::string& decorator : classDef.decorators()) attributes.push_back("decorator=" + decorator);
  (void)module_->addType(std::make_unique<serem::TypeDef>(
      classDef.name(), serem::IRType::structType(classDef.name(), std::move(fields)),
      std::move(attributes)));
}

void SeremGenerator::declareEnum(const EnumDef& enumDef) {
  std::vector<std::string> attributes;
  attributes.push_back("enum");
  if (enumDef.isFlags()) attributes.push_back("flags");
  for (const EnumVariant& variant : enumDef.variants()) attributes.push_back("variant=" + variant.name);
  (void)module_->addType(std::make_unique<serem::TypeDef>(
      enumDef.name(), serem::IRType::structType(enumDef.name(), {}), std::move(attributes)));
}

bool SeremGenerator::emitFunction(const FunctionDef& function) {
  const Type* type = functionType(function);
  if (type == nullptr) return unsupported(function, "function without a resolved type");
  std::vector<serem::IRType> params;
  for (const Type* param : type->paramTypes()) params.push_back(lowerType(param));
  auto irFunction = std::make_unique<serem::IRFunction>(functionName(function), std::move(params),
                                                        lowerType(type->returnType()));
  function_ = &module_->addFunction(std::move(irFunction));
  function_->setAsync(function.isAsync());
  function_->setGenerator(function.isGenerator());
  for (const std::string& decorator : function.decorators()) {
    function_->setAttribute("decorator." + decorator, "true");
  }
  builder_ = std::make_unique<serem::IRBuilder>(*function_);
  locals_.clear();
  coroutineToken_.reset();
  if (function.isAsync() || function.isGenerator()) coroutineToken_ = builder_->coroBegin();
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    if (index < function_->arguments().size()) {
      locals_[function.params()[index].name] = function_->argument(index);
    }
  }
  if (!emitBlock(function.body())) return false;
  if (!builder_->currentBlock().isTerminated()) {
    if (type->returnType() != nullptr && type->returnType()->isVoidLike()) {
      builder_->retVoid();
    } else {
      (void)builder_->unreachable();
    }
  }
  if (coroutineToken_ != nullptr) builder_->coroEnd();
  builder_.reset();
  function_ = nullptr;
  locals_.clear();
  return true;
}

bool SeremGenerator::emitBlock(const std::vector<std::unique_ptr<Stmt>>& statements) {
  for (const std::unique_ptr<Stmt>& statement : statements) {
    if (statement != nullptr && !emitStatement(*statement)) return false;
  }
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
    serem::ValuePtr value = emitExpression(assign.value());
    if (assign.op() != AssignOp::Assign) {
      const serem::ValuePtr current = builder_->load(slot, lowerType(assign.target().resolvedType()));
      BinaryOp binaryOp = BinaryOp::Add;
      if (binaryOpForAssign(assign.op(), binaryOp)) {
        switch (binaryOp) {
        case BinaryOp::Add: value = builder_->add(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::Sub: value = builder_->sub(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::Mul: value = builder_->mul(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::Div: value = builder_->div(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::Mod: value = builder_->rem(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::BitAnd: value = builder_->bitAnd(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::BitOr: value = builder_->bitOr(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::BitXor: value = builder_->bitXor(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::Shl: value = builder_->shiftLeft(current, value, lowerType(assign.target().resolvedType())); break;
        case BinaryOp::Shr: value = builder_->shiftRight(current, value, lowerType(assign.target().resolvedType())); break;
        default: break;
        }
      }
    }
    builder_->store(std::move(value), slot);
    return true;
  }
  case NodeKind::IfStmt:
    return emitIf(static_cast<const IfStmt&>(statement));
  case NodeKind::WhileStmt:
    return emitWhile(static_cast<const WhileStmt&>(statement));
  case NodeKind::ForStmt:
    return emitFor(static_cast<const ForStmt&>(statement));
  case NodeKind::BreakStmt:
    if (breakTargets_.empty()) return unsupported(statement, "break outside a loop");
    (void)builder_->branch(*breakTargets_.back());
    return true;
  case NodeKind::ContinueStmt:
    if (continueTargets_.empty()) return unsupported(statement, "continue outside a loop");
    (void)builder_->branch(*continueTargets_.back());
    return true;
  case NodeKind::YieldStmt: {
    const auto& yield = static_cast<const YieldStmt&>(statement);
    if (coroutineToken_ == nullptr) return unsupported(statement, "yield in a non-generator");
    if (yield.value() == nullptr) {
      (void)builder_->operation("yield", serem::IRType::voidType());
    } else {
      builder_->yield(emitExpression(*yield.value()));
    }
    builder_->coroSuspend(coroutineToken_);
    return true;
  }
  case NodeKind::RaiseStmt: {
    const auto& raise = static_cast<const RaiseStmt&>(statement);
    if (raise.value() == nullptr) {
      (void)builder_->operation("throw", serem::IRType::voidType());
    } else {
      builder_->throwValue(emitExpression(*raise.value()));
    }
    return true;
  }
  case NodeKind::AssertStmt: {
    const auto& assertion = static_cast<const AssertStmt&>(statement);
    (void)builder_->operation("assert", serem::IRType::voidType(),
                              {emitExpression(assertion.condition())});
    return true;
  }
  case NodeKind::WithStmt: {
    const auto& with = static_cast<const WithStmt&>(statement);
    const serem::ValuePtr context = emitExpression(with.context());
    (void)builder_->operation("with.enter", serem::IRType::voidType(), {context});
    if (!with.name().empty()) (void)bindLocal(with.name(), context);
    if (!emitBlock(with.body())) return false;
    (void)builder_->operation("with.exit", serem::IRType::voidType(), {context});
    return true;
  }
  case NodeKind::TryStmt: {
    const auto& tryStatement = static_cast<const TryStmt&>(statement);
    (void)builder_->operation("try.begin", serem::IRType::voidType());
    if (!emitBlock(tryStatement.body())) return false;
    for (const ExceptHandler& handler : tryStatement.handlers()) {
      (void)builder_->operation("catch", serem::IRType::voidType(), {},
                                {{"type", handler.type == nullptr ? "" : handler.type->name()}});
      if (!emitBlock(handler.body)) return false;
    }
    if (!emitBlock(tryStatement.elseBody()) || !emitBlock(tryStatement.finallyBody())) return false;
    (void)builder_->operation("try.end", serem::IRType::voidType());
    return true;
  }
  case NodeKind::DeferStmt: {
    const auto& defer = static_cast<const DeferStmt&>(statement);
    (void)builder_->operation("defer.begin", serem::IRType::voidType());
    if (!emitBlock(defer.body())) return false;
    (void)builder_->operation("defer.end", serem::IRType::voidType());
    return true;
  }
  case NodeKind::DelStmt:
    (void)builder_->operation("destroy", serem::IRType::voidType(),
                              {emitExpression(static_cast<const DelStmt&>(statement).target())});
    return true;
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

bool SeremGenerator::emitIf(const IfStmt& statement) {
  serem::BasicBlock* merge = &function_->addBlock("if.end");
  std::vector<serem::BasicBlock*> bodies;
  for (std::size_t index = 0; index < statement.branches().size(); ++index) {
    bodies.push_back(&function_->addBlock("if.body" + std::to_string(index)));
  }
  for (std::size_t index = 0; index < statement.branches().size(); ++index) {
    const IfBranch& branch = statement.branches()[index];
    serem::BasicBlock* next = index + 1 < bodies.size() ? bodies[index + 1] : merge;
    const serem::ValuePtr condition = emitExpression(*branch.condition);
    (void)builder_->conditionalBranch(condition, *bodies[index], *next);
    builder_->setInsertBlock(*bodies[index]);
    if (!emitBlock(branch.body)) return false;
    if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*merge);
    if (next != merge) builder_->setInsertBlock(*next);
  }
  builder_->setInsertBlock(*merge);
  return true;
}

bool SeremGenerator::emitWhile(const WhileStmt& statement) {
  serem::BasicBlock* condition = &function_->addBlock("while.cond");
  serem::BasicBlock* body = &function_->addBlock("while.body");
  serem::BasicBlock* exit = &function_->addBlock("while.end");
  if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  (void)builder_->conditionalBranch(emitExpression(statement.condition()), *body, *exit);
  builder_->setInsertBlock(*body);
  breakTargets_.push_back(exit);
  continueTargets_.push_back(condition);
  const bool ok = emitBlock(statement.body());
  continueTargets_.pop_back();
  breakTargets_.pop_back();
  if (!ok) return false;
  if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*condition);
  builder_->setInsertBlock(*exit);
  return true;
}

bool SeremGenerator::emitFor(const ForStmt& statement) {
  const serem::ValuePtr iterable = emitExpression(statement.iterable());
  (void)builder_->operation("iter.begin", serem::IRType::voidType(), {iterable});
  serem::BasicBlock* condition = &function_->addBlock("for.cond");
  serem::BasicBlock* body = &function_->addBlock("for.body");
  serem::BasicBlock* exit = &function_->addBlock("for.end");
  if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  auto hasNext = builder_->operation("iter.has_next", serem::IRType::boolType(), {iterable});
  (void)builder_->conditionalBranch(hasNext, *body, *exit);
  builder_->setInsertBlock(*body);
  auto next = builder_->operation("iter.next", lowerType(statement.iterable().resolvedType()), {iterable});
  auto slot = builder_->alloca(next->type());
  builder_->store(next, slot);
  locals_[statement.name()] = slot;
  breakTargets_.push_back(exit);
  continueTargets_.push_back(condition);
  const bool ok = emitBlock(statement.body());
  continueTargets_.pop_back();
  breakTargets_.pop_back();
  if (!ok) return false;
  if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*condition);
  builder_->setInsertBlock(*exit);
  return true;
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
  case NodeKind::AwaitExpr: {
    const auto& await = static_cast<const AwaitExpr&>(expression);
    return builder_->await(emitExpression(await.operand()), lowerType(expression.resolvedType()));
  }
  case NodeKind::MemberExpr:
    return emitMember(static_cast<const MemberExpr&>(expression));
  case NodeKind::IndexExpr:
    return emitIndex(static_cast<const IndexExpr&>(expression));
  case NodeKind::ListLiteral:
  case NodeKind::DictLiteral:
  case NodeKind::TupleExpr:
    return emitAggregate(expression);
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expression);
    return builder_->select(emitExpression(ternary.condition()),
                            emitExpression(ternary.thenValue()),
                            emitExpression(ternary.elseValue()),
                            lowerType(expression.resolvedType()));
  }
  case NodeKind::WalrusExpr: {
    const auto& walrus = static_cast<const WalrusExpr&>(expression);
    const serem::ValuePtr value = emitExpression(walrus.value());
    auto slot = builder_->alloca(value->type());
    builder_->store(value, slot);
    (void)bindLocal(walrus.name(), slot);
    return value;
  }
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

serem::ValuePtr SeremGenerator::emitMember(const MemberExpr& expression) {
  return builder_->operation("member.get", lowerType(expression.resolvedType()),
                             {emitExpression(expression.object())},
                             {{"field", expression.field()}});
}

serem::ValuePtr SeremGenerator::emitIndex(const IndexExpr& expression) {
  std::vector<serem::ValuePtr> operands{emitExpression(expression.object())};
  if (expression.hasStart()) operands.push_back(emitExpression(*expression.start()));
  if (expression.hasStop()) operands.push_back(emitExpression(*expression.stop()));
  return builder_->operation(expression.isSlice() ? "slice" : "index",
                             lowerType(expression.resolvedType()), std::move(operands));
}

serem::ValuePtr SeremGenerator::emitAggregate(const Expr& expression) {
  std::vector<serem::ValuePtr> operands;
  std::string kind;
  if (expression.kind() == NodeKind::ListLiteral) {
    kind = "list";
    for (const std::unique_ptr<Expr>& element : static_cast<const ListLiteral&>(expression).elements()) {
      operands.push_back(emitExpression(*element));
    }
  } else if (expression.kind() == NodeKind::TupleExpr) {
    kind = "tuple";
    for (const std::unique_ptr<Expr>& element : static_cast<const TupleExpr&>(expression).elements()) {
      operands.push_back(emitExpression(*element));
    }
  } else {
    kind = "dict";
    const auto& dict = static_cast<const DictLiteral&>(expression);
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      operands.push_back(emitExpression(*dict.keys()[index]));
      operands.push_back(emitExpression(*dict.values()[index]));
    }
  }
  return builder_->operation("aggregate." + kind, lowerType(expression.resolvedType()),
                             std::move(operands));
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
