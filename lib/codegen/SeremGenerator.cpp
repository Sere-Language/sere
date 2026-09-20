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

/// Element interpretation for list repr and list construction. The code is part
/// of the Serem dialect contract so the backend layout and the runtime formatter
/// agree without either side keeping its own type-name switch.
[[nodiscard]] serem::ListElementKind listElementKindOf(const Type* type) {
  if (type == nullptr) return serem::ListElementKind::Ptr;
  const Type* canonical = type->canonical();
  if (canonical->isNamed("str")) return serem::ListElementKind::Str;
  if (canonical->isNamed("bool")) return serem::ListElementKind::Bool;
  if (canonical->isNamed("f32")) return serem::ListElementKind::Float32;
  if (canonical->isNamed("f64")) return serem::ListElementKind::Float64;
  if (canonical->isNamed("i32") || canonical->isNamed("u32")) return serem::ListElementKind::Int32;
  if (canonical->isNamed("i8")) return serem::ListElementKind::Int8;
  if (canonical->isNamed("i16")) return serem::ListElementKind::Int16;
  if (canonical->isNamed("u8")) return serem::ListElementKind::UInt8;
  if (canonical->isNamed("u16")) return serem::ListElementKind::UInt16;
  if (canonical->isInteger()) return serem::ListElementKind::Int64;
  return serem::ListElementKind::Ptr;
}

[[nodiscard]] std::string listElementKindText(const Type* type) {
  return std::to_string(static_cast<std::int32_t>(listElementKindOf(type)));
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
  if (type->isRecord() && !type->isEnum() && !type->isStruct()) {
    return serem::IRType::ptr(serem::IRType::structType(type->name()));
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
  if (type->isUnion()) return serem::IRType::structType("union", {serem::IRType::i32(), serem::IRType::i64()});
  return serem::IRType::ptr(serem::IRType::i8());
}

std::string SeremGenerator::functionName(const FunctionDef& function) const {
  const auto known = functionNames_.find(&function);
  if (known != functionNames_.end()) return known->second;
  if (function.isExtern()) return function.externName();
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
  definitions_.clear();
  functionSymbols_.clear();
  methodSymbols_.clear();
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
      if (!prefix.empty() && !function.isExtern()) functionNames_[&function] = prefix + "_" + function.name();
      if (!function.decorators().empty()) {
        decorators_[functionName(function)] = function.decorators().front();
        decorators_[function.name()] = function.decorators().front();
      }
      const Type* type = functionType(function);
      if (type != nullptr) {
        const std::string symbol = functionName(function);
        functions_.insert_or_assign(symbol, lowerType(type));
        definitions_[symbol] = &function;
        functionSymbols_.insert_or_assign(function.name(), symbol);
      }
    } else if (statement != nullptr && statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        if (!prefix.empty()) functionNames_[method.get()] = prefix + "_" + functionName(*method);
        const Type* type = functionType(*method);
        if (type != nullptr) {
          functions_.insert_or_assign(functionName(*method), lowerType(type));
          definitions_[functionName(*method)] = method.get();
          methodSymbols_[classDef.name() + "::" + method->name()] = functionName(*method);
        }
      }
    }
    }
  }
  for (const Module* current : modules) {
    for (const auto& statement : current->statements()) {
      if (statement->kind() == NodeKind::FunctionDef &&
          !emitFunction(static_cast<const FunctionDef&>(*statement))) return nullptr;
      if (statement->kind() == NodeKind::ClassDef) {
        for (const auto& method : static_cast<const ClassDef&>(*statement).methods())
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
  if (function.isExtern() && function.body().empty()) {
    // Extern declarations are defined by the runtime or a C library. Emitting a
    // body here would clash with that definition at link time, so keep the
    // function bodyless and let the backend emit a declaration.
    irFunction->setExternal(true);
    (void)module_->addFunction(std::move(irFunction));
    return true;
  }
  function_ = &module_->addFunction(std::move(irFunction));
  currentOwnerClass_ = function.ownerClass();
  function_->setAsync(function.isAsync());
  function_->setGenerator(function.isGenerator());
  function_->setExternal(function.isExtern());
  if (function.isExtern()) {
    // External functions are declarations only: the implementation is supplied by
    // the runtime library or a linked native library under the extern symbol.
    return true;
  }
  for (const std::string& decorator : function.decorators()) {
    function_->setAttribute("decorator." + decorator, "true");
  }
  builder_ = std::make_unique<serem::IRBuilder>(*function_);
  locals_.clear();
  localTypes_.clear();
  returnType_ = type->returnType();
  coroutineToken_.reset();
  if (function.isAsync() || function.isGenerator()) coroutineToken_ = builder_->coroBegin();
  for (std::size_t index = 0; index < function.params().size(); ++index) {
    if (index < function_->arguments().size()) {
      locals_[function.params()[index].name] = function_->argument(index);
      localTypes_[function.params()[index].name] = type->paramTypes()[index];
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
    localTypes_[declaration.name()] = type;
    if (declaration.init() != nullptr) {
      serem::ValuePtr value = coerce(emitExpression(*declaration.init()),
                                      declaration.init()->resolvedType(), type);
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
    serem::ValuePtr value = coerce(emitExpression(assign.value()), assign.value().resolvedType(),
                                    assign.target().resolvedType());
    if (assign.target().kind() == NodeKind::MemberExpr) {
      const auto& member = static_cast<const MemberExpr&>(assign.target());
      (void)builder_->operation("member.set", serem::IRType::voidType(),
                                {emitExpression(member.object()), value},
                                {{"field", member.field()},
                                 {"index", std::to_string(member.object().resolvedType()->fieldIndex(member.field()))}});
      return true;
    }
    if (assign.target().kind() == NodeKind::IndexExpr) {
      const auto& index = static_cast<const IndexExpr&>(assign.target());
      const Type* objectType = index.object().resolvedType();
      const Type* record = objectType == nullptr ? nullptr : objectType->valueType();
      if (record != nullptr && record->isRecord() &&
          record->methodIndex("__setitem__") >= 0) {
        serem::ValuePtr receiver = emitExpression(index.object());
        std::vector<serem::ValuePtr> arguments;
        if (index.hasStart()) arguments.push_back(emitExpression(*index.start()));
        arguments.push_back(value);
        (void)callMethod(record, "__setitem__", std::move(receiver), arguments);
        return true;
      }
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
    const Type* contextType = with.context().resolvedType();
    const Type* record = contextType == nullptr ? nullptr : contextType->valueType();
    const serem::ValuePtr context = emitExpression(with.context());
    const bool managed = record != nullptr && record->isRecord() &&
                         record->methodIndex("__enter__") >= 0;
    if (managed) {
      const Type* entered = record->dunderReturn("__enter__");
      const Type* bound = entered == nullptr ? record : entered;
      const serem::ValuePtr value = callMethod(record, "__enter__", context, {});
      if (!with.name().empty()) {
        auto slot = builder_->alloca(lowerType(bound));
        builder_->store(coerce(value, entered, bound), slot);
        locals_[with.name()] = slot;
        localTypes_[with.name()] = bound;
      }
    } else if (!with.name().empty()) {
      (void)bindLocal(with.name(), context);
    }
    if (!emitBlock(with.body())) return false;
    if (managed) (void)callMethod(record, "__exit__", context, {});
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
    else builder_->ret(coerce(emitExpression(*result.value()), result.value()->resolvedType(), returnType_));
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
  const Type* iterable = statement.iterable().resolvedType();
  const Type* custom = iterable == nullptr ? nullptr : iterable->valueType();
  // A class iterator is a coroutine (`Iterator[T]`), and coroutines lower as
  // no-ops in the Serem backend, so iterating one would read garbage.
  if (custom != nullptr && custom->isRecord() && custom->methodIndex("__iter__") >= 0) {
    return unsupported(statement, "for-in over a class __iter__");
  }
  const serem::ValuePtr source = emitExpression(statement.iterable());
  const Type* iterableType = statement.iterable().resolvedType();
  const Type* elementType = iterableType == nullptr ? nullptr : iterableType->elementType();
  const std::unordered_map<std::string, std::string> iteratorAttributes{
      {"element", elementType == nullptr ? std::string{} : elementType->display()}};
  const serem::ValuePtr iterator =
      builder_->operation("iter.begin", serem::IRType::ptr(serem::IRType::i64()), {source},
                          iteratorAttributes);
  serem::BasicBlock* condition = &function_->addBlock("for.cond");
  serem::BasicBlock* body = &function_->addBlock("for.body");
  serem::BasicBlock* exit = &function_->addBlock("for.end");
  if (!builder_->currentBlock().isTerminated()) (void)builder_->branch(*condition);
  builder_->setInsertBlock(*condition);
  auto hasNext = builder_->operation("iter.has_next", serem::IRType::boolType(),
                                    {source, iterator}, iteratorAttributes);
  (void)builder_->conditionalBranch(hasNext, *body, *exit);
  builder_->setInsertBlock(*body);
  auto next = builder_->operation("iter.next", lowerType(elementType), {source, iterator},
                                 iteratorAttributes);
  auto slot = builder_->alloca(next->type());
  builder_->store(next, slot);
  locals_[statement.name()] = slot;
  localTypes_[statement.name()] = elementType;
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

serem::ValuePtr SeremGenerator::stringValue(std::string_view value) {
  const std::string text(value);
  const std::string name = "str." + std::to_string(module_->globals().size());
  (void)module_->addGlobal(std::make_unique<serem::GlobalConstant>(
      name, serem::IRType::stringType(), serem::ConstantString(text).display()));
  return std::make_shared<serem::ConstantString>(text, name);
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
    return stringValue(value);
  }
  case NodeKind::InterpolatedStringExpr: {
    const auto& interpolated = static_cast<const InterpolatedStringExpr&>(expression);
    std::vector<serem::ValuePtr> parts;
    for (const StringPart& part : interpolated.parts()) {
      if (!part.literal.empty()) parts.push_back(stringValue(part.literal));
      if (part.value != nullptr) {
        serem::ValuePtr value = emitExpression(*part.value);
        const Type* valueType = part.value->resolvedType();
        value = printable(value, valueType);
        parts.push_back(std::move(value));
      }
    }
    if (parts.empty()) return stringValue("");
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
    return coerce(emitExpression(cast.value()), cast.value().resolvedType(), expression.resolvedType());
  }
  default:
    return builder_->operation("sere.expression", lowerType(expression.resolvedType()), {},
                               {{"kind", std::to_string(static_cast<int>(expression.kind()))}});
  }
}

serem::ValuePtr SeremGenerator::coerce(serem::ValuePtr value, const Type* from, const Type* to) {
  if (value == nullptr || from == nullptr || to == nullptr || from == to) return value;
  if (from->isVoidLike() && lowerType(to).kind() == serem::IRType::Kind::Ptr)
    return builder_->operation("pointer.null", lowerType(to));
  if (to->isUnion() && !from->isUnion()) {
    int tag = to->unionMemberIndex(from);
    if (tag < 0) {
      for (std::size_t i = 0; i < to->args().size(); ++i) {
        if (from->isSubtypeOf(to->args()[i])) {
          tag = static_cast<int>(i);
          value = coerce(value, from, to->args()[i]);
          break;
        }
      }
    }
    return builder_->operation("union.pack", lowerType(to), {value}, {{"tag", std::to_string(tag)}});
  }
  if (from->isUnion() && !to->isUnion()) {
    return builder_->operation("union.extract", lowerType(to), {value});
  }
  if (lowerType(from).display() != lowerType(to).display()) {
    return builder_->operation("cast.value", lowerType(to), {value},
                               {{"unsigned", from->isUnsignedInteger() ? "true" : "false"}});
  }
  return value;
}

std::string SeremGenerator::methodSymbol(const Type* record, std::string_view name) const {
  if (record == nullptr) return {};
  // Candidate class names: the type's own name, its bare name without generic
  // arguments, and the same without a module qualifier, then each base class.
  std::vector<std::string> candidates;
  const auto addCandidate = [&](std::string text) {
    if (text.empty()) return;
    const std::size_t bracket = text.find('[');
    if (bracket != std::string::npos) text.erase(bracket);
    if (text.empty()) return;
    candidates.push_back(text);
    const std::size_t dot = text.rfind('.');
    if (dot != std::string::npos && dot + 1 < text.size()) {
      candidates.push_back(text.substr(dot + 1));
    }
  };
  addCandidate(record->name());
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const std::string& owner = candidates[index];
    const auto found = methodSymbols_.find(owner + "::" + std::string(name));
    if (found != methodSymbols_.end()) return found->second;
    const auto base = classBases_.find(owner);
    if (base != classBases_.end()) addCandidate(base->second);
  }
  return {};
}

serem::ValuePtr SeremGenerator::callMethod(const Type* record,
                                           const std::string& name,
                                           serem::ValuePtr self,
                                           const std::vector<serem::ValuePtr>& arguments) {
  if (record == nullptr || self == nullptr) return nullptr;
  const std::string symbol = methodSymbol(record, name);
  if (symbol.empty()) return nullptr;
  const auto found = functions_.find(symbol);
  if (found == functions_.end()) return nullptr;
  std::vector<serem::ValuePtr> args;
  args.push_back(std::move(self));
  args.insert(args.end(), arguments.begin(), arguments.end());
  // The backend converts every argument to the callee's declared parameter type.
  return builder_->call(std::make_shared<serem::FunctionRef>(symbol, found->second), std::move(args),
                        lowerType(record->dunderReturn(name)));
}

std::string SeremGenerator::renderSymbol(const Type* record) const {
  std::string symbol = methodSymbol(record, "__repr__");
  return symbol.empty() ? methodSymbol(record, "__str__") : symbol;
}

serem::ValuePtr SeremGenerator::printable(serem::ValuePtr value, const Type* type) {
  if (type == nullptr) return value;
  if (type->isList() || type->isArray()) {
    const Type* element = type->elementType();
    std::unordered_map<std::string, std::string> attributes{
        {"kind", "list"}, {"element.kind", listElementKindText(element)}};
    if (element != nullptr && element->isRecord() && !element->isStruct() && !element->isEnum()) {
      attributes["element.name"] = element->name();
      attributes["element.repr"] = renderSymbol(element);
    }
    return builder_->operation("value.repr", serem::IRType::stringType(), {value}, attributes);
  }
  if (type->isRecord() && !type->isEnum()) {
    const std::string symbol = methodSymbol(type, "__str__");
    if (!symbol.empty()) {
      return builder_->call(std::make_shared<serem::FunctionRef>(symbol, functions_.at(symbol)),
                            {value}, serem::IRType::stringType());
    }
    return stringValue(type->name());
  }
  return value;
}

serem::ValuePtr SeremGenerator::emitName(const NameExpr& expression) {
  if (serem::ValuePtr value = local(expression.name())) {
    const auto known = localTypes_.find(expression.name());
    const Type* stored = known == localTypes_.end() ? expression.resolvedType() : known->second;
    if (value->valueKind() != serem::ValueKind::Argument) {
      value = builder_->load(value, lowerType(stored));
    }
    return coerce(value, stored, expression.resolvedType());
  }
  const auto symbol = functionSymbols_.find(expression.name());
  const auto found = functions_.find(symbol == functionSymbols_.end() ? expression.name() : symbol->second);
  if (found != functions_.end()) {
    return std::make_shared<serem::FunctionRef>(
        symbol == functionSymbols_.end() ? expression.name() : symbol->second, found->second);
  }
  return std::make_shared<serem::FunctionRef>(expression.name(), lowerType(expression.resolvedType()));
}

serem::ValuePtr SeremGenerator::emitBinary(const BinaryExpr& expression) {
  // `and` / `or` short-circuit, so the right operand gets its own block and the
  // result is read back from a slot afterwards.
  if (expression.op() == BinaryOp::And || expression.op() == BinaryOp::Or) {
    const serem::IRType type = lowerType(expression.resolvedType());
    const serem::ValuePtr left = emitExpression(expression.left());
    const serem::ValuePtr slot = builder_->alloca(type);
    builder_->store(left, slot);
    serem::BasicBlock* rhs = &function_->addBlock("logic.rhs");
    serem::BasicBlock* done = &function_->addBlock("logic.end");
    // `and` evaluates the right operand only when the left is true; `or` only
    // when it is false.
    const bool conjunction = expression.op() == BinaryOp::And;
    (void)builder_->conditionalBranch(left, conjunction ? *rhs : *done,
                                      conjunction ? *done : *rhs);
    builder_->setInsertBlock(*rhs);
    builder_->store(emitExpression(expression.right()), slot);
    (void)builder_->branch(*done);
    builder_->setInsertBlock(*done);
    return builder_->load(slot, type);
  }
  // The type checker records which class implements an operator, so lowering
  // only has to call that method.
  const BinaryDunderNames names = binaryDunderNames(expression.op());
  if (expression.overload() != BinaryOverload::None) {
    const bool reflected = expression.overload() == BinaryOverload::Right;
    const Expr& receiver = reflected ? expression.right() : expression.left();
    const Expr& argument = reflected ? expression.left() : expression.right();
    const Type* record =
        receiver.resolvedType() == nullptr ? nullptr : receiver.resolvedType()->valueType();
    const char* method = expression.overload() == BinaryOverload::EqFallback
                             ? "__eq__"
                             : (reflected ? names.reflected : names.method);
    if (record != nullptr && record->isRecord() && method != nullptr) {
      serem::ValuePtr result = callMethod(record, method, emitExpression(receiver),
                                          {emitExpression(argument)});
      if (result != nullptr) {
        if (expression.overload() == BinaryOverload::EqFallback) {
          return builder_->operation("not", serem::IRType::boolType(), {result});
        }
        return result;
      }
    }
  }
  serem::ValuePtr left = emitExpression(expression.left());
  serem::ValuePtr right = expression.op() == BinaryOp::Is || expression.op() == BinaryOp::IsNot
                              ? nullptr : emitExpression(expression.right());
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
    const Type* container = rightType == nullptr ? nullptr : rightType->valueType();
    if (container != nullptr && container->isRecord() &&
        container->methodIndex("__contains__") >= 0) {
      serem::ValuePtr result = callMethod(
          container, "__contains__", emitExpression(expression.right()), {left});
      if (result != nullptr) {
        return expression.op() == BinaryOp::NotIn
                   ? builder_->operation("not", serem::IRType::boolType(), {result})
                   : result;
      }
    }
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
    if (expression.right().kind() == NodeKind::NoneLiteral) {
      return builder_->operation("pointer.is_null", serem::IRType::boolType(), {left},
          {{"negated", expression.op() == BinaryOp::IsNot ? "true" : "false"}});
    }
    const Type* source = expression.left().resolvedType();
    if (source != nullptr && source->isUnion()) {
      return builder_->operation("union.is", serem::IRType::boolType(), {left},
          {{"tag", std::to_string(source->unionMemberIndex(tested))},
           {"negated", expression.op() == BinaryOp::IsNot ? "true" : "false"}});
    }
    bool matches = source != nullptr && tested != nullptr && source->isSubtypeOf(tested);
    if (expression.op() == BinaryOp::IsNot) matches = !matches;
    return std::make_shared<serem::ConstantInt>(matches, serem::IRType::boolType());
  }
  default:
    return builder_->operation("binary.dynamic", type, {left, right},
                               {{"operator", std::to_string(static_cast<int>(expression.op()))}});
  }
}

void SeremGenerator::appendDefaults(const std::string& symbol, std::vector<serem::ValuePtr>& arguments) {
  const auto found = definitions_.find(symbol);
  if (found == definitions_.end()) return;
  const FunctionDef& function = *found->second;
  for (std::size_t index = arguments.size(); index < function.params().size(); ++index) {
    const auto& parameter = function.params()[index];
    if (parameter.defaultValue == nullptr) break;
    arguments.push_back(coerce(emitExpression(*parameter.defaultValue),
                              parameter.defaultValue->resolvedType(),
                              function.resolvedType()->paramTypes()[index]));
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
  if (expression.intrinsic() == IntrinsicKind::ArrayNew ||
      expression.intrinsic() == IntrinsicKind::Range) {
    std::vector<serem::ValuePtr> args;
    for (const auto& argument : expression.arguments()) args.push_back(emitExpression(*argument));
    const Type* element = expression.resolvedType()->elementType();
    return builder_->operation(expression.intrinsic() == IntrinsicKind::ArrayNew
                                   ? "aggregate.array" : "aggregate.range",
        lowerType(expression.resolvedType()), args, {{"element.kind", listElementKindText(element)}});
  }
  if (expression.intrinsic() == IntrinsicKind::ListNew) {
    std::vector<serem::ValuePtr> args;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      args.push_back(emitExpression(*argument));
    }
    const Type* element = expression.resolvedType() == nullptr
                              ? nullptr
                              : expression.resolvedType()->elementType();
    return builder_->operation(
        "aggregate.list", lowerType(expression.resolvedType()), std::move(args),
        {{"element", element == nullptr ? std::string{} : element->display()},
         {"element.kind", listElementKindText(element)}});
  }
  if (expression.intrinsic() == IntrinsicKind::Len) {
    const Type* argumentType = expression.arguments().empty()
                                   ? nullptr
                                   : expression.arguments()[0]->resolvedType();
    serem::ValuePtr value = expression.arguments().empty()
                                ? nullptr
                                : emitExpression(*expression.arguments()[0]);
    if (argumentType != nullptr && argumentType->isRecord()) {
      const std::string symbol = methodSymbol(argumentType, "__len__");
      if (!symbol.empty()) {
        return builder_->call(std::make_shared<serem::FunctionRef>(symbol, functions_.at(symbol)),
                              {value}, lowerType(expression.resolvedType()));
      }
    }
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
      value = printable(value, valueType);
      args.push_back(std::move(value));
    }
    return builder_->operation("runtime.print", serem::IRType::voidType(), std::move(args));
  }
  if (expression.intrinsic() == IntrinsicKind::TypeOf) {
    const std::string typeName = expression.arguments().empty() ||
                                         expression.arguments()[0]->resolvedType() == nullptr
                                     ? "Any"
                                     : expression.arguments()[0]->resolvedType()->display();
    return stringValue(typeName);
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
                                             : expression.resolvedType()->name()},
                                {"init", methodSymbol(expression.resolvedType(), "__init__")}});
  }
  if (expression.callee().kind() == NodeKind::MemberExpr && !expression.isMethod()) {
    const auto& member = static_cast<const MemberExpr&>(expression.callee());
    if (member.object().kind() == NodeKind::NameExpr) {
      const auto& object = static_cast<const NameExpr&>(member.object());
      const std::string qualified = object.name() + "_" + member.field();
      const auto found = functions_.find(qualified);
      if (found != functions_.end()) {
        const auto& bound = expression.boundArguments();
        if (!bound.empty()) {
          for (const Expr* argument : bound) if (argument != nullptr) args.push_back(emitExpression(*argument));
        } else {
          for (const auto& argument : expression.arguments()) args.push_back(emitExpression(*argument));
        }
        appendDefaults(qualified, args);
        return builder_->call(std::make_shared<serem::FunctionRef>(qualified, found->second),
                              args, lowerType(expression.resolvedType()));
      }
    }
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
      const std::string method = methodSymbol(member.object().resolvedType(), member.field());
      const std::string symbol = !method.empty() ? method
          : (functions_.contains(qualified) ? qualified : member.field());
      serem::ValuePtr callee = std::make_shared<serem::FunctionRef>(
          symbol, lowerType(expression.callee().resolvedType()));
      args.push_back(emitExpression(member.object()));
      for (const Expr* argument : expression.boundArguments()) {
        if (argument != nullptr) args.push_back(emitExpression(*argument));
      }
      if (expression.boundArguments().empty()) for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
        args.push_back(emitExpression(*argument));
      }
      return builder_->call(std::move(callee), std::move(args), lowerType(expression.resolvedType()));
    }
  }
  // A record value with `__call__` is invoked through that method.
  const Type* signature = expression.callee().resolvedType();
  const Type* callable = signature == nullptr ? nullptr : signature->valueType();
  if (callable != nullptr && callable->isRecord() && callable->methodIndex("__call__") >= 0 &&
      !methodSymbol(callable, "__call__").empty()) {
    std::vector<serem::ValuePtr> arguments;
    for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
      arguments.push_back(emitExpression(*argument));
    }
    return callMethod(callable, "__call__", emitExpression(expression.callee()), arguments);
  }
  serem::ValuePtr callee = emitExpression(expression.callee());
  for (const std::unique_ptr<Expr>& argument : expression.arguments()) {
    const Type* expected = signature != nullptr && args.size() < signature->paramTypes().size()
                               ? signature->paramTypes()[args.size()] : argument->resolvedType();
    args.push_back(coerce(emitExpression(*argument), argument->resolvedType(), expected));
  }
  if (callee != nullptr && callee->valueKind() == serem::ValueKind::FunctionRef)
    appendDefaults(static_cast<const serem::FunctionRef&>(*callee).name(), args);
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
  const Type* objectType = expression.object().resolvedType();
  const Type* record = objectType == nullptr ? nullptr : objectType->valueType();
  if (record != nullptr && record->isRecord() && !expression.isSlice() &&
      expression.hasStart() && record->methodIndex("__getitem__") >= 0) {
    return callMethod(record, "__getitem__", emitExpression(expression.object()),
                      {emitExpression(*expression.start())});
  }
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
    const Type* element = expression.resolvedType()->elementType();
    attributes["element"] = element->display();
    attributes["element.kind"] = listElementKindText(element);
  }
  return builder_->operation("aggregate." + kind, lowerType(expression.resolvedType()),
                             std::move(operands), std::move(attributes));
}

serem::ValuePtr SeremGenerator::emitUnary(const UnaryExpr& expression) {
  // A class operand is converted by its dunder method: `not v` calls `__bool__`
  // and `-v` calls `__neg__`. Records are pointers in the Serem ABI, so the
  // LLVM instruction forms would be invalid.
  const Type* operandType = expression.operand().resolvedType();
  const Type* record = operandType == nullptr ? nullptr : operandType->valueType();
  const char* dunder = nullptr;
  switch (expression.op()) {
  case UnaryOp::Not: dunder = "__bool__"; break;
  case UnaryOp::Invert: dunder = "__invert__"; break;
  case UnaryOp::Neg: dunder = "__neg__"; break;
  case UnaryOp::Pos: dunder = "__pos__"; break;
  default: break;
  }
  if (record != nullptr && record->isRecord() && dunder != nullptr &&
      record->methodIndex(dunder) >= 0) {
    serem::ValuePtr result = callMethod(record, dunder, emitExpression(expression.operand()), {});
    if (result != nullptr) {
      if (expression.op() == UnaryOp::Not) {
        return builder_->operation("not", serem::IRType::boolType(), {result});
      }
      return coerce(result, record->dunderReturn(dunder), expression.resolvedType());
    }
  }
  if (expression.op() == UnaryOp::AddrOf && expression.operand().kind() == NodeKind::IndexExpr) {
    const auto& index = static_cast<const IndexExpr&>(expression.operand());
    return builder_->operation("index.address", lowerType(expression.resolvedType()),
                               {emitExpression(index.object()), emitExpression(*index.start())});
  }
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
