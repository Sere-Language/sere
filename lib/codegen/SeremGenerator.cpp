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
  if (type->isEnum()) {
    return serem::IRType::structType(type->name(),
                                     {serem::IRType::i32(), serem::IRType::ptr(serem::IRType::i8())});
  }
  if (type->isUnion()) return serem::IRType::structType("union", {});
  return serem::IRType::ptr(serem::IRType::i8());
}

std::string SeremGenerator::functionName(const FunctionDef& function) const {
  const auto known = functionNames_.find(&function);
  if (known != functionNames_.end()) return known->second;
  if (!function.modulePrefix().empty()) return function.modulePrefix() + "_" + function.name();
  if (function.isMethod() && !function.ownerClass().empty()) {
    return function.ownerClass() + "." + function.name();
  }
  return function.name();
}

std::unique_ptr<serem::IRModule> SeremGenerator::emit(const Module& module,
                                                       std::string moduleName,
                                                       const std::vector<const Module*>* imported,
                                                       const std::vector<std::string>* importedNames) {
  module_ = std::make_unique<serem::IRModule>(std::move(moduleName));
  functions_.clear();
  decorators_.clear();
  classBases_.clear();
  classFields_.clear();
  functionNames_.clear();
  std::vector<const Module*> modules{&module};
  if (imported != nullptr) modules.insert(modules.end(), imported->begin(), imported->end());
  for (const Module* current : modules) declareTypes(*current);
  for (const Module* current : modules) for (const std::unique_ptr<Stmt>& statement : current->statements()) {
    if (statement != nullptr && statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      if (!classDef.bases().empty()) classBases_[classDef.name()] = classDef.bases().front();
    }
  }
  for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
    const Module* current = modules[moduleIndex];
    const std::string prefix = moduleIndex == 0 || importedNames == nullptr ||
                                       moduleIndex - 1 >= importedNames->size()
                                   ? std::string{}
                                   : (*importedNames)[moduleIndex - 1];
    for (const std::unique_ptr<Stmt>& statement : current->statements()) {
    if (statement != nullptr && statement->kind() == NodeKind::FunctionDef) {
      const auto& function = static_cast<const FunctionDef&>(*statement);
      if (!prefix.empty()) functionNames_[&function] = prefix + "_" + function.name();
      if (!function.decorators().empty()) {
        decorators_[functionName(function)] = function.decorators().front();
        decorators_[function.name()] = function.decorators().front();
      }
      const Type* type = functionType(function);
      if (type != nullptr) functions_.insert_or_assign(functionName(function), lowerType(type));
    } else if (statement != nullptr && statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        if (!prefix.empty()) functionNames_[method.get()] = prefix + "_" + functionName(*method);
        const Type* type = functionType(*method);
        if (type != nullptr) functions_.insert_or_assign(functionName(*method), lowerType(type));
      }
    }
    }
  }
  for (const Module* current : modules) for (const std::unique_ptr<Stmt>& statement : current->statements()) {
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
  std::vector<std::string> attributes;
  attributes.push_back(classDef.isStruct() ? "struct" : "class");
  if (!classDef.bases().empty()) {
    const auto base = classFields_.find(classDef.bases().front());
    if (base != classFields_.end()) fields = base->second;
  }
  std::size_t fieldIndex = 0;
  fieldIndex = fields.size();
  for (const FieldDecl& field : classDef.fields()) {
    if (!field.isStatic) {
      fields.push_back(lowerType(field.type == nullptr ? nullptr : field.type->resolvedType()));
      attributes.push_back("field=" + field.name + ":" + std::to_string(fieldIndex));
      ++fieldIndex;
    }
  }
  classFields_[classDef.name()] = fields;
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
      enumDef.name(), serem::IRType::structType(
        enumDef.name(), {serem::IRType::i32(), serem::IRType::ptr(serem::IRType::i8())}),
      std::move(attributes)));
}

bool SeremGenerator::emitFunction(const FunctionDef& function) {
  const Type* type = functionType(function);
  if (type == nullptr) return unsupported(function, "function without a resolved type");
  std::vector<serem::IRType> params;
  for (const Type* param : type->paramTypes()) params.push_back(lowerType(param));
  auto irFunction = std::make_unique<serem::IRFunction>(functionName(function), std::move(params),
                                                        lowerType(type->returnType()));
  function_ = &module_->addFunction(std::move(irFunction));
  currentOwnerClass_ = function.ownerClass();
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
  currentOwnerClass_.clear();
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
    if (declaration.init() != nullptr) {
      serem::ValuePtr value = emitExpression(*declaration.init());
      if (value != nullptr && value->valueKind() == serem::ValueKind::ConstantFloat &&
          (irType.kind() == serem::IRType::Kind::F32 || irType.kind() == serem::IRType::Kind::F64)) {
        value = std::make_shared<serem::ConstantFloat>(
            static_cast<const serem::ConstantFloat&>(*value).value(), irType);
      }
      builder_->store(std::move(value), slot);
    }
    return true;
  }
  case NodeKind::AssignStmt: {
    const auto& assign = static_cast<const AssignStmt&>(statement);
    serem::ValuePtr value = emitExpression(assign.value());
    if (assign.target().kind() == NodeKind::MemberExpr) {
      const auto& member = static_cast<const MemberExpr&>(assign.target());
      (void)builder_->operation("member.set", serem::IRType::voidType(),
                                {emitExpression(member.object()), value},
                                {{"field", member.field()}});
      return true;
    }
    if (assign.target().kind() == NodeKind::IndexExpr) {
      const auto& index = static_cast<const IndexExpr&>(assign.target());
      std::vector<serem::ValuePtr> operands{emitExpression(index.object())};
      if (index.hasStart()) operands.push_back(emitExpression(*index.start()));
      operands.push_back(value);
      (void)builder_->operation("index.set", serem::IRType::voidType(), std::move(operands));
      return true;
    }
    if (assign.target().kind() == NodeKind::UnaryExpr &&
        static_cast<const UnaryExpr&>(assign.target()).op() == UnaryOp::Deref) {
      const auto& deref = static_cast<const UnaryExpr&>(assign.target());
      (void)builder_->operation("store.indirect", serem::IRType::voidType(),
                                {value, emitExpression(deref.operand())});
      return true;
    }
    if (assign.target().kind() != NodeKind::NameExpr) {
      (void)builder_->operation("assign.dynamic", serem::IRType::voidType(), {value});
      return true;
    }
    const auto& name = static_cast<const NameExpr&>(assign.target());
    serem::ValuePtr slot = local(name.name());
    if (slot == nullptr) {
      slot = builder_->alloca(value->type());
      (void)bindLocal(name.name(), slot);
      builder_->store(std::move(value), slot);
      return true;
    }
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
  case NodeKind::MatchStmt: {
    const auto& match = static_cast<const MatchStmt&>(statement);
    const serem::ValuePtr subject = emitExpression(match.subject());
    const auto tag = builder_->operation("enum.tag", serem::IRType::i32(), {subject});
    serem::BasicBlock* merge = &function_->addBlock("match.end");
    std::vector<serem::BasicBlock*> arms;
    for (std::size_t index = 0; index < match.arms().size(); ++index) {
      arms.push_back(&function_->addBlock("match.arm" + std::to_string(index)));
    }
    for (std::size_t index = 0; index < match.arms().size(); ++index) {
      const MatchArm& arm = match.arms()[index];
      serem::BasicBlock* next = index + 1 < arms.size() ? arms[index + 1] : merge;
      serem::BasicBlock* body = &function_->addBlock("match.body" + std::to_string(index));
      auto patternTag = std::make_shared<serem::ConstantInt>(static_cast<std::int64_t>(index),
                          serem::IRType::i32());
      auto condition = builder_->compare("eq", tag, patternTag);
      (void)builder_->conditionalBranch(condition, *body, *next);
      builder_->setInsertBlock(*body);
      if (arm.pattern->kind() == NodeKind::CallExpr) {
        const auto& call = static_cast<const CallExpr&>(*arm.pattern);
        for (std::size_t argument = 0; argument < call.arguments().size(); ++argument) {
          if (call.arguments()[argument]->kind() == NodeKind::NameExpr) {
            const auto& name = static_cast<const NameExpr&>(*call.arguments()[argument]);
            auto payload = builder_->operation("enum.payload", lowerType(name.resolvedType()),
                                               {subject}, {{"index", std::to_string(argument)}});
            auto slot = builder_->alloca(payload->type());
            builder_->store(payload, slot);
            (void)bindLocal(name.name(), slot);
          }
        }
      }
      if (!emitBlock(arm.body)) return false;
      if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*merge);
      if (next == merge) break;
      builder_->setInsertBlock(*next);
    }
    builder_->setInsertBlock(*merge);
    return true;
  }
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
    (void)builder_->operation("sere.statement", serem::IRType::voidType(), {},
                              {{"kind", std::to_string(static_cast<int>(statement.kind()))}});
    return true;
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
    if (branch.condition == nullptr) {
      if (&builder_->currentBlock() != bodies[index] && !builder_->currentBlock().isTerminated()) {
        (void)builder_->branch(*bodies[index]);
      }
    } else {
      const serem::ValuePtr condition = emitExpression(*branch.condition);
      (void)builder_->conditionalBranch(condition, *bodies[index], *next);
    }
    builder_->setInsertBlock(*bodies[index]);
    if (!emitBlock(branch.body)) return false;
    if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*merge);
    if (next != merge) builder_->setInsertBlock(*next);
    if (branch.condition == nullptr) break;
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
  const Type* iterableType = statement.iterable().resolvedType();
  const Type* elementType = iterableType == nullptr ? nullptr : iterableType->elementType();
  const std::unordered_map<std::string, std::string> iteratorAttributes{
      {"element", elementType == nullptr ? std::string{} : elementType->display()}};
  const serem::ValuePtr iterator =
      builder_->operation("iter.begin", serem::IRType::ptr(serem::IRType::i64()), {iterable},
                          iteratorAttributes);
  serem::BasicBlock* condition = &function_->addBlock("for.cond");
  serem::BasicBlock* body = &function_->addBlock("for.body");
  serem::BasicBlock* exit = &function_->addBlock("for.end");
  if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  auto hasNext = builder_->operation("iter.has_next", serem::IRType::boolType(),
                                    {iterable, iterator}, iteratorAttributes);
  (void)builder_->conditionalBranch(hasNext, *body, *exit);
  builder_->setInsertBlock(*body);
  auto next = builder_->operation("iter.next", lowerType(elementType), {iterable, iterator},
                                 iteratorAttributes);
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
  case NodeKind::StringLiteral: {
    const std::string& value = static_cast<const StringLiteral&>(expression).value();
    const std::string name = "str." + std::to_string(module_->globals().size());
    (void)module_->addGlobal(std::make_unique<serem::GlobalConstant>(
        name, serem::IRType::stringType(), serem::ConstantString(value).display()));
    return std::make_shared<serem::ConstantString>(value);
  }
  case NodeKind::InterpolatedStringExpr: {
    const auto& interpolated = static_cast<const InterpolatedStringExpr&>(expression);
    std::vector<serem::ValuePtr> parts;
    for (const StringPart& part : interpolated.parts()) {
      if (!part.literal.empty()) parts.push_back(std::make_shared<serem::ConstantString>(part.literal));
      if (part.value != nullptr) {
        serem::ValuePtr value = emitExpression(*part.value);
        const Type* valueType = part.value->resolvedType();
        if (valueType != nullptr && valueType->isList()) {
          value = builder_->operation("value.repr", serem::IRType::stringType(), {value},
                                      {{"kind", "list.str"}});
        }
        parts.push_back(std::move(value));
      }
    }
    if (parts.empty()) return std::make_shared<serem::ConstantString>("");
    return builder_->operation("string.concat", serem::IRType::stringType(), std::move(parts));
  }
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
    return builder_->operation("sere.expression", lowerType(expression.resolvedType()), {},
                               {{"kind", std::to_string(static_cast<int>(expression.kind()))}});
  }
}

serem::ValuePtr SeremGenerator::emitName(const NameExpr& expression) {
  if (serem::ValuePtr value = local(expression.name())) {
    const serem::IRType type = lowerType(expression.resolvedType());
    if (value->valueKind() == serem::ValueKind::Argument) {
      if (value->type().display() != type.display()) {
        return builder_->operation("union.extract", type, {value},
                                   {{"type", expression.resolvedType() == nullptr
                                                     ? std::string{}
                                                     : expression.resolvedType()->display()}});
      }
      return value;
    }
    return builder_->load(value, type);
  }
  const auto found = functions_.find(expression.name());
  if (found != functions_.end()) return std::make_shared<serem::FunctionRef>(expression.name(), found->second);
  return std::make_shared<serem::FunctionRef>(expression.name(), lowerType(expression.resolvedType()));
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
  case BinaryOp::In:
  case BinaryOp::NotIn: {
    std::unordered_map<std::string, std::string> attributes{
        {"negated", expression.op() == BinaryOp::NotIn ? "true" : "false"}};
    const Type* rightType = expression.right().resolvedType();
    if (rightType != nullptr && rightType->isList() && rightType->elementType() != nullptr) {
      attributes["element"] = rightType->elementType()->display();
    }
    return builder_->operation("contains", serem::IRType::boolType(), {left, right},
                               std::move(attributes));
  }
  case BinaryOp::Lt: return builder_->compare("lt", left, right);
  case BinaryOp::Le: return builder_->compare("le", left, right);
  case BinaryOp::Gt: return builder_->compare("gt", left, right);
  case BinaryOp::Ge: return builder_->compare("ge", left, right);
  case BinaryOp::Is:
  case BinaryOp::IsNot: {
    const Type* tested = expression.right().resolvedType();
    if (tested != nullptr && tested->isTypeObject() && tested->typeObjectInstance() != nullptr) {
      tested = tested->typeObjectInstance();
    }
    return builder_->operation("type.is", serem::IRType::boolType(), {left},
                               {{"type", tested == nullptr ? "Any" : tested->display()},
                                {"negated", expression.op() == BinaryOp::IsNot ? "true" : "false"}});
  }
  default:
    return builder_->operation("binary.dynamic", type, {left, right},
                               {{"operator", std::to_string(static_cast<int>(expression.op()))}});
  }
}

serem::ValuePtr SeremGenerator::emitCall(const CallExpr& expression) {
  if (expression.intrinsic() == IntrinsicKind::SharedNew ||
      expression.intrinsic() == IntrinsicKind::UniqueNew) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    return builder_->operation(expression.intrinsic() == IntrinsicKind::SharedNew
                                   ? "shared.new"
                                   : "unique.new",
                               lowerType(expression.resolvedType()), std::move(args),
                               {{"pointee", expression.resolvedType() == nullptr
                                                 ? std::string{}
                                                 : expression.resolvedType()->display()}});
  }
  if (expression.intrinsic() == IntrinsicKind::ListNew) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    const Type* element = expression.resolvedType() == nullptr
                              ? nullptr
                              : expression.resolvedType()->elementType();
    return builder_->operation("aggregate.list", lowerType(expression.resolvedType()),
                               std::move(args),
                               {{"element", element == nullptr ? std::string{} : element->display()}});
  }
  if (expression.intrinsic() == IntrinsicKind::Len) {
    const Type* argumentType = expression.arguments().empty()
                                   ? nullptr
                                   : expression.arguments()[0]->resolvedType();
    serem::ValuePtr value = expression.arguments().empty()
                                ? nullptr
                                : emitExpression(*expression.arguments()[0]);
    return builder_->operation("runtime.len", lowerType(expression.resolvedType()), {value},
                               {{"kind", argumentType == nullptr ? std::string{}
                                                                    : argumentType->valueType()->display()}});
  }
  if (expression.intrinsic() == IntrinsicKind::BuiltinMethod &&
      expression.callee().kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    std::vector<serem::ValuePtr> args{emitExpression(member.object())};
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    std::unordered_map<std::string, std::string> attributes{{"name", expression.loweredName()}};
    const Type* objectType = member.object().resolvedType();
    if (objectType != nullptr && objectType->isList() && objectType->elementType() != nullptr) {
      attributes["element"] = objectType->elementType()->display();
    }
    return builder_->operation("builtin.method", lowerType(expression.resolvedType()),
                               std::move(args), std::move(attributes));
  }
  if (expression.callee().kind() == NodeKind::NameExpr &&
      static_cast<const NameExpr&>(expression.callee()).name() == "input") {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    return builder_->operation("runtime.input", lowerType(expression.resolvedType()),
                               std::move(args));
  }
  if (expression.callee().kind() == NodeKind::NameExpr &&
      static_cast<const NameExpr&>(expression.callee()).name() == "super") {
    if (serem::ValuePtr self = local("self")) return self;
  }
  if (expression.intrinsic() == IntrinsicKind::Print) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      serem::ValuePtr value = emitExpression(*argument);
      const Type* valueType = argument->resolvedType();
      if (valueType != nullptr && valueType->isList()) {
        value = builder_->operation("value.repr", serem::IRType::stringType(), {value},
                                    {{"kind", "list.str"}});
      }
      args.push_back(std::move(value));
    }
    return builder_->operation("runtime.print", serem::IRType::voidType(), std::move(args));
  }
  if (expression.intrinsic() == IntrinsicKind::TypeOf) {
    const std::string typeName = expression.arguments().empty() ||
                                         expression.arguments()[0]->resolvedType() == nullptr
                                     ? "Any"
                                     : expression.arguments()[0]->resolvedType()->display();
    return std::make_shared<serem::ConstantString>(typeName);
  }
  std::vector<serem::ValuePtr> args;
  if (expression.callee().kind() == NodeKind::NameExpr) {
    const std::string& name = static_cast<const NameExpr&>(expression.callee()).name();
    const auto decorated = decorators_.find(name);
    if (decorated != decorators_.end()) {
      std::vector<serem::ValuePtr> decoratedArgs;
      decoratedArgs.push_back(std::make_shared<serem::FunctionRef>(
          decorated->second, serem::IRType::function(serem::IRType::ptr(serem::IRType::i8()),
                                                       {serem::IRType::ptr(serem::IRType::i8())})));
      decoratedArgs.push_back(std::make_shared<serem::FunctionRef>(
          name, serem::IRType::function(lowerType(expression.resolvedType()), {})));
      for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
        decoratedArgs.push_back(emitExpression(*argument));
      }
      return builder_->operation("decorated.call", lowerType(expression.resolvedType()),
                                std::move(decoratedArgs));
    }
  }
  if (expression.isConstructor()) {
    for (const Expr* argument : expression.boundArguments()) {
      if (argument != nullptr) args.push_back(emitExpression(*argument));
    }
    if (args.empty()) {
      for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
        args.push_back(emitExpression(*argument));
      }
    }
    return builder_->operation("construct", lowerType(expression.resolvedType()), std::move(args),
                               {{"type", expression.resolvedType() == nullptr
                                             ? std::string{}
                                             : expression.resolvedType()->name()}});
  }
  if (expression.callee().kind() == NodeKind::MemberExpr &&
      expression.callee().resolvedType() != nullptr &&
      expression.callee().resolvedType()->kind() == TypeKind::Function) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    if (member.object().kind() == NodeKind::CallExpr &&
        static_cast<const CallExpr&>(member.object()).callee().kind() == NodeKind::NameExpr &&
        static_cast<const NameExpr&>(static_cast<const CallExpr&>(member.object()).callee()).name() == "super") {
      const auto base = classBases_.find(currentOwnerClass_);
      if (base != classBases_.end()) {
        serem::ValuePtr callee = std::make_shared<serem::FunctionRef>(
            base->second + "." + member.field(), lowerType(expression.callee().resolvedType()));
        args.push_back(local("self"));
        for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
          args.push_back(emitExpression(*argument));
        }
        return builder_->call(std::move(callee), std::move(args), lowerType(expression.resolvedType()));
      }
    }
    if (member.object().resolvedType() != nullptr) {
      const std::string owner = member.object().resolvedType()->canonical()->name();
      const std::string qualified = owner + "." + member.field();
      const std::string symbol = functions_.contains(qualified) ? qualified : member.field();
      serem::ValuePtr callee = std::make_shared<serem::FunctionRef>(
          symbol, lowerType(expression.callee().resolvedType()));
      args.push_back(emitExpression(member.object()));
      for (const Expr* argument : expression.boundArguments()) {
        if (argument != nullptr) args.push_back(emitExpression(*argument));
      }
      if (args.empty()) for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
        args.push_back(emitExpression(*argument));
      }
      return builder_->call(std::move(callee), std::move(args), lowerType(expression.resolvedType()));
    }
  }
  serem::ValuePtr callee = emitExpression(expression.callee());
  for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
    args.push_back(emitExpression(*argument));
  }
  return builder_->call(std::move(callee), std::move(args), lowerType(expression.resolvedType()));
}

serem::ValuePtr SeremGenerator::emitMember(const MemberExpr& expression) {
  if (expression.object().kind() == NodeKind::NameExpr &&
      functions_.contains(expression.field())) {
    return std::make_shared<serem::FunctionRef>(
        expression.field(), lowerType(expression.resolvedType()));
  }
  if (expression.object().kind() == NodeKind::NameExpr) {
    const auto& object = static_cast<const NameExpr&>(expression.object());
    const std::string qualified = object.name() + "_" + expression.field();
    if (functions_.contains(qualified)) {
      return std::make_shared<serem::FunctionRef>(qualified,
                                                  lowerType(expression.resolvedType()));
    }
  }
  if (expression.resolvedType() != nullptr && expression.resolvedType()->kind() == TypeKind::Function &&
      expression.object().kind() == NodeKind::NameExpr) {
    return std::make_shared<serem::FunctionRef>(expression.field(),
                                                lowerType(expression.resolvedType()));
  }
  std::unordered_map<std::string, std::string> attributes{{"field", expression.field()}};
  if (expression.object().resolvedType() != nullptr) {
    attributes["index"] = std::to_string(
        expression.object().resolvedType()->fieldIndex(expression.field()));
  }
  return builder_->operation("member.get", lowerType(expression.resolvedType()),
                             {emitExpression(expression.object())},
                             std::move(attributes));
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
  std::unordered_map<std::string, std::string> attributes;
  if (expression.kind() == NodeKind::ListLiteral && expression.resolvedType() != nullptr &&
      expression.resolvedType()->elementType() != nullptr) {
    attributes["element"] = expression.resolvedType()->elementType()->display();
  }
  return builder_->operation("aggregate." + kind, lowerType(expression.resolvedType()),
                             std::move(operands), std::move(attributes));
}

serem::ValuePtr SeremGenerator::emitUnary(const UnaryExpr& expression) {
  const serem::ValuePtr operand = emitExpression(expression.operand());
  const serem::IRType type = lowerType(expression.resolvedType());
  switch (expression.op()) {
  case UnaryOp::Neg: return builder_->operation("neg", type, {operand});
  case UnaryOp::Pos: return operand;
  case UnaryOp::Not: return builder_->operation("not", type, {operand});
  case UnaryOp::Invert: return builder_->operation("invert", type, {operand});
  case UnaryOp::AddrOf:
    if (expression.operand().kind() == NodeKind::NameExpr) {
      const auto& name = static_cast<const NameExpr&>(expression.operand());
      if (serem::ValuePtr slot = local(name.name())) return slot;
    }
    return builder_->operation("address.of", type, {operand});
  case UnaryOp::Deref:
    return builder_->operation("deref", type, {operand});
  default:
    return builder_->operation("unary.dynamic", type, {operand},
                               {{"operator", std::to_string(static_cast<int>(expression.op()))}});
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
