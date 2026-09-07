/// @file IRGenerator.cpp
/// LLVM lowering for typed functions, records, and pointer/memory intrinsics.

#include "sere/codegen/IRGenerator.h"

#include "sere/ast/Syntax.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/source/SourceLocation.h"
#include "sere/types/Intrinsic.h"
#include "sere/types/Type.h"
#include "sere/types/TypeContext.h"

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] const NameExpr* asName(const Expr& expr) {
  if (expr.kind() != NodeKind::NameExpr) {
    return nullptr;
  }
  return static_cast<const NameExpr*>(&expr);
}

[[nodiscard]] bool recordHasTypeId(const Type* type) {
  return type != nullptr && type->isRecord() && !type->isEnum() && !type->isStruct();
}

[[nodiscard]] std::uint32_t recordTypeId(const Type* type) {
  std::uint32_t hash = 2166136261u;
  if (type == nullptr) {
    return hash;
  }
  for (const char character : type->name()) {
    hash ^= static_cast<unsigned char>(character);
    hash *= 16777619u;
  }
  return hash;
}

[[nodiscard]] unsigned llvmFieldIndex(const Type* type, int semanticIndex) {
  unsigned llvm = recordHasTypeId(type) ? 1u : 0u;
  if (type == nullptr) {
    return llvm;
  }
  const auto& fields = type->fields();
  const int limit = semanticIndex < 0 ? 0 : semanticIndex;
  for (int index = 0; index < limit && static_cast<std::size_t>(index) < fields.size(); ++index) {
    const RecordField& field = fields[static_cast<std::size_t>(index)];
    if (!field.isStatic && field.stored) {
      llvm += 1;
    }
  }
  return llvm;
}

void appendExceptionName(std::string& chain, std::string_view name) {
  if (name.empty()) {
    return;
  }
  std::size_t pos = 0;
  while (pos < chain.size()) {
    const std::size_t end = chain.find(';', pos);
    const std::size_t stop = end == std::string::npos ? chain.size() : end;
    if (chain.compare(pos, stop - pos, name) == 0) {
      return;
    }
    pos = stop == chain.size() ? chain.size() : stop + 1;
  }
  if (!chain.empty()) {
    chain += ';';
  }
  chain.append(name);
}

void appendExceptionType(std::string& chain, const Type* type) {
  if (type == nullptr) {
    return;
  }
  appendExceptionName(chain, type->name());
  const Type* canonical = type->canonical();
  if (canonical != nullptr && canonical != type) {
    appendExceptionName(chain, canonical->name());
    type = canonical;
  }
  for (const Type* base : type->bases()) {
    appendExceptionType(chain, base);
  }
}

[[nodiscard]] std::optional<bool> constBool(const Expr& expr) {
  if (expr.kind() == NodeKind::BooleanLiteral) {
    return static_cast<const BooleanLiteral&>(expr).value();
  }
  if (const NameExpr* name = asName(expr)) {
    if (name->hasCompileTimeBool()) {
      return name->compileTimeBool();
    }
  }
  if (expr.kind() == NodeKind::UnaryExpr) {
    const auto& unary = static_cast<const UnaryExpr&>(expr);
    if (unary.op() == UnaryOp::Not) {
      const std::optional<bool> inner = constBool(unary.operand());
      if (inner.has_value()) {
        return !*inner;
      }
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::string llvmNameFor(const FunctionDef& function) {
  if (function.isExtern()) {
    return function.externName();
  }
  if (function.isMethod()) {
    if (!function.modulePrefix().empty()) {
      return function.modulePrefix() + "_" + function.ownerClass() + "_" + function.name();
    }
    return function.ownerClass() + "_" + function.name();
  }
  if (function.name() == "main" && !function.isExtern()) {
    return "sere_main";
  }
  if (!function.modulePrefix().empty()) {
    return function.modulePrefix() + "_" + function.name();
  }
  return function.name();
}

[[nodiscard]] std::string decoratorSlotName(const FunctionDef& function) {
  return "sere.dec." + llvmNameFor(function);
}

[[nodiscard]] std::string classDecoratorSlotName(const ClassDef& classDef) {
  return "sere.dec.class." + classDef.name();
}

[[nodiscard]] llvm::Value*
matchParamType(llvm::IRBuilder<>& builder, llvm::Value* value, llvm::Type* wanted) {
  if (value == nullptr || wanted == nullptr || value->getType() == wanted) {
    return value;
  }
  if (value->getType()->isIntegerTy() && wanted->isIntegerTy()) {
    return builder.CreateIntCast(value, wanted, true);
  }
  llvm::Value* tmp = builder.CreateAlloca(value->getType(), nullptr, "match.tmp");
  builder.CreateStore(value, tmp);
  return builder.CreateLoad(wanted, tmp);
}

void matchCallArgs(llvm::IRBuilder<>& builder,
                   llvm::Function* callee,
                   std::vector<llvm::Value*>& args) {
  if (callee == nullptr) {
    return;
  }
  llvm::FunctionType* type = callee->getFunctionType();
  const unsigned count = type->getNumParams();
  for (unsigned index = 0; index < args.size() && index < count; ++index) {
    args[index] = matchParamType(builder, args[index], type->getParamType(index));
  }
}

[[nodiscard]] bool isStrLlvmType(llvm::Type* type) {
  auto* structType = llvm::dyn_cast<llvm::StructType>(type);
  if (structType == nullptr || structType->getNumElements() != 2) {
    return false;
  }
  return structType->getElementType(0)->isPointerTy() &&
         structType->getElementType(1)->isIntegerTy(64);
}

} // namespace

IRGenerator::IRGenerator(llvm::LLVMContext& context,
                         DiagnosticEngine& diagnostics,
                         TypeContext& types)
    : context_(&context), diagnostics_(&diagnostics), types_(&types) {
}

const Type* IRGenerator::resolveType(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }
  type = type->canonical();
  if (subst_.empty()) {
    return type;
  }
  const auto found = subst_.find(type->name());
  return found != subst_.end() ? found->second : types_->substitute(type, subst_);
}

llvm::Type* IRGenerator::lower(const Type* type) {
  if (type == nullptr) {
    return llvm::Type::getVoidTy(*context_);
  }
  type = resolveType(type);
  if (type->isTypeParam()) {
    const auto found = subst_.find(type->name());
    if (found != subst_.end()) {
      return lower(found->second);
    }
    return llvm::Type::getInt32Ty(*context_);
  }
  const auto found = lowered_.find(type);
  if (found != lowered_.end()) {
    return found->second;
  }
  if (type->kind() == TypeKind::Record && !type->isEnum()) {
    llvm::StructType* opaque = llvm::StructType::create(*context_, type->name());
    lowered_[type] = opaque;
    std::vector<llvm::Type*> fields;
    if (recordHasTypeId(type)) {
      fields.push_back(llvm::Type::getInt32Ty(*context_));
    }
    for (const RecordField& field : type->fields()) {
      if (!field.isStatic && field.stored) {
        fields.push_back(lower(field.type));
      }
    }
    opaque->setBody(fields);
    return opaque;
  }
  llvm::Type* llvmType = nullptr;
  if (type->isPointerLike() || type->isSequence() || type->isDict()) {
    llvmType = llvm::PointerType::getUnqual(*context_);
  } else if (type->isVoidLike()) {
    llvmType = llvm::Type::getVoidTy(*context_);
  } else if (type->isAny()) {
    llvmType = llvm::StructType::get(
        *context_,
        {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)});
  } else if (type->isNamed("bool") || type->isNamed("i8") || type->isNamed("u8")) {
    llvmType = llvm::Type::getIntNTy(*context_, type->isNamed("bool") ? 1 : 8);
  } else if (type->isNamed("i16") || type->isNamed("u16")) {
    llvmType = llvm::Type::getInt16Ty(*context_);
  } else if (type->isNamed("i32") || type->isNamed("u32")) {
    llvmType = llvm::Type::getInt32Ty(*context_);
  } else if (type->isNamed("i64") || type->isNamed("u64")) {
    llvmType = llvm::Type::getInt64Ty(*context_);
  } else if (type->isNamed("f32")) {
    llvmType = llvm::Type::getFloatTy(*context_);
  } else if (type->isNamed("f64")) {
    llvmType = llvm::Type::getDoubleTy(*context_);
  } else if (type->isEnum()) {
    if (type->hasEnumPayload()) {
      llvmType = llvm::StructType::get(
          *context_, {llvm::Type::getInt32Ty(*context_), llvm::PointerType::getUnqual(*context_)});
    } else {
      llvmType = llvm::Type::getInt32Ty(*context_);
    }
  } else if (type->isNamed("never")) {
    llvmType = llvm::Type::getInt8Ty(*context_);
  } else if (type->isUnion()) {
    llvm::Type* common = nullptr;
    bool same = !type->args().empty();
    for (const Type* member : type->args()) {
      llvm::Type* lowered = lower(member);
      if (common == nullptr) {
        common = lowered;
      } else if (lowered != common) {
        same = false;
      }
    }
    llvmType = same && common != nullptr
                   ? common
                   : llvm::StructType::get(
                         *context_,
                         {llvm::Type::getInt32Ty(*context_), llvm::Type::getInt64Ty(*context_)});
  } else if (type->isStrLayout()) {
    llvmType = llvm::StructType::get(
        *context_, {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
  } else if (type->isTypeObject() || type->isClassConstraint()) {
    llvmType = llvm::Type::getInt32Ty(*context_);
  } else if (type->isCallableConstraint() || type->kind() == TypeKind::Function) {
    llvmType = llvm::PointerType::getUnqual(*context_);
  } else if (type->kind() == TypeKind::Generic && type->name() == "tuple") {
    std::vector<llvm::Type*> elems;
    for (const Type* arg : type->args()) {
      elems.push_back(lower(arg));
    }
    llvmType = llvm::StructType::get(*context_, elems);
  } else {
    llvmType = llvm::PointerType::getUnqual(*context_);
  }
  lowered_[type] = llvmType;
  return llvmType;
}

std::uint64_t IRGenerator::valueSize(const Type* type) {
  if (type == nullptr) {
    return 0;
  }

  llvm::Type* llvmType = lower(type);

  if (llvmType == nullptr || llvmType->isVoidTy()) {
    return 0;
  }

  const llvm::DataLayout& layout = module_->getDataLayout();

  if (!llvmType->isSized()) {
    return 0;
  }

  return layout.getTypeAllocSize(llvmType).getFixedValue();
}

llvm::Function* IRGenerator::runtimeDecl(const char* name,
                                         llvm::Type* returnType,
                                         const std::vector<llvm::Type*>& params) {
  if (llvm::Function* existing = module_->getFunction(name)) {
    return existing;
  }
  llvm::FunctionType* type = llvm::FunctionType::get(returnType, params, false);
  return llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, module_);
}

llvm::FunctionType* IRGenerator::llvmFunctionType(const FunctionDef& function) {
  const Type* fnType = function.resolvedType();
  if (fnType == nullptr) {
    return llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), false);
  }
  std::vector<llvm::Type*> params;
  if (!function.captures().empty()) {
    params.push_back(llvm::PointerType::getUnqual(*context_));
  }
  const std::vector<const Type*>& sereParams = fnType->paramTypes();
  for (std::size_t index = 0; index < sereParams.size(); ++index) {
    if (function.isMethod() && index == 0) {
      params.push_back(llvm::PointerType::getUnqual(*context_));
      continue;
    }
    const Type* sereType = sereParams[index];
    if (function.isExtern() && sereType->isStrLayout()) {
      params.push_back(llvm::PointerType::getUnqual(*context_));
      params.push_back(llvm::Type::getInt64Ty(*context_));
    } else {
      params.push_back(lower(sereType));
    }
  }
  if (function.isExtern() && fnType->returnType() != nullptr &&
      fnType->returnType()->isStrLayout()) {
    params.push_back(llvm::PointerType::getUnqual(*context_));
    params.push_back(llvm::PointerType::getUnqual(*context_));
    return llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), params, false);
  }
  // An async function is a coroutine: its ramp returns the coroutine handle
  // (a pointer), never the declared result value. The result travels through
  // the coroutine promise instead.
  if (function.isAsync()) {
    return llvm::FunctionType::get(llvm::PointerType::getUnqual(*context_), params, false);
  }
  return llvm::FunctionType::get(lower(fnType->returnType()), params, false);
}

void IRGenerator::rememberLocal(const std::string& name,
                                llvm::Value* allocaInst,
                                const Type* type) {
  locals_[name] = allocaInst;
  if (type != nullptr && (type->isGenericCtor("Unique") || type->isGenericCtor("Shared"))) {
    dropStack_.emplace_back(allocaInst, type);
  }
}

void IRGenerator::emitDrops(llvm::IRBuilder<>& builder) {
  llvm::Function* freeFn = runtimeDecl("sere_free", builder.getVoidTy(), {builder.getPtrTy()});
  llvm::Function* releaseFn =
      runtimeDecl("sere_shared_release", builder.getVoidTy(), {builder.getPtrTy()});
  for (auto it = dropStack_.rbegin(); it != dropStack_.rend(); ++it) {
    llvm::Value* pointer = builder.CreateLoad(builder.getPtrTy(), it->first);
    llvm::Value* isNull = builder.CreateIsNull(pointer);
    llvm::BasicBlock* dropBlock =
        llvm::BasicBlock::Create(*context_, "drop", builder.GetInsertBlock()->getParent());
    llvm::BasicBlock* contBlock =
        llvm::BasicBlock::Create(*context_, "drop.cont", builder.GetInsertBlock()->getParent());
    builder.CreateCondBr(isNull, contBlock, dropBlock);
    builder.SetInsertPoint(dropBlock);
    if (it->second->isGenericCtor("Shared")) {
      builder.CreateCall(releaseFn, {pointer});
    } else {
      builder.CreateCall(freeFn, {pointer});
    }
    builder.CreateBr(contBlock);
    builder.SetInsertPoint(contBlock);
  }
}

int IRGenerator::dictKeyKind(const Type* keyType) {
  if (keyType == nullptr) {
    return 0;
  }
  keyType = keyType->canonical();
  if (keyType->isNamed("str")) {
    return 3;
  }
  if (keyType->isNamed("i32") || keyType->isNamed("u32")) {
    return 1;
  }
  if (keyType->isNamed("i64") || keyType->isNamed("u64")) {
    return 2;
  }
  return 0;
}

std::string IRGenerator::classStaticName(const Type* record, std::string_view field) {
  if (record == nullptr) {
    return std::string(field);
  }
  record = record->canonical();
  if (record->isTypeObject() && record->typeObjectInstance() != nullptr) {
    record = record->typeObjectInstance()->canonical();
  }
  const std::string owner =
      record->qualifier().empty() ? record->name() : record->qualifier() + "." + record->name();
  return owner + "." + std::string(field);
}

std::string IRGenerator::functionStaticName(const FunctionDef& function, std::string_view name) {
  if (function.isMethod()) {
    return function.ownerClass() + "_" + function.name() + "." + std::string(name);
  }
  return function.name() + "." + std::string(name);
}

llvm::Value* IRGenerator::declareGlobal(const std::string& name, const Type* type) {
  const auto found = globals_.find(name);
  if (found != globals_.end()) {
    return found->second;
  }
  llvm::Type* llvmType = lower(type);
  auto* global = new llvm::GlobalVariable(*module_,
                                          llvmType,
                                          false,
                                          llvm::GlobalValue::InternalLinkage,
                                          llvm::Constant::getNullValue(llvmType),
                                          name);
  globals_[name] = global;
  return global;
}

llvm::Value*
IRGenerator::emitTempSlot(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* type) {
  llvm::Value* slot = builder.CreateAlloca(lower(type), nullptr, "tmp.slot");
  builder.CreateStore(value, slot);
  return slot;
}

llvm::Value* IRGenerator::emitIndexI64(llvm::IRBuilder<>& builder, const Expr& index) {
  llvm::Value* value = emitExpr(builder, index);
  if (value == nullptr) {
    return nullptr;
  }
  if (value->getType() != builder.getInt64Ty()) {
    value = builder.CreateSExt(value, builder.getInt64Ty());
  }
  return value;
}

void IRGenerator::declareGlobals(const Module& ast) {
  for (const std::unique_ptr<Stmt>& statement : ast.statements()) {
    if (statement->kind() == NodeKind::VarDecl) {
      const auto& decl = static_cast<const VarDecl&>(*statement);
      declareGlobal(decl.name(), decl.resolvedType());
      continue;
    }
    if (statement->kind() == NodeKind::ClassDef) {
      const auto& classDef = static_cast<const ClassDef&>(*statement);
      const Type* record = classDef.resolvedType();
      for (const FieldDecl& field : classDef.fields()) {
        if (!field.isStatic || field.type == nullptr || field.type->resolvedType() == nullptr) {
          continue;
        }
        const RecordField* info = record == nullptr ? nullptr : record->findField(field.name);
        const std::string globalName = info != nullptr && !info->llvmName.empty()
                                           ? info->llvmName
                                           : classStaticName(record, field.name);
        declareGlobal(globalName, field.type->resolvedType());
      }
      if (classDef.decoratedType() != nullptr) {
        llvm::Value* slot =
            declareGlobal(classDecoratorSlotName(classDef), classDef.decoratedType());
        decoratorSlots_[classDecoratorSlotName(classDef)] = slot;
      }
      for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
        if (method->decoratedType() != nullptr) {
          llvm::Value* slot = declareGlobal(decoratorSlotName(*method), method->decoratedType());
          decoratorSlots_[llvmNameFor(*method)] = slot;
        }
        for (const std::unique_ptr<Stmt>& bodyStmt : method->body()) {
          if (bodyStmt->kind() == NodeKind::VarDecl) {
            rememberStaticDecl(
                functionStaticName(*method, static_cast<const VarDecl&>(*bodyStmt).name()),
                *method,
                static_cast<const VarDecl&>(*bodyStmt));
          }
        }
      }
      continue;
    }
    if (statement->kind() != NodeKind::FunctionDef) {
      continue;
    }
    const auto& function = static_cast<const FunctionDef&>(*statement);
    if (function.decoratedType() != nullptr) {
      llvm::Value* slot = declareGlobal(decoratorSlotName(function), function.decoratedType());
      decoratorSlots_[llvmNameFor(function)] = slot;
    }
    for (const std::unique_ptr<Stmt>& bodyStmt : function.body()) {
      if (bodyStmt->kind() == NodeKind::VarDecl) {
        rememberStaticDecl(
            functionStaticName(function, static_cast<const VarDecl&>(*bodyStmt).name()),
            function,
            static_cast<const VarDecl&>(*bodyStmt));
      }
    }
  }
}

void IRGenerator::rememberStaticDecl(const std::string& name,
                                     const FunctionDef& function,
                                     const VarDecl& decl) {
  (void)function;
  if (!decl.isStatic()) {
    return;
  }
  declareGlobal(name, decl.resolvedType());
}

void IRGenerator::emitModuleInitFn(const Module& ast, const std::vector<const Module*>* imported) {
  llvm::FunctionType* type = llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), false);
  moduleInitFn_ =
      llvm::Function::Create(type, llvm::Function::InternalLinkage, "sere.module.init", module_);
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", moduleInitFn_);
  llvm::IRBuilder<> builder(entry);
  llvm::GlobalVariable* guard = new llvm::GlobalVariable(*module_,
                                                         builder.getInt8Ty(),
                                                         false,
                                                         llvm::GlobalValue::InternalLinkage,
                                                         builder.getInt8(0),
                                                         "sere.module.init.done");
  llvm::Value* done = builder.CreateLoad(builder.getInt8Ty(), guard);
  llvm::BasicBlock* initBlock = llvm::BasicBlock::Create(*context_, "init", moduleInitFn_);
  llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context_, "end", moduleInitFn_);
  builder.CreateCondBr(builder.CreateICmpEQ(done, builder.getInt8(0)), initBlock, endBlock);
  builder.SetInsertPoint(initBlock);
  auto wrapDecorated = [&](const FunctionDef& function) {
    if (function.decoratedType() == nullptr) {
      return;
    }
    llvm::Value* slot = decoratorSlots_[llvmNameFor(function)];
    llvm::Function* raw = functions_[llvmNameFor(function)];
    if (slot == nullptr || raw == nullptr) {
      return;
    }
    llvm::Value* current = packCallable(builder, raw, nullptr);
    const Type* currentType = function.resolvedType();
    const std::vector<std::unique_ptr<Expr>>& exprs = function.decoratorExprs();
    for (int index = static_cast<int>(exprs.size()) - 1; index >= 0; --index) {
      if (exprs[static_cast<std::size_t>(index)] == nullptr) {
        continue;
      }
      Expr& deco = *exprs[static_cast<std::size_t>(index)];
      if (isReservedDecoratorExpr(deco)) {
        continue;
      }
      llvm::Value* wrapper = emitExpr(builder, deco);
      const Type* wrapperType = deco.resolvedType();
      llvm::FunctionType* llvmFn = nullptr;
      if (wrapperType != nullptr && wrapperType->kind() == TypeKind::Function) {
        llvmFn = llvmFunctionTypeFrom(wrapperType);
      } else if (wrapperType != nullptr) {
        // The decorator value is a non-function callable, typically a bare
        // `Callable` constraint produced by a factory such as `@route("/x")`.
        // It is invoked with the decorated object and, Python-like, assumed to
        // return it, so the result keeps the decorated object's type/ABI rather
        // than being mis-assumed to return void.
        const Type* resultType = wrapperType->returnType();
        llvm::Type* ret = nullptr;
        if (resultType != nullptr && !resultType->isVoidLike() && !resultType->isAny()) {
          ret = lower(resultType);
        }
        if (ret == nullptr && currentType != nullptr) {
          ret = lower(currentType);
        }
        if (ret == nullptr) {
          ret = llvm::Type::getVoidTy(*context_);
        }
        llvmFn = llvm::FunctionType::get(ret, {lower(currentType)}, false);
      }
      if (wrapper == nullptr || llvmFn == nullptr) {
        continue;
      }
      if (wrapperType != nullptr && wrapperType->kind() == TypeKind::Function &&
          !wrapperType->paramTypes().empty()) {
        current = emitCoerce(builder, current, currentType, wrapperType->paramTypes()[0]);
        currentType = wrapperType->returnType();
      }
      current = emitIndirectCallable(builder, wrapper, llvmFn, {current});
    }
    if (current != nullptr) {
      builder.CreateStore(current, slot);
    }
  };
  auto emitInits = [&](const Module& source) {
    for (const std::unique_ptr<Stmt>& statement : source.statements()) {
      if (statement->kind() == NodeKind::VarDecl) {
        const auto& decl = static_cast<const VarDecl&>(*statement);
        llvm::Value* slot = globals_[decl.name()];
        llvm::Value* init = decl.init() == nullptr ? emitDefault(decl.resolvedType())
                                                   : emitCoerce(builder,
                                                                emitExpr(builder, *decl.init()),
                                                                decl.init()->resolvedType(),
                                                                decl.resolvedType());
        if (slot != nullptr && init != nullptr) {
          builder.CreateStore(init, slot);
        }
      } else if (statement->kind() == NodeKind::ClassDef) {
        const auto& classDef = static_cast<const ClassDef&>(*statement);
        const Type* record = classDef.resolvedType();
        for (const FieldDecl& field : classDef.fields()) {
          if (!field.isStatic || field.init == nullptr) {
            continue;
          }
          const RecordField* info = record == nullptr ? nullptr : record->findField(field.name);
          const std::string globalName = info != nullptr && !info->llvmName.empty()
                                             ? info->llvmName
                                             : classStaticName(record, field.name);
          llvm::Value* slot = globals_[globalName];
          llvm::Value* init = emitCoerce(builder,
                                         emitExpr(builder, *field.init),
                                         field.init->resolvedType(),
                                         field.type->resolvedType());
          if (slot != nullptr && init != nullptr) {
            builder.CreateStore(init, slot);
          }
        }
        for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
          wrapDecorated(*method);
        }
      } else if (statement->kind() == NodeKind::EnumDef) {
        for (const std::unique_ptr<FunctionDef>& method :
             static_cast<const EnumDef&>(*statement).methods()) {
          wrapDecorated(*method);
        }
      } else if (statement->kind() == NodeKind::FunctionDef) {
        wrapDecorated(static_cast<const FunctionDef&>(*statement));
      }
    }
  };
  if (imported != nullptr) {
    for (const Module* extra : *imported) {
      if (extra != nullptr) {
        emitInits(*extra);
      }
    }
  }
  emitInits(ast);
  builder.CreateStore(builder.getInt8(1), guard);
  builder.CreateBr(endBlock);
  builder.SetInsertPoint(endBlock);
  builder.CreateRetVoid();
}

llvm::Value* IRGenerator::emitDefault(const Type* type) {
  llvm::Type* llvmType = lower(type);
  if (llvmType == nullptr || llvmType->isVoidTy()) {
    return nullptr;
  }
  return llvm::Constant::getNullValue(llvmType);
}

unsigned IRGenerator::enumTagFromField(const RecordField* field) {
  if (field == nullptr || field->llvmName.empty()) {
    return 0;
  }
  char* end = nullptr;
  const long long parsed = std::strtoll(field->llvmName.c_str(), &end, 10);
  if (end == field->llvmName.c_str() || *end != '\0') {
    return 0;
  }
  if (parsed < std::numeric_limits<std::int32_t>::min() ||
      parsed > std::numeric_limits<std::int32_t>::max()) {
    return 0;
  }
  return static_cast<unsigned>(static_cast<std::int32_t>(parsed));
}

llvm::Value* IRGenerator::createLocalSlot(llvm::IRBuilder<>& builder,
                                          const std::string& name,
                                          const Type* type) {
  llvm::Type* llvmType = lower(type);
  if (llvmType == nullptr || llvmType->isVoidTy()) {
    return nullptr;
  }
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::IRBuilder<> entry(&function->getEntryBlock(), function->getEntryBlock().begin());
  llvm::Value* slot = entry.CreateAlloca(llvmType, nullptr, name);
  rememberLocal(name, slot, type);
  return slot;
}

void IRGenerator::widenIntegerPair(llvm::IRBuilder<>& builder,
                                   llvm::Value*& left,
                                   llvm::Value*& right,
                                   const Type* leftType,
                                   const Type* rightType) {
  if (left == nullptr || right == nullptr || left->getType() == right->getType()) {
    return;
  }
  if (left->getType()->isFloatingPointTy() && right->getType()->isFloatingPointTy()) {
    llvm::Type* wide = left->getType()->isDoubleTy() || right->getType()->isDoubleTy()
                           ? builder.getDoubleTy()
                           : left->getType();
    if (left->getType() != wide) {
      left = builder.CreateFPExt(left, wide);
    }
    if (right->getType() != wide) {
      right = builder.CreateFPExt(right, wide);
    }
    return;
  }
  if (!left->getType()->isIntegerTy() || !right->getType()->isIntegerTy()) {
    if (left->getType()->isIntegerTy() && right->getType()->isFloatingPointTy()) {
      left = (leftType != nullptr && leftType->isUnsignedInteger())
                 ? builder.CreateUIToFP(left, right->getType())
                 : builder.CreateSIToFP(left, right->getType());
    } else if (right->getType()->isIntegerTy() && left->getType()->isFloatingPointTy()) {
      right = (rightType != nullptr && rightType->isUnsignedInteger())
                  ? builder.CreateUIToFP(right, left->getType())
                  : builder.CreateSIToFP(right, left->getType());
    }
    return;
  }
  llvm::Type* wide = left->getType()->getIntegerBitWidth() >= right->getType()->getIntegerBitWidth()
                         ? left->getType()
                         : right->getType();
  if (left->getType() != wide) {
    left = (leftType != nullptr && leftType->isUnsignedInteger()) ? builder.CreateZExt(left, wide)
                                                                  : builder.CreateSExt(left, wide);
  }
  if (right->getType() != wide) {
    right = (rightType != nullptr && rightType->isUnsignedInteger())
                ? builder.CreateZExt(right, wide)
                : builder.CreateSExt(right, wide);
  }
}

llvm::Value* IRGenerator::emitStrCompare(llvm::IRBuilder<>& builder,
                                         BinaryOp op,
                                         llvm::Value* left,
                                         llvm::Value* right) {
  llvm::Function* cmpFn = runtimeDecl(
      "sere_str_cmp",
      builder.getInt32Ty(),
      {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()});
  llvm::Value* cmp = builder.CreateCall(cmpFn,
                                        {builder.CreateExtractValue(left, {0}),
                                         builder.CreateExtractValue(left, {1}),
                                         builder.CreateExtractValue(right, {0}),
                                         builder.CreateExtractValue(right, {1})});
  switch (op) {
  case BinaryOp::Eq:
  case BinaryOp::Is:
    return builder.CreateICmpEQ(cmp, builder.getInt32(0));
  case BinaryOp::Ne:
  case BinaryOp::IsNot:
    return builder.CreateICmpNE(cmp, builder.getInt32(0));
  case BinaryOp::Lt:
    return builder.CreateICmpSLT(cmp, builder.getInt32(0));
  case BinaryOp::Le:
    return builder.CreateICmpSLE(cmp, builder.getInt32(0));
  case BinaryOp::Gt:
    return builder.CreateICmpSGT(cmp, builder.getInt32(0));
  case BinaryOp::Ge:
    return builder.CreateICmpSGE(cmp, builder.getInt32(0));
  default:
    return builder.getInt1(false);
  }
}

void IRGenerator::emitReturn(llvm::IRBuilder<>& builder,
                             llvm::Value* value,
                             const Type* returnType) {
  emitDeferred(builder, returnType);
  emitDrops(builder);
  if (asyncFn_) {
    if (asyncPromise_ != nullptr && value != nullptr && !value->getType()->isVoidTy()) {
      llvm::Value* stored = value;
      if (stored->getType() != asyncResultTy_) {
        if (stored->getType()->isIntegerTy() && asyncResultTy_ != nullptr &&
            asyncResultTy_->isIntegerTy()) {
          stored = builder.CreateIntCast(stored, asyncResultTy_, true);
        } else {
          stored = nullptr;
        }
      }
      if (stored != nullptr) {
        builder.CreateStore(stored, asyncPromise_);
      }
    }
    builder.CreateBr(asyncFinal_);
    return;
  }
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::Type* llvmRet = function->getReturnType();
  if (llvmRet->isVoidTy()) {
    builder.CreateRetVoid();
    return;
  }
  const bool sereVoid = returnType != nullptr && returnType->isVoidLike();
  if (sereVoid || value == nullptr || value->getType()->isVoidTy()) {
    builder.CreateRet(llvm::Constant::getNullValue(llvmRet));
    return;
  }
  if (value->getType() != llvmRet) {
    if (value->getType()->isIntegerTy() && llvmRet->isIntegerTy()) {
      value = builder.CreateIntCast(value, llvmRet, true);
    } else {
      value = emitDefault(returnType);
    }
  }
  if (value == nullptr || value->getType()->isVoidTy() || value->getType() != llvmRet) {
    value = llvm::Constant::getNullValue(llvmRet);
  }
  builder.CreateRet(value);
}

namespace {

llvm::Value* bitsFromValue(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* from) {
  if (value == nullptr || from == nullptr) {
    return builder.getInt64(0);
  }
  if (from->isScalarInteger() || from->isNamed("bool")) {
    if (!value->getType()->isIntegerTy()) {
      return builder.getInt64(0);
    }
    return builder.CreateZExt(value, builder.getInt64Ty());
  }
  if (from->isEnum()) {
    if (value->getType()->isStructTy()) {
      value = builder.CreateExtractValue(value, {0});
    }
    return builder.CreateZExt(value, builder.getInt64Ty());
  }
  if (from->isNamed("f32")) {
    return builder.CreateZExt(builder.CreateBitCast(value, builder.getInt32Ty()),
                              builder.getInt64Ty());
  }
  if (from->isNamed("f64")) {
    return builder.CreateBitCast(value, builder.getInt64Ty());
  }
  if (from->isPointerLike() || from->isSequence() || from->isDict()) {
    return builder.CreatePtrToInt(value, builder.getInt64Ty());
  }
  return builder.getInt64(0);
}

llvm::Value*
valueFromBits(llvm::IRBuilder<>& builder, llvm::Value* bits, const Type* to, llvm::Type* dest) {
  if (to == nullptr || dest == nullptr) {
    return bits;
  }
  if (to->isScalarInteger() || to->isNamed("bool")) {
    return builder.CreateTrunc(bits, dest);
  }
  if (to->isEnum()) {
    llvm::Value* tag = builder.CreateTrunc(bits, builder.getInt32Ty());
    if (!dest->isStructTy()) {
      return tag;
    }
    llvm::Value* agg = llvm::UndefValue::get(dest);
    agg = builder.CreateInsertValue(agg, tag, {0});
    agg = builder.CreateInsertValue(agg, llvm::ConstantPointerNull::get(builder.getPtrTy()), {1});
    return agg;
  }
  if (to->isNamed("f32")) {
    return builder.CreateBitCast(builder.CreateTrunc(bits, builder.getInt32Ty()), dest);
  }
  if (to->isNamed("f64")) {
    return builder.CreateBitCast(bits, dest);
  }
  if (to->isPointerLike() || to->isSequence() || to->isDict()) {
    return builder.CreateIntToPtr(bits, dest);
  }
  return llvm::Constant::getNullValue(dest);
}

} // namespace

llvm::Value*
IRGenerator::emitAnyTypeMatch(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* target) {
  target = target->canonical();
  if (target->isAny()) {
    return builder.getTrue();
  }
  llvm::Value* tag = builder.CreateExtractValue(value, {0});
  // A default-initialized Any represents None.
  tag = builder.CreateSelect(builder.CreateIsNull(tag), builder.CreateGlobalString("None"), tag);
  llvm::Value* wanted =
      builder.CreateGlobalString(target->isVoidLike() ? "None" : target->display());
  llvm::Function* compare =
      runtimeDecl("strcmp", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
  return builder.CreateICmpEQ(builder.CreateCall(compare, {tag, wanted}), builder.getInt32(0));
}

llvm::Value* IRGenerator::emitCoerce(llvm::IRBuilder<>& builder,
                                     llvm::Value* value,
                                     const Type* from,
                                     const Type* to) {
  if (to != nullptr && to->isVoidLike()) {
    return nullptr;
  }
  if (from == nullptr || to == nullptr) {
    return value;
  }
  from = resolveType(from);
  to = resolveType(to);
  if (to->isAny() && !from->isAny()) {
    if (std::find(boxedTypes_.begin(), boxedTypes_.end(), from) == boxedTypes_.end()) {
      boxedTypes_.push_back(from);
    }
    llvm::Value* memory = llvm::ConstantPointerNull::get(builder.getPtrTy());
    if (!from->isVoidLike() && value != nullptr && !value->getType()->isVoidTy()) {
      llvm::Function* alloc = runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
      // Use the LLVM allocation size, including aggregate padding and closures.
      llvm::Constant* size = llvm::ConstantExpr::getSizeOf(value->getType());
      memory = builder.CreateCall(alloc, {size});
      builder.CreateStore(value, memory);
    }
    llvm::Value* packed = llvm::UndefValue::get(lower(to));
    packed = builder.CreateInsertValue(
        packed, builder.CreateGlobalString(from->isVoidLike() ? "None" : from->display()), {0});
    return builder.CreateInsertValue(packed, memory, {1});
  }
  if (from->isVoidLike()) {
    return emitDefault(to);
  }
  if (value == nullptr) {
    return value;
  }
  if (from == to) {
    return value;
  }
  if (from->valueType() != from && !to->isRecord()) {
    llvm::Value* payload =
        builder.CreateExtractValue(value, {llvmFieldIndex(from, from->fieldIndex("$value"))});
    return emitCoerce(builder, payload, from->valueType(), to);
  }
  llvm::Type* fromTy = lower(from);
  llvm::Type* toTy = lower(to);
  if (from->isAny()) {
    llvm::Function* function = builder.GetInsertBlock()->getParent();
    auto* success = llvm::BasicBlock::Create(*context_, "any.checked", function);
    auto* failure = llvm::BasicBlock::Create(*context_, "any.mismatch", function);
    builder.CreateCondBr(emitAnyTypeMatch(builder, value, to), success, failure);
    builder.SetInsertPoint(failure);
    llvm::Value* message = emitStrLiteral(builder, "Any type mismatch: expected " + to->display());
    llvm::Function* panic =
        runtimeDecl("sere_panic", builder.getVoidTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    builder.CreateCall(
        panic,
        {builder.CreateExtractValue(message, {0}), builder.CreateExtractValue(message, {1})});
    builder.CreateUnreachable();
    builder.SetInsertPoint(success);
    return builder.CreateLoad(toTy, builder.CreateExtractValue(value, {1}));
  }
  if (to->isUnion()) {
    if (fromTy == toTy) {
      return value;
    }
    int tag = to->unionMemberIndex(from);
    if (tag < 0) {
      for (std::size_t index = 0; index < to->args().size(); ++index) {
        const Type* member = to->args()[index];
        if (member != nullptr && member->canonical() == from) {
          tag = static_cast<int>(index);
          break;
        }
      }
    }
    llvm::Value* packed = llvm::UndefValue::get(toTy);
    packed = builder.CreateInsertValue(packed, builder.getInt32(tag < 0 ? 0 : tag), {0});
    packed = builder.CreateInsertValue(packed, bitsFromValue(builder, value, from), {1});
    return packed;
  }
  if (from->isUnion() && fromTy != toTy) {
    return valueFromBits(builder, builder.CreateExtractValue(value, {1}), to, toTy);
  }
  if (from->isRecord() && to->isRecord() && fromTy != toTy) {
    if (from->isSubtypeOf(to) || valueSize(from) == valueSize(to)) {
      llvm::Value* tmp = builder.CreateAlloca(fromTy, nullptr, "coerce.tmp");
      builder.CreateStore(value, tmp);
      return builder.CreateLoad(toTy, tmp);
    }
  }
  if ((from->isInteger() || from->isNamed("bool") || from->isFloat()) &&
      (to->isInteger() || to->isNamed("bool") || to->isFloat())) {
    return emitNumericCast(builder, value, from, to);
  }
  if (from->isIntEnum() && to->isInteger()) {
    return emitNumericCast(builder, emitEnumTag(builder, value), types_->i32Type(), to);
  }
  if (from->isTypeObject() && to->isCallableConstraint()) {
    return packCallable(builder, emitConstructorThunk(from->typeObjectInstance()), nullptr);
  }
  if ((from->kind() == TypeKind::Function || from->isCallableConstraint()) &&
      (to->isCallableConstraint() || to->kind() == TypeKind::Function)) {
    return value;
  }
  return value;
}

void IRGenerator::appendDefaultArgs(llvm::IRBuilder<>& builder,
                                    std::vector<llvm::Value*>& args,
                                    const FunctionDef& function,
                                    std::size_t provided,
                                    std::size_t skip) {
  for (std::size_t index = skip + provided; index < function.params().size(); ++index) {
    if (function.params()[index].defaultValue == nullptr) {
      break;
    }
    llvm::Value* value = emitExpr(builder, *function.params()[index].defaultValue);
    const Type* fromType = function.params()[index].defaultValue->resolvedType();
    const Type* toType = function.params()[index].type == nullptr
                             ? nullptr
                             : function.params()[index].type->resolvedType();
    args.push_back(emitCoerce(builder, value, fromType, toType));
  }
}

void IRGenerator::appendBoundCallArgs(llvm::IRBuilder<>& builder,
                                      const CallExpr& expr,
                                      const FunctionDef& function,
                                      const Type* fnType,
                                      std::vector<llvm::Value*>& args,
                                      const std::size_t skipParams) {
  std::unordered_map<std::string, const NamedArgument*> keywords;
  for (const NamedArgument& kw : expr.keywordArguments()) {
    keywords[kw.name] = &kw;
  }
  std::size_t positionalIndex = 0;
  const auto takePositional = [&]() -> const Expr* {
    if (positionalIndex < expr.arguments().size()) {
      return expr.arguments()[positionalIndex++].get();
    }
    return nullptr;
  };
  const auto takeKeyword = [&](const std::string& name) -> const Expr* {
    const auto found = keywords.find(name);
    if (found == keywords.end()) {
      return nullptr;
    }
    const Expr* value = found->second->value.get();
    keywords.erase(found);
    return value;
  };
  const auto emitArgument = [&](const Expr* argument, const Type* toType) -> llvm::Value* {
    if (argument == nullptr) {
      return nullptr;
    }
    llvm::Value* value = emitExpr(builder, *argument);
    if (value == nullptr) {
      return nullptr;
    }
    return emitCoerce(builder, value, argument->resolvedType(), toType);
  };
  const auto appendArgument = [&](const Expr* argument, const Type* toType) {
    llvm::Value* value = emitArgument(argument, toType);
    if (value == nullptr) {
      return;
    }
    const Type* layout =
        toType != nullptr ? toType : (argument == nullptr ? nullptr : argument->resolvedType());
    if (function.isExtern() && layout != nullptr && layout->isStrLayout()) {
      args.push_back(builder.CreateExtractValue(value, {0}));
      args.push_back(builder.CreateExtractValue(value, {1}));
      return;
    }
    args.push_back(value);
  };

  std::optional<std::size_t> varArgIndex;
  std::optional<std::size_t> kwArgIndex;
  for (std::size_t index = skipParams; index < function.params().size(); ++index) {
    if (function.params()[index].kind == ParamKind::VarArg) {
      varArgIndex = index;
    } else if (function.params()[index].kind == ParamKind::KwArg) {
      kwArgIndex = index;
    }
  }
  const std::size_t preVarArgEnd =
      varArgIndex.has_value() ? varArgIndex.value() : function.params().size();

  for (std::size_t index = skipParams; index < preVarArgEnd; ++index) {
    const ParamDecl& param = function.params()[index];
    if (param.kind != ParamKind::Normal) {
      continue;
    }
    const Expr* argument = takePositional();
    if (argument == nullptr) {
      argument = takeKeyword(param.name);
    }
    if (argument == nullptr) {
      argument = param.defaultValue.get();
    }
    const Type* paramType = fnType != nullptr && index < fnType->paramTypes().size()
                                ? fnType->paramTypes()[index]
                                : nullptr;
    appendArgument(argument, paramType);
  }

  if (varArgIndex.has_value()) {
    const std::size_t index = varArgIndex.value();
    const ParamDecl& param = function.params()[index];
    const Type* listType = fnType != nullptr && index < fnType->paramTypes().size()
                               ? fnType->paramTypes()[index]
                               : nullptr;
    const Type* elementType =
        listType != nullptr && listType->isList() ? listType->elementType() : types_->anyType();
    if (const Expr* explicitArg = takeKeyword(param.name)) {
      args.push_back(emitArgument(explicitArg, listType));
    } else {
      llvm::Function* newFn =
          runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
      llvm::Function* pushFn = runtimeDecl(
          "sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
      llvm::Value* list = builder.CreateCall(newFn, {builder.getInt64(valueSize(elementType))});
      while (positionalIndex < expr.arguments().size()) {
        llvm::Value* value = emitArgument(expr.arguments()[positionalIndex++].get(), elementType);
        if (value != nullptr) {
          builder.CreateCall(pushFn, {list, emitTempSlot(builder, value, elementType)});
        }
      }
      args.push_back(list);
    }
  }

  const std::size_t keywordOnlyStart =
      varArgIndex.has_value() ? varArgIndex.value() + 1 : preVarArgEnd;
  const std::size_t keywordOnlyEnd =
      kwArgIndex.has_value() ? kwArgIndex.value() : function.params().size();
  for (std::size_t index = keywordOnlyStart; index < keywordOnlyEnd; ++index) {
    const ParamDecl& param = function.params()[index];
    if (param.kind != ParamKind::Normal) {
      continue;
    }
    const Expr* argument = takeKeyword(param.name);
    if (argument == nullptr) {
      argument = param.defaultValue.get();
    }
    const Type* paramType = fnType != nullptr && index < fnType->paramTypes().size()
                                ? fnType->paramTypes()[index]
                                : nullptr;
    appendArgument(argument, paramType);
  }

  if (kwArgIndex.has_value()) {
    const std::size_t index = kwArgIndex.value();
    const ParamDecl& param = function.params()[index];
    const Type* dictType = fnType != nullptr && index < fnType->paramTypes().size()
                               ? fnType->paramTypes()[index]
                               : nullptr;
    const Type* keyType =
        dictType != nullptr && dictType->isDict() ? dictType->dictKeyType() : types_->strType();
    const Type* valueType =
        dictType != nullptr && dictType->isDict() ? dictType->dictValueType() : types_->anyType();
    if (const Expr* explicitArg = takeKeyword(param.name)) {
      args.push_back(emitArgument(explicitArg, dictType));
    } else {
      llvm::Function* newFn =
          runtimeDecl("sere_dict_new",
                      builder.getPtrTy(),
                      {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt32Ty()});
      llvm::Function* setFn =
          runtimeDecl("sere_dict_set",
                      builder.getVoidTy(),
                      {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
      llvm::Value* dict = builder.CreateCall(newFn,
                                             {builder.getInt64(valueSize(keyType)),
                                              builder.getInt64(valueSize(valueType)),
                                              builder.getInt32(dictKeyKind(keyType))});
      for (const auto& entry : keywords) {
        const NamedArgument& kw = *entry.second;
        llvm::Value* keyValue = emitStrLiteral(builder, kw.name);
        llvm::Value* value = emitArgument(kw.value.get(), valueType);
        if (value != nullptr) {
          builder.CreateCall(setFn,
                             {dict,
                              emitTempSlot(builder, keyValue, keyType),
                              emitTempSlot(builder, value, valueType)});
        }
      }
      args.push_back(dict);
    }
  }
}

llvm::Value* IRGenerator::emitAddress(llvm::IRBuilder<>& builder, const Expr& expr, bool required) {
  if (expr.kind() == NodeKind::CallExpr &&
      static_cast<const CallExpr&>(expr).intrinsic() == IntrinsicKind::Super) {
    const auto found = locals_.find("self");
    if (found != locals_.end()) {
      return found->second;
    }
    if (required) {
      diagnostics_->error(expr.range(), "super() requires self");
    }
    return nullptr;
  }
  if (const NameExpr* name = asName(expr)) {
    const auto local = locals_.find(name->name());
    if (local != locals_.end()) {
      return local->second;
    }
    const auto global = globals_.find(name->name());
    if (global != globals_.end()) {
      return global->second;
    }
    if (required) {
      diagnostics_->error(expr.range(), "unknown local '" + name->name() + "'");
    }
    return nullptr;
  }
  if (expr.kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expr);
    if (!member.usesBackingField() &&
        (!member.propertyGet().empty() || !member.propertySet().empty())) {
      if (required) {
        diagnostics_->error(expr.range(), "property '" + member.field() + "' is not addressable");
      }
      return nullptr;
    }
    const Type* objectType = member.object().resolvedType();
    if (objectType == nullptr) {
      return nullptr;
    }
    objectType = resolveType(objectType);
    if (objectType->isTypeObject() && objectType->typeObjectInstance() != nullptr) {
      objectType = objectType->typeObjectInstance()->canonical();
    }
    const RecordField* field = objectType->findField(member.field());
    if (field != nullptr && field->isStatic) {
      const std::string globalName =
          field->llvmName.empty() ? classStaticName(objectType, member.field()) : field->llvmName;
      const auto found = globals_.find(globalName);
      return found == globals_.end() ? nullptr : found->second;
    }
    llvm::Value* object = emitAddress(builder, member.object(), required);
    if (object == nullptr) {
      return nullptr;
    }
    const int index = objectType->fieldIndex(member.field());
    if (index < 0) {
      if (required) {
        diagnostics_->error(expr.range(), "unknown field '" + member.field() + "'");
      }
      return nullptr;
    }
    return builder.CreateStructGEP(lower(objectType), object, llvmFieldIndex(objectType, index));
  }
  if (expr.kind() == NodeKind::IndexExpr) {
    const auto& index = static_cast<const IndexExpr&>(expr);
    if (index.isSlice()) {
      if (required) {
        diagnostics_->error(expr.range(), "slice is not assignable");
      }
      return nullptr;
    }
    const Type* objectType = index.object().resolvedType() == nullptr
                                 ? nullptr
                                 : index.object().resolvedType()->valueType();
    if (objectType == nullptr || !objectType->isSequence() || !index.hasStart()) {
      if (required) {
        diagnostics_->error(expr.range(), "expression is not assignable");
      }
      return nullptr;
    }
    llvm::Function* itemFn = runtimeDecl(
        "sere_list_item", builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    return builder.CreateCall(
        itemFn, {emitBuiltinExpr(builder, index.object()), emitIndexI64(builder, *index.start())});
  }
  if (expr.kind() == NodeKind::UnaryExpr) {
    const auto& unary = static_cast<const UnaryExpr&>(expr);
    if (unary.op() == UnaryOp::Deref) {
      return emitExpr(builder, unary.operand());
    }
  }
  if (required) {
    diagnostics_->error(expr.range(), "expression is not assignable");
  }
  return nullptr;
}

llvm::Value* IRGenerator::emitIndex(llvm::IRBuilder<>& builder, const IndexExpr& expr) {
  const Type* objectType =
      resolveType(expr.object().resolvedType());
  if (objectType != nullptr && objectType->methodIndex("__getitem__") < 0) {
    objectType = objectType->valueType();
  }
  if (expr.isSlice() && objectType != nullptr && objectType->isNamed("str")) {
    llvm::Value* str = emitBuiltinExpr(builder, expr.object());
    llvm::Function* fn = runtimeDecl("sere_str_slice",
                                     builder.getVoidTy(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getInt64Ty(),
                                      builder.getInt64Ty(),
                                      builder.getInt32Ty(),
                                      builder.getInt32Ty(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy()});
    llvm::Value* dataSlot = builder.CreateAlloca(builder.getPtrTy(), nullptr, "sl.data");
    llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "sl.len");
    llvm::Value* start =
        expr.hasStart() ? emitIndexI64(builder, *expr.start()) : builder.getInt64(0);
    llvm::Value* stop = expr.hasStop() ? emitIndexI64(builder, *expr.stop()) : builder.getInt64(0);
    builder.CreateCall(fn,
                       {builder.CreateExtractValue(str, {0}),
                        builder.CreateExtractValue(str, {1}),
                        start,
                        stop,
                        builder.getInt32(expr.hasStart() ? 1 : 0),
                        builder.getInt32(expr.hasStop() ? 1 : 0),
                        dataSlot,
                        lenSlot});
    return packStr(builder,
                   builder.CreateLoad(builder.getPtrTy(), dataSlot),
                   builder.CreateLoad(builder.getInt64Ty(), lenSlot));
  }
  if (expr.isSlice()) {
    llvm::Function* sliceFn = runtimeDecl("sere_list_slice",
                                          builder.getPtrTy(),
                                          {builder.getPtrTy(),
                                           builder.getInt64Ty(),
                                           builder.getInt64Ty(),
                                           builder.getInt32Ty(),
                                           builder.getInt32Ty()});
    llvm::Value* start =
        expr.hasStart() ? emitIndexI64(builder, *expr.start()) : builder.getInt64(0);
    llvm::Value* stop = expr.hasStop() ? emitIndexI64(builder, *expr.stop()) : builder.getInt64(0);
    return builder.CreateCall(sliceFn,
                              {emitBuiltinExpr(builder, expr.object()),
                               start,
                               stop,
                               builder.getInt32(expr.hasStart() ? 1 : 0),
                               builder.getInt32(expr.hasStop() ? 1 : 0)});
  }
  if (objectType != nullptr && objectType->isDict()) {
    llvm::Function* getFn =
        runtimeDecl("sere_dict_get",
                    builder.getInt32Ty(),
                    {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
    llvm::Value* out = builder.CreateAlloca(lower(expr.resolvedType()), nullptr, "dict.out");
    builder.CreateCall(
        getFn,
        {emitBuiltinExpr(builder, expr.object()),
         emitTempSlot(builder, emitExpr(builder, *expr.start()), expr.start()->resolvedType()),
         out});
    return builder.CreateLoad(lower(expr.resolvedType()), out);
  }
  if (objectType != nullptr && objectType->isNamed("str") && expr.hasStart()) {
    llvm::Value* str = emitBuiltinExpr(builder, expr.object());
    llvm::Function* fn = runtimeDecl("sere_str_index",
                                     builder.getVoidTy(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy()});
    llvm::Value* dataSlot = builder.CreateAlloca(builder.getPtrTy(), nullptr, "si.data");
    llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "si.len");
    builder.CreateCall(fn,
                       {builder.CreateExtractValue(str, {0}),
                        builder.CreateExtractValue(str, {1}),
                        emitIndexI64(builder, *expr.start()),
                        dataSlot,
                        lenSlot});
    return packStr(builder,
                   builder.CreateLoad(builder.getPtrTy(), dataSlot),
                   builder.CreateLoad(builder.getInt64Ty(), lenSlot));
  }
  if (objectType != nullptr && objectType->methodIndex("__getitem__") >= 0 && expr.hasStart()) {
    return emitDunderCall(
        builder, expr.object(), "__getitem__", {emitExpr(builder, *expr.start())});
  }
  llvm::Value* address = emitAddress(builder, expr);
  if (address == nullptr) {
    return nullptr;
  }
  return builder.CreateLoad(lower(expr.resolvedType()), address);
}

llvm::Value* IRGenerator::emitListLiteral(llvm::IRBuilder<>& builder, const ListLiteral& expr) {
  const Type* type = expr.resolvedType();
  const Type* element = type == nullptr ? nullptr : type->elementType();
  llvm::Function* newFn = runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* arrayFn = runtimeDecl(
      "sere_array_new", builder.getPtrTy(), {builder.getInt64Ty(), builder.getInt64Ty()});
  llvm::Function* pushFn =
      runtimeDecl("sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
  llvm::Function* itemFn =
      runtimeDecl("sere_list_item", builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()});
  const std::int64_t count = static_cast<std::int64_t>(expr.elements().size());
  llvm::Value* list =
      type != nullptr && type->isArray()
          ? builder.CreateCall(arrayFn,
                               {builder.getInt64(valueSize(element)),
                                builder.getInt64(static_cast<std::uint64_t>(count))})
          : builder.CreateCall(newFn, {builder.getInt64(valueSize(element))});
  for (std::size_t index = 0; index < expr.elements().size(); ++index) {
    llvm::Value* value = emitCoerce(builder,
                                    emitExpr(builder, *expr.elements()[index]),
                                    expr.elements()[index]->resolvedType(),
                                    element);
    if (type != nullptr && type->isArray()) {
      llvm::Value* slot =
          builder.CreateCall(itemFn, {list, builder.getInt64(static_cast<std::uint64_t>(index))});
      builder.CreateStore(value, slot);
    } else {
      builder.CreateCall(pushFn, {list, emitTempSlot(builder, value, element)});
    }
  }
  return list;
}

llvm::Value* IRGenerator::emitDictLiteral(llvm::IRBuilder<>& builder, const DictLiteral& expr) {
  const Type* type = expr.resolvedType();
  const Type* keyType = type == nullptr ? nullptr : type->dictKeyType();
  const Type* valueType = type == nullptr ? nullptr : type->dictValueType();
  llvm::Function* newFn =
      runtimeDecl("sere_dict_new",
                  builder.getPtrTy(),
                  {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt32Ty()});
  llvm::Function* setFn = runtimeDecl("sere_dict_set",
                                      builder.getVoidTy(),
                                      {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
  llvm::Value* dict = builder.CreateCall(newFn,
                                         {builder.getInt64(valueSize(keyType)),
                                          builder.getInt64(valueSize(valueType)),
                                          builder.getInt32(dictKeyKind(keyType))});
  for (std::size_t index = 0; index < expr.keys().size(); ++index) {
    builder.CreateCall(setFn,
                       {dict,
                        emitTempSlot(builder,
                                     emitCoerce(builder,
                                                emitExpr(builder, *expr.keys()[index]),
                                                expr.keys()[index]->resolvedType(),
                                                keyType),
                                     keyType),
                        emitTempSlot(builder,
                                     emitCoerce(builder,
                                                emitExpr(builder, *expr.values()[index]),
                                                expr.values()[index]->resolvedType(),
                                                valueType),
                                     valueType)});
  }
  return dict;
}

llvm::Value* IRGenerator::emitCollectionNew(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  const Type* type = expr.resolvedType();
  if (expr.intrinsic() == IntrinsicKind::DictNew) {
    llvm::Function* newFn =
        runtimeDecl("sere_dict_new",
                    builder.getPtrTy(),
                    {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt32Ty()});
    return builder.CreateCall(newFn,
                              {builder.getInt64(valueSize(type->dictKeyType())),
                               builder.getInt64(valueSize(type->dictValueType())),
                               builder.getInt32(dictKeyKind(type->dictKeyType()))});
  }
  if (expr.intrinsic() == IntrinsicKind::ArrayNew) {
    llvm::Function* arrayFn = runtimeDecl(
        "sere_array_new", builder.getPtrTy(), {builder.getInt64Ty(), builder.getInt64Ty()});
    llvm::Function* itemFn = runtimeDecl(
        "sere_list_item", builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    llvm::Value* array =
        builder.CreateCall(arrayFn,
                           {builder.getInt64(valueSize(type->elementType())),
                            builder.getInt64(static_cast<std::uint64_t>(expr.arguments().size()))});
    for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
      llvm::Value* slot =
          builder.CreateCall(itemFn, {array, builder.getInt64(static_cast<std::uint64_t>(index))});
      builder.CreateStore(emitCoerce(builder,
                                     emitExpr(builder, *expr.arguments()[index]),
                                     expr.arguments()[index]->resolvedType(),
                                     type->elementType()),
                          slot);
    }
    return array;
  }
  llvm::Function* newFn = runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* pushFn =
      runtimeDecl("sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
  llvm::Value* list = builder.CreateCall(newFn, {builder.getInt64(valueSize(type->elementType()))});
  for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
    builder.CreateCall(pushFn,
                       {list,
                        emitTempSlot(builder,
                                     emitCoerce(builder,
                                                emitExpr(builder, *argument),
                                                argument->resolvedType(),
                                                type->elementType()),
                                     type->elementType())});
  }
  return list;
}

llvm::Value* IRGenerator::emitAppend(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  llvm::Function* pushFn =
      runtimeDecl("sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
  llvm::Value* list = nullptr;
  const Expr* item = nullptr;
  const Type* itemType = nullptr;
  const Type* destType = itemType;
  if (expr.callee().kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expr.callee());
    list = emitExpr(builder, member.object());
    item = expr.arguments()[0].get();
    itemType = item->resolvedType();
    const Type* objectType = member.object().resolvedType();
    destType = objectType != nullptr ? objectType->elementType() : itemType;
  } else {
    list = emitExpr(builder, *expr.arguments()[0]);
    item = expr.arguments()[1].get();
    itemType = item->resolvedType();
    const Type* objectType = expr.arguments()[0]->resolvedType();
    destType = objectType != nullptr ? objectType->elementType() : itemType;
  }
  builder.CreateCall(
      pushFn,
      {list,
       emitTempSlot(
           builder, emitCoerce(builder, emitExpr(builder, *item), itemType, destType), destType)});
  return nullptr;
}

llvm::Value* IRGenerator::emitBuiltinMethod(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  if (expr.callee().kind() != NodeKind::MemberExpr) {
    return nullptr;
  }
  const auto& member = static_cast<const MemberExpr&>(expr.callee());
  llvm::Value* object = emitBuiltinExpr(builder, member.object());
  const Type* objectType = member.object().resolvedType() == nullptr
                               ? nullptr
                               : member.object().resolvedType()->valueType();
  const std::string& name = expr.loweredName();
  auto toI64 = [&](llvm::Value* value) -> llvm::Value* {
    if (value->getType() != builder.getInt64Ty()) {
      value = builder.CreateSExt(value, builder.getInt64Ty());
    }
    return value;
  };
  auto i1 = [&](llvm::Value* value) -> llvm::Value* {
    return builder.CreateICmpNE(value, builder.getInt32(0));
  };
  auto slot = [&](const Expr& value, const Type* dest) -> llvm::Value* {
    llvm::Value* emitted = emitExpr(builder, value);
    return emitTempSlot(builder, emitCoerce(builder, emitted, value.resolvedType(), dest), dest);
  };
  auto packOutStr = [&](llvm::Function* fn, std::vector<llvm::Value*> callArgs) -> llvm::Value* {
    llvm::Value* dataSlot = builder.CreateAlloca(builder.getPtrTy(), nullptr, "bm.data");
    llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "bm.len");
    callArgs.push_back(dataSlot);
    callArgs.push_back(lenSlot);
    builder.CreateCall(fn, callArgs);
    return packStr(builder,
                   builder.CreateLoad(builder.getPtrTy(), dataSlot),
                   builder.CreateLoad(builder.getInt64Ty(), lenSlot));
  };

  if (name == "list.append" || name == "list.push") {
    return emitAppend(builder, expr);
  }
  if (objectType != nullptr && objectType->isList()) {
    const Type* elem = objectType->elementType();
    if (name == "list.insert") {
      llvm::Function* fn =
          runtimeDecl("sere_list_insert",
                      builder.getVoidTy(),
                      {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy()});
      builder.CreateCall(fn,
                         {object,
                          toI64(emitExpr(builder, *expr.arguments()[0])),
                          slot(*expr.arguments()[1], elem)});
      return nullptr;
    }
    if (name == "list.pop") {
      llvm::Value* out = builder.CreateAlloca(lower(elem), nullptr, "list.pop");
      if (expr.arguments().empty()) {
        llvm::Function* fn = runtimeDecl(
            "sere_list_pop", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
        builder.CreateCall(fn, {object, out});
      } else {
        llvm::Function* fn =
            runtimeDecl("sere_list_pop_at",
                        builder.getVoidTy(),
                        {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy()});
        builder.CreateCall(fn, {object, toI64(emitExpr(builder, *expr.arguments()[0])), out});
      }
      return builder.CreateLoad(lower(elem), out);
    }
    if (name == "list.remove") {
      llvm::Function* fn = runtimeDecl(
          "sere_list_remove_value", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      return i1(builder.CreateCall(fn, {object, slot(*expr.arguments()[0], elem)}));
    }
    if (name == "list.find" || name == "list.index") {
      llvm::Function* fn = runtimeDecl(
          "sere_list_index_of", builder.getInt64Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      return builder.CreateCall(fn, {object, slot(*expr.arguments()[0], elem)});
    }
    if (name == "list.count") {
      llvm::Function* fn = runtimeDecl(
          "sere_list_count", builder.getInt64Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      return builder.CreateCall(fn, {object, slot(*expr.arguments()[0], elem)});
    }
    if (name == "list.contains" || name == "list.has") {
      llvm::Function* fn = runtimeDecl(
          "sere_list_contains", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      return i1(builder.CreateCall(fn, {object, slot(*expr.arguments()[0], elem)}));
    }
    if (name == "list.clear") {
      builder.CreateCall(runtimeDecl("sere_list_clear", builder.getVoidTy(), {builder.getPtrTy()}),
                         {object});
      return nullptr;
    }
    if (name == "list.reverse") {
      builder.CreateCall(
          runtimeDecl("sere_list_reverse", builder.getVoidTy(), {builder.getPtrTy()}), {object});
      return nullptr;
    }
    if (name == "list.copy" || name == "list.clone") {
      return builder.CreateCall(
          runtimeDecl("sere_list_copy", builder.getPtrTy(), {builder.getPtrTy()}), {object});
    }
    if (name == "list.extend") {
      llvm::Function* fn = runtimeDecl(
          "sere_list_extend", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
      builder.CreateCall(fn, {object, emitExpr(builder, *expr.arguments()[0])});
      return nullptr;
    }
  }

  if (objectType != nullptr && objectType->isDict()) {
    const Type* key = objectType->dictKeyType();
    const Type* value = objectType->dictValueType();
    if (name == "dict.get" || name == "dict.pop") {
      llvm::Value* out = builder.CreateAlloca(lower(value), nullptr, "dict.out");
      const char* fnName = name == "dict.pop" ? "sere_dict_pop" : "sere_dict_get";
      llvm::Function* fn =
          runtimeDecl(fnName,
                      builder.getInt32Ty(),
                      {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
      builder.CreateCall(fn, {object, slot(*expr.arguments()[0], key), out});
      return builder.CreateLoad(lower(value), out);
    }
    if (name == "dict.set") {
      llvm::Function* fn =
          runtimeDecl("sere_dict_set",
                      builder.getVoidTy(),
                      {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
      builder.CreateCall(
          fn, {object, slot(*expr.arguments()[0], key), slot(*expr.arguments()[1], value)});
      return nullptr;
    }
    if (name == "dict.remove" || name == "dict.delete") {
      llvm::Function* fn = runtimeDecl(
          "sere_dict_del", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      return i1(builder.CreateCall(fn, {object, slot(*expr.arguments()[0], key)}));
    }
    if (name == "dict.contains" || name == "dict.has") {
      llvm::Function* fn = runtimeDecl(
          "sere_dict_has", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      return i1(builder.CreateCall(fn, {object, slot(*expr.arguments()[0], key)}));
    }
    if (name == "dict.keys") {
      return builder.CreateCall(
          runtimeDecl("sere_dict_keys", builder.getPtrTy(), {builder.getPtrTy()}), {object});
    }
    if (name == "dict.values") {
      return builder.CreateCall(
          runtimeDecl("sere_dict_values", builder.getPtrTy(), {builder.getPtrTy()}), {object});
    }
    if (name == "dict.clear") {
      builder.CreateCall(runtimeDecl("sere_dict_clear", builder.getVoidTy(), {builder.getPtrTy()}),
                         {object});
      return nullptr;
    }
    if (name == "dict.copy" || name == "dict.clone") {
      return builder.CreateCall(
          runtimeDecl("sere_dict_copy", builder.getPtrTy(), {builder.getPtrTy()}), {object});
    }
  }

  llvm::Value* data = builder.CreateExtractValue(object, {0});
  llvm::Value* len = builder.CreateExtractValue(object, {1});
  auto strUnary = [&](const char* fnName) {
    llvm::Function* fn = runtimeDecl(
        fnName,
        builder.getVoidTy(),
        {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getPtrTy()});
    return packOutStr(fn, {data, len});
  };
  auto strNeedleI32 = [&](const char* fnName) {
    llvm::Value* needle = emitExpr(builder, *expr.arguments()[0]);
    llvm::Function* fn = runtimeDecl(
        fnName,
        builder.getInt32Ty(),
        {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()});
    return i1(builder.CreateCall(fn,
                                 {data,
                                  len,
                                  builder.CreateExtractValue(needle, {0}),
                                  builder.CreateExtractValue(needle, {1})}));
  };
  auto strNeedleI64 = [&](const char* fnName) {
    llvm::Value* needle = emitExpr(builder, *expr.arguments()[0]);
    llvm::Function* fn = runtimeDecl(
        fnName,
        builder.getInt64Ty(),
        {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()});
    return builder.CreateCall(fn,
                              {data,
                               len,
                               builder.CreateExtractValue(needle, {0}),
                               builder.CreateExtractValue(needle, {1})});
  };
  if (name == "str.upper") {
    return strUnary("sere_string_upper");
  }
  if (name == "str.lower") {
    return strUnary("sere_string_lower");
  }
  if (name == "str.strip") {
    return strUnary("sere_string_strip");
  }
  if (name == "str.lstrip") {
    return strUnary("sere_string_lstrip");
  }
  if (name == "str.rstrip") {
    return strUnary("sere_string_rstrip");
  }
  if (name == "str.capitalize") {
    return strUnary("sere_string_capitalize");
  }
  if (name == "str.title") {
    return strUnary("sere_string_title");
  }
  if (name == "str.starts_with" || name == "str.startswith") {
    return strNeedleI32("sere_string_starts_with");
  }
  if (name == "str.ends_with" || name == "str.endswith") {
    return strNeedleI32("sere_string_ends_with");
  }
  if (name == "str.contains" || name == "str.has") {
    llvm::Value* needle = emitExpr(builder, *expr.arguments()[0]);
    llvm::Function* fn = runtimeDecl(
        "sere_str_contains",
        builder.getInt32Ty(),
        {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()});
    return i1(builder.CreateCall(fn,
                                 {data,
                                  len,
                                  builder.CreateExtractValue(needle, {0}),
                                  builder.CreateExtractValue(needle, {1})}));
  }
  if (name == "str.find") {
    return strNeedleI64("sere_string_find");
  }
  if (name == "str.rfind") {
    return strNeedleI64("sere_string_rfind");
  }
  if (name == "str.count") {
    return strNeedleI64("sere_string_count");
  }
  if (name == "str.replace") {
    llvm::Value* oldStr = emitExpr(builder, *expr.arguments()[0]);
    llvm::Value* newStr = emitExpr(builder, *expr.arguments()[1]);
    llvm::Function* fn = runtimeDecl("sere_string_replace",
                                     builder.getVoidTy(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy()});
    return packOutStr(fn,
                      {data,
                       len,
                       builder.CreateExtractValue(oldStr, {0}),
                       builder.CreateExtractValue(oldStr, {1}),
                       builder.CreateExtractValue(newStr, {0}),
                       builder.CreateExtractValue(newStr, {1})});
  }
  if (name == "str.split") {
    llvm::Value* sep = emitExpr(builder, *expr.arguments()[0]);
    llvm::Function* fn = runtimeDecl(
        "sere_string_split",
        builder.getPtrTy(),
        {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()});
    return builder.CreateCall(
        fn,
        {data, len, builder.CreateExtractValue(sep, {0}), builder.CreateExtractValue(sep, {1})});
  }
  if (name == "str.join") {
    llvm::Function* fn = runtimeDecl("sere_string_join",
                                     builder.getVoidTy(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy()});
    return packOutStr(fn, {data, len, emitExpr(builder, *expr.arguments()[0])});
  }
  if (name == "str.repeat") {
    llvm::Function* fn = runtimeDecl("sere_string_repeat",
                                     builder.getVoidTy(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy()});
    return packOutStr(fn, {data, len, toI64(emitExpr(builder, *expr.arguments()[0]))});
  }
  if (name == "str.is_empty") {
    llvm::Function* fn = runtimeDecl(
        "sere_string_is_empty", builder.getInt32Ty(), {builder.getPtrTy(), builder.getInt64Ty()});
    return i1(builder.CreateCall(fn, {data, len}));
  }
  if (name == "str.is_digit") {
    llvm::Function* fn = runtimeDecl(
        "sere_string_is_digit", builder.getInt32Ty(), {builder.getPtrTy(), builder.getInt64Ty()});
    return i1(builder.CreateCall(fn, {data, len}));
  }
  if (name == "str.is_alpha") {
    llvm::Function* fn = runtimeDecl(
        "sere_string_is_alpha", builder.getInt32Ty(), {builder.getPtrTy(), builder.getInt64Ty()});
    return i1(builder.CreateCall(fn, {data, len}));
  }
  if (name == "str.is_space") {
    llvm::Function* fn = runtimeDecl(
        "sere_string_is_space", builder.getInt32Ty(), {builder.getPtrTy(), builder.getInt64Ty()});
    return i1(builder.CreateCall(fn, {data, len}));
  }
  diagnostics_->error(expr.range(), "unknown builtin method");
  return nullptr;
}

bool IRGenerator::emitDictAssign(llvm::IRBuilder<>& builder,
                                 const IndexExpr& target,
                                 const Expr& value) {
  llvm::Function* setFn = runtimeDecl("sere_dict_set",
                                      builder.getVoidTy(),
                                      {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
  const Type* objectType = resolveType(target.object().resolvedType());
  const Type* keyType =
      objectType != nullptr ? objectType->dictKeyType() : target.start()->resolvedType();
  const Type* valueType =
      objectType != nullptr ? objectType->dictValueType() : value.resolvedType();
  builder.CreateCall(
      setFn,
      {emitExpr(builder, target.object()),
       emitTempSlot(builder,
                    emitCoerce(builder,
                               emitExpr(builder, *target.start()),
                               target.start()->resolvedType(),
                               keyType),
                    keyType),
       emitTempSlot(builder,
                    emitCoerce(builder, emitExpr(builder, value), value.resolvedType(), valueType),
                    valueType)});
  return true;
}

llvm::Value* IRGenerator::emitIntrinsic(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  llvm::Function* allocFn = runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* sharedNew =
      runtimeDecl("sere_shared_new", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* lenFn = runtimeDecl("sere_list_len", builder.getInt64Ty(), {builder.getPtrTy()});
  const IntrinsicKind kind = expr.intrinsic();
  if (kind == IntrinsicKind::Super) {
    llvm::Value* self = emitAddress(builder, expr, true);
    if (self == nullptr) {
      return nullptr;
    }
    return builder.CreateLoad(lower(expr.resolvedType()), self);
  }
  if (kind == IntrinsicKind::UniqueNew || kind == IntrinsicKind::Alloc ||
      kind == IntrinsicKind::SharedNew) {
    const Type* resultType = expr.resolvedType();
    const Type* pointee = resultType == nullptr ? nullptr : resultType->pointeeType();
    llvm::Value* size = builder.getInt64(valueSize(pointee));
    llvm::Value* memory =
        builder.CreateCall(kind == IntrinsicKind::SharedNew ? sharedNew : allocFn, {size});
    if (kind != IntrinsicKind::Alloc && !expr.arguments().empty()) {
      llvm::Value* init = emitExpr(builder, *expr.arguments()[0]);
      builder.CreateStore(init, memory);
    }
    return memory;
  }
  if (kind == IntrinsicKind::Free) {
    llvm::Function* freeFn = runtimeDecl("sere_free", builder.getVoidTy(), {builder.getPtrTy()});
    builder.CreateCall(freeFn, {emitExpr(builder, *expr.arguments()[0])});
    return nullptr;
  }
  if (kind == IntrinsicKind::Load) {
    llvm::Value* pointer = emitExpr(builder, *expr.arguments()[0]);
    return builder.CreateLoad(lower(expr.resolvedType()), pointer);
  }
  if (kind == IntrinsicKind::Store) {
    llvm::Value* pointer = emitExpr(builder, *expr.arguments()[0]);
    llvm::Value* value = emitExpr(builder, *expr.arguments()[1]);
    builder.CreateStore(value, pointer);
    return nullptr;
  }
  if (kind == IntrinsicKind::Len) {
    const Type* argType = expr.arguments()[0]->resolvedType();
    if (argType != nullptr && argType->methodIndex("__len__") >= 0) {
      return emitDunderCall(builder, *expr.arguments()[0], "__len__", {});
    }
    argType = argType == nullptr ? nullptr : argType->valueType();
    if (argType != nullptr && argType->isStrLayout()) {
      llvm::Value* str = emitBuiltinExpr(builder, *expr.arguments()[0]);
      return builder.CreateExtractValue(str, {1});
    }
    if (argType != nullptr && argType->methodIndex("__len__") >= 0) {
      return emitDunderCall(builder, *expr.arguments()[0], "__len__", {});
    }
    if (argType != nullptr && argType->isDict()) {
      llvm::Function* dictLen =
          runtimeDecl("sere_dict_len", builder.getInt64Ty(), {builder.getPtrTy()});
      return builder.CreateCall(dictLen, {emitBuiltinExpr(builder, *expr.arguments()[0])});
    }
    return builder.CreateCall(lenFn, {emitBuiltinExpr(builder, *expr.arguments()[0])});
  }
  if (kind == IntrinsicKind::Append) {
    return emitAppend(builder, expr);
  }
  if (kind == IntrinsicKind::BuiltinMethod) {
    return emitBuiltinMethod(builder, expr);
  }
  if (kind == IntrinsicKind::ListNew || kind == IntrinsicKind::ArrayNew ||
      kind == IntrinsicKind::DictNew) {
    return emitCollectionNew(builder, expr);
  }
  if (kind == IntrinsicKind::Range) {
    return emitRange(builder, expr);
  }
  if (kind == IntrinsicKind::Print) {
    return emitPrint(builder, expr);
  }
  if (kind == IntrinsicKind::Str) {
    return emitToStr(builder, *expr.arguments()[0]);
  }
  if (kind == IntrinsicKind::Repr) {
    return emitValueRepr(
        builder, emitExpr(builder, *expr.arguments()[0]), expr.arguments()[0]->resolvedType());
  }
  if (kind == IntrinsicKind::TypeOf) {
    const Type* type = expr.arguments()[0]->resolvedType();
    llvm::Value* value = emitExpr(builder, *expr.arguments()[0]);
    if (type != nullptr && type->isAny()) {
      llvm::Value* tag = builder.CreateExtractValue(value, {0});
      tag =
          builder.CreateSelect(builder.CreateIsNull(tag), builder.CreateGlobalString("None"), tag);
      llvm::Function* length = runtimeDecl("strlen", builder.getInt64Ty(), {builder.getPtrTy()});
      llvm::Value* result = llvm::UndefValue::get(lower(types_->strType()));
      result = builder.CreateInsertValue(result, tag, {0});
      return builder.CreateInsertValue(result, builder.CreateCall(length, {tag}), {1});
    }
    return emitStrLiteral(builder, type == nullptr ? "?" : type->display());
  }
  if (kind == IntrinsicKind::IsInstance) {
    const Type* valueType = expr.arguments()[0]->resolvedType() == nullptr
                                ? nullptr
                                : expr.arguments()[0]->resolvedType()->canonical();
    const Type* target = nullptr;
    if (!expr.typeArgs().empty() && expr.typeArgs()[0]->resolvedType() != nullptr) {
      target = expr.typeArgs()[0]->resolvedType()->canonical();
    } else if (expr.arguments().size() >= 2 && expr.arguments()[1]->resolvedType() != nullptr) {
      target = expr.arguments()[1]->resolvedType()->canonical();
    }
    llvm::Value* value = emitExpr(builder, *expr.arguments()[0]);
    if (valueType != nullptr && valueType->isAny() && target != nullptr) {
      return emitAnyTypeMatch(builder, value, target);
    }
    bool match = valueType != nullptr && target != nullptr &&
                 (target->isAny() || valueType->matchesInstance(target));
    return builder.getInt1(match);
  }
  if (kind == IntrinsicKind::Inspect) {
    const Type* type = expr.arguments()[0]->resolvedType();
    std::string text = type == nullptr ? "?" : type->display();
    if (type != nullptr && type->kind() == TypeKind::Function) {
      text = "fn " + type->display();
    }
    if (type != nullptr && type->isRecord()) {
      text += " {";
      bool first = true;
      for (const RecordField& field : type->fields()) {
        if (!first) {
          text += ", ";
        }
        first = false;
        text += field.name;
      }
      for (const RecordMethod& method : type->methods()) {
        if (!first) {
          text += ", ";
        }
        first = false;
        text += method.name + "()";
      }
      text += "}";
    }
    return emitStrLiteral(builder, text);
  }
  if (kind == IntrinsicKind::Dir) {
    if (expr.arguments().empty() || !expr.compileTimeNames().empty()) {
      return emitNamesList(builder, expr.compileTimeNames());
    }
    const Type* type = expr.arguments()[0]->resolvedType();
    llvm::Function* newFn =
        runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
    llvm::Function* pushFn = runtimeDecl(
        "sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
    llvm::Value* list = builder.CreateCall(newFn, {builder.getInt64(16)});
    if (type != nullptr) {
      auto pushName = [&](const std::string& name) {
        llvm::Value* str = emitStrLiteral(builder, name);
        builder.CreateCall(pushFn, {list, emitTempSlot(builder, str, types_->strType())});
      };
      for (const RecordField& field : type->fields()) {
        pushName(field.name);
      }
      for (const RecordMethod& method : type->methods()) {
        pushName(method.name);
      }
      if (type->fields().empty() && type->methods().empty()) {
        pushName(type->display());
      }
    }
    return list;
  }
  if (kind == IntrinsicKind::SizeOf || kind == IntrinsicKind::AlignOf) {
    const Type* type = expr.typeArgs().empty() ? nullptr : expr.typeArgs()[0]->resolvedType();
    const std::uint64_t size = valueSize(type);
    if (kind == IntrinsicKind::AlignOf) {
      std::uint64_t align = 1;
      if (size >= 8) {
        align = 8;
      } else if (size >= 4) {
        align = 4;
      } else if (size >= 2) {
        align = 2;
      }
      return builder.getInt64(align);
    }
    return builder.getInt64(size);
  }
  if (kind == IntrinsicKind::Parse) {
    return emitParse(builder, expr, false);
  }
  if (kind == IntrinsicKind::TryParse) {
    return emitParse(builder, expr, true);
  }
  if (kind == IntrinsicKind::Panic) {
    llvm::Value* message = emitExpr(builder, *expr.arguments()[0]);
    llvm::Function* fn =
        runtimeDecl("sere_panic", builder.getVoidTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    builder.CreateCall(
        fn, {builder.CreateExtractValue(message, {0}), builder.CreateExtractValue(message, {1})});
    builder.CreateUnreachable();
    return llvm::UndefValue::get(lower(expr.resolvedType()));
  }
  diagnostics_->error(expr.range(), "unknown intrinsic");
  return nullptr;
}

llvm::Value* IRGenerator::emitStrLiteral(llvm::IRBuilder<>& builder, std::string_view text) {
  const std::string owned(text);
  llvm::Constant* data = builder.CreateGlobalString(owned);
  llvm::Value* str = llvm::UndefValue::get(lower(types_->strType()));
  str = builder.CreateInsertValue(str, data, {0});
  str = builder.CreateInsertValue(str, builder.getInt64(owned.size()), {1});
  return str;
}

llvm::Value*
IRGenerator::emitStrFromC(llvm::IRBuilder<>& builder, const char* fnName, llvm::Value* value) {
  llvm::Function* fn =
      runtimeDecl(fnName, builder.getPtrTy(), {value->getType(), builder.getPtrTy()});
  llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "str.len");
  llvm::Value* data = builder.CreateCall(fn, {value, lenSlot});
  llvm::Value* len = builder.CreateLoad(builder.getInt64Ty(), lenSlot);
  llvm::Value* str = llvm::UndefValue::get(lower(types_->strType()));
  str = builder.CreateInsertValue(str, data, {0});
  str = builder.CreateInsertValue(str, len, {1});
  return str;
}

llvm::Value*
IRGenerator::emitStrConcat(llvm::IRBuilder<>& builder, llvm::Value* left, llvm::Value* right) {
  llvm::Function* fn = runtimeDecl("sere_str_concat_data",
                                   builder.getPtrTy(),
                                   {builder.getPtrTy(),
                                    builder.getInt64Ty(),
                                    builder.getPtrTy(),
                                    builder.getInt64Ty(),
                                    builder.getPtrTy()});
  llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "cat.len");
  llvm::Value* data = builder.CreateCall(fn,
                                         {builder.CreateExtractValue(left, {0}),
                                          builder.CreateExtractValue(left, {1}),
                                          builder.CreateExtractValue(right, {0}),
                                          builder.CreateExtractValue(right, {1}),
                                          lenSlot});
  llvm::Value* len = builder.CreateLoad(builder.getInt64Ty(), lenSlot);
  llvm::Value* str = llvm::UndefValue::get(lower(types_->strType()));
  str = builder.CreateInsertValue(str, data, {0});
  str = builder.CreateInsertValue(str, len, {1});
  return str;
}

llvm::Value* IRGenerator::emitRecordStr(llvm::IRBuilder<>& builder, const Expr& object) {
  const Type* record =
      resolveType(object.resolvedType());
  if (record == nullptr) {
    return emitStrLiteral(builder, "?");
  }
  const int index = record->methodIndex("__str__");
  if (index < 0) {
    if (record->valueType()->isStrLayout())
      return emitBuiltinExpr(builder, object);
    return emitValueRepr(builder, emitExpr(builder, object), record);
  }
  const RecordMethod& method = record->methods()[static_cast<std::size_t>(index)];
  llvm::Value* thisPtr = emitAddress(builder, object, false);
  if (thisPtr == nullptr) {
    llvm::Value* value = emitExpr(builder, object);
    if (value == nullptr) {
      return nullptr;
    }
    thisPtr = builder.CreateAlloca(lower(record), nullptr, "str.tmp");
    builder.CreateStore(value, thisPtr);
  }
  const auto found = functions_.find(method.llvmName);
  if (found == functions_.end()) {
    return emitStrLiteral(builder, record->name());
  }
  return builder.CreateCall(found->second, {thisPtr});
}

llvm::Value* IRGenerator::emitEnumSwitchStr(llvm::IRBuilder<>& builder,
                                            llvm::Value* tag,
                                            const Type* type,
                                            bool qualified) {
  if (type == nullptr || tag == nullptr || !tag->getType()->isIntegerTy()) {
    return emitStrLiteral(builder, type == nullptr ? "?" : type->name());
  }
  if (tag->getType() != builder.getInt32Ty()) {
    tag = builder.CreateIntCast(tag, builder.getInt32Ty(), false);
  }
  llvm::Value* slot = builder.CreateAlloca(lower(types_->strType()), nullptr, "enum.str.slot");
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "enum.str.end", function);
  llvm::BasicBlock* fallback = llvm::BasicBlock::Create(*context_, "enum.str.def", function);
  llvm::SwitchInst* sw = builder.CreateSwitch(tag, fallback);
  std::unordered_set<unsigned> seen;
  for (const RecordField& field : type->fields()) {
    if (field.llvmName.empty()) {
      continue;
    }
    const unsigned disc = enumTagFromField(&field);
    if (!seen.insert(disc).second) {
      continue;
    }
    llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, "enum.str.case", function);
    sw->addCase(builder.getInt32(disc), block);
    builder.SetInsertPoint(block);
    const std::string text = qualified ? type->name() + "." + field.name : field.name;
    builder.CreateStore(emitStrLiteral(builder, text), slot);
    builder.CreateBr(merge);
  }
  builder.SetInsertPoint(fallback);
  builder.CreateStore(emitStrLiteral(builder, type->name()), slot);
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  return builder.CreateLoad(lower(types_->strType()), slot);
}

llvm::Value* IRGenerator::emitEnumStr(llvm::IRBuilder<>& builder, const Expr& expr) {
  const Type* type = resolveType(expr.resolvedType());
  if (type == nullptr) {
    return emitStrLiteral(builder, "?");
  }
  llvm::Value* value = emitExpr(builder, expr);
  if (value == nullptr) {
    return emitStrLiteral(builder, type->name());
  }
  return emitEnumSwitchStr(builder, emitEnumTag(builder, value), type, true);
}

llvm::Value*
IRGenerator::emitScalarToStr(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* type) {
  if (value == nullptr || type == nullptr) {
    return emitStrLiteral(builder, "?");
  }
  type = resolveType(type);
  if (type->isNamed("bool")) {
    return emitStrFromC(
        builder, "sere_str_bool_data", builder.CreateZExt(value, builder.getInt8Ty()));
  }
  if (type->isNamed("i64") || type->isNamed("u64")) {
    return emitStrFromC(builder, "sere_str_i64_data", value);
  }
  if (type->isScalarInteger()) {
    if (value->getType()->isIntegerTy() && value->getType() != builder.getInt32Ty()) {
      value = type->isUnsignedInteger() ? builder.CreateZExt(value, builder.getInt32Ty())
                                        : builder.CreateSExt(value, builder.getInt32Ty());
    }
    return emitStrFromC(builder, "sere_str_i32_data", value);
  }
  if (type->isFloat()) {
    if (type->isNamed("f32")) {
      value = builder.CreateFPExt(value, builder.getDoubleTy());
    }
    return emitStrFromC(builder, "sere_str_f64_data", value);
  }
  if (type->isStrLayout()) {
    return value;
  }
  return emitStrLiteral(builder, type->display());
}

llvm::Value* IRGenerator::emitUnionStr(llvm::IRBuilder<>& builder, const Expr& expr) {
  const Type* type = resolveType(expr.resolvedType());
  llvm::Value* packed = emitExpr(builder, expr);
  if (type == nullptr || packed == nullptr) {
    return emitStrLiteral(builder, "?");
  }
  if (!packed->getType()->isStructTy() && !type->args().empty()) {
    return emitScalarToStr(builder, packed, type->args()[0]);
  }
  if (!packed->getType()->isStructTy()) {
    return emitStrLiteral(builder, type->display());
  }
  llvm::Value* tag = builder.CreateExtractValue(packed, {0});
  llvm::Value* bits = builder.CreateExtractValue(packed, {1});
  llvm::Value* slot = builder.CreateAlloca(lower(types_->strType()), nullptr, "union.str");
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "union.str.end", function);
  llvm::BasicBlock* fallback = llvm::BasicBlock::Create(*context_, "union.str.def", function);
  llvm::SwitchInst* sw = builder.CreateSwitch(tag, fallback);
  for (std::size_t index = 0; index < type->args().size(); ++index) {
    const Type* member = type->args()[index];
    llvm::BasicBlock* block = llvm::BasicBlock::Create(*context_, "union.str.case", function);
    sw->addCase(builder.getInt32(static_cast<unsigned>(index)), block);
    builder.SetInsertPoint(block);
    llvm::Value* text = nullptr;
    if (member->isVoidLike()) {
      text = emitStrLiteral(builder, "None");
    } else {
      llvm::Value* unpacked = valueFromBits(builder, bits, member, lower(member));
      text = emitScalarToStr(builder, unpacked, member);
    }
    builder.CreateStore(text, slot);
    builder.CreateBr(merge);
  }
  builder.SetInsertPoint(fallback);
  builder.CreateStore(emitStrLiteral(builder, type->display()), slot);
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  return builder.CreateLoad(lower(types_->strType()), slot);
}

llvm::Value* IRGenerator::emitToStr(llvm::IRBuilder<>& builder, const Expr& expr) {
  const Type* type = resolveType(expr.resolvedType());
  if (type != nullptr && type->isAny()) {
    return emitValueRepr(builder, emitExpr(builder, expr), type);
  }
  if (type != nullptr && type->isEnum()) {
    return emitEnumStr(builder, expr);
  }
  if (type != nullptr && type->isStrLayout()) {
    return emitExpr(builder, expr);
  }
  if (type != nullptr && type->isUnion()) {
    return emitUnionStr(builder, expr);
  }
  if (type != nullptr && type->isNamed("bool")) {
    llvm::Value* value = emitExpr(builder, expr);
    return emitStrFromC(
        builder, "sere_str_bool_data", builder.CreateZExt(value, builder.getInt8Ty()));
  }
  if (type != nullptr && (type->isNamed("i64") || type->isNamed("u64"))) {
    return emitStrFromC(builder, "sere_str_i64_data", emitExpr(builder, expr));
  }
  if (type != nullptr && type->isScalarInteger()) {
    llvm::Value* value = emitExpr(builder, expr);
    return emitScalarToStr(builder, value, type);
  }
  if (type != nullptr && type->isRecord()) {
    return emitRecordStr(builder, expr);
  }
  if (type != nullptr && type->isPointerLike()) {
    return emitPointerStr(builder, expr);
  }
  if (type != nullptr && (type->isSequence() || type->isDict())) {
    return emitListStr(builder, expr);
  }
  if (type != nullptr && type->isFloat()) {
    llvm::Value* value = emitExpr(builder, expr);
    if (type->isNamed("f32")) {
      value = builder.CreateFPExt(value, builder.getDoubleTy());
    }
    return emitStrFromC(builder, "sere_str_f64_data", value);
  }
  if (type != nullptr && type->isVoidLike()) {
    return emitStrLiteral(builder, "None");
  }
  return emitStrLiteral(builder, type == nullptr ? "?" : type->display());
}

llvm::Value* IRGenerator::emitPointerStr(llvm::IRBuilder<>& builder, const Expr& expr) {
  const Type* type = expr.resolvedType();
  llvm::Value* prefix = emitStrLiteral(builder, type->display() + "(");
  llvm::Value* address = emitStrFromC(builder, "sere_str_ptr_data", emitExpr(builder, expr));
  return emitStrConcat(
      builder, emitStrConcat(builder, prefix, address), emitStrLiteral(builder, ")"));
}

llvm::Value* IRGenerator::emitBuiltinExpr(llvm::IRBuilder<>& builder, const Expr& expr) {
  llvm::Value* value = emitExpr(builder, expr);
  const Type* type = expr.resolvedType();
  return type == nullptr ? value : emitCoerce(builder, value, type, type->valueType());
}

llvm::Value* IRGenerator::emitBuiltinDefault(llvm::IRBuilder<>& builder, const Type* type) {
  if (type->isStrLayout())
    return emitStrLiteral(builder, "");
  if (type->isSequence()) {
    return builder.CreateCall(
        runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()}),
        {builder.getInt64(valueSize(type->elementType()))});
  }
  if (type->isDict()) {
    return builder.CreateCall(
        runtimeDecl("sere_dict_new",
                    builder.getPtrTy(),
                    {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt32Ty()}),
        {builder.getInt64(valueSize(type->dictKeyType())),
         builder.getInt64(valueSize(type->dictValueType())),
         builder.getInt32(dictKeyKind(type->dictKeyType()))});
  }
  return emitDefault(type);
}

llvm::Value* IRGenerator::emitListStr(llvm::IRBuilder<>& builder, const Expr& expr) {
  return emitValueRepr(builder, emitExpr(builder, expr), expr.resolvedType());
}

llvm::Value*
IRGenerator::emitValueRepr(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* type) {
  if (type == nullptr)
    return emitStrLiteral(builder, "?");
  type = resolveType(type);
  if (type->isVoidLike())
    return emitStrLiteral(builder, "None");
  if (value == nullptr)
    return emitStrLiteral(builder, "?");
  if (type->isAny()) {
    return builder.CreateCall(
        runtimeDecl("__sere_repr_any", lower(types_->strType()), {lower(type)}), {value});
  }
  if (type->isStrLayout()) {
    llvm::Value* length = builder.CreateAlloca(builder.getInt64Ty());
    llvm::Function* quote =
        runtimeDecl("sere_str_repr_data",
                    builder.getPtrTy(),
                    {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy()});
    llvm::Value* data = builder.CreateCall(
        quote,
        {builder.CreateExtractValue(value, {0}), builder.CreateExtractValue(value, {1}), length});
    llvm::Value* str = llvm::UndefValue::get(lower(types_->strType()));
    str = builder.CreateInsertValue(str, data, {0});
    return builder.CreateInsertValue(str, builder.CreateLoad(builder.getInt64Ty(), length), {1});
  }
  if (!type->isRecord() && !type->isSequence() && !type->isDict()) {
    return emitScalarToStr(builder, value, type);
  }
  // Declare before generating the body so recursive container types can call it.
  const std::string name = "__sere_repr_" + type->display();
  if (llvm::Function* existing = module_->getFunction(name)) {
    return builder.CreateCall(existing, {value});
  }
  llvm::Function* function = llvm::Function::Create(
      llvm::FunctionType::get(lower(types_->strType()), {lower(type)}, false),
      llvm::GlobalValue::InternalLinkage,
      name,
      module_);
  llvm::IRBuilder<> body(llvm::BasicBlock::Create(*context_, "entry", function));
  llvm::Value* object = function->getArg(0);
  llvm::Value* result = nullptr;
  if (type->isRecord()) {
    const int repr = type->methodIndex("__repr__");
    if (repr >= 0 && functions_.contains(type->methods()[repr].llvmName)) {
      llvm::Value* address = body.CreateAlloca(lower(type));
      body.CreateStore(object, address);
      result = body.CreateCall(functions_.at(type->methods()[repr].llvmName), {address});
    } else if (type->valueType() != type) {
      result = emitValueRepr(
          body,
          body.CreateExtractValue(object, {llvmFieldIndex(type, type->fieldIndex("$value"))}),
          type->valueType());
    } else if (type->isStruct()) {
      result = emitStrLiteral(body, "{");
      bool first = true;
      for (const RecordField& field : type->fields()) {
        if (field.isStatic || !field.stored)
          continue;
        if (!first)
          result = emitStrConcat(body, result, emitStrLiteral(body, ", "));
        first = false;
        result = emitStrConcat(
            body, result, emitValueRepr(body, emitStrLiteral(body, field.name), types_->strType()));
        result = emitStrConcat(body, result, emitStrLiteral(body, ": "));
        llvm::Value* item =
            body.CreateExtractValue(object, {llvmFieldIndex(type, type->fieldIndex(field.name))});
        result = emitStrConcat(body, result, emitValueRepr(body, item, field.type));
      }
      result = emitStrConcat(body, result, emitStrLiteral(body, "}"));
    } else {
      result = emitStrLiteral(body, type->name());
    }
  } else {
    const bool dict = type->isDict();
    llvm::Value* keys = object;
    llvm::Value* values = nullptr;
    if (dict) {
      keys = body.CreateCall(runtimeDecl("sere_dict_keys", body.getPtrTy(), {body.getPtrTy()}),
                             {object});
      values = body.CreateCall(runtimeDecl("sere_dict_values", body.getPtrTy(), {body.getPtrTy()}),
                               {object});
    }
    llvm::Value* length =
        body.CreateCall(runtimeDecl("sere_list_len", body.getInt64Ty(), {body.getPtrTy()}), {keys});
    llvm::Value* slot = body.CreateAlloca(lower(types_->strType()));
    body.CreateStore(emitStrLiteral(body, dict ? "{" : "["), slot);
    llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "loop", function);
    llvm::BasicBlock* itemBlock = llvm::BasicBlock::Create(*context_, "item", function);
    llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function);
    llvm::BasicBlock* entry = body.GetInsertBlock();
    body.CreateBr(header);
    body.SetInsertPoint(header);
    llvm::PHINode* index = body.CreatePHI(body.getInt64Ty(), 2);
    index->addIncoming(body.getInt64(0), entry);
    body.CreateCondBr(body.CreateICmpSLT(index, length), itemBlock, end);
    body.SetInsertPoint(itemBlock);
    llvm::Value* separator = body.CreateSelect(body.CreateICmpEQ(index, body.getInt64(0)),
                                               emitStrLiteral(body, ""),
                                               emitStrLiteral(body, ", "));
    llvm::Value* text =
        emitStrConcat(body, body.CreateLoad(lower(types_->strType()), slot), separator);
    const auto item = [&](llvm::Value* list, const Type* element) {
      llvm::Value* address = body.CreateCall(
          runtimeDecl("sere_list_item", body.getPtrTy(), {body.getPtrTy(), body.getInt64Ty()}),
          {list, index});
      return emitValueRepr(body, body.CreateLoad(lower(element), address), element);
    };
    text = emitStrConcat(body, text, item(keys, dict ? type->dictKeyType() : type->elementType()));
    if (dict) {
      text = emitStrConcat(body, text, emitStrLiteral(body, ": "));
      text = emitStrConcat(body, text, item(values, type->dictValueType()));
    }
    body.CreateStore(text, slot);
    llvm::Value* next = body.CreateAdd(index, body.getInt64(1));
    index->addIncoming(next, body.GetInsertBlock());
    body.CreateBr(header);
    body.SetInsertPoint(end);
    result = emitStrConcat(body,
                           body.CreateLoad(lower(types_->strType()), slot),
                           emitStrLiteral(body, dict ? "}" : "]"));
  }
  body.CreateRet(result);
  return builder.CreateCall(function, {value});
}

void IRGenerator::emitAnyRepr() {
  llvm::Function* function = module_->getFunction("__sere_repr_any");
  if (function == nullptr || !function->empty())
    return;
  function->setLinkage(llvm::GlobalValue::InternalLinkage);
  llvm::IRBuilder<> builder(llvm::BasicBlock::Create(*context_, "entry", function));
  llvm::Value* value = function->getArg(0);
  llvm::Value* memory = builder.CreateExtractValue(value, {1});
  llvm::BasicBlock* none = llvm::BasicBlock::Create(*context_, "none", function);
  llvm::BasicBlock* dispatch = llvm::BasicBlock::Create(*context_, "dispatch", function);
  builder.CreateCondBr(builder.CreateIsNull(memory), none, dispatch);
  builder.SetInsertPoint(none);
  builder.CreateRet(emitStrLiteral(builder, "None"));
  builder.SetInsertPoint(dispatch);
  for (std::size_t index = 0; index < boxedTypes_.size(); ++index) {
    const Type* type = boxedTypes_[index];
    if (type->isVoidLike())
      continue;
    llvm::BasicBlock* match = llvm::BasicBlock::Create(*context_, "match", function);
    llvm::BasicBlock* next = llvm::BasicBlock::Create(*context_, "next", function);
    builder.CreateCondBr(emitAnyTypeMatch(builder, value, type), match, next);
    builder.SetInsertPoint(match);
    builder.CreateRet(emitValueRepr(builder, builder.CreateLoad(lower(type), memory), type));
    builder.SetInsertPoint(next);
  }
  builder.CreateRet(emitStrLiteral(builder, "Any"));
}

llvm::Value* IRGenerator::emitNumericCast(llvm::IRBuilder<>& builder,
                                          llvm::Value* value,
                                          const Type* from,
                                          const Type* to) {
  if (to->isNamed("bool")) {
    if (from->isFloat()) {
      return builder.CreateFCmpUNE(value, llvm::ConstantFP::get(value->getType(), 0.0));
    }
    return builder.CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0));
  }
  llvm::Type* dest = lower(to);
  if (from->isFloat() && to->isFloat()) {
    return from->isNamed("f32") ? builder.CreateFPExt(value, dest)
                                : builder.CreateFPTrunc(value, dest);
  }
  if (from->isFloat()) {
    return to->isUnsignedInteger() ? builder.CreateFPToUI(value, dest)
                                   : builder.CreateFPToSI(value, dest);
  }
  if (to->isFloat()) {
    return from->isUnsignedInteger() || from->isNamed("bool") ? builder.CreateUIToFP(value, dest)
                                                              : builder.CreateSIToFP(value, dest);
  }
  if (!value->getType()->isIntegerTy() || dest == nullptr || !dest->isIntegerTy()) {
    return value;
  }
  const int fromBits = from->integerBitWidth();
  const int toBits = to->integerBitWidth();
  if (fromBits == toBits) {
    return value;
  }
  if (toBits < fromBits) {
    return builder.CreateTrunc(value, dest);
  }
  if (from->isUnsignedInteger() || from->isNamed("bool")) {
    return builder.CreateZExt(value, dest);
  }
  return builder.CreateSExt(value, dest);
}

llvm::Value* IRGenerator::emitCastValue(llvm::IRBuilder<>& builder,
                                        const Expr& value,
                                        const Type* from,
                                        const Type* to,
                                        SourceRange range) {
  if (from == nullptr || to == nullptr) {
    return nullptr;
  }
  from = resolveType(from);
  to = resolveType(to);
  llvm::Value* source = emitExpr(builder, value);
  if (source == nullptr) {
    return nullptr;
  }
  if (from == to) {
    return source;
  }
  if (from->isStrLayout() && to->isStrLayout()) {
    return source;
  }
  if (from->isUnion() || to->isUnion()) {
    return emitCoerce(builder, source, from, to);
  }
  if (from->isPointerLike() && to->isPointerLike()) {
    return source;
  }
  if (from->isPointerLike() && to->isInteger()) {
    return builder.CreatePtrToInt(source, lower(to));
  }
  if (from->isInteger() && to->isPointerLike()) {
    return builder.CreateIntToPtr(source, lower(to));
  }
  if ((from->isVoidLike() || from->isNamed("null")) && to->isPointerLike()) {
    return source->getType()->isPointerTy() ? source
                                            : llvm::ConstantPointerNull::get(builder.getPtrTy());
  }
  if ((from->isInteger() || from->isNamed("bool") || from->isFloat()) &&
      (to->isInteger() || to->isNamed("bool") || to->isFloat())) {
    return emitNumericCast(builder, source, from, to);
  }
  if (from->isEnum() && to->isInteger()) {
    llvm::Value* tag = emitEnumTag(builder, source);
    return emitNumericCast(builder, tag, types_->i32Type(), to);
  }
  if (from->isInteger() && to->isEnum()) {
    llvm::Value* tag = emitNumericCast(builder, source, from, types_->i32Type());
    if (!to->hasEnumPayload()) {
      return tag;
    }
    llvm::Value* agg = llvm::UndefValue::get(lower(to));
    agg = builder.CreateInsertValue(agg, tag, {0});
    agg = builder.CreateInsertValue(agg, llvm::ConstantPointerNull::get(builder.getPtrTy()), {1});
    return agg;
  }
  diagnostics_->error(range, "cannot lower cast to " + to->display());
  return nullptr;
}

llvm::Value* IRGenerator::emitCast(llvm::IRBuilder<>& builder, const CastExpr& expr) {
  return emitCastValue(
      builder, expr.value(), expr.value().resolvedType(), expr.resolvedType(), expr.range());
}

void IRGenerator::emitWriteStr(llvm::IRBuilder<>& builder, llvm::Value* str) {
  llvm::Function* writeFn =
      runtimeDecl("sere_write", builder.getVoidTy(), {builder.getPtrTy(), builder.getInt64Ty()});
  builder.CreateCall(writeFn,
                     {builder.CreateExtractValue(str, {0}), builder.CreateExtractValue(str, {1})});
}

void IRGenerator::emitWriteValue(llvm::IRBuilder<>& builder, const Expr& expr) {
  const Type* type = resolveType(expr.resolvedType());
  if (type != nullptr && type->isEnum()) {
    emitWriteStr(builder, emitEnumStr(builder, expr));
    return;
  }
  if (type != nullptr && type->isStrLayout()) {
    emitWriteStr(builder, emitExpr(builder, expr));
    return;
  }
  if (type != nullptr && type->isUnion()) {
    emitWriteStr(builder, emitToStr(builder, expr));
    return;
  }
  if (type != nullptr && type->isNamed("bool")) {
    llvm::Function* fn = runtimeDecl("sere_write_bool", builder.getVoidTy(), {builder.getInt8Ty()});
    builder.CreateCall(fn, {builder.CreateZExt(emitExpr(builder, expr), builder.getInt8Ty())});
    return;
  }
  if (type != nullptr && (type->isNamed("i64") || type->isNamed("u64"))) {
    llvm::Function* fn = runtimeDecl("sere_write_i64", builder.getVoidTy(), {builder.getInt64Ty()});
    builder.CreateCall(fn, {emitExpr(builder, expr)});
    return;
  }
  if (type != nullptr && type->isScalarInteger()) {
    llvm::Function* fn = runtimeDecl("sere_write_i32", builder.getVoidTy(), {builder.getInt32Ty()});
    llvm::Value* value = emitExpr(builder, expr);
    if (value->getType()->isIntegerTy() && value->getType() != builder.getInt32Ty()) {
      value = type->isUnsignedInteger() ? builder.CreateZExt(value, builder.getInt32Ty())
                                        : builder.CreateSExt(value, builder.getInt32Ty());
    }
    builder.CreateCall(fn, {value});
    return;
  }
  if (type != nullptr && type->isRecord()) {
    emitWriteStr(builder, emitRecordStr(builder, expr));
    return;
  }
  emitWriteStr(builder, emitToStr(builder, expr));
}

llvm::Value* IRGenerator::emitPrint(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  llvm::Function* newline = runtimeDecl("sere_write_nl", builder.getVoidTy(), {});
  const Expr* sepExpr = nullptr;
  const Expr* endExpr = nullptr;
  for (const NamedArgument& kw : expr.keywordArguments()) {
    if (kw.name == "sep") {
      sepExpr = kw.value.get();
    } else if (kw.name == "end") {
      endExpr = kw.value.get();
    }
  }
  const auto writeStrValue = [&](llvm::Value* str) {
    if (str == nullptr) {
      return;
    }
    emitWriteStr(builder, str);
  };
  if (expr.arguments().size() == 1 && expr.arguments()[0]->kind() == NodeKind::ComprehensionExpr) {
    const auto& comp = static_cast<const ComprehensionExpr&>(*expr.arguments()[0]);
    llvm::Value* list = emitExpr(builder, comp.iterable());
    const Type* bindType =
        comp.iterable().resolvedType() != nullptr && comp.iterable().resolvedType()->isSequence()
            ? comp.iterable().resolvedType()->elementType()
            : types_->i32Type();
    llvm::Function* lenFn =
        runtimeDecl("sere_list_len", builder.getInt64Ty(), {builder.getPtrTy()});
    llvm::Function* itemFn = runtimeDecl(
        "sere_list_item", builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    llvm::Function* function = builder.GetInsertBlock()->getParent();
    llvm::Value* index = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "print.i");
    llvm::Value* item = builder.CreateAlloca(lower(bindType), nullptr, comp.name());
    rememberLocal(comp.name(), item, bindType);
    builder.CreateStore(builder.getInt64(0), index);
    llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "print.cond", function);
    llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "print.body", function);
    llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "print.end", function);
    builder.CreateBr(header);
    builder.SetInsertPoint(header);
    llvm::Value* current = builder.CreateLoad(builder.getInt64Ty(), index);
    builder.CreateCondBr(
        builder.CreateICmpSLT(current, builder.CreateCall(lenFn, {list})), body, exit);
    builder.SetInsertPoint(body);
    llvm::Value* slot = builder.CreateCall(itemFn, {list, current});
    builder.CreateStore(builder.CreateLoad(lower(bindType), slot), item);
    emitWriteValue(builder, comp.element());
    builder.CreateCall(newline, {});
    builder.CreateStore(builder.CreateAdd(current, builder.getInt64(1)), index);
    builder.CreateBr(header);
    builder.SetInsertPoint(exit);
    if (endExpr == nullptr) {
      builder.CreateCall(newline, {});
    } else {
      writeStrValue(emitExpr(builder, *endExpr));
    }
    return nullptr;
  }
  llvm::Value* sepValue =
      sepExpr == nullptr ? emitStrLiteral(builder, " ") : emitExpr(builder, *sepExpr);
  for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
    if (index != 0) {
      writeStrValue(sepValue);
    }
    emitWriteValue(builder, *expr.arguments()[index]);
  }
  if (endExpr == nullptr) {
    builder.CreateCall(newline, {});
  } else {
    writeStrValue(emitExpr(builder, *endExpr));
  }
  return nullptr;
}

llvm::Value* IRGenerator::emitInterpolated(llvm::IRBuilder<>& builder,
                                           const InterpolatedStringExpr& expr) {
  llvm::Value* result = nullptr;
  for (const StringPart& part : expr.parts()) {
    llvm::Value* piece = part.value == nullptr ? emitStrLiteral(builder, part.literal)
                                               : emitToStr(builder, *part.value);
    if (piece == nullptr) {
      return nullptr;
    }
    result = result == nullptr ? piece : emitStrConcat(builder, result, piece);
  }
  return result == nullptr ? emitStrLiteral(builder, "") : result;
}

llvm::Value* IRGenerator::emitCall(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  if (!expr.compileTimeNames().empty()) {
    return emitNamesList(builder, expr.compileTimeNames());
  }
  if (expr.intrinsic() != IntrinsicKind::None) {
    return emitIntrinsic(builder, expr);
  }
  if (expr.isConstructor()) {
    return emitConstruct(builder, expr);
  }
  if (expr.isMethod()) {
    return emitMethodCall(builder, expr);
  }
  if (expr.isUnboundMethodCall()) {
    llvm::Function* unbound = functions_[expr.loweredName()];
    if (unbound == nullptr) {
      diagnostics_->error(expr.range(), "no LLVM function for '" + expr.loweredName() + "'");
      return nullptr;
    }
    std::vector<llvm::Value*> unboundArgs;
    unboundArgs.push_back(llvm::ConstantPointerNull::get(builder.getPtrTy()));
    const auto unboundDef = functionDefs_.find(expr.loweredName());
    const FunctionDef* unboundFn = unboundDef == functionDefs_.end() ? nullptr : unboundDef->second;
    const Type* unboundType = unboundFn == nullptr ? nullptr : unboundFn->resolvedType();
    if (unboundFn != nullptr) {
      appendBoundCallArgs(builder, expr, *unboundFn, unboundType, unboundArgs, 1);
    } else {
      for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
        llvm::Value* value = emitExpr(builder, *argument);
        if (value == nullptr) {
          return nullptr;
        }
        unboundArgs.push_back(value);
      }
    }
    matchCallArgs(builder, unbound, unboundArgs);
    if (unbound->getReturnType()->isVoidTy()) {
      builder.CreateCall(unbound, unboundArgs);
      return nullptr;
    }
    return builder.CreateCall(unbound, unboundArgs);
  }
  if (expr.isCast()) {
    return emitCastValue(builder,
                         *expr.arguments()[0],
                         expr.arguments()[0]->resolvedType(),
                         expr.resolvedType(),
                         expr.range());
  }
  std::string calleeName = expr.loweredName();
  if (calleeName.empty()) {
    if (const NameExpr* name = asName(expr.callee())) {
      calleeName = name->name();
    }
  }
  llvm::Function* callee = nullptr;
  if (!calleeName.empty()) {
    const auto found = functions_.find(calleeName);
    if (found != functions_.end()) {
      callee = found->second;
    }
  }
  if (callee == nullptr) {
    const Type* fnType = expr.callee().resolvedType();
    if (fnType != nullptr &&
        (fnType->kind() == TypeKind::Function || fnType->isCallableConstraint())) {
      llvm::Value* fnptr = emitExpr(builder, expr.callee());
      llvm::FunctionType* llvmFn = fnType->kind() == TypeKind::Function
                                       ? llvmFunctionTypeFrom(fnType)
                                       : llvmFunctionTypeFromCall(expr);
      if (fnptr == nullptr || llvmFn == nullptr) {
        return nullptr;
      }
      std::vector<llvm::Value*> args;
      for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
        llvm::Value* value = emitExpr(builder, *argument);
        if (value == nullptr) {
          return nullptr;
        }
        args.push_back(value);
      }
      return emitIndirectCallable(builder, fnptr, llvmFn, args);
    }
    diagnostics_->error(expr.range(),
                        calleeName.empty() ? "callee is not a function"
                                           : "no LLVM function for '" + calleeName + "'");
    return nullptr;
  }
  std::vector<llvm::Value*> args;
  bool isExtern = externFunctions_.contains(calleeName);
  const FunctionDef* functionDef = nullptr;
  const Type* fnType = nullptr;
  const auto defFound = functionDefs_.find(calleeName);
  if (defFound != functionDefs_.end()) {
    functionDef = defFound->second;
    if (functionDef != nullptr) {
      isExtern = isExtern || functionDef->isExtern();
      fnType = functionDef->resolvedType();
    }
  }
  if (functionDef != nullptr) {
    appendBoundCallArgs(builder, expr, *functionDef, fnType, args, 0);
  } else {
    for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
      llvm::Value* value = emitExpr(builder, *argument);
      if (isExtern && argument->resolvedType() != nullptr &&
          argument->resolvedType()->isStrLayout()) {
        args.push_back(builder.CreateExtractValue(value, {0}));
        args.push_back(builder.CreateExtractValue(value, {1}));
      } else {
        args.push_back(value);
      }
    }
    if (functionDef != nullptr) {
      std::size_t llvmIndex = 0;
      for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
        const Type* from = expr.arguments()[index]->resolvedType();
        if (from != nullptr && from->isStrLayout() && functionDef->isExtern()) {
          llvmIndex += 2;
          continue;
        }
        if (llvmIndex < args.size() && fnType != nullptr && index < fnType->paramTypes().size()) {
          args[llvmIndex] = emitCoerce(builder, args[llvmIndex], from, fnType->paramTypes()[index]);
        }
        llvmIndex += 1;
      }
      appendDefaultArgs(builder, args, *functionDef, expr.arguments().size(), 0);
    }
  }
  matchCallArgs(builder, callee, args);
  if (functionDef != nullptr && functionDef->decoratedType() != nullptr) {
    llvm::Value* slot = decoratorSlots_[calleeName];
    if (slot == nullptr) {
      slot = decoratorSlots_[llvmNameFor(*functionDef)];
    }
    if (slot != nullptr) {
      llvm::Value* packed = builder.CreateLoad(lower(functionDef->decoratedType()), slot);
      const Type* callType = functionDef->decoratedType()->kind() == TypeKind::Function
                                 ? functionDef->decoratedType()
                                 : functionDef->resolvedType();
      llvm::FunctionType* llvmFn = llvmFunctionTypeFrom(callType);
      return emitIndirectCallable(builder, packed, llvmFn, args);
    }
  }
  const bool externStrRet =
      isExtern && expr.resolvedType() != nullptr && expr.resolvedType()->isStrLayout();
  if (externStrRet) {
    llvm::Value* dataSlot = builder.CreateAlloca(builder.getPtrTy(), nullptr, "ext.str.data");
    llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "ext.str.len");
    args.push_back(dataSlot);
    args.push_back(lenSlot);
    builder.CreateCall(callee, args);
    return packStr(builder,
                   builder.CreateLoad(builder.getPtrTy(), dataSlot),
                   builder.CreateLoad(builder.getInt64Ty(), lenSlot));
  }
  if (callee->getReturnType()->isVoidTy()) {
    builder.CreateCall(callee, args);
    return nullptr;
  }
  return builder.CreateCall(callee, args);
}

llvm::Value* IRGenerator::emitConstruct(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  if (!expr.loweredName().empty()) {
    return emitInitConstruct(builder, expr);
  }
  const Type* record = expr.resolvedType();
  if (record == nullptr) {
    return nullptr;
  }
  record = resolveType(record);
  if (record->isPointerLike()) {
    if (expr.arguments().empty()) {
      return emitDefault(record);
    }
    const Type* pointee = record->pointeeType();
    llvm::Value* argVal = emitExpr(builder, *expr.arguments()[0]);
    if (expr.arguments()[0]->resolvedType() != nullptr &&
        expr.arguments()[0]->resolvedType()->isPointerLike()) {
      return argVal;
    }
    llvm::Function* sharedNew =
        runtimeDecl("sere_shared_new", builder.getPtrTy(), {builder.getInt64Ty()});
    llvm::Function* allocFn = runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
    llvm::Value* size = builder.getInt64(valueSize(pointee));
    llvm::Value* memory =
        builder.CreateCall(record->isGenericCtor("Shared") ? sharedNew : allocFn, {size});
    builder.CreateStore(argVal, memory);
    return memory;
  }
  if (!record->isRecord() || record->valueType() != record) {
    const Type* payloadType = record->valueType();
    llvm::Value* payload = nullptr;
    if (expr.arguments().empty()) {
      payload = emitBuiltinDefault(builder, payloadType);
    } else if (payloadType->isNamed("str")) {
      payload = emitToStr(builder, *expr.arguments()[0]);
    } else {
      payload = emitCoerce(builder,
                           emitExpr(builder, *expr.arguments()[0]),
                           expr.arguments()[0]->resolvedType(),
                           payloadType);
      if (payloadType->isSequence() || payloadType->isDict()) {
        payload = builder.CreateCall(
            runtimeDecl(payloadType->isDict() ? "sere_dict_copy" : "sere_list_copy",
                        builder.getPtrTy(),
                        {builder.getPtrTy()}),
            {payload});
      }
    }
    if (!record->isRecord())
      return payload;
    llvm::Value* aggregate = emitDefault(record);
    if (recordHasTypeId(record)) {
      aggregate = builder.CreateInsertValue(aggregate, builder.getInt32(recordTypeId(record)), {0});
    }
    return builder.CreateInsertValue(
        aggregate, payload, {llvmFieldIndex(record, record->fieldIndex("$value"))});
  }
  if (record->isEnum() && expr.callee().kind() == NodeKind::MemberExpr) {
    const auto& member = static_cast<const MemberExpr&>(expr.callee());
    const RecordField* field = record->findField(member.field());
    unsigned tag = 0;
    if (field != nullptr && !field->llvmName.empty()) {
      tag = enumTagFromField(field);
    }
    if (expr.arguments().empty() || field == nullptr || field->payloadTypes.empty()) {
      return emitEnumUnit(builder, record, tag);
    }
    std::vector<llvm::Type*> payloadTypes;
    std::uint64_t bytes = 0;
    for (const Type* payload : field->payloadTypes) {
      payloadTypes.push_back(lower(payload));
      bytes += valueSize(payload);
    }
    llvm::StructType* payloadTy = llvm::StructType::get(*context_, payloadTypes);
    llvm::Function* allocFn = runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
    llvm::Value* memory = builder.CreateCall(allocFn, {builder.getInt64(bytes == 0 ? 1 : bytes)});
    llvm::Value* typed = builder.CreateBitCast(memory, builder.getPtrTy());
    for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
      llvm::Value* value = emitExpr(builder, *expr.arguments()[index]);
      llvm::Value* slot = builder.CreateStructGEP(payloadTy, typed, static_cast<unsigned>(index));
      builder.CreateStore(value, slot);
    }
    llvm::Value* agg = llvm::UndefValue::get(lower(record));
    agg = builder.CreateInsertValue(agg, builder.getInt32(tag), {0});
    agg = builder.CreateInsertValue(agg, memory, {1});
    return agg;
  }
  if (expr.arguments().empty()) {
    llvm::Value* value = emitDefault(record);
    if (recordHasTypeId(record) && value != nullptr) {
      value = builder.CreateInsertValue(value, builder.getInt32(recordTypeId(record)), {0});
    }
    return value;
  }
  llvm::Value* aggregate = llvm::UndefValue::get(lower(record));
  if (recordHasTypeId(record)) {
    aggregate = builder.CreateInsertValue(aggregate, builder.getInt32(recordTypeId(record)), {0});
  }
  std::vector<const RecordField*> instanceFields;
  for (const RecordField& fieldDecl : record->fields()) {
    if (!fieldDecl.isStatic && fieldDecl.stored) {
      instanceFields.push_back(&fieldDecl);
    }
  }
  for (std::size_t index = 0; index < expr.arguments().size(); ++index) {
    llvm::Value* field = emitExpr(builder, *expr.arguments()[index]);
    if (field == nullptr) {
      return nullptr;
    }
    int semantic = static_cast<int>(index);
    if (index < instanceFields.size()) {
      field = emitCoerce(
          builder, field, expr.arguments()[index]->resolvedType(), instanceFields[index]->type);
      semantic = record->fieldIndex(instanceFields[index]->name);
    }
    aggregate = builder.CreateInsertValue(aggregate, field, {llvmFieldIndex(record, semantic)});
  }
  return aggregate;
}

llvm::Value* IRGenerator::emitInitConstruct(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  const Type* record = expr.resolvedType();
  if (record == nullptr) {
    return nullptr;
  }
  llvm::Value* slot = builder.CreateAlloca(lower(record), nullptr, "init.tmp");
  builder.CreateStore(emitDefault(record), slot);
  if (recordHasTypeId(record)) {
    builder.CreateStore(builder.getInt32(recordTypeId(record)),
                        builder.CreateStructGEP(lower(record), slot, 0u));
  }
  const auto found = functions_.find(expr.loweredName());
  if (found == functions_.end()) {
    diagnostics_->error(expr.range(),
                        "no LLVM function for constructor '" + expr.loweredName() + "'");
    return nullptr;
  }
  std::vector<llvm::Value*> args;
  args.push_back(slot);
  const auto defFound = functionDefs_.find(expr.loweredName());
  const Type* initType = nullptr;
  const FunctionDef* initDef = nullptr;
  if (defFound != functionDefs_.end() && defFound->second != nullptr) {
    initDef = defFound->second;
    initType = initDef->resolvedType();
  }
  if (initDef != nullptr) {
    appendBoundCallArgs(builder, expr, *initDef, initType, args, 1);
  } else {
    std::size_t paramIndex = 1;
    for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
      llvm::Value* value = emitExpr(builder, *argument);
      if (initType != nullptr && paramIndex < initType->paramTypes().size()) {
        value = emitCoerce(
            builder, value, argument->resolvedType(), initType->paramTypes()[paramIndex]);
      }
      args.push_back(value);
      ++paramIndex;
    }
    if (initDef != nullptr) {
      appendDefaultArgs(builder, args, *initDef, expr.arguments().size(), 1);
    }
  }
  matchCallArgs(builder, found->second, args);
  builder.CreateCall(found->second, args);
  return builder.CreateLoad(lower(record), slot);
}

llvm::Value* IRGenerator::emitMethodCall(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  const auto& member = static_cast<const MemberExpr&>(expr.callee());
  llvm::Value* thisPtr = emitAddress(builder, member.object(), false);
  if (thisPtr == nullptr) {
    llvm::Value* value = emitExpr(builder, member.object());
    if (value == nullptr || member.object().resolvedType() == nullptr) {
      return nullptr;
    }
    thisPtr = builder.CreateAlloca(lower(member.object().resolvedType()), nullptr, "this.tmp");
    builder.CreateStore(value, thisPtr);
  }
  std::string methodName = expr.loweredName();
  const Type* receiver = resolveType(member.object().resolvedType());
  if (!subst_.empty() && receiver != nullptr) {
    const int index = receiver->methodIndex(member.field());
    if (index >= 0) {
      methodName = receiver->methods()[static_cast<std::size_t>(index)].llvmName;
    }
  }
  const auto found = functions_.find(methodName);
  if (found == functions_.end()) {
    diagnostics_->error(expr.range(), "no LLVM function for method '" + expr.loweredName() + "'");
    return nullptr;
  }
  std::vector<llvm::Value*> args;
  args.push_back(thisPtr);
  const auto defFound = functionDefs_.find(methodName);
  const Type* methodType = nullptr;
  const FunctionDef* methodDef = nullptr;
  if (defFound != functionDefs_.end() && defFound->second != nullptr) {
    methodDef = defFound->second;
    methodType = methodDef->resolvedType();
  }
  if (methodType == nullptr) {
    methodType = expr.callee().resolvedType();
  }
  if (methodDef != nullptr) {
    appendBoundCallArgs(builder, expr, *methodDef, methodType, args, 1);
  } else {
    std::size_t paramIndex = 1;
    for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
      llvm::Value* value = emitExpr(builder, *argument);
      if (methodType != nullptr && paramIndex < methodType->paramTypes().size()) {
        value = emitCoerce(
            builder, value, argument->resolvedType(), methodType->paramTypes()[paramIndex]);
      }
      args.push_back(value);
      ++paramIndex;
    }
    if (methodDef != nullptr) {
      appendDefaultArgs(builder, args, *methodDef, expr.arguments().size(), 1);
    }
  }
  if (methodDef != nullptr && methodDef->decoratedType() != nullptr) {
    llvm::Value* slot = decoratorSlots_[expr.loweredName()];
    if (slot != nullptr) {
      llvm::Value* packed = builder.CreateLoad(lower(methodDef->decoratedType()), slot);
      llvm::FunctionType* llvmFn = llvmFunctionTypeFrom(methodDef->decoratedType());
      return emitIndirectCallable(builder, packed, llvmFn, args);
    }
  }
  llvm::Function* callee = found->second;
  const Type* staticType = resolveType(member.object().resolvedType());
  const bool superCall =
      member.object().kind() == NodeKind::CallExpr &&
      static_cast<const CallExpr&>(member.object()).intrinsic() == IntrinsicKind::Super;
  struct VirtTarget {
    std::uint32_t typeId = 0;
    llvm::Function* fn = nullptr;
  };
  std::vector<VirtTarget> targets;
  if (!superCall && recordHasTypeId(staticType)) {
    for (const Type* record : classTypes_) {
      if (record == nullptr || !record->isSubtypeOf(staticType)) {
        continue;
      }
      const int index = record->methodIndex(member.field());
      if (index < 0) {
        continue;
      }
      const RecordMethod& method = record->methods()[static_cast<std::size_t>(index)];
      if (method.llvmName.empty() || method.llvmName == expr.loweredName()) {
        continue;
      }
      const auto overrideFn = functions_.find(method.llvmName);
      if (overrideFn == functions_.end() || overrideFn->second == callee) {
        continue;
      }
      targets.push_back({recordTypeId(record), overrideFn->second});
    }
  }
  auto invoke = [&](llvm::Function* fn) -> llvm::Value* {
    std::vector<llvm::Value*> callArgs = args;
    matchCallArgs(builder, fn, callArgs);
    if (fn->getReturnType()->isVoidTy()) {
      builder.CreateCall(fn, callArgs);
      return nullptr;
    }
    return builder.CreateCall(fn, callArgs);
  };
  if (targets.empty()) {
    return invoke(callee);
  }
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::Value* idPtr = builder.CreateStructGEP(lower(staticType), thisPtr, 0u);
  llvm::Value* gotId = builder.CreateLoad(builder.getInt32Ty(), idPtr, "type.id");
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "virt.end", function);
  llvm::BasicBlock* defBlock = llvm::BasicBlock::Create(*context_, "virt.def", function);
  std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incoming;
  llvm::BasicBlock* check = builder.GetInsertBlock();
  for (std::size_t index = 0; index < targets.size(); ++index) {
    llvm::BasicBlock* hit = llvm::BasicBlock::Create(*context_, "virt.hit", function);
    llvm::BasicBlock* miss = index + 1 < targets.size()
                                 ? llvm::BasicBlock::Create(*context_, "virt.chk", function)
                                 : defBlock;
    builder.SetInsertPoint(check);
    builder.CreateCondBr(
        builder.CreateICmpEQ(gotId, builder.getInt32(targets[index].typeId)), hit, miss);
    builder.SetInsertPoint(hit);
    llvm::Value* result = invoke(targets[index].fn);
    incoming.emplace_back(result, builder.GetInsertBlock());
    builder.CreateBr(merge);
    check = miss;
  }
  builder.SetInsertPoint(defBlock);
  llvm::Value* fallback = invoke(callee);
  incoming.emplace_back(fallback, builder.GetInsertBlock());
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  if (callee->getReturnType()->isVoidTy()) {
    return nullptr;
  }
  llvm::PHINode* phi = builder.CreatePHI(
      callee->getReturnType(), static_cast<unsigned>(incoming.size()), "virt.phi");
  for (const auto& edge : incoming) {
    llvm::Value* value =
        edge.first != nullptr ? edge.first : llvm::UndefValue::get(callee->getReturnType());
    phi->addIncoming(value, edge.second);
  }
  return phi;
}

llvm::Value* IRGenerator::emitLogical(llvm::IRBuilder<>& builder, const BinaryExpr& expr) {
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::Value* left = emitExpr(builder, expr.left());
  if (left == nullptr) {
    return nullptr;
  }
  llvm::BasicBlock* lhsBlock = builder.GetInsertBlock();
  llvm::BasicBlock* rhsBlock = llvm::BasicBlock::Create(*context_, "log.rhs", function);
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "log.end", function);
  const bool isAnd = expr.op() == BinaryOp::And;
  builder.CreateCondBr(left, isAnd ? rhsBlock : merge, isAnd ? merge : rhsBlock);
  builder.SetInsertPoint(rhsBlock);
  llvm::Value* right = emitExpr(builder, expr.right());
  if (right == nullptr) {
    return nullptr;
  }
  llvm::BasicBlock* rhsEnd = builder.GetInsertBlock();
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  llvm::PHINode* phi = builder.CreatePHI(builder.getInt1Ty(), 2, "log.phi");
  phi->addIncoming(left, lhsBlock);
  phi->addIncoming(right, rhsEnd);
  return phi;
}

llvm::Value* IRGenerator::emitBinary(llvm::IRBuilder<>& builder, const BinaryExpr& expr) {
  if (expr.op() == BinaryOp::And || expr.op() == BinaryOp::Or) {
    return emitLogical(builder, expr);
  }
  if (expr.op() == BinaryOp::Is || expr.op() == BinaryOp::IsNot) {
    const CallExpr* typeofCall = expr.left().kind() == NodeKind::CallExpr
                                     ? static_cast<const CallExpr*>(&expr.left())
                                     : nullptr;
    const NameExpr* typeofName = typeofCall == nullptr ? nullptr : asName(typeofCall->callee());
    if (typeofName != nullptr && typeofName->name() == "typeof" &&
        !typeofCall->arguments().empty()) {
      const Type* valueType = typeofCall->arguments()[0]->resolvedType();
      const Type* target = expr.right().resolvedType();
      const bool match =
          valueType != nullptr && target != nullptr && valueType->matchesInstance(target);
      return builder.getInt1(expr.op() == BinaryOp::Is ? match : !match);
    }
  }
  if ((expr.op() == BinaryOp::Is || expr.op() == BinaryOp::IsNot) &&
      asName(expr.right()) != nullptr) {
    const Type* target =
        resolveType(expr.right().resolvedType());
    if (recordHasTypeId(target) && locals_.find(asName(expr.right())->name()) == locals_.end() &&
        globals_.find(asName(expr.right())->name()) == globals_.end()) {
      llvm::Value* left = emitExpr(builder, expr.left());
      if (left == nullptr) {
        return nullptr;
      }
      llvm::Value* got = left->getType()->isStructTy() ? builder.CreateExtractValue(left, {0})
                                                       : builder.getInt32(0);
      llvm::Value* match = builder.CreateICmpEQ(got, builder.getInt32(recordTypeId(target)));
      return expr.op() == BinaryOp::Is ? match : builder.CreateNot(match);
    }
  }
  llvm::Value* left = emitExpr(builder, expr.left());
  llvm::Value* right = emitExpr(builder, expr.right());
  if (left == nullptr || right == nullptr) {
    return nullptr;
  }
  const Type* leftType =
      resolveType(expr.left().resolvedType());
  const Type* rightType =
      resolveType(expr.right().resolvedType());
  // Comparing a pointer-like value to None is a null check. None's type is
  // void-like, so emitCoerce below would drop the None operand to nullptr and
  // the generic compare path would dereference it. Handle identity/equality
  // against None here so the test is both real and null-safe.
  const bool leftIsNone = leftType != nullptr && leftType->isVoidLike();
  const bool rightIsNone = rightType != nullptr && rightType->isVoidLike();
  const bool noneCompare =
      (leftIsNone != rightIsNone) &&
      (expr.op() == BinaryOp::Is || expr.op() == BinaryOp::IsNot ||
       expr.op() == BinaryOp::Eq || expr.op() == BinaryOp::Ne);
  if (noneCompare) {
    llvm::Value* operand = leftIsNone ? right : left;
    const Type* operandType = leftIsNone ? rightType : leftType;
    if (operand != nullptr && operandType != nullptr &&
        (operandType->isPointerLike() || operandType->isAny() ||
         operand->getType()->isPointerTy() || operand->getType()->isIntegerTy())) {
      llvm::Value* isNull = builder.CreateIsNull(operand);
      const bool positive =
          expr.op() == BinaryOp::Is || expr.op() == BinaryOp::Eq;
      return positive ? isNull : builder.CreateNot(isNull);
    }
  }
  if (leftType != nullptr) {
    left = emitCoerce(builder, left, leftType, leftType->valueType());
    leftType = leftType->valueType();
  }
  if (rightType != nullptr) {
    right = emitCoerce(builder, right, rightType, rightType->valueType());
    rightType = rightType->valueType();
  }
  if (left == nullptr || right == nullptr) {
    // A None operand was coerced away; only compare operations can survive
    // this, and they cannot dereference a missing operand.
    return builder.getInt1(false);
  }
  if (expr.op() == BinaryOp::Add && leftType != nullptr && leftType->isNamed("str") &&
      rightType != nullptr && rightType->isNamed("str")) {
    return emitStrConcat(builder, left, right);
  }
  if (expr.op() == BinaryOp::Mul && leftType != nullptr && rightType != nullptr &&
      ((leftType->isNamed("str") && rightType->isInteger()) ||
       (leftType->isInteger() && rightType->isNamed("str")))) {
    llvm::Value* str = leftType->isNamed("str") ? left : right;
    llvm::Value* count = leftType->isNamed("str") ? right : left;
    if (!count->getType()->isIntegerTy(64)) {
      count = builder.CreateSExt(count, builder.getInt64Ty());
    }
    llvm::Function* fn = runtimeDecl("sere_str_repeat",
                                     builder.getVoidTy(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getInt64Ty(),
                                      builder.getPtrTy(),
                                      builder.getPtrTy()});
    llvm::Value* dataSlot = builder.CreateAlloca(builder.getPtrTy(), nullptr, "rep.data");
    llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "rep.len");
    builder.CreateCall(fn,
                       {builder.CreateExtractValue(str, {0}),
                        builder.CreateExtractValue(str, {1}),
                        count,
                        dataSlot,
                        lenSlot});
    return packStr(builder,
                   builder.CreateLoad(builder.getPtrTy(), dataSlot),
                   builder.CreateLoad(builder.getInt64Ty(), lenSlot));
  }
  if (expr.op() == BinaryOp::Add && leftType != nullptr && leftType->isList() &&
      rightType != nullptr && rightType->isList()) {
    llvm::Function* fn = runtimeDecl(
        "sere_list_concat", builder.getPtrTy(), {builder.getPtrTy(), builder.getPtrTy()});
    return builder.CreateCall(fn, {left, right});
  }
  if ((expr.op() == BinaryOp::Eq || expr.op() == BinaryOp::Ne || expr.op() == BinaryOp::Lt ||
       expr.op() == BinaryOp::Le || expr.op() == BinaryOp::Gt || expr.op() == BinaryOp::Ge ||
       expr.op() == BinaryOp::Is || expr.op() == BinaryOp::IsNot) &&
      ((leftType != nullptr && leftType->isNamed("str") && rightType != nullptr &&
        rightType->isNamed("str")) ||
       (isStrLlvmType(left->getType()) && isStrLlvmType(right->getType())))) {
    return emitStrCompare(builder, expr.op(), left, right);
  }
  if (leftType != nullptr && leftType->isEnum()) {
    left = emitEnumTag(builder, left);
  }
  if (rightType != nullptr && rightType->isEnum()) {
    right = emitEnumTag(builder, right);
  }
  widenIntegerPair(builder, left, right, leftType, rightType);
  const bool compare = expr.op() == BinaryOp::Eq || expr.op() == BinaryOp::Ne ||
                       expr.op() == BinaryOp::Lt || expr.op() == BinaryOp::Le ||
                       expr.op() == BinaryOp::Gt || expr.op() == BinaryOp::Ge ||
                       expr.op() == BinaryOp::Is || expr.op() == BinaryOp::IsNot;
  if (compare && isStrLlvmType(left->getType()) && isStrLlvmType(right->getType())) {
    return emitStrCompare(builder, expr.op(), left, right);
  }
  if (compare && (left->getType() != right->getType() ||
                  (!left->getType()->isIntegerTy() && !left->getType()->isFloatingPointTy()))) {
    return builder.getInt1(false);
  }
  switch (expr.op()) {
  case BinaryOp::Add:
    return left->getType()->isFloatingPointTy() ? builder.CreateFAdd(left, right)
                                                : builder.CreateAdd(left, right);
  case BinaryOp::Sub:
    return left->getType()->isFloatingPointTy() ? builder.CreateFSub(left, right)
                                                : builder.CreateSub(left, right);
  case BinaryOp::Mul:
    return left->getType()->isFloatingPointTy() ? builder.CreateFMul(left, right)
                                                : builder.CreateMul(left, right);
  case BinaryOp::Div:
    return left->getType()->isFloatingPointTy() ? builder.CreateFDiv(left, right)
                                                : builder.CreateSDiv(left, right);
  case BinaryOp::FloorDiv:
    if (left->getType()->isFloatingPointTy()) {
      llvm::Value* quotient = builder.CreateFDiv(left, right);
      llvm::Function* floorFn = llvm::Intrinsic::getOrInsertDeclaration(
          module_, llvm::Intrinsic::floor, {quotient->getType()});
      return builder.CreateCall(floorFn, {quotient});
    }
    return builder.CreateSDiv(left, right);
  case BinaryOp::Mod:
    return left->getType()->isFloatingPointTy() ? builder.CreateFRem(left, right)
                                                : builder.CreateSRem(left, right);
  case BinaryOp::Pow: {
    llvm::Function* powFn = runtimeDecl(
        "sere_math_pow", builder.getDoubleTy(), {builder.getDoubleTy(), builder.getDoubleTy()});
    auto toDouble = [&](llvm::Value* value) -> llvm::Value* {
      if (value->getType()->isDoubleTy()) {
        return value;
      }
      if (value->getType()->isFloatingPointTy()) {
        return builder.CreateFPExt(value, builder.getDoubleTy());
      }
      const Type* sourceType = value == left ? leftType : rightType;
      return (sourceType != nullptr && sourceType->isUnsignedInteger())
                 ? builder.CreateUIToFP(value, builder.getDoubleTy())
                 : builder.CreateSIToFP(value, builder.getDoubleTy());
    };
    llvm::Value* base = toDouble(left);
    llvm::Value* exp = toDouble(right);
    llvm::Value* result = builder.CreateCall(powFn, {base, exp});
    if (expr.resolvedType() != nullptr && expr.resolvedType()->isInteger()) {
      return builder.CreateFPToSI(result, lower(expr.resolvedType()));
    }
    if (expr.resolvedType() != nullptr && expr.resolvedType()->isNamed("f32")) {
      return builder.CreateFPTrunc(result, builder.getFloatTy());
    }
    return result;
  }
  case BinaryOp::Eq:
  case BinaryOp::Is:
    return left->getType()->isFloatingPointTy() ? builder.CreateFCmpOEQ(left, right)
                                                : builder.CreateICmpEQ(left, right);
  case BinaryOp::Ne:
  case BinaryOp::IsNot:
    return left->getType()->isFloatingPointTy() ? builder.CreateFCmpONE(left, right)
                                                : builder.CreateICmpNE(left, right);
  case BinaryOp::Lt:
    return left->getType()->isFloatingPointTy() ? builder.CreateFCmpOLT(left, right)
                                                : builder.CreateICmpSLT(left, right);
  case BinaryOp::Le:
    return left->getType()->isFloatingPointTy() ? builder.CreateFCmpOLE(left, right)
                                                : builder.CreateICmpSLE(left, right);
  case BinaryOp::Gt:
    return left->getType()->isFloatingPointTy() ? builder.CreateFCmpOGT(left, right)
                                                : builder.CreateICmpSGT(left, right);
  case BinaryOp::Ge:
    return left->getType()->isFloatingPointTy() ? builder.CreateFCmpOGE(left, right)
                                                : builder.CreateICmpSGE(left, right);
  case BinaryOp::BitAnd:
    return builder.CreateAnd(left, right);
  case BinaryOp::BitOr:
    return builder.CreateOr(left, right);
  case BinaryOp::BitXor:
    return builder.CreateXor(left, right);
  case BinaryOp::Shl:
    return builder.CreateShl(left, right);
  case BinaryOp::Shr:
    return builder.CreateAShr(left, right);
  case BinaryOp::In:
  case BinaryOp::NotIn: {
    llvm::Value* contained = builder.getInt1(false);
    if (rightType != nullptr && rightType->isNamed("str") && leftType != nullptr &&
        leftType->isNamed("str")) {
      llvm::Function* fn = runtimeDecl(
          "sere_str_contains",
          builder.getInt32Ty(),
          {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()});
      contained = builder.CreateICmpNE(builder.CreateCall(fn,
                                                          {builder.CreateExtractValue(right, {0}),
                                                           builder.CreateExtractValue(right, {1}),
                                                           builder.CreateExtractValue(left, {0}),
                                                           builder.CreateExtractValue(left, {1})}),
                                       builder.getInt32(0));
    } else if (rightType != nullptr && rightType->isSequence()) {
      llvm::Function* fn = runtimeDecl(
          "sere_list_contains", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
      contained = builder.CreateICmpNE(
          builder.CreateCall(fn, {right, emitTempSlot(builder, left, leftType)}),
          builder.getInt32(0));
    } else if (rightType != nullptr && rightType->methodIndex("__contains__") >= 0) {
      llvm::Value* result = emitDunderCall(builder, expr.right(), "__contains__", {left});
      contained = result == nullptr ? builder.getInt1(false) : result;
    } else if (rightType != nullptr && rightType->isEnum() && rightType->isFlags() &&
               leftType != nullptr && leftType->isEnum()) {
      llvm::Value* leftTag = emitEnumTag(builder, left);
      llvm::Value* rightTag = emitEnumTag(builder, right);
      contained = builder.CreateICmpNE(builder.CreateAnd(leftTag, rightTag), builder.getInt32(0));
    } else if (leftType != nullptr && leftType->isIntEnum() && rightType != nullptr &&
               rightType->isInteger()) {
      llvm::Value* leftTag = emitEnumTag(builder, left);
      llvm::Value* mask = right->getType() == builder.getInt32Ty()
                              ? right
                              : builder.CreateIntCast(right, builder.getInt32Ty(), false);
      contained = builder.CreateICmpNE(builder.CreateAnd(leftTag, mask), builder.getInt32(0));
    } else if (rightType != nullptr && rightType->isDict()) {
      llvm::Function* getFn =
          runtimeDecl("sere_dict_get",
                      builder.getInt32Ty(),
                      {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()});
      llvm::Value* out = builder.CreateAlloca(lower(rightType->dictValueType()), nullptr, "in.out");
      contained = builder.CreateICmpNE(
          builder.CreateCall(getFn, {right, emitTempSlot(builder, left, leftType), out}),
          builder.getInt32(0));
    }
    return expr.op() == BinaryOp::NotIn ? builder.CreateNot(contained) : contained;
  }
  case BinaryOp::And:
  case BinaryOp::Or:
    return nullptr;
  }
  return nullptr;
}

llvm::Value* IRGenerator::emitUnary(llvm::IRBuilder<>& builder, const UnaryExpr& expr) {
  if (expr.op() == UnaryOp::AddrOf) {
    return emitAddress(builder, expr.operand());
  }
  if (expr.op() == UnaryOp::Deref) {
    llvm::Value* pointer = emitExpr(builder, expr.operand());
    if (pointer == nullptr || expr.resolvedType() == nullptr) {
      return nullptr;
    }
    return builder.CreateLoad(lower(expr.resolvedType()), pointer);
  }
  if (expr.op() == UnaryOp::PreInc || expr.op() == UnaryOp::PreDec ||
      expr.op() == UnaryOp::PostInc || expr.op() == UnaryOp::PostDec) {
    llvm::Value* address = emitAddress(builder, expr.operand());
    if (address == nullptr) {
      return nullptr;
    }
    llvm::Value* current = builder.CreateLoad(lower(expr.resolvedType()), address);
    const bool isFloat = current->getType()->isFloatingPointTy();
    llvm::Value* one =
        isFloat ? static_cast<llvm::Value*>(llvm::ConstantFP::get(current->getType(), 1.0))
                : static_cast<llvm::Value*>(llvm::ConstantInt::get(current->getType(), 1));
    const bool isDec = expr.op() == UnaryOp::PreDec || expr.op() == UnaryOp::PostDec;
    llvm::Value* next =
        isDec ? (isFloat ? builder.CreateFSub(current, one) : builder.CreateSub(current, one))
              : (isFloat ? builder.CreateFAdd(current, one) : builder.CreateAdd(current, one));
    builder.CreateStore(next, address);
    return expr.op() == UnaryOp::PostInc || expr.op() == UnaryOp::PostDec ? current : next;
  }
  llvm::Value* operand = emitBuiltinExpr(builder, expr.operand());
  if (operand == nullptr) {
    return nullptr;
  }
  if (expr.op() == UnaryOp::Not) {
    return builder.CreateNot(operand);
  }
  if (expr.op() == UnaryOp::Invert) {
    return builder.CreateNot(operand);
  }
  if (expr.op() == UnaryOp::Pos) {
    return operand;
  }
  if (operand->getType()->isFloatingPointTy()) {
    return builder.CreateFNeg(operand);
  }
  return builder.CreateNeg(operand);
}

bool IRGenerator::emitBlock(llvm::IRBuilder<>& builder,
                            const std::vector<std::unique_ptr<Stmt>>& body,
                            const Type* returnType) {
  for (const std::unique_ptr<Stmt>& statement : body) {
    if (!emitStatement(builder, *statement, returnType)) {
      return false;
    }
    if (builder.GetInsertBlock()->getTerminator() != nullptr) {
      return true;
    }
    emitErrorCheck(builder);
  }
  return true;
}

bool IRGenerator::emitIf(llvm::IRBuilder<>& builder,
                         const IfStmt& statement,
                         const Type* returnType) {
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "if.end", function);
  for (const IfBranch& branch : statement.branches()) {
    if (branch.condition == nullptr) {
      if (!emitBlock(builder, branch.body, returnType)) {
        return false;
      }
      if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        builder.CreateBr(merge);
      }
      break;
    }
    const std::optional<bool> known = constBool(*branch.condition);
    if (known.has_value() && !*known) {
      continue;
    }
    if (known.has_value() && *known) {
      if (!emitBlock(builder, branch.body, returnType)) {
        return false;
      }
      if (builder.GetInsertBlock()->getTerminator() == nullptr) {
        builder.CreateBr(merge);
      }
      break;
    }
    llvm::Value* cond = emitExpr(builder, *branch.condition);
    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*context_, "if.then", function);
    llvm::BasicBlock* nextBlock = llvm::BasicBlock::Create(*context_, "if.next", function);
    builder.CreateCondBr(cond, thenBlock, nextBlock);
    builder.SetInsertPoint(thenBlock);
    if (!emitBlock(builder, branch.body, returnType)) {
      return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
      builder.CreateBr(merge);
    }
    builder.SetInsertPoint(nextBlock);
  }
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    builder.CreateBr(merge);
  }
  if (merge->hasNPredecessors(0)) {
    merge->eraseFromParent();
  } else {
    builder.SetInsertPoint(merge);
  }
  return true;
}

bool IRGenerator::emitWhile(llvm::IRBuilder<>& builder,
                            const WhileStmt& statement,
                            const Type* returnType) {
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "while.cond", function);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "while.body", function);
  llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "while.end", function);
  builder.CreateBr(header);
  builder.SetInsertPoint(header);
  llvm::Value* cond = emitExpr(builder, statement.condition());
  builder.CreateCondBr(cond, body, exit);
  builder.SetInsertPoint(body);
  loops_.emplace_back(header, exit);
  if (!emitBlock(builder, statement.body(), returnType)) {
    loops_.pop_back();
    return false;
  }
  loops_.pop_back();
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    builder.CreateBr(header);
  }
  builder.SetInsertPoint(exit);
  return true;
}

llvm::Value* IRGenerator::emitRange(llvm::IRBuilder<>& builder, const CallExpr& expr) {
  llvm::Value* start = builder.getInt32(0);
  llvm::Value* stop = emitExpr(builder, *expr.arguments()[0]);
  llvm::Value* step = builder.getInt32(1);
  if (expr.arguments().size() >= 2) {
    start = stop;
    stop = emitExpr(builder, *expr.arguments()[1]);
  }
  if (expr.arguments().size() == 3) {
    step = emitExpr(builder, *expr.arguments()[2]);
  }
  llvm::Function* newFn = runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* pushFn =
      runtimeDecl("sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
  llvm::Value* list = builder.CreateCall(newFn, {builder.getInt64(4)});
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::Value* index = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "range.i");
  builder.CreateStore(start, index);
  llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "range.cond", function);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "range.body", function);
  llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "range.end", function);
  builder.CreateBr(header);
  builder.SetInsertPoint(header);
  llvm::Value* current = builder.CreateLoad(builder.getInt32Ty(), index);
  llvm::Value* positive = builder.CreateICmpSGT(step, builder.getInt32(0));
  llvm::Value* fwd = builder.CreateICmpSLT(current, stop);
  llvm::Value* back = builder.CreateICmpSGT(current, stop);
  builder.CreateCondBr(builder.CreateSelect(positive, fwd, back), body, exit);
  builder.SetInsertPoint(body);
  llvm::Value* slot = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "range.el");
  builder.CreateStore(current, slot);
  builder.CreateCall(pushFn, {list, slot});
  builder.CreateStore(builder.CreateAdd(current, step), index);
  builder.CreateBr(header);
  builder.SetInsertPoint(exit);
  return list;
}

llvm::Value*
IRGenerator::emitParse(llvm::IRBuilder<>& builder, const CallExpr& expr, bool optional) {
  const Type* requested = expr.typeArgs().empty() ? nullptr : expr.typeArgs()[0]->resolvedType();
  if (requested == nullptr || expr.arguments().empty()) {
    return nullptr;
  }
  llvm::Value* text = emitExpr(builder, *expr.arguments()[0]);
  if (text == nullptr) {
    return nullptr;
  }
  llvm::Value* data = builder.CreateExtractValue(text, {0});
  llvm::Value* len = builder.CreateExtractValue(text, {1});
  const Type* parsedType = requested->canonical();
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* okBlock = llvm::BasicBlock::Create(*context_, "parse.ok", function);
  llvm::BasicBlock* failBlock = llvm::BasicBlock::Create(*context_, "parse.fail", function);
  llvm::BasicBlock* doneBlock =
      optional ? llvm::BasicBlock::Create(*context_, "parse.end", function) : nullptr;
  llvm::Value* ok = nullptr;
  llvm::Value* raw = nullptr;
  const Type* rawType = parsedType;
  if (parsedType->isNamed("str")) {
    ok = builder.getInt1(true);
    raw = text;
    rawType = types_->strType();
  } else if (parsedType->isVoidLike()) {
    llvm::Function* fn = runtimeDecl(
        "sere_parse_none", builder.getInt32Ty(), {builder.getPtrTy(), builder.getInt64Ty()});
    ok = builder.CreateICmpNE(builder.CreateCall(fn, {data, len}), builder.getInt32(0));
    raw = emitDefault(types_->noneType());
    rawType = types_->noneType();
  } else if (parsedType->isNamed("bool")) {
    llvm::Value* slot = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "parse.bool");
    llvm::Function* fn =
        runtimeDecl("sere_parse_bool",
                    builder.getInt32Ty(),
                    {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy()});
    ok = builder.CreateICmpNE(builder.CreateCall(fn, {data, len, slot}), builder.getInt32(0));
    raw = builder.CreateTrunc(builder.CreateLoad(builder.getInt32Ty(), slot), builder.getInt1Ty());
    rawType = types_->boolType();
  } else if (parsedType->isNamed("f32") || parsedType->isNamed("f64") ||
             (parsedType->isUnion() && !parsedType->args().empty() &&
              parsedType->args()[0] != nullptr && parsedType->args()[0]->isFloat())) {
    rawType = parsedType->isNamed("f32") ? types_->f32Type() : types_->f64Type();
    llvm::Value* slot = builder.CreateAlloca(builder.getDoubleTy(), nullptr, "parse.f");
    llvm::Function* fn = runtimeDecl(
        "sere_parse_float",
        builder.getInt32Ty(),
        {builder.getPtrTy(), builder.getInt64Ty(), builder.getInt32Ty(), builder.getPtrTy()});
    ok = builder.CreateICmpNE(
        builder.CreateCall(fn,
                           {data, len, builder.getInt32(rawType->isNamed("f32") ? 1 : 0), slot}),
        builder.getInt32(0));
    raw = builder.CreateLoad(builder.getDoubleTy(), slot);
    if (rawType->isNamed("f32")) {
      raw = builder.CreateFPTrunc(raw, builder.getFloatTy());
    }
  } else {
    rawType = parsedType->isScalarInteger() ? parsedType : types_->i64Type();
    if (parsedType->isUnion() && parsedType->isInteger()) {
      rawType = types_->i64Type();
    }
    llvm::Value* slot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "parse.i");
    llvm::Function* fn = runtimeDecl("sere_parse_int",
                                     builder.getInt32Ty(),
                                     {builder.getPtrTy(),
                                      builder.getInt64Ty(),
                                      builder.getInt32Ty(),
                                      builder.getInt32Ty(),
                                      builder.getPtrTy()});
    const int bits = rawType->integerBitWidth() <= 0 ? 64 : rawType->integerBitWidth();
    ok = builder.CreateICmpNE(
        builder.CreateCall(fn,
                           {data,
                            len,
                            builder.getInt32(bits),
                            builder.getInt32(rawType->isUnsignedInteger() ? 0 : 1),
                            slot}),
        builder.getInt32(0));
    raw = builder.CreateLoad(builder.getInt64Ty(), slot);
    if (rawType != types_->i64Type() && rawType != types_->primitive("u64")) {
      raw = emitNumericCast(builder, raw, types_->i64Type(), rawType);
    }
  }
  builder.CreateCondBr(ok, okBlock, failBlock);
  builder.SetInsertPoint(failBlock);
  if (optional) {
    llvm::Value* failure = emitDefault(expr.resolvedType());
    builder.CreateBr(doneBlock);
    llvm::BasicBlock* failEnd = builder.GetInsertBlock();
    builder.SetInsertPoint(okBlock);
    llvm::Value* success = emitCoerce(builder, raw, rawType, expr.resolvedType());
    builder.CreateBr(doneBlock);
    llvm::BasicBlock* okEnd = builder.GetInsertBlock();
    builder.SetInsertPoint(doneBlock);
    llvm::PHINode* phi = builder.CreatePHI(lower(expr.resolvedType()), 2, "parse.val");
    phi->addIncoming(success, okEnd);
    phi->addIncoming(failure, failEnd);
    return phi;
  }
  llvm::Value* message = emitStrLiteral(builder, "cannot parse string");
  llvm::Function* panic =
      runtimeDecl("sere_panic", builder.getVoidTy(), {builder.getPtrTy(), builder.getInt64Ty()});
  builder.CreateCall(
      panic, {builder.CreateExtractValue(message, {0}), builder.CreateExtractValue(message, {1})});
  builder.CreateUnreachable();
  builder.SetInsertPoint(okBlock);
  return emitCoerce(builder, raw, rawType, expr.resolvedType());
}

bool IRGenerator::emitFor(llvm::IRBuilder<>& builder,
                          const ForStmt& statement,
                          const Type* returnType) {
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  const Expr& iterable = statement.iterable();
  if (iterable.kind() == NodeKind::CallExpr &&
      static_cast<const CallExpr&>(iterable).intrinsic() == IntrinsicKind::Range) {
    const auto& range = static_cast<const CallExpr&>(iterable);
    llvm::Value* start = builder.getInt32(0);
    llvm::Value* stop = emitExpr(builder, *range.arguments()[0]);
    llvm::Value* step = builder.getInt32(1);
    if (range.arguments().size() >= 2) {
      start = stop;
      stop = emitExpr(builder, *range.arguments()[1]);
    }
    if (range.arguments().size() == 3) {
      step = emitExpr(builder, *range.arguments()[2]);
    }
    llvm::Value* index = builder.CreateAlloca(builder.getInt32Ty(), nullptr, statement.name());
    builder.CreateStore(start, index);
    rememberLocal(statement.name(), index, types_->i32Type());
    llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "for.cond", function);
    llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "for.body", function);
    llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "for.end", function);
    builder.CreateBr(header);
    builder.SetInsertPoint(header);
    llvm::Value* current = builder.CreateLoad(builder.getInt32Ty(), index);
    llvm::Value* positive = builder.CreateICmpSGT(step, builder.getInt32(0));
    llvm::Value* fwd = builder.CreateICmpSLT(current, stop);
    llvm::Value* back = builder.CreateICmpSGT(current, stop);
    builder.CreateCondBr(builder.CreateSelect(positive, fwd, back), body, exit);
    builder.SetInsertPoint(body);
    loops_.emplace_back(header, exit);
    if (!emitBlock(builder, statement.body(), returnType)) {
      loops_.pop_back();
      return false;
    }
    loops_.pop_back();
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
      llvm::Value* now = builder.CreateLoad(builder.getInt32Ty(), index);
      builder.CreateStore(builder.CreateAdd(now, step), index);
      builder.CreateBr(header);
    }
    builder.SetInsertPoint(exit);
    return true;
  }
  if (iterable.resolvedType() != nullptr && iterable.resolvedType()->isNamed("str")) {
    llvm::Value* str = emitExpr(builder, iterable);
    llvm::Value* length = builder.CreateExtractValue(str, {1});
    llvm::Value* cursor = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "for.i");
    llvm::Value* item = builder.CreateAlloca(lower(types_->strType()), nullptr, statement.name());
    builder.CreateStore(builder.getInt64(0), cursor);
    rememberLocal(statement.name(), item, types_->strType());
    llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "for.cond", function);
    llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "for.body", function);
    llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "for.end", function);
    builder.CreateBr(header);
    builder.SetInsertPoint(header);
    llvm::Value* current = builder.CreateLoad(builder.getInt64Ty(), cursor);
    builder.CreateCondBr(builder.CreateICmpSLT(current, length), body, exit);
    builder.SetInsertPoint(body);
    llvm::Function* indexFn = runtimeDecl("sere_str_index",
                                          builder.getVoidTy(),
                                          {builder.getPtrTy(),
                                           builder.getInt64Ty(),
                                           builder.getInt64Ty(),
                                           builder.getPtrTy(),
                                           builder.getPtrTy()});
    llvm::Value* dataSlot = builder.CreateAlloca(builder.getPtrTy(), nullptr, "ch.data");
    llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "ch.len");
    builder.CreateCall(indexFn,
                       {builder.CreateExtractValue(str, {0}), length, current, dataSlot, lenSlot});
    builder.CreateStore(packStr(builder,
                                builder.CreateLoad(builder.getPtrTy(), dataSlot),
                                builder.CreateLoad(builder.getInt64Ty(), lenSlot)),
                        item);
    loops_.emplace_back(header, exit);
    if (!emitBlock(builder, statement.body(), returnType)) {
      loops_.pop_back();
      return false;
    }
    loops_.pop_back();
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
      llvm::Value* now = builder.CreateLoad(builder.getInt64Ty(), cursor);
      builder.CreateStore(builder.CreateAdd(now, builder.getInt64(1)), cursor);
      builder.CreateBr(header);
    }
    builder.SetInsertPoint(exit);
    return true;
  }
  llvm::Value* list = emitExpr(builder, iterable);
  llvm::Function* lenFn = runtimeDecl("sere_list_len", builder.getInt64Ty(), {builder.getPtrTy()});
  llvm::Function* itemFn =
      runtimeDecl("sere_list_item", builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()});
  const Type* element = iterable.resolvedType() == nullptr ? types_->i32Type()
                                                           : iterable.resolvedType()->elementType();
  llvm::Value* index = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "for.i");
  llvm::Value* item = builder.CreateAlloca(lower(element), nullptr, statement.name());
  builder.CreateStore(builder.getInt64(0), index);
  rememberLocal(statement.name(), item, element);
  llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "for.cond", function);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "for.body", function);
  llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "for.end", function);
  builder.CreateBr(header);
  builder.SetInsertPoint(header);
  llvm::Value* current = builder.CreateLoad(builder.getInt64Ty(), index);
  llvm::Value* length = builder.CreateCall(lenFn, {list});
  builder.CreateCondBr(builder.CreateICmpSLT(current, length), body, exit);
  builder.SetInsertPoint(body);
  llvm::Value* slot = builder.CreateCall(itemFn, {list, current});
  builder.CreateStore(builder.CreateLoad(lower(element), slot), item);
  loops_.emplace_back(header, exit);
  if (!emitBlock(builder, statement.body(), returnType)) {
    loops_.pop_back();
    return false;
  }
  loops_.pop_back();
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    llvm::Value* now = builder.CreateLoad(builder.getInt64Ty(), index);
    builder.CreateStore(builder.CreateAdd(now, builder.getInt64(1)), index);
    builder.CreateBr(header);
  }
  builder.SetInsertPoint(exit);
  return true;
}

llvm::Value* IRGenerator::emitComprehension(llvm::IRBuilder<>& builder,
                                            const ComprehensionExpr& expr) {
  llvm::Value* source = emitExpr(builder, expr.iterable());
  const Type* bindType =
      expr.iterable().resolvedType() != nullptr && expr.iterable().resolvedType()->isSequence()
          ? expr.iterable().resolvedType()->elementType()
          : types_->i32Type();
  const Type* valueType =
      expr.element().resolvedType() == nullptr ? bindType : expr.element().resolvedType();
  llvm::Function* newFn = runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* pushFn =
      runtimeDecl("sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
  llvm::Function* lenFn = runtimeDecl("sere_list_len", builder.getInt64Ty(), {builder.getPtrTy()});
  llvm::Function* itemFn =
      runtimeDecl("sere_list_item", builder.getPtrTy(), {builder.getPtrTy(), builder.getInt64Ty()});
  llvm::Value* out = builder.CreateCall(newFn, {builder.getInt64(valueSize(valueType))});
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::Value* index = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "comp.i");
  llvm::Value* item = builder.CreateAlloca(lower(bindType), nullptr, expr.name());
  rememberLocal(expr.name(), item, bindType);
  builder.CreateStore(builder.getInt64(0), index);
  llvm::BasicBlock* header = llvm::BasicBlock::Create(*context_, "comp.cond", function);
  llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "comp.body", function);
  llvm::BasicBlock* exit = llvm::BasicBlock::Create(*context_, "comp.end", function);
  builder.CreateBr(header);
  builder.SetInsertPoint(header);
  llvm::Value* current = builder.CreateLoad(builder.getInt64Ty(), index);
  builder.CreateCondBr(
      builder.CreateICmpSLT(current, builder.CreateCall(lenFn, {source})), body, exit);
  builder.SetInsertPoint(body);
  llvm::Value* slot = builder.CreateCall(itemFn, {source, current});
  builder.CreateStore(builder.CreateLoad(lower(bindType), slot), item);
  llvm::Value* mapped = emitExpr(builder, expr.element());
  llvm::Value* mappedSlot = builder.CreateAlloca(lower(valueType), nullptr, "comp.el");
  builder.CreateStore(mapped, mappedSlot);
  builder.CreateCall(pushFn, {out, mappedSlot});
  builder.CreateStore(builder.CreateAdd(current, builder.getInt64(1)), index);
  builder.CreateBr(header);
  builder.SetInsertPoint(exit);
  return out;
}

bool IRGenerator::emitAssert(llvm::IRBuilder<>& builder, const AssertStmt& statement) {
  llvm::Value* cond = emitExpr(builder, statement.condition());
  if (cond == nullptr) {
    return false;
  }
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* fail = llvm::BasicBlock::Create(*context_, "assert.fail", function);
  llvm::BasicBlock* ok = llvm::BasicBlock::Create(*context_, "assert.ok", function);
  builder.CreateCondBr(cond, ok, fail);
  builder.SetInsertPoint(fail);
  llvm::Value* message = statement.message() == nullptr
                             ? emitStrLiteral(builder, "assertion failed")
                             : emitToStr(builder, *statement.message());
  llvm::Function* raiseFn =
      runtimeDecl("sere_raise",
                  builder.getVoidTy(),
                  {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()});
  builder.CreateCall(raiseFn,
                     {builder.CreateGlobalString("AssertionError;Exception", "", 0, module_),
                      builder.CreateExtractValue(message, {0}),
                      builder.CreateExtractValue(message, {1})});
  emitErrorCheck(builder);
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    builder.CreateUnreachable();
  }
  builder.SetInsertPoint(ok);
  return true;
}

llvm::Value* IRGenerator::emitExpr(llvm::IRBuilder<>& builder, const Expr& expr) {
  switch (expr.kind()) {
  case NodeKind::IntegerLiteral: {
    const auto& literal = static_cast<const IntegerLiteral&>(expr);
    const Type* type = resolveType(expr.resolvedType());
    if (literal.isByte() || (type != nullptr && (type->isNamed("i8") || type->isNamed("u8")))) {
      return builder.getInt8(static_cast<std::uint8_t>(literal.value()));
    }
    return builder.getInt32(static_cast<std::uint32_t>(literal.value()));
  }
  case NodeKind::FloatLiteral: {
    const auto& literal = static_cast<const FloatLiteral&>(expr);
    llvm::Type* llvmType = literal.isF32() ? builder.getFloatTy() : builder.getDoubleTy();
    return llvm::ConstantFP::get(llvmType, literal.value());
  }
  case NodeKind::BooleanLiteral:
    return builder.getInt1(static_cast<const BooleanLiteral&>(expr).value());
  case NodeKind::NoneLiteral:
    return llvm::ConstantPointerNull::get(builder.getPtrTy());
  case NodeKind::AwaitExpr:
    return emitAwait(builder, static_cast<const AwaitExpr&>(expr));
  case NodeKind::StringLiteral: {
    return emitStrLiteral(builder, static_cast<const StringLiteral&>(expr).value());
  }
  case NodeKind::InterpolatedStringExpr:
    return emitInterpolated(builder, static_cast<const InterpolatedStringExpr&>(expr));
  case NodeKind::NameExpr: {
    const auto& name = static_cast<const NameExpr&>(expr);
    if (!name.compileTimeText().empty()) {
      return emitStrLiteral(builder, name.compileTimeText());
    }
    if (name.hasCompileTimeBool()) {
      return builder.getInt1(name.compileTimeBool());
    }
    const Type* type = resolveType(expr.resolvedType());
    if (type != nullptr && type->isVoidLike()) {
      return nullptr;
    }
    if (type != nullptr && type->isEnum()) {
      const RecordField* field = type->findField(name.name());
      if (field != nullptr && !field->llvmName.empty()) {
        return emitEnumUnit(builder, type, enumTagFromField(field));
      }
    }
    if (type != nullptr && type->isTypeObject()) {
      if (llvm::Value* address = emitAddress(builder, expr, false)) {
        return builder.CreateLoad(lower(type), address);
      }
      return builder.getInt32(recordTypeId(type->typeObjectInstance()));
    }
    if (type != nullptr && (type->kind() == TypeKind::Function || type->isCallableConstraint())) {
      if (llvm::Value* address = emitAddress(builder, expr, false)) {
        return builder.CreateLoad(lower(type), address);
      }
      const auto found = functions_.find(name.name());
      if (found != functions_.end()) {
        const auto defFound = functionDefs_.find(name.name());
        if (defFound != functionDefs_.end() && defFound->second != nullptr &&
            defFound->second->decoratedType() != nullptr) {
          llvm::Value* slot = decoratorSlots_[llvmNameFor(*defFound->second)];
          if (slot != nullptr) {
            return builder.CreateLoad(lower(defFound->second->decoratedType()), slot);
          }
        }
        return packCallable(builder, found->second, nullptr);
      }
    }
    llvm::Value* address = emitAddress(builder, expr);
    if (address == nullptr) {
      return nullptr;
    }
    return builder.CreateLoad(lower(expr.resolvedType()), address);
  }
  case NodeKind::MemberExpr: {
    const auto& member = static_cast<const MemberExpr&>(expr);
    if (!member.compileTimeText().empty()) {
      return emitStrLiteral(builder, member.compileTimeText());
    }
    const Type* objectType = resolveType(member.object().resolvedType());
    // A bare type name (class or enum) resolves to a TypeObject; enum
    // variants such as `Color.Red` are then accessed through that meta type.
    const Type* instanceType = objectType;
    if (objectType != nullptr && objectType->isTypeObject() &&
        objectType->typeObjectInstance() != nullptr) {
      instanceType = objectType->typeObjectInstance()->canonical();
    }
    if (instanceType != nullptr && instanceType->isEnum() && member.field() == "name") {
      return emitEnumName(builder, member.object());
    }
    if (instanceType != nullptr && instanceType->isEnum() && member.field() == "value") {
      return emitEnumTag(builder, emitExpr(builder, member.object()));
    }
    if (instanceType != nullptr && instanceType->isEnum()) {
      const RecordField* field = instanceType->findField(member.field());
      if (field != nullptr && !field->llvmName.empty()) {
        return emitEnumUnit(builder, instanceType, enumTagFromField(field));
      }
    }
    if (member.isUnboundMethod()) {
      const auto found = functions_.find(member.boundMethodLlvm());
      if (found == functions_.end()) {
        diagnostics_->error(expr.range(),
                            "no LLVM function for method '" + member.boundMethodLlvm() + "'");
        return nullptr;
      }
      llvm::Value* fakeSelf = builder.CreateIntToPtr(builder.getInt64(1), builder.getPtrTy());
      return packCallable(builder, found->second, fakeSelf);
    }
    if (member.isBoundMethod()) {
      const auto found = functions_.find(member.boundMethodLlvm());
      if (found == functions_.end()) {
        diagnostics_->error(expr.range(),
                            "no LLVM function for method '" + member.boundMethodLlvm() + "'");
        return nullptr;
      }
      llvm::Value* self = emitObjectPointer(builder, member.object(), objectType);
      if (self == nullptr) {
        return nullptr;
      }
      return packCallable(builder, found->second, self);
    }
    if (!member.usesBackingField() && !member.propertyGet().empty()) {
      llvm::Value* self = emitObjectPointer(builder, member.object(), objectType);
      return emitNamedMethod(builder, member.propertyGet(), self, {});
    }
    llvm::Value* address = emitAddress(builder, expr);
    if (address == nullptr) {
      return nullptr;
    }
    return builder.CreateLoad(lower(expr.resolvedType()), address);
  }
  case NodeKind::CallExpr:
    return emitCall(builder, static_cast<const CallExpr&>(expr));
  case NodeKind::BinaryExpr:
    return emitBinary(builder, static_cast<const BinaryExpr&>(expr));
  case NodeKind::UnaryExpr:
    return emitUnary(builder, static_cast<const UnaryExpr&>(expr));
  case NodeKind::CastExpr:
    return emitCast(builder, static_cast<const CastExpr&>(expr));
  case NodeKind::IndexExpr:
    return emitIndex(builder, static_cast<const IndexExpr&>(expr));
  case NodeKind::ListLiteral:
    return emitListLiteral(builder, static_cast<const ListLiteral&>(expr));
  case NodeKind::DictLiteral:
    return emitDictLiteral(builder, static_cast<const DictLiteral&>(expr));
  case NodeKind::ComprehensionExpr:
    return emitComprehension(builder, static_cast<const ComprehensionExpr&>(expr));
  case NodeKind::TernaryExpr:
    return emitTernary(builder, static_cast<const TernaryExpr&>(expr));
  case NodeKind::TupleExpr:
    return emitTuple(builder, static_cast<const TupleExpr&>(expr));
  case NodeKind::WalrusExpr:
    return emitWalrus(builder, static_cast<const WalrusExpr&>(expr));
  case NodeKind::LambdaExpr:
    return emitLambda(builder, static_cast<const LambdaExpr&>(expr));
  default:
    diagnostics_->error(expr.range(), "unsupported expression in codegen");
    return nullptr;
  }
}

bool IRGenerator::emitStatement(llvm::IRBuilder<>& builder,
                                const Stmt& statement,
                                const Type* returnType) {
  if (statement.kind() == NodeKind::VarDecl) {
    const auto& decl = static_cast<const VarDecl&>(statement);
    if (decl.isStatic() && currentFunction_ != nullptr) {
      const std::string globalName = functionStaticName(*currentFunction_, decl.name());
      llvm::Value* slot = globals_.contains(globalName)
                              ? globals_[globalName]
                              : declareGlobal(globalName, decl.resolvedType());
      rememberLocal(decl.name(), slot, decl.resolvedType());
      if (decl.init() != nullptr) {
        llvm::GlobalVariable* guard =
            new llvm::GlobalVariable(*module_,
                                     builder.getInt8Ty(),
                                     false,
                                     llvm::GlobalValue::InternalLinkage,
                                     builder.getInt8(0),
                                     functionStaticName(*currentFunction_, decl.name()) + ".once");
        llvm::Value* done = builder.CreateLoad(builder.getInt8Ty(), guard);
        llvm::Function* function = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* initBlock = llvm::BasicBlock::Create(*context_, "static.init", function);
        llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(*context_, "static.cont", function);
        builder.CreateCondBr(builder.CreateICmpEQ(done, builder.getInt8(0)), initBlock, contBlock);
        builder.SetInsertPoint(initBlock);
        builder.CreateStore(emitExpr(builder, *decl.init()), slot);
        builder.CreateStore(builder.getInt8(1), guard);
        builder.CreateBr(contBlock);
        builder.SetInsertPoint(contBlock);
      }
      return slot != nullptr;
    }
    llvm::Value* slot = builder.CreateAlloca(lower(decl.resolvedType()), nullptr, decl.name());
    llvm::Value* init = decl.init() == nullptr ? emitDefault(decl.resolvedType())
                                               : emitCoerce(builder,
                                                            emitExpr(builder, *decl.init()),
                                                            decl.init()->resolvedType(),
                                                            decl.resolvedType());
    if (init != nullptr && !init->getType()->isVoidTy()) {
      builder.CreateStore(init, slot);
    }
    rememberLocal(decl.name(), slot, decl.resolvedType());
    return true;
  }
  if (statement.kind() == NodeKind::AssignStmt) {
    const auto& assign = static_cast<const AssignStmt&>(statement);
    if (assign.isNameAlias()) {
      return true;
    }
    if (assign.target().kind() == NodeKind::TupleExpr) {
      llvm::Value* packed = emitExpr(builder, assign.value());
      return emitUnpack(builder,
                        static_cast<const TupleExpr&>(assign.target()),
                        packed,
                        assign.value().resolvedType());
    }
    if (assign.target().kind() == NodeKind::IndexExpr) {
      const auto& index = static_cast<const IndexExpr&>(assign.target());
      const Type* objectType = resolveType(index.object().resolvedType());
      if (objectType != nullptr && objectType->isDict()) {
        return emitDictAssign(builder, index, assign.value());
      }
      if (objectType != nullptr && objectType->methodIndex("__setitem__") >= 0 &&
          index.hasStart()) {
        llvm::Value* key = emitExpr(builder, *index.start());
        llvm::Value* stored = emitExpr(builder, assign.value());
        const RecordMethod& method = objectType->methods()[
            static_cast<std::size_t>(objectType->methodIndex("__setitem__"))];
        if (method.type != nullptr && method.type->paramTypes().size() == 3) {
          key = emitCoerce(builder, key, index.start()->resolvedType(),
                           method.type->paramTypes()[1]);
          stored = emitCoerce(builder, stored, assign.value().resolvedType(),
                              method.type->paramTypes()[2]);
        }
        emitDunderCall(builder, index.object(), "__setitem__", {key, stored});
        return true;
      }
    }
    llvm::Value* address = nullptr;
    llvm::Value* propertySelf = nullptr;
    const MemberExpr* propertyTarget = nullptr;
    if (assign.target().kind() == NodeKind::MemberExpr) {
      const auto& member = static_cast<const MemberExpr&>(assign.target());
      if (!member.usesBackingField() && !member.propertySet().empty()) {
        propertyTarget = &member;
        const Type* record = member.object().resolvedType();
        propertySelf = emitObjectPointer(builder, member.object(), record);
        const Type* slotType = assign.target().resolvedType();
        llvm::Type* llvmType = lower(slotType);
        if (llvmType == nullptr || llvmType->isVoidTy() || propertySelf == nullptr) {
          return false;
        }
        address = builder.CreateAlloca(llvmType, nullptr, "prop.tmp");
        if (assign.op() != AssignOp::Assign) {
          llvm::Value* current = emitExpr(builder, assign.target());
          if (current == nullptr) {
            return false;
          }
          builder.CreateStore(current, address);
        }
      }
    }
    if (address == nullptr) {
      if (const NameExpr* name = asName(assign.target())) {
        if (!locals_.contains(name->name()) && !globals_.contains(name->name())) {
          createLocalSlot(builder, name->name(), assign.value().resolvedType());
        }
      }
      address = emitAddress(builder, assign.target());
    }
    llvm::Value* value = emitExpr(builder, assign.value());
    if (address == nullptr || value == nullptr || value->getType()->isVoidTy()) {
      return false;
    }
    if (assign.op() != AssignOp::Assign) {
      llvm::Value* current = builder.CreateLoad(lower(assign.target().resolvedType()), address);
      widenIntegerPair(
          builder, current, value, assign.target().resolvedType(), assign.value().resolvedType());
      switch (assign.op()) {
      case AssignOp::Add:
        value = current->getType()->isFloatingPointTy() ? builder.CreateFAdd(current, value)
                                                        : builder.CreateAdd(current, value);
        break;
      case AssignOp::Sub:
        value = current->getType()->isFloatingPointTy() ? builder.CreateFSub(current, value)
                                                        : builder.CreateSub(current, value);
        break;
      case AssignOp::Mul:
        value = current->getType()->isFloatingPointTy() ? builder.CreateFMul(current, value)
                                                        : builder.CreateMul(current, value);
        break;
      case AssignOp::Div:
        value = current->getType()->isFloatingPointTy() ? builder.CreateFDiv(current, value)
                                                        : builder.CreateSDiv(current, value);
        break;
      case AssignOp::Mod:
        value = current->getType()->isFloatingPointTy() ? builder.CreateFRem(current, value)
                                                        : builder.CreateSRem(current, value);
        break;
      case AssignOp::BitAnd:
        value = builder.CreateAnd(current, value);
        break;
      case AssignOp::BitOr:
        value = builder.CreateOr(current, value);
        break;
      case AssignOp::BitXor:
        value = builder.CreateXor(current, value);
        break;
      case AssignOp::Shl:
        value = builder.CreateShl(current, value);
        break;
      case AssignOp::Shr:
        value = builder.CreateAShr(current, value);
        break;
      case AssignOp::FloorDiv:
        if (current->getType()->isFloatingPointTy()) {
          llvm::Value* quotient = builder.CreateFDiv(current, value);
          llvm::Function* floorFn = llvm::Intrinsic::getOrInsertDeclaration(
              module_, llvm::Intrinsic::floor, {quotient->getType()});
          value = builder.CreateCall(floorFn, {quotient});
        } else {
          value = builder.CreateSDiv(current, value);
        }
        break;
      case AssignOp::Pow: {
        llvm::Function* powFn = runtimeDecl(
            "sere_math_pow", builder.getDoubleTy(), {builder.getDoubleTy(), builder.getDoubleTy()});
        auto toDouble = [&](llvm::Value* operand) -> llvm::Value* {
          if (operand->getType()->isDoubleTy()) {
            return operand;
          }
          if (operand->getType()->isFloatingPointTy()) {
            return builder.CreateFPExt(operand, builder.getDoubleTy());
          }
          return builder.CreateSIToFP(operand, builder.getDoubleTy());
        };
        llvm::Value* result = builder.CreateCall(powFn, {toDouble(current), toDouble(value)});
        if (current->getType()->isIntegerTy()) {
          value = builder.CreateFPToSI(result, current->getType());
        } else if (current->getType()->isFloatTy()) {
          value = builder.CreateFPTrunc(result, current->getType());
        } else {
          value = result;
        }
        break;
      }
      case AssignOp::Assign:
        break;
      }
    }
    const Type* from = assign.value().resolvedType();
    const Type* to = assign.target().resolvedType();
    if (assign.op() != AssignOp::Assign && from != nullptr && to != nullptr) {
      if (from->isFloat() || to->isFloat()) {
        from = (from->isNamed("f64") || to->isNamed("f64")) ? types_->f64Type()
                                                            : (from->isFloat() ? from : to);
      } else if (from->isInteger() && to->isInteger()) {
        from = to->integerBitWidth() >= from->integerBitWidth() ? to : from;
      }
    }
    if (from != nullptr && to != nullptr && from->isRecord() && to->isRecord() && from != to &&
        from->isSubtypeOf(to)) {
      llvm::Value* sliced = llvm::UndefValue::get(lower(to));
      unsigned count = 0;
      for (const RecordField& field : to->fields()) {
        if (!field.isStatic && field.stored) {
          ++count;
        }
      }
      for (unsigned index = 0; index < count; ++index) {
        sliced =
            builder.CreateInsertValue(sliced, builder.CreateExtractValue(value, {index}), {index});
      }
      value = sliced;
    }
    value = emitCoerce(builder, value, from, to);
    if (propertyTarget != nullptr) {
      if (value == nullptr) {
        return false;
      }
      emitNamedMethod(builder, propertyTarget->propertySet(), propertySelf, {value});
      return true;
    }
    if (value != nullptr && !value->getType()->isVoidTy()) {
      builder.CreateStore(value, address);
    }
    return true;
  }
  if (statement.kind() == NodeKind::ReturnStmt) {
    const auto& ret = static_cast<const ReturnStmt&>(statement);
    llvm::Value* value = ret.value() == nullptr ? nullptr : emitExpr(builder, *ret.value());
    if (value != nullptr && ret.value() != nullptr) {
      value = emitCoerce(builder, value, ret.value()->resolvedType(), returnType);
    }
    emitReturn(builder, value, returnType);
    return true;
  }
  if (statement.kind() == NodeKind::ExprStmt) {
    emitExpr(builder, static_cast<const ExprStmt&>(statement).expression());
    return true;
  }
  if (statement.kind() == NodeKind::IfStmt) {
    return emitIf(builder, static_cast<const IfStmt&>(statement), returnType);
  }
  if (statement.kind() == NodeKind::WhileStmt) {
    return emitWhile(builder, static_cast<const WhileStmt&>(statement), returnType);
  }
  if (statement.kind() == NodeKind::ForStmt) {
    return emitFor(builder, static_cast<const ForStmt&>(statement), returnType);
  }
  if (statement.kind() == NodeKind::AssertStmt) {
    return emitAssert(builder, static_cast<const AssertStmt&>(statement));
  }
  if (statement.kind() == NodeKind::BreakStmt) {
    if (loops_.empty()) {
      diagnostics_->error(statement.range(), "'break' outside loop");
      return false;
    }
    builder.CreateBr(loops_.back().second);
    return true;
  }
  if (statement.kind() == NodeKind::ContinueStmt) {
    if (loops_.empty()) {
      diagnostics_->error(statement.range(), "'continue' outside loop");
      return false;
    }
    builder.CreateBr(loops_.back().first);
    return true;
  }
  if (statement.kind() == NodeKind::RaiseStmt) {
    return emitRaise(builder, static_cast<const RaiseStmt&>(statement));
  }
  if (statement.kind() == NodeKind::TryStmt) {
    return emitTry(builder, static_cast<const TryStmt&>(statement), returnType);
  }
  if (statement.kind() == NodeKind::MatchStmt) {
    return emitMatch(builder, static_cast<const MatchStmt&>(statement), returnType);
  }
  if (statement.kind() == NodeKind::DelStmt) {
    return emitDel(builder, static_cast<const DelStmt&>(statement));
  }
  if (statement.kind() == NodeKind::DeferStmt) {
    defers_.push_back(&static_cast<const DeferStmt&>(statement));
    return true;
  }
  if (statement.kind() == NodeKind::WithStmt) {
    return emitWith(builder, static_cast<const WithStmt&>(statement), returnType);
  }
  if (statement.kind() == NodeKind::FunctionDef) {
    const auto& function = static_cast<const FunctionDef&>(statement);
    const auto found = functions_.find(llvmNameFor(function));
    if (found == functions_.end()) {
      diagnostics_->error(function.range(),
                          "internal: missing nested function '" + function.name() + "'");
      return false;
    }
    llvm::Value* environment = nullptr;
    if (!function.captures().empty()) {
      std::vector<llvm::Type*> fields;
      std::uint64_t bytes = 0;
      for (const FunctionDef::Capture& capture : function.captures()) {
        fields.push_back(lower(capture.type));
        bytes += valueSize(capture.type);
      }
      llvm::StructType* environmentType = llvm::StructType::get(*context_, fields);
      llvm::Function* allocate =
          runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
      environment = builder.CreateCall(
          allocate, {builder.getInt64(bytes == 0 ? 1 : bytes)}, function.name() + ".env");
      for (std::size_t index = 0; index < function.captures().size(); ++index) {
        const FunctionDef::Capture& capture = function.captures()[index];
        const auto local = locals_.find(capture.name);
        if (local == locals_.end()) {
          diagnostics_->error(function.range(),
                              "internal: missing captured local '" + capture.name + "'");
          return false;
        }
        llvm::Value* value = builder.CreateLoad(lower(capture.type), local->second);
        builder.CreateStore(
            value,
            builder.CreateStructGEP(environmentType, environment, static_cast<unsigned>(index)));
      }
    }
    llvm::Value* packed = packCallable(builder, found->second, environment);
    llvm::Type* callableType = lower(function.resolvedType());
    llvm::Value* slot = builder.CreateAlloca(callableType, nullptr, function.name());
    builder.CreateStore(builder.CreateLoad(callableType, packed), slot);
    rememberLocal(function.name(), slot, function.resolvedType());
    return true;
  }
  return statement.kind() == NodeKind::PassStmt || statement.kind() == NodeKind::ClassDef ||
         statement.kind() == NodeKind::EnumDef || statement.kind() == NodeKind::TypeAlias ||
         statement.kind() == NodeKind::ImportStmt || statement.kind() == NodeKind::MacroDef;
}

namespace {

void collectExprUses(const Expr& expr,
                     std::vector<std::string>& names,
                     std::vector<const Type*>& printed) {
  if (expr.kind() == NodeKind::CallExpr) {
    const auto& call = static_cast<const CallExpr&>(expr);
    if (!call.loweredName().empty()) {
      names.push_back(call.loweredName());
    }
    if (call.intrinsic() == IntrinsicKind::Print || call.intrinsic() == IntrinsicKind::Str) {
      for (const std::unique_ptr<Expr>& argument : call.arguments()) {
        if (argument->resolvedType() != nullptr && argument->resolvedType()->isRecord()) {
          printed.push_back(argument->resolvedType());
        }
      }
    }
    if (call.intrinsic() == IntrinsicKind::Len && !call.arguments().empty()) {
      const Type* type = call.arguments()[0]->resolvedType();
      if (type != nullptr) {
        const int method = type->methodIndex("__len__");
        if (method >= 0) {
          names.push_back(type->methods()[static_cast<std::size_t>(method)].llvmName);
        }
      }
    }
    collectExprUses(call.callee(), names, printed);
    for (const std::unique_ptr<Expr>& argument : call.arguments()) {
      collectExprUses(*argument, names, printed);
    }
    return;
  }
  if (expr.kind() == NodeKind::MemberExpr) {
    collectExprUses(static_cast<const MemberExpr&>(expr).object(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::BinaryExpr) {
    const auto& binary = static_cast<const BinaryExpr&>(expr);
    collectExprUses(binary.left(), names, printed);
    collectExprUses(binary.right(), names, printed);
    if (binary.op() == BinaryOp::In || binary.op() == BinaryOp::NotIn) {
      const Type* container = binary.right().resolvedType();
      if (container != nullptr) {
        const int contains = container->methodIndex("__contains__");
        if (contains >= 0) {
          names.push_back(container->methods()[static_cast<std::size_t>(contains)].llvmName);
        }
      }
    }
    return;
  }
  if (expr.kind() == NodeKind::UnaryExpr) {
    collectExprUses(static_cast<const UnaryExpr&>(expr).operand(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::CastExpr) {
    collectExprUses(static_cast<const CastExpr&>(expr).value(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::TernaryExpr) {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    collectExprUses(ternary.thenValue(), names, printed);
    collectExprUses(ternary.condition(), names, printed);
    collectExprUses(ternary.elseValue(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::IndexExpr) {
    const auto& index = static_cast<const IndexExpr&>(expr);
    collectExprUses(index.object(), names, printed);
    if (index.start() != nullptr) {
      collectExprUses(*index.start(), names, printed);
    }
    if (index.stop() != nullptr) {
      collectExprUses(*index.stop(), names, printed);
    }
    const Type* objectType = index.object().resolvedType();
    if (objectType != nullptr) {
      const int getItem = objectType->methodIndex("__getitem__");
      if (getItem >= 0) {
        names.push_back(objectType->methods()[static_cast<std::size_t>(getItem)].llvmName);
      }
    }
    return;
  }
  if (expr.kind() == NodeKind::ListLiteral) {
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      collectExprUses(*item, names, printed);
    }
    return;
  }
  if (expr.kind() == NodeKind::DictLiteral) {
    const auto& dict = static_cast<const DictLiteral&>(expr);
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      collectExprUses(*dict.keys()[index], names, printed);
      collectExprUses(*dict.values()[index], names, printed);
    }
    return;
  }
  if (expr.kind() == NodeKind::InterpolatedStringExpr) {
    for (const StringPart& part : static_cast<const InterpolatedStringExpr&>(expr).parts()) {
      if (part.value != nullptr) {
        collectExprUses(*part.value, names, printed);
      }
    }
    return;
  }
  if (expr.kind() == NodeKind::ComprehensionExpr) {
    const auto& comp = static_cast<const ComprehensionExpr&>(expr);
    collectExprUses(comp.element(), names, printed);
    collectExprUses(comp.iterable(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::TupleExpr) {
    for (const std::unique_ptr<Expr>& item : static_cast<const TupleExpr&>(expr).elements()) {
      collectExprUses(*item, names, printed);
    }
    return;
  }
  if (expr.kind() == NodeKind::WalrusExpr) {
    collectExprUses(static_cast<const WalrusExpr&>(expr).value(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::LambdaExpr) {
    collectExprUses(static_cast<const LambdaExpr&>(expr).body(), names, printed);
    return;
  }
  if (expr.kind() == NodeKind::NameExpr) {
    const auto& name = static_cast<const NameExpr&>(expr);
    const Type* type = name.resolvedType();
    if (type != nullptr && (type->kind() == TypeKind::Function || type->isCallableConstraint())) {
      names.push_back(name.name());
    }
  }
}

void collectStmtUses(const Stmt& stmt,
                     std::vector<std::string>& names,
                     std::vector<const Type*>& printed) {
  switch (stmt.kind()) {
  case NodeKind::VarDecl:
    if (static_cast<const VarDecl&>(stmt).init() != nullptr) {
      collectExprUses(*static_cast<const VarDecl&>(stmt).init(), names, printed);
    }
    break;
  case NodeKind::AssignStmt:
    collectExprUses(static_cast<const AssignStmt&>(stmt).target(), names, printed);
    collectExprUses(static_cast<const AssignStmt&>(stmt).value(), names, printed);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      collectExprUses(*static_cast<const ReturnStmt&>(stmt).value(), names, printed);
    }
    break;
  case NodeKind::ExprStmt:
    collectExprUses(static_cast<const ExprStmt&>(stmt).expression(), names, printed);
    break;
  case NodeKind::AssertStmt:
    collectExprUses(static_cast<const AssertStmt&>(stmt).condition(), names, printed);
    if (static_cast<const AssertStmt&>(stmt).message() != nullptr) {
      collectExprUses(*static_cast<const AssertStmt&>(stmt).message(), names, printed);
    }
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        collectExprUses(*branch.condition, names, printed);
      }
      for (const std::unique_ptr<Stmt>& bodyStmt : branch.body) {
        collectStmtUses(*bodyStmt, names, printed);
      }
    }
    break;
  case NodeKind::WhileStmt:
    collectExprUses(static_cast<const WhileStmt&>(stmt).condition(), names, printed);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const WhileStmt&>(stmt).body()) {
      collectStmtUses(*bodyStmt, names, printed);
    }
    break;
  case NodeKind::ForStmt:
    collectExprUses(static_cast<const ForStmt&>(stmt).iterable(), names, printed);
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const ForStmt&>(stmt).body()) {
      collectStmtUses(*bodyStmt, names, printed);
    }
    break;
  case NodeKind::FunctionDef:
    for (const std::unique_ptr<Stmt>& bodyStmt : static_cast<const FunctionDef&>(stmt).body()) {
      collectStmtUses(*bodyStmt, names, printed);
    }
    break;
  case NodeKind::ClassDef:
    for (const std::unique_ptr<FunctionDef>& method :
         static_cast<const ClassDef&>(stmt).methods()) {
      collectStmtUses(*method, names, printed);
    }
    break;
  case NodeKind::EnumDef:
    for (const std::unique_ptr<FunctionDef>& method : static_cast<const EnumDef&>(stmt).methods()) {
      collectStmtUses(*method, names, printed);
    }
    break;
  case NodeKind::TryStmt: {
    const auto& tryStmt = static_cast<const TryStmt&>(stmt);
    for (const std::unique_ptr<Stmt>& bodyStmt : tryStmt.body()) {
      collectStmtUses(*bodyStmt, names, printed);
    }
    for (const ExceptHandler& handler : tryStmt.handlers()) {
      for (const std::unique_ptr<Stmt>& bodyStmt : handler.body) {
        collectStmtUses(*bodyStmt, names, printed);
      }
    }
    break;
  }
  case NodeKind::MatchStmt:
    collectExprUses(static_cast<const MatchStmt&>(stmt).subject(), names, printed);
    for (const MatchArm& arm : static_cast<const MatchStmt&>(stmt).arms()) {
      for (const std::unique_ptr<Stmt>& bodyStmt : arm.body) {
        collectStmtUses(*bodyStmt, names, printed);
      }
    }
    break;
  default:
    break;
  }
}

} // namespace

void IRGenerator::collectClassTypes(const Module& ast) {
  for (const std::unique_ptr<Stmt>& statement : ast.statements()) {
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    const Type* record = static_cast<const ClassDef&>(*statement).resolvedType();
    if (record != nullptr && record->isRecord() && !record->isEnum() && !record->isStruct()) {
      classTypes_.push_back(record);
    }
  }
}

void IRGenerator::collectReachable(const std::vector<const Module*>& modules) {
  reachable_.clear();
  std::unordered_map<std::string, const FunctionDef*> defs;
  for (const Module* module : modules) {
    if (module == nullptr) {
      continue;
    }
    for (const std::unique_ptr<Stmt>& statement : module->statements()) {
      if (statement->kind() == NodeKind::FunctionDef) {
        const auto& function = static_cast<const FunctionDef&>(*statement);
        const std::string llvmName = llvmNameFor(function);
        defs[llvmName] = &function;
        defs[function.name()] = &function;
      } else if (statement->kind() == NodeKind::ClassDef) {
        const auto& classDef = static_cast<const ClassDef&>(*statement);
        for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
          defs[llvmNameFor(*method)] = method.get();
        }
      } else if (statement->kind() == NodeKind::EnumDef) {
        const auto& enumDef = static_cast<const EnumDef&>(*statement);
        for (const std::unique_ptr<FunctionDef>& method : enumDef.methods()) {
          defs[llvmNameFor(*method)] = method.get();
        }
      }
    }
  }
  std::queue<std::string> pending;
  auto mark = [&](const std::string& name) {
    if (name.empty() || !reachable_.insert(name).second) {
      return;
    }
    pending.push(name);
  };
  mark("main");
  mark("sere_main");
  for (const auto& entry : defs) {
    if (entry.second->isMethod() || entry.second->isExtern()) {
      mark(entry.first);
    }
  }
  for (const Module* module : modules) {
    if (module == nullptr) {
      continue;
    }
    std::vector<std::string> names;
    std::vector<const Type*> printed;
    for (const std::unique_ptr<Stmt>& statement : module->statements()) {
      if (statement->kind() == NodeKind::VarDecl || statement->kind() == NodeKind::ExprStmt) {
        collectStmtUses(*statement, names, printed);
      }
    }
    for (const std::string& name : names) {
      mark(name);
    }
    for (const Type* record : printed) {
      mark(record->name() + "___str__");
    }
  }
  if (!defs.contains("main") && !defs.contains("sere_main")) {
    reachable_.clear();
    return;
  }
  while (!pending.empty()) {
    const std::string name = pending.front();
    pending.pop();
    const auto found = defs.find(name);
    if (found == defs.end()) {
      continue;
    }
    std::vector<std::string> names;
    std::vector<const Type*> printed;
    collectStmtUses(*found->second, names, printed);
    for (const std::string& next : names) {
      mark(next);
    }
    for (const Type* record : printed) {
      mark(record->name() + "___str__");
    }
  }
}

// Task[T] values are represented at runtime as a pointer to a small heap cell
// that holds the produced value of type T. `async def` bodies run to
// completion (there is no I/O suspend primitive yet) and `await` simply
// unwraps the cell. This keeps async/await fully type-correct and executable
// today, and reserves the Task handle that real coroutine suspension can later
// drive.
llvm::Value*
IRGenerator::emitTaskBox(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* inner) {
  (void)inner;
  llvm::Function* alloc = runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
  std::uint64_t size = 1;
  if (value != nullptr && !value->getType()->isVoidTy() && value->getType()->isSized()) {
    size = module_->getDataLayout().getTypeAllocSize(value->getType()).getFixedValue();
  }
  llvm::Value* memory = builder.CreateCall(alloc, {builder.getInt64(size)});
  if (value != nullptr && !value->getType()->isVoidTy()) {
    builder.CreateStore(value, memory);
  }
  return memory;
}

llvm::Value*
IRGenerator::emitTaskUnbox(llvm::IRBuilder<>& builder, llvm::Value* task, const Type* inner) {
  if (task == nullptr) {
    return nullptr;
  }
  const Type* target = inner == nullptr ? nullptr : inner->canonical();
  if (target == nullptr || target->isVoidLike()) {
    return nullptr;
  }
  llvm::Type* ty = lower(target);
  if (ty == nullptr || ty->isVoidTy()) {
    return nullptr;
  }
  return builder.CreateLoad(ty, task);
}

llvm::Value* IRGenerator::emitAwait(llvm::IRBuilder<>& builder, const AwaitExpr& expr) {
  const Type* inner = nullptr;
  const Type* operandRaw = expr.operand().resolvedType();
  const Type* operandType = operandRaw == nullptr ? nullptr : operandRaw->canonical();
  const Type* probe = operandRaw != nullptr ? operandRaw : operandType;
  if (probe != nullptr && probe->isGenericCtor("Task") && probe->args().size() == 1) {
    inner = probe->args()[0];
  }
  if (inner == nullptr && operandType != nullptr && operandType->isGenericCtor("Task") &&
      operandType->args().size() == 1) {
    inner = operandType->args()[0];
  }
  if (inner == nullptr) {
    const Type* resultType =
        resolveType(expr.resolvedType());
    if (resultType != nullptr && !resultType->isGenericCtor("Task")) {
      inner = resultType;
    }
  }
  llvm::Value* child = emitExpr(builder, expr.operand());
  if (child == nullptr) {
    return nullptr;
  }
  llvm::Function* fn = builder.GetInsertBlock()->getParent();
  // The awaited child is a coroutine handle produced by an async call; its
  // ramp already ran it to its first suspend point or to completion. Drive it
  // to completion with @llvm.coro.resume, then read the typed result from the
  // promise and destroy the frame.
  llvm::BasicBlock* poll = llvm::BasicBlock::Create(*context_, "await.poll", fn);
  llvm::BasicBlock* ready = llvm::BasicBlock::Create(*context_, "await.ready", fn);
  llvm::Value* done0 = builder.CreateCall(asyncCoroDoneFn_, {child});
  builder.CreateCondBr(done0, ready, poll);
  builder.SetInsertPoint(poll);
  builder.CreateCall(asyncCoroResumeFn_, {child});
  llvm::Value* done1 = builder.CreateCall(asyncCoroDoneFn_, {child});
  builder.CreateCondBr(done1, ready, poll);
  builder.SetInsertPoint(ready);
  llvm::Value* result = nullptr;
  if (inner != nullptr && !inner->isVoidLike()) {
    llvm::Type* ty = lower(inner->canonical());
    if (ty != nullptr && !ty->isVoidTy()) {
      const unsigned align =
          static_cast<unsigned>(module_->getDataLayout().getABITypeAlign(ty).value());
      llvm::Value* addr =
          builder.CreateCall(asyncCoroPromiseFn_,
                             {child, builder.getInt32(static_cast<int>(align)), builder.getFalse()},
                             "await.result");
      result = builder.CreateLoad(ty, addr);
    }
  }
  builder.CreateCall(asyncCoroDestroyFn_, {child});
  return result;
}

bool IRGenerator::shouldEmit(const FunctionDef& function) const {
  // Functions are first-class values and may be reached through decorators or
  // closures without appearing as the lowered target of a direct call.
  (void)function;
  return true;
}

void IRGenerator::declareFunctions(const Module& ast) {
  for (const std::unique_ptr<Stmt>& statement : ast.statements()) {
    if (statement->kind() == NodeKind::FunctionDef) {
      const auto& function = static_cast<const FunctionDef&>(*statement);
      if (!function.typeParams().empty()) {
        continue;
      }
      functionDefs_[llvmNameFor(function)] = &function;
      functionDefs_[function.name()] = &function;
      if (!shouldEmit(function)) {
        continue;
      }
      const std::string llvmName = llvmNameFor(function);
      llvm::Function* fn =
          functions_.contains(llvmName) ? functions_[llvmName] : module_->getFunction(llvmName);
      if (fn == nullptr) {
        fn = llvm::Function::Create(
            llvmFunctionType(function), llvm::Function::ExternalLinkage, llvmName, module_);
      }
      functions_[llvmName] = fn;
      functions_[function.name()] = fn;
      if (function.isExtern()) {
        externFunctions_[function.name()] = true;
        externFunctions_[llvmName] = true;
      }
      if (function.name() == "main" && !function.isExtern()) {
        userMain_ = &function;
      }
      declareNestedFunctions(function);
      continue;
    }
    if (statement->kind() == NodeKind::EnumDef) {
      const auto& enumDef = static_cast<const EnumDef&>(*statement);
      for (const std::unique_ptr<FunctionDef>& method : enumDef.methods()) {
        functionDefs_[llvmNameFor(*method)] = method.get();
        if (!method->typeParams().empty() || !shouldEmit(*method)) {
          continue;
        }
        llvm::Function* fn = llvm::Function::Create(llvmFunctionType(*method),
                                                    llvm::Function::ExternalLinkage,
                                                    llvmNameFor(*method),
                                                    module_);
        functions_[llvmNameFor(*method)] = fn;
      }
      continue;
    }
    if (statement->kind() != NodeKind::ClassDef) {
      continue;
    }
    const auto& classDef = static_cast<const ClassDef&>(*statement);
    if (!classDef.typeParams().empty()) {
      continue;
    }
    for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
      functionDefs_[llvmNameFor(*method)] = method.get();
      if (!method->typeParams().empty() || !shouldEmit(*method)) {
        continue;
      }
      llvm::Function* fn = llvm::Function::Create(llvmFunctionType(*method),
                                                  llvm::Function::ExternalLinkage,
                                                  llvmNameFor(*method),
                                                  module_);
      functions_[llvmNameFor(*method)] = fn;
    }
  }
}

void IRGenerator::declareNestedFunctions(const FunctionDef& function) {
  for (const std::unique_ptr<Stmt>& statement : function.body()) {
    if (statement == nullptr || statement->kind() != NodeKind::FunctionDef) {
      continue;
    }
    const auto& nested = static_cast<const FunctionDef&>(*statement);
    const std::string name = llvmNameFor(nested);
    functionDefs_[name] = &nested;
    if (module_->getFunction(name) == nullptr) {
      functions_[name] = llvm::Function::Create(
          llvmFunctionType(nested), llvm::Function::InternalLinkage, name, module_);
    } else {
      functions_[name] = module_->getFunction(name);
    }
    declareNestedFunctions(nested);
  }
}

bool IRGenerator::emitCMainWrapper(llvm::Function* userMain) {
  if (userMain == nullptr) {
    return false;
  }
  llvm::Type* i32 = llvm::Type::getInt32Ty(*context_);
  llvm::Type* ptr = llvm::PointerType::getUnqual(*context_);
  llvm::FunctionType* cMainType = llvm::FunctionType::get(i32, {i32, ptr}, false);
  llvm::Function* cMain =
      llvm::Function::Create(cMainType, llvm::Function::ExternalLinkage, "main", module_);
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", cMain);
  llvm::IRBuilder<> builder(entry);
  llvm::Function* nativeInit = runtimeDecl("sere_mod_init", llvm::Type::getVoidTy(*context_), {});
  if (nativeInit->empty()) {
    nativeInit->setLinkage(llvm::GlobalValue::WeakAnyLinkage);
    llvm::BasicBlock* initEntry = llvm::BasicBlock::Create(*context_, "entry", nativeInit);
    llvm::IRBuilder<> initBuilder(initEntry);
    initBuilder.CreateRetVoid();
  }
  builder.CreateCall(nativeInit);
  if (moduleInitFn_ != nullptr) {
    builder.CreateCall(moduleInitFn_);
  }
  const auto mainDef = functionDefs_.find("sere_main");
  const FunctionDef* asyncDef =
      mainDef == functionDefs_.end() ? nullptr : mainDef->second;
  const bool asyncMain = asyncDef != nullptr && asyncDef->isAsync();
  if (!asyncMain) {
    llvm::Value* result = nullptr;
    if (userMain->arg_size() == 1) {
      llvm::Function* fromArgv = runtimeDecl("sere_list_from_argv", ptr, {i32, ptr});
      llvm::Value* list = builder.CreateCall(fromArgv, {cMain->getArg(0), cMain->getArg(1)});
      if (userMain->getReturnType()->isVoidTy()) {
        builder.CreateCall(userMain, {list});
      } else {
        result = builder.CreateCall(userMain, {list});
      }
    } else if (userMain->getReturnType()->isVoidTy()) {
      builder.CreateCall(userMain);
    } else {
      result = builder.CreateCall(userMain);
    }
    if (result != nullptr && (result->getType()->isVoidTy() || result->getType() != i32)) {
      result = result->getType()->isIntegerTy() ? builder.CreateIntCast(result, i32, true)
                                                : builder.getInt32(0);
    }
    builder.CreateRet(result == nullptr || result->getType()->isVoidTy() ? builder.getInt32(0)
                                                                         : result);
    return true;
  }

  // Async main: spawn the root coroutine, then drive it with @llvm.coro.resume
  // until @llvm.coro.done, read its typed result from the promise, destroy the
  // frame, and return the exit code.
  llvm::Value* hdl = nullptr;
  if (userMain->arg_size() == 1) {
    llvm::Function* fromArgv = runtimeDecl("sere_list_from_argv", ptr, {i32, ptr});
    llvm::Value* list = builder.CreateCall(fromArgv, {cMain->getArg(0), cMain->getArg(1)});
    hdl = builder.CreateCall(userMain, {list}, "root.coro");
  } else {
    hdl = builder.CreateCall(userMain, {}, "root.coro");
  }
  llvm::Function* coroResume =
      llvm::Intrinsic::getOrInsertDeclaration(module_, llvm::Intrinsic::coro_resume);
  llvm::Function* coroDone =
      llvm::Intrinsic::getOrInsertDeclaration(module_, llvm::Intrinsic::coro_done);
  llvm::Function* coroPromise =
      llvm::Intrinsic::getOrInsertDeclaration(module_, llvm::Intrinsic::coro_promise);
  llvm::Function* coroDestroy =
      llvm::Intrinsic::getOrInsertDeclaration(module_, llvm::Intrinsic::coro_destroy);

  llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(*context_, "drive.loop", cMain);
  llvm::BasicBlock* doneBB = llvm::BasicBlock::Create(*context_, "drive.done", cMain);
  builder.CreateBr(loopBB);
  builder.SetInsertPoint(loopBB);
  builder.CreateCall(coroResume, {hdl});
  llvm::Value* isDone = builder.CreateCall(coroDone, {hdl});
  builder.CreateCondBr(isDone, doneBB, loopBB);
  builder.SetInsertPoint(doneBB);
  llvm::Value* code = builder.getInt32(0);
  const Type* retType = asyncDef->resolvedType() == nullptr
                            ? nullptr
                            : asyncDef->resolvedType()->returnType();
  if (retType != nullptr && !retType->isVoidLike()) {
    llvm::Type* retLL = lower(retType);
    if (retLL != nullptr && !retLL->isVoidTy() && retLL->isIntegerTy()) {
      const unsigned align =
          static_cast<unsigned>(module_->getDataLayout().getABITypeAlign(retLL).value());
      llvm::Value* addr = builder.CreateCall(
          coroPromise, {hdl, builder.getInt32(static_cast<int>(align)), builder.getFalse()},
          "root.result");
      llvm::Value* value = builder.CreateLoad(retLL, addr);
      code = builder.CreateIntCast(value, i32, true);
    }
  }
  builder.CreateCall(coroDestroy, {hdl});
  builder.CreateRet(code);
  return true;
}

void IRGenerator::declareInstantiations() {
  for (const FunctionInstantiation& inst : types_->functionInstantiations()) {
    if (inst.specializedType == nullptr || functions_.contains(inst.llvmName)) {
      continue;
    }
    std::vector<llvm::Type*> params;
    for (std::size_t index = 0; index < inst.specializedType->paramTypes().size(); ++index) {
      const Type* param = inst.specializedType->paramTypes()[index];
      if (inst.isMethod && index == 0) {
        params.push_back(llvm::PointerType::getUnqual(*context_));
      } else {
        params.push_back(lower(param));
      }
    }
    llvm::FunctionType* type =
        llvm::FunctionType::get(lower(inst.specializedType->returnType()), params, false);
    llvm::Function* fn =
        llvm::Function::Create(type, llvm::Function::ExternalLinkage, inst.llvmName, module_);
    functions_[inst.llvmName] = fn;
  }
  for (const auto& entry : types_->instantiations()) {
    const Type* instance = entry.second;
    for (const RecordMethod& method : instance->methods()) {
      if (method.type == nullptr || functions_.contains(method.llvmName)) {
        continue;
      }
      std::vector<llvm::Type*> params;
      const std::vector<const Type*>& sereParams = method.type->paramTypes();
      for (std::size_t index = 0; index < sereParams.size(); ++index) {
        if (index == 0) {
          params.push_back(llvm::PointerType::getUnqual(*context_));
        } else {
          params.push_back(lower(sereParams[index]));
        }
      }
      llvm::FunctionType* type =
          llvm::FunctionType::get(lower(method.type->returnType()), params, false);
      llvm::Function* fn =
          llvm::Function::Create(type, llvm::Function::ExternalLinkage, method.llvmName, module_);
      functions_[method.llvmName] = fn;
    }
  }
}

bool IRGenerator::emitInstantiations(const std::vector<const Module*>& modules) {
  for (const FunctionInstantiation& inst : types_->functionInstantiations()) {
    subst_.clear();
    for (std::size_t index = 0; index < inst.typeParams.size() && index < inst.args.size();
         ++index) {
      subst_[inst.typeParams[index]] = inst.args[index];
    }
    const FunctionDef* source = nullptr;
    for (const Module* module : modules) {
      if (module == nullptr) {
        continue;
      }
      for (const std::unique_ptr<Stmt>& statement : module->statements()) {
        if (statement->kind() == NodeKind::FunctionDef &&
            static_cast<const FunctionDef&>(*statement).name() == inst.sourceName) {
          source = static_cast<const FunctionDef*>(statement.get());
          break;
        }
        if (statement->kind() == NodeKind::ClassDef) {
          const auto& classDef = static_cast<const ClassDef&>(*statement);
          for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
            const std::string unqualified = classDef.name() + "_" + method->name();
            const std::string qualified =
                (method->modulePrefix().empty() ? "" : method->modulePrefix() + "_") + unqualified;
            if (method->name() == inst.sourceName || unqualified == inst.sourceName ||
                qualified == inst.sourceName) {
              source = method.get();
              break;
            }
          }
          if (source != nullptr) {
            break;
          }
        }
      }
      if (source != nullptr) {
        break;
      }
    }
    if (source == nullptr || !emitFunction(*source, inst.llvmName)) {
      subst_.clear();
      return false;
    }
    subst_.clear();
  }
  for (const auto& entry : types_->instantiations()) {
    const Type* generic = entry.first;
    const Type* instance = entry.second;
    subst_.clear();
    for (std::size_t index = 0;
         index < generic->typeParams().size() && index < instance->args().size();
         ++index) {
      subst_[generic->typeParams()[index]] = instance->args()[index];
    }
    subst_[generic->name()] = instance;
    const ClassDef* source = nullptr;
    for (const Module* module : modules) {
      if (module == nullptr) {
        continue;
      }
      for (const std::unique_ptr<Stmt>& statement : module->statements()) {
        if (statement->kind() == NodeKind::ClassDef &&
            static_cast<const ClassDef&>(*statement).name() == generic->name()) {
          source = static_cast<const ClassDef*>(statement.get());
        }
      }
    }
    if (source == nullptr) {
      subst_.clear();
      continue;
    }
    for (const std::unique_ptr<FunctionDef>& method : source->methods()) {
      const int index = instance->methodIndex(method->name());
      if (index < 0) {
        continue;
      }
      if (!emitFunction(*method, instance->methods()[static_cast<std::size_t>(index)].llvmName)) {
        subst_.clear();
        return false;
      }
    }
    subst_.clear();
  }
  return true;
}

// ---------------------------------------------------------------------------
// Async coroutine lowering (LLVM switched-resume coroutines).
//
// An `async def f() -> T` becomes a coroutine whose ramp returns the coroutine
// handle (the Task[T] value). The declared result `T` is stored in a typed
// promise `alloca` (passed to @llvm.coro.id) before the final suspend and read
// back by the awaiter through @llvm.coro.promise. The CFG follows LLVM's
// Coroutines documentation: suspend switches route resume(0)/destroy(1)/
// suspend(default); cleanup frees the GC-allocated frame; suspend ends with
// @llvm.coro.end and returns the handle.
// ---------------------------------------------------------------------------

static llvm::Function* sereCoroIntrinsic(llvm::Module* module,
                                         unsigned id,
                                         llvm::ArrayRef<llvm::Type*> tys = {}) {
  return llvm::Intrinsic::getOrInsertDeclaration(
      module, static_cast<llvm::Intrinsic::ID>(id), tys);
}

bool IRGenerator::setupAsyncCoroutine(llvm::IRBuilder<>& builder,
                                      llvm::Function* llvmFn,
                                      const Type* returnType) {
  llvm::PointerType* ptrTy = builder.getPtrTy();
  asyncCoroIdFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_id);
  asyncCoroSizeFn_ =
      sereCoroIntrinsic(module_, llvm::Intrinsic::coro_size, {builder.getInt64Ty()});
  asyncCoroBeginFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_begin);
  asyncCoroSuspendFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_suspend);
  asyncCoroFreeFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_free);
  asyncCoroEndFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_end);
  asyncCoroPromiseFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_promise);
  asyncCoroDoneFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_done);
  asyncCoroResumeFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_resume);
  asyncCoroDestroyFn_ = sereCoroIntrinsic(module_, llvm::Intrinsic::coro_destroy);

  asyncResultTy_ = nullptr;
  asyncPromise_ = nullptr;
  if (returnType != nullptr && !returnType->isVoidLike()) {
    asyncResultTy_ = lower(returnType);
    if (asyncResultTy_ != nullptr && !asyncResultTy_->isVoidTy()) {
      asyncPromise_ = builder.CreateAlloca(asyncResultTy_, nullptr, "promise");
    }
  }

  llvm::Value* id = builder.CreateCall(asyncCoroIdFn_,
                                       {builder.getInt32(0),
                                        asyncPromise_ == nullptr
                                            ? llvm::ConstantPointerNull::get(ptrTy)
                                            : asyncPromise_,
                                        llvm::ConstantPointerNull::get(ptrTy),
                                        llvm::ConstantPointerNull::get(ptrTy)},
                                       "coro.id");
  llvm::Value* size = builder.CreateCall(asyncCoroSizeFn_, {}, "coro.size");
  llvm::Function* alloc = runtimeDecl("sere_alloc", ptrTy, {builder.getInt64Ty()});
  llvm::Value* mem = builder.CreateCall(alloc, {size}, "coro.alloc");
  llvm::Function* rootFn = runtimeDecl("sere_gc_add_root", builder.getVoidTy(), {ptrTy});
  builder.CreateCall(rootFn, {mem});
  llvm::Value* hdl = builder.CreateCall(asyncCoroBeginFn_, {id, mem}, "coro.hdl");
  asyncId_ = id;
  asyncMem_ = mem;
  asyncHdl_ = hdl;

  asyncFinal_ = llvm::BasicBlock::Create(*context_, "coro.final", llvmFn);
  asyncCleanup_ = llvm::BasicBlock::Create(*context_, "coro.cleanup", llvmFn);
  asyncSuspend_ = llvm::BasicBlock::Create(*context_, "coro.suspend", llvmFn);

  // Every async coroutine begins with an initial non-final suspend so that the
  // coroutine is a genuine switched-resume coroutine from CoroSplit's point of
  // view (a coroutine with only a final suspend is never split by LLVM). The
  // ramp therefore returns a suspended (lazy) task handle; the executor or an
  // `await` resumes it through `coro.resume`.
  llvm::BasicBlock* coroInit = llvm::BasicBlock::Create(*context_, "coro.init", llvmFn);
  llvm::BasicBlock* coroBody = llvm::BasicBlock::Create(*context_, "coro.body", llvmFn);
  builder.CreateBr(coroInit);
  builder.SetInsertPoint(coroInit);
  llvm::Value* initSuspend = builder.CreateCall(
      asyncCoroSuspendFn_,
      {llvm::ConstantTokenNone::get(*context_), builder.getFalse()},
      "coro.init.suspend");
  llvm::SwitchInst* initSwitch = builder.CreateSwitch(initSuspend, asyncSuspend_, 2);
  initSwitch->addCase(builder.getInt8(0), coroBody);
  initSwitch->addCase(builder.getInt8(1), asyncCleanup_);
  builder.SetInsertPoint(coroBody);

  llvmFn->addFnAttr("presplitcoroutine");
  return true;
}

void IRGenerator::buildAsyncTail(llvm::IRBuilder<>& builder, llvm::Function* llvmFn) {
  llvm::Type* ptrTy = builder.getPtrTy();
  (void)llvmFn;
  llvm::Value* noneToken = llvm::ConstantTokenNone::get(*context_);

  // coro.cleanup: free the GC frame and release its root.
  {
    llvm::IRBuilder<> cb(asyncCleanup_);
    llvm::Value* freeMem = cb.CreateCall(asyncCoroFreeFn_, {asyncId_, asyncHdl_}, "coro.free");
    llvm::Function* unrootFn = runtimeDecl("sere_gc_remove_root", cb.getVoidTy(), {ptrTy});
    cb.CreateCall(unrootFn, {freeMem});
    llvm::Function* freeFn = runtimeDecl("sere_free", cb.getVoidTy(), {ptrTy});
    cb.CreateCall(freeFn, {freeMem});
    cb.CreateBr(asyncSuspend_);
  }

  // coro.suspend: mark the end of coroutine access and return the handle.
  {
    llvm::IRBuilder<> sb(asyncSuspend_);
    sb.CreateCall(asyncCoroEndFn_, {asyncHdl_, sb.getFalse(), noneToken});
    sb.CreateRet(asyncHdl_);
  }

  // coro.final: final suspend; resuming a completed coroutine is UB (trap).
  {
    llvm::IRBuilder<> fb(asyncFinal_);
    llvm::Value* suspended = fb.CreateCall(asyncCoroSuspendFn_, {noneToken, fb.getTrue()},
                                           "coro.final.suspend");
    llvm::BasicBlock* trap =
        llvm::BasicBlock::Create(*context_, "coro.trap", asyncFinal_->getParent());
    llvm::SwitchInst* sw = fb.CreateSwitch(suspended, asyncSuspend_, 2);
    sw->addCase(fb.getInt8(0), trap);
    sw->addCase(fb.getInt8(1), asyncCleanup_);
    llvm::IRBuilder<> tb(trap);
    llvm::Function* trapFn =
        llvm::Intrinsic::getOrInsertDeclaration(module_, llvm::Intrinsic::trap);
    tb.CreateCall(trapFn, {});
    tb.CreateUnreachable();
  }
}

bool IRGenerator::emitFunction(const FunctionDef& function, const std::string& overrideName) {
  if (function.isExtern()) {
    return true;
  }
  const std::string name = overrideName.empty() ? llvmNameFor(function) : overrideName;
  llvm::Function* llvmFn = module_->getFunction(name);
  if (llvmFn == nullptr || !llvmFn->empty()) {
    return true;
  }
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", llvmFn);
  llvm::IRBuilder<> builder(entry);
  currentFunction_ = &function;
  locals_.clear();
  dropStack_.clear();
  loops_.clear();
  defers_.clear();
  withStack_.clear();
  asyncFn_ = function.isAsync();
  if (moduleInitFn_ != nullptr && function.name() == "main" && function.params().size() != 1) {
    builder.CreateCall(moduleInitFn_);
  }
  const Type* fnType = function.resolvedType();
  if (fnType == nullptr) {
    diagnostics_->error(function.range(), "internal: missing type for '" + function.name() + "'");
    currentFunction_ = nullptr;
    return false;
  }
  if (!subst_.empty()) {
    fnType = types_->substitute(fnType, subst_);
  }
  std::size_t index = 0;
  auto argIt = llvmFn->arg_begin();
  if (!function.captures().empty()) {
    llvm::Argument& environment = *argIt++;
    environment.setName("closure.env");
    std::vector<llvm::Type*> fields;
    for (const FunctionDef::Capture& capture : function.captures()) {
      fields.push_back(lower(capture.type));
    }
    llvm::StructType* environmentType = llvm::StructType::get(*context_, fields);
    for (std::size_t captureIndex = 0; captureIndex < function.captures().size(); ++captureIndex) {
      const FunctionDef::Capture& capture = function.captures()[captureIndex];
      rememberLocal(capture.name,
                    builder.CreateStructGEP(
                        environmentType, &environment, static_cast<unsigned>(captureIndex)),
                    capture.type);
    }
  }
  for (; argIt != llvmFn->arg_end(); ++argIt) {
    llvm::Argument& arg = *argIt;
    if (index >= function.params().size() || index >= fnType->paramTypes().size()) {
      diagnostics_->error(function.range(),
                          "internal: argument mismatch in '" + function.name() + "'");
      currentFunction_ = nullptr;
      return false;
    }
    const ParamDecl& param = function.params()[index];
    const Type* paramType = fnType->paramTypes()[index];
    arg.setName(param.name);
    if (function.isMethod() && index == 0) {
      rememberLocal(param.name, &arg, paramType);
    } else {
      llvm::Value* slot = builder.CreateAlloca(arg.getType(), nullptr, param.name);
      builder.CreateStore(&arg, slot);
      rememberLocal(param.name, slot, paramType);
    }
    ++index;
  }
  const Type* returnType = fnType->returnType();
  if (asyncFn_) {
    if (!setupAsyncCoroutine(builder, llvmFn, returnType)) {
      currentFunction_ = nullptr;
      asyncFn_ = false;
      return false;
    }
  }
  for (const std::unique_ptr<Stmt>& statement : function.body()) {
    if (!emitStatement(builder, *statement, returnType)) {
      return false;
    }
    if (builder.GetInsertBlock()->getTerminator() != nullptr) {
      break;
    }
  }
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    emitReturn(builder, nullptr, returnType);
  }
  if (asyncFn_) {
    buildAsyncTail(builder, llvmFn);
  }
  currentFunction_ = nullptr;
  asyncFn_ = false;
  return true;
}

bool IRGenerator::emitNestedFunctions(const FunctionDef& function) {
  for (const std::unique_ptr<Stmt>& statement : function.body()) {
    if (statement == nullptr || statement->kind() != NodeKind::FunctionDef) {
      continue;
    }
    const auto& nested = static_cast<const FunctionDef&>(*statement);
    if (!emitFunction(nested) || !emitNestedFunctions(nested)) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<llvm::Module> IRGenerator::emit(const Module& ast,
                                                const std::string& moduleName,
                                                const std::vector<const Module*>* imported) {
  auto module = std::make_unique<llvm::Module>(moduleName, *context_);
  module->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
  module_ = module.get();
  std::vector<const Module*> allForReach;
  if (imported != nullptr) {
    allForReach = *imported;
  }
  allForReach.push_back(&ast);
  classTypes_.clear();
  if (imported != nullptr) {
    for (const Module* extra : *imported) {
      if (extra != nullptr) {
        collectClassTypes(*extra);
      }
    }
  }
  collectClassTypes(ast);
  collectReachable(allForReach);
  if (imported != nullptr) {
    for (const Module* extra : *imported) {
      if (extra != nullptr) {
        declareFunctions(*extra);
      }
    }
  }
  declareFunctions(ast);
  declareLambdas(ast);
  declareInstantiations();
  if (imported != nullptr) {
    for (const Module* extra : *imported) {
      if (extra != nullptr) {
        declareGlobals(*extra);
      }
    }
  }
  declareGlobals(ast);
  emitModuleInitFn(ast, imported);
  auto emitModuleFns = [this](const Module& source) -> bool {
    for (const std::unique_ptr<Stmt>& statement : source.statements()) {
      if (statement->kind() == NodeKind::FunctionDef) {
        const auto& function = static_cast<const FunctionDef&>(*statement);
        if (!function.typeParams().empty() || !shouldEmit(function)) {
          continue;
        }
        if (!emitFunction(function)) {
          return false;
        }
        if (!emitNestedFunctions(function)) {
          return false;
        }
      } else if (statement->kind() == NodeKind::ClassDef) {
        const auto& classDef = static_cast<const ClassDef&>(*statement);
        if (!classDef.typeParams().empty()) {
          continue;
        }
        for (const std::unique_ptr<FunctionDef>& method : classDef.methods()) {
          if (!method->typeParams().empty() || !shouldEmit(*method)) {
            continue;
          }
          if (!emitFunction(*method)) {
            return false;
          }
        }
      } else if (statement->kind() == NodeKind::EnumDef) {
        const auto& enumDef = static_cast<const EnumDef&>(*statement);
        for (const std::unique_ptr<FunctionDef>& method : enumDef.methods()) {
          if (!method->typeParams().empty() || !shouldEmit(*method)) {
            continue;
          }
          if (!emitFunction(*method)) {
            return false;
          }
        }
      }
    }
    return true;
  };
  if (imported != nullptr) {
    for (const Module* extra : *imported) {
      if (extra != nullptr && !emitModuleFns(*extra)) {
        return nullptr;
      }
    }
  }
  if (!emitModuleFns(ast)) {
    return nullptr;
  }
  std::vector<const LambdaExpr*> lambdas;
  for (const std::unique_ptr<Stmt>& statement : ast.statements()) {
    collectLambdas(*statement, lambdas);
  }
  for (const LambdaExpr* lambda : lambdas) {
    if (lambda != nullptr && !emitLambdaFunction(*lambda)) {
      return nullptr;
    }
  }
  std::vector<const Module*> allModules;
  if (imported != nullptr) {
    allModules = *imported;
  }
  allModules.push_back(&ast);
  if (!emitInstantiations(allModules)) {
    return nullptr;
  }
  if (userMain_ != nullptr) {
    emitCMainWrapper(module_->getFunction("sere_main"));
  }
  emitAnyRepr();
  std::string verifyError;
  llvm::raw_string_ostream errorStream(verifyError);
  if (llvm::verifyModule(*module, &errorStream)) {
    diagnostics_->error("LLVM IR verification failed: " + errorStream.str());
    return nullptr;
  }
  module_ = nullptr;
  return module;
}

llvm::Value* IRGenerator::packStr(llvm::IRBuilder<>& builder, llvm::Value* data, llvm::Value* len) {
  llvm::Value* str = llvm::UndefValue::get(lower(types_->strType()));
  str = builder.CreateInsertValue(str, data, {0});
  str = builder.CreateInsertValue(str, len, {1});
  return str;
}

llvm::Value* IRGenerator::emitEnumTag(llvm::IRBuilder<>& builder, llvm::Value* value) {
  if (value == nullptr) {
    return builder.getInt32(0);
  }
  if (value->getType()->isStructTy() &&
      llvm::cast<llvm::StructType>(value->getType())->getNumElements() > 0) {
    value = builder.CreateExtractValue(value, {0});
  }
  if (value->getType()->isPointerTy()) {
    return builder.CreatePtrToInt(value, builder.getInt32Ty());
  }
  if (value->getType() != builder.getInt32Ty()) {
    if (!value->getType()->isIntegerTy()) {
      return builder.getInt32(0);
    }
    return builder.CreateIntCast(value, builder.getInt32Ty(), false);
  }
  return value;
}

llvm::Value* IRGenerator::emitEnumUnit(llvm::IRBuilder<>& builder, const Type* type, unsigned tag) {
  llvm::Value* tagValue = builder.getInt32(tag);
  if (type == nullptr || !type->hasEnumPayload()) {
    return tagValue;
  }
  llvm::Value* agg = llvm::UndefValue::get(lower(type));
  agg = builder.CreateInsertValue(agg, tagValue, {0});
  agg = builder.CreateInsertValue(agg, llvm::ConstantPointerNull::get(builder.getPtrTy()), {1});
  return agg;
}

llvm::Value* IRGenerator::emitEnumName(llvm::IRBuilder<>& builder, const Expr& expr) {
  const Type* type = resolveType(expr.resolvedType());
  if (type == nullptr || !type->isEnum()) {
    return emitStrLiteral(builder, "?");
  }
  return emitEnumSwitchStr(builder, emitEnumTag(builder, emitExpr(builder, expr)), type, false);
}

llvm::Value* IRGenerator::emitTernary(llvm::IRBuilder<>& builder, const TernaryExpr& expr) {
  llvm::Value* cond = emitExpr(builder, expr.condition());
  if (cond == nullptr) {
    return nullptr;
  }
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*context_, "tern.then", function);
  llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(*context_, "tern.else", function);
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "tern.end", function);
  builder.CreateCondBr(cond, thenBlock, elseBlock);
  builder.SetInsertPoint(thenBlock);
  llvm::Value* thenValue = emitExpr(builder, expr.thenValue());
  llvm::BasicBlock* thenEnd = builder.GetInsertBlock();
  builder.CreateBr(merge);
  builder.SetInsertPoint(elseBlock);
  llvm::Value* elseValue = emitExpr(builder, expr.elseValue());
  llvm::BasicBlock* elseEnd = builder.GetInsertBlock();
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  if (thenValue == nullptr || elseValue == nullptr) {
    return nullptr;
  }
  llvm::PHINode* phi = builder.CreatePHI(thenValue->getType(), 2, "tern.phi");
  phi->addIncoming(thenValue, thenEnd);
  phi->addIncoming(elseValue, elseEnd);
  return phi;
}

llvm::Value* IRGenerator::emitTuple(llvm::IRBuilder<>& builder, const TupleExpr& expr) {
  llvm::Value* agg = llvm::UndefValue::get(lower(expr.resolvedType()));
  for (std::size_t index = 0; index < expr.elements().size(); ++index) {
    llvm::Value* item = emitExpr(builder, *expr.elements()[index]);
    if (item == nullptr) {
      return nullptr;
    }
    agg = builder.CreateInsertValue(agg, item, {static_cast<unsigned>(index)});
  }
  return agg;
}

llvm::Value* IRGenerator::emitNamesList(llvm::IRBuilder<>& builder,
                                        const std::vector<std::string>& names) {
  llvm::Function* newFn = runtimeDecl("sere_list_new", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Function* pushFn =
      runtimeDecl("sere_list_push", builder.getVoidTy(), {builder.getPtrTy(), builder.getPtrTy()});
  llvm::Value* list = builder.CreateCall(newFn, {builder.getInt64(16)});
  for (const std::string& name : names) {
    llvm::Value* str = emitStrLiteral(builder, name);
    builder.CreateCall(pushFn, {list, emitTempSlot(builder, str, types_->strType())});
  }
  return list;
}

llvm::Value* IRGenerator::emitDunderOnSelf(llvm::IRBuilder<>& builder,
                                           const Type* record,
                                           llvm::Value* self,
                                           std::string_view name,
                                           const std::vector<llvm::Value*>& extra) {
  if (record == nullptr || self == nullptr) {
    return nullptr;
  }
  record = resolveType(record);
  const int index = record->methodIndex(name);
  if (index < 0) {
    return nullptr;
  }
  const RecordMethod& method = record->methods()[static_cast<std::size_t>(index)];
  const auto found = functions_.find(method.llvmName);
  if (found == functions_.end()) {
    return nullptr;
  }
  std::vector<llvm::Value*> args;
  args.push_back(self);
  args.insert(args.end(), extra.begin(), extra.end());
  if (found->second->getReturnType()->isVoidTy()) {
    builder.CreateCall(found->second, args);
    return nullptr;
  }
  return builder.CreateCall(found->second, args);
}

llvm::Value* IRGenerator::emitDunderCall(llvm::IRBuilder<>& builder,
                                         const Expr& object,
                                         std::string_view name,
                                         const std::vector<llvm::Value*>& extra) {
  const Type* record =
      resolveType(object.resolvedType());
  if (record == nullptr) {
    return nullptr;
  }
  llvm::Value* thisPtr = emitAddress(builder, object, false);
  if (thisPtr == nullptr) {
    llvm::Value* value = emitExpr(builder, object);
    if (value == nullptr) {
      return nullptr;
    }
    thisPtr = builder.CreateAlloca(lower(record), nullptr, "dunder.tmp");
    builder.CreateStore(value, thisPtr);
  }
  return emitDunderOnSelf(builder, record, thisPtr, name, extra);
}

llvm::Value*
IRGenerator::emitObjectPointer(llvm::IRBuilder<>& builder, const Expr& object, const Type* record) {
  if (record == nullptr) {
    return nullptr;
  }
  record = resolveType(record);
  llvm::Value* thisPtr = emitAddress(builder, object, false);
  if (thisPtr != nullptr) {
    return thisPtr;
  }
  llvm::Value* value = emitExpr(builder, object);
  if (value == nullptr) {
    return nullptr;
  }
  thisPtr = builder.CreateAlloca(lower(record), nullptr, "prop.self");
  builder.CreateStore(value, thisPtr);
  return thisPtr;
}

llvm::Value* IRGenerator::emitNamedMethod(llvm::IRBuilder<>& builder,
                                          const std::string& llvmName,
                                          llvm::Value* self,
                                          const std::vector<llvm::Value*>& extra) {
  if (self == nullptr || llvmName.empty()) {
    return nullptr;
  }
  const auto found = functions_.find(llvmName);
  if (found == functions_.end()) {
    diagnostics_->error("no LLVM function for '" + llvmName + "'");
    return nullptr;
  }
  std::vector<llvm::Value*> args;
  args.push_back(self);
  args.insert(args.end(), extra.begin(), extra.end());
  if (found->second->getReturnType()->isVoidTy()) {
    builder.CreateCall(found->second, args);
    return nullptr;
  }
  return builder.CreateCall(found->second, args);
}

void IRGenerator::emitErrorCheck(llvm::IRBuilder<>& builder) {
  if (tryHandlers_.empty() || builder.GetInsertBlock()->getTerminator() != nullptr) {
    return;
  }
  llvm::Function* hasFn = runtimeDecl("sere_has_error", builder.getInt32Ty(), {});
  llvm::Value* has = builder.CreateICmpNE(builder.CreateCall(hasFn), builder.getInt32(0));
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* fail = llvm::BasicBlock::Create(*context_, "err", function);
  llvm::BasicBlock* ok = llvm::BasicBlock::Create(*context_, "err.ok", function);
  builder.CreateCondBr(has, fail, ok);
  builder.SetInsertPoint(fail);
  if (!tryHandlers_.empty()) {
    builder.CreateBr(tryHandlers_.back());
  } else {
    llvm::Function* panic =
        runtimeDecl("sere_panic", builder.getVoidTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    llvm::Value* msg = emitStrLiteral(builder, "uncaught error");
    builder.CreateCall(
        panic, {builder.CreateExtractValue(msg, {0}), builder.CreateExtractValue(msg, {1})});
    builder.CreateUnreachable();
  }
  builder.SetInsertPoint(ok);
}

bool IRGenerator::emitRaise(llvm::IRBuilder<>& builder, const RaiseStmt& statement) {
  std::string chain;
  llvm::Value* message = nullptr;
  if (statement.value() != nullptr) {
    const Expr& value = *statement.value();
    const Type* type =
        resolveType(value.resolvedType());
    if (const NameExpr* name = asName(value)) {
      appendExceptionName(chain, name->name());
      appendExceptionType(chain, type);
      if (type != nullptr && type->isRecord() && locals_.find(name->name()) == locals_.end() &&
          globals_.find(name->name()) == globals_.end()) {
        message = emitStrLiteral(builder, "");
      }
    } else if (value.kind() == NodeKind::CallExpr) {
      const auto& call = static_cast<const CallExpr&>(value);
      if (const NameExpr* callee = asName(call.callee())) {
        appendExceptionName(chain, callee->name());
      }
      appendExceptionType(chain, type);
      if (!call.arguments().empty()) {
        message = emitToStr(builder, *call.arguments()[0]);
      }
      (void)emitExpr(builder, value);
    } else if (type != nullptr && type->isNamed("str")) {
      appendExceptionName(chain, "Exception");
      message = emitToStr(builder, value);
    } else {
      appendExceptionType(chain, type);
      message = emitToStr(builder, value);
    }
  }
  if (chain.empty()) {
    chain = "Exception";
  }
  if (message == nullptr) {
    message = emitStrLiteral(builder, "");
  }
  llvm::Function* raiseFn =
      runtimeDecl("sere_raise",
                  builder.getVoidTy(),
                  {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()});
  builder.CreateCall(raiseFn,
                     {builder.CreateGlobalString(chain, "", 0, module_),
                      builder.CreateExtractValue(message, {0}),
                      builder.CreateExtractValue(message, {1})});
  emitErrorCheck(builder);
  return true;
}

bool IRGenerator::emitTry(llvm::IRBuilder<>& builder,
                          const TryStmt& statement,
                          const Type* returnType) {
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* dispatch = llvm::BasicBlock::Create(*context_, "try.dispatch", function);
  llvm::BasicBlock* after = llvm::BasicBlock::Create(*context_, "try.after", function);
  tryHandlers_.push_back(dispatch);
  tryDepth_ += 1;
  const bool bodyOk = emitBlock(builder, statement.body(), returnType);
  tryHandlers_.pop_back();
  tryDepth_ -= 1;
  if (!bodyOk) {
    return false;
  }
  llvm::Function* hasFn = runtimeDecl("sere_has_error", builder.getInt32Ty(), {});
  llvm::Function* clearFn = runtimeDecl("sere_clear_error", builder.getVoidTy(), {});
  llvm::Function* isaFn = runtimeDecl("sere_error_isa", builder.getInt32Ty(), {builder.getPtrTy()});
  llvm::Function* msgFn =
      runtimeDecl("sere_error_message", builder.getPtrTy(), {builder.getPtrTy()});
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    llvm::Value* failed = builder.CreateICmpNE(builder.CreateCall(hasFn), builder.getInt32(0));
    llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(*context_, "try.else", function);
    builder.CreateCondBr(failed, dispatch, elseBlock);
    builder.SetInsertPoint(elseBlock);
    if (!emitBlock(builder, statement.elseBody(), returnType)) {
      return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
      builder.CreateBr(after);
    }
  }
  builder.SetInsertPoint(dispatch);
  llvm::BasicBlock* unmatched = llvm::BasicBlock::Create(*context_, "try.unmatched", function);
  if (statement.handlers().empty()) {
    builder.CreateBr(unmatched);
  }
  llvm::BasicBlock* next = statement.handlers().empty() ? unmatched : nullptr;
  for (std::size_t index = 0; index < statement.handlers().size(); ++index) {
    const ExceptHandler& handler = statement.handlers()[index];
    llvm::BasicBlock* taken = llvm::BasicBlock::Create(*context_, "try.except", function);
    next = index + 1 == statement.handlers().size()
               ? unmatched
               : llvm::BasicBlock::Create(*context_, "try.next", function);
    if (handler.type == nullptr) {
      builder.CreateBr(taken);
    } else {
      llvm::Value* matched = builder.CreateICmpNE(
          builder.CreateCall(isaFn,
                             {builder.CreateGlobalString(handler.type->name(), "", 0, module_)}),
          builder.getInt32(0));
      builder.CreateCondBr(matched, taken, next);
    }
    builder.SetInsertPoint(taken);
    if (!handler.name.empty()) {
      const Type* caught = handler.type != nullptr && handler.type->resolvedType() != nullptr
                               ? handler.type->resolvedType()
                               : types_->record("Exception");
      if (caught != nullptr && caught->isRecord()) {
        llvm::Value* slot = createLocalSlot(builder, handler.name, caught);
        llvm::Value* object = emitDefault(caught);
        if (recordHasTypeId(caught)) {
          object = builder.CreateInsertValue(object, builder.getInt32(recordTypeId(caught)), {0});
        }
        llvm::Value* lenSlot = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "exc.len");
        llvm::Value* data = builder.CreateCall(msgFn, {lenSlot});
        llvm::Value* text =
            packStr(builder, data, builder.CreateLoad(builder.getInt64Ty(), lenSlot));
        const int field = caught->fieldIndex("message");
        if (field >= 0) {
          object = builder.CreateInsertValue(object, text, {llvmFieldIndex(caught, field)});
        }
        builder.CreateStore(object, slot);
      }
    }
    builder.CreateCall(clearFn);
    if (!emitBlock(builder, handler.body, returnType)) {
      return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
      builder.CreateBr(after);
    }
    builder.SetInsertPoint(next);
  }
  builder.SetInsertPoint(unmatched);
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    builder.CreateBr(after);
  }
  builder.SetInsertPoint(after);
  if (!emitBlock(builder, statement.finallyBody(), returnType)) {
    return false;
  }
  emitErrorCheck(builder);
  return true;
}

bool IRGenerator::emitMatch(llvm::IRBuilder<>& builder,
                            const MatchStmt& statement,
                            const Type* returnType) {
  llvm::Value* subjectValue = emitExpr(builder, statement.subject());
  if (subjectValue == nullptr) {
    return false;
  }
  const Type* subjectType = statement.subject().resolvedType();
  llvm::Value* subject = subjectValue;
  if (subjectType != nullptr && subjectType->isEnum()) {
    subject = emitEnumTag(builder, subjectValue);
  }
  llvm::Function* function = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* merge = llvm::BasicBlock::Create(*context_, "match.end", function);
  for (const MatchArm& arm : statement.arms()) {
    llvm::Value* matched = builder.getInt1(true);
    const Expr* pattern = arm.pattern.get();
    const bool wildcard = pattern != nullptr && pattern->kind() == NodeKind::NameExpr &&
                          static_cast<const NameExpr*>(pattern)->name() == "_";
    if (pattern != nullptr && !wildcard) {
      std::string fieldName;
      if (pattern->kind() == NodeKind::MemberExpr) {
        fieldName = static_cast<const MemberExpr*>(pattern)->field();
      } else if (pattern->kind() == NodeKind::CallExpr &&
                 static_cast<const CallExpr*>(pattern)->callee().kind() == NodeKind::MemberExpr) {
        fieldName =
            static_cast<const MemberExpr&>(static_cast<const CallExpr*>(pattern)->callee()).field();
      }
      if (!fieldName.empty() && subjectType != nullptr && subjectType->isEnum()) {
        const RecordField* field = subjectType->findField(fieldName);
        const unsigned expected = enumTagFromField(field);
        llvm::Value* want = builder.getInt32(expected);
        if (subject->getType() != want->getType() && subject->getType()->isIntegerTy()) {
          want = builder.CreateIntCast(want, subject->getType(), false);
        }
        matched = subject->getType() == want->getType() ? builder.CreateICmpEQ(subject, want)
                                                        : builder.getInt1(false);
      } else {
        llvm::Value* pat = emitExpr(builder, *pattern);
        if (pat == nullptr) {
          return false;
        }
        if (pattern->resolvedType() != nullptr && pattern->resolvedType()->isEnum()) {
          pat = emitEnumTag(builder, pat);
        }
        if (subject->getType() != pat->getType()) {
          if (subject->getType()->isIntegerTy() && pat->getType()->isIntegerTy()) {
            pat = builder.CreateIntCast(pat, subject->getType(), false);
          } else {
            matched = builder.getInt1(false);
          }
        }
        if (matched->getType()->isIntegerTy(1) && subject->getType() == pat->getType() &&
            subject->getType()->isIntegerTy()) {
          matched = builder.CreateICmpEQ(subject, pat);
        }
      }
    }
    if (arm.guard != nullptr) {
      llvm::Value* guard = emitExpr(builder, *arm.guard);
      if (guard == nullptr) {
        return false;
      }
      matched = builder.CreateAnd(matched, guard);
    }
    llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "match.arm", function);
    llvm::BasicBlock* next = llvm::BasicBlock::Create(*context_, "match.next", function);
    builder.CreateCondBr(matched, body, next);
    builder.SetInsertPoint(body);
    if (pattern != nullptr && pattern->kind() == NodeKind::CallExpr && subjectType != nullptr &&
        subjectType->hasEnumPayload()) {
      const auto& call = static_cast<const CallExpr&>(*pattern);
      if (call.callee().kind() == NodeKind::MemberExpr && subjectValue->getType()->isStructTy()) {
        const RecordField* field =
            subjectType->findField(static_cast<const MemberExpr&>(call.callee()).field());
        if (field != nullptr) {
          std::vector<llvm::Type*> payloadLlvm;
          for (const Type* payloadType : field->payloadTypes) {
            payloadLlvm.push_back(lower(payloadType));
          }
          llvm::StructType* payloadTy = llvm::StructType::get(*context_, payloadLlvm);
          llvm::Value* payload = builder.CreateExtractValue(subjectValue, {1});
          for (std::size_t index = 0;
               index < call.arguments().size() && index < field->payloadTypes.size();
               ++index) {
            if (call.arguments()[index]->kind() != NodeKind::NameExpr) {
              continue;
            }
            const std::string& bind = static_cast<const NameExpr&>(*call.arguments()[index]).name();
            if (bind.empty() || bind == "_") {
              continue;
            }
            llvm::Value* slot =
                builder.CreateAlloca(lower(field->payloadTypes[index]), nullptr, bind);
            llvm::Value* gep =
                builder.CreateStructGEP(payloadTy, payload, static_cast<unsigned>(index));
            builder.CreateStore(builder.CreateLoad(lower(field->payloadTypes[index]), gep), slot);
            rememberLocal(bind, slot, field->payloadTypes[index]);
          }
        }
      }
    }
    if (!emitBlock(builder, arm.body, returnType)) {
      return false;
    }
    if (builder.GetInsertBlock()->getTerminator() == nullptr) {
      builder.CreateBr(merge);
    }
    builder.SetInsertPoint(next);
  }
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    builder.CreateBr(merge);
  }
  builder.SetInsertPoint(merge);
  return true;
}

void IRGenerator::emitDeferred(llvm::IRBuilder<>& builder, const Type* returnType) {
  for (auto it = withStack_.rbegin(); it != withStack_.rend(); ++it) {
    (void)emitWithExit(builder, *it);
  }
  for (auto it = defers_.rbegin(); it != defers_.rend(); ++it) {
    (void)emitBlock(builder, (*it)->body(), returnType);
  }
}

llvm::Value* IRGenerator::emitWalrus(llvm::IRBuilder<>& builder, const WalrusExpr& expr) {
  llvm::Value* value = emitExpr(builder, expr.value());
  if (value == nullptr) {
    return nullptr;
  }
  if (!locals_.contains(expr.name()) && !globals_.contains(expr.name())) {
    createLocalSlot(builder, expr.name(), expr.resolvedType());
  }
  llvm::Value* slot = locals_.contains(expr.name()) ? locals_[expr.name()] : globals_[expr.name()];
  if (slot != nullptr && !value->getType()->isVoidTy()) {
    builder.CreateStore(value, slot);
  }
  return value;
}

llvm::FunctionType* IRGenerator::llvmFunctionTypeFrom(const Type* type) {
  if (type == nullptr) {
    return llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), false);
  }
  std::vector<llvm::Type*> params;
  for (const Type* param : type->paramTypes()) {
    params.push_back(lower(param));
  }
  llvm::Type* ret = type->returnType() == nullptr || type->returnType()->isVoidLike()
                        ? llvm::Type::getVoidTy(*context_)
                        : lower(type->returnType());
  return llvm::FunctionType::get(ret, params, false);
}

llvm::FunctionType* IRGenerator::llvmFunctionTypeFromCall(const CallExpr& expr) {
  std::vector<llvm::Type*> params;
  const Type* constraint = expr.callee().resolvedType();
  const Type* paramList = nullptr;
  if (constraint != nullptr && constraint->isCallableConstraint() && !constraint->args().empty() &&
      constraint->args()[0] != nullptr && constraint->args()[0]->isParamList()) {
    paramList = constraint->args()[0];
  }
  if (paramList != nullptr) {
    for (const Type* param : paramList->args()) {
      if (param != nullptr && param->isEllipsis()) {
        break;
      }
      params.push_back(lower(param));
    }
    for (std::size_t index = params.size(); index < expr.arguments().size(); ++index) {
      params.push_back(lower(expr.arguments()[index]->resolvedType()));
    }
  } else {
    for (const std::unique_ptr<Expr>& argument : expr.arguments()) {
      params.push_back(lower(argument->resolvedType()));
    }
  }
  const Type* returnType = expr.resolvedType();
  llvm::Type* ret = returnType == nullptr || returnType->isVoidLike()
                        ? llvm::Type::getVoidTy(*context_)
                        : lower(returnType);
  return llvm::FunctionType::get(ret, params, false);
}

llvm::Value* IRGenerator::emitConstructorThunk(const Type* record) {
  if (record == nullptr) {
    return nullptr;
  }
  record = resolveType(record);
  const std::string name = "__sere_callable_" + record->name();
  if (llvm::Function* existing = module_->getFunction(name)) {
    return existing;
  }
  std::vector<llvm::Type*> params;
  std::string initName;
  const Type* initType = nullptr;
  const int initIndex = record->methodIndex("__init__");
  if (initIndex >= 0) {
    const RecordMethod& init = record->methods()[static_cast<std::size_t>(initIndex)];
    initName = init.llvmName;
    initType = init.type;
    if (initType != nullptr) {
      for (std::size_t index = 1; index < initType->paramTypes().size(); ++index) {
        params.push_back(lower(initType->paramTypes()[index]));
      }
    }
  } else {
    for (const RecordField& field : record->fields()) {
      if (!field.isStatic && field.stored) {
        params.push_back(lower(field.type));
      }
    }
  }
  llvm::FunctionType* functionType = llvm::FunctionType::get(lower(record), params, false);
  llvm::Function* fn =
      llvm::Function::Create(functionType, llvm::Function::InternalLinkage, name, module_);
  functions_[name] = fn;
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
  llvm::IRBuilder<> builder(entry);
  llvm::Value* slot = builder.CreateAlloca(lower(record), nullptr, "init.tmp");
  builder.CreateStore(emitDefault(record), slot);
  if (recordHasTypeId(record)) {
    builder.CreateStore(builder.getInt32(recordTypeId(record)),
                        builder.CreateStructGEP(lower(record), slot, 0u));
  }
  if (!initName.empty()) {
    const auto found = functions_.find(initName);
    if (found == functions_.end()) {
      diagnostics_->error("no LLVM function for constructor '" + initName + "'");
      builder.CreateUnreachable();
      return fn;
    }
    std::vector<llvm::Value*> args;
    args.push_back(slot);
    for (llvm::Argument& arg : fn->args()) {
      args.push_back(&arg);
    }
    matchCallArgs(builder, found->second, args);
    builder.CreateCall(found->second, args);
  } else {
    llvm::Value* aggregate = builder.CreateLoad(lower(record), slot);
    unsigned fieldIndex = recordHasTypeId(record) ? 1u : 0u;
    for (llvm::Argument& arg : fn->args()) {
      aggregate = builder.CreateInsertValue(aggregate, &arg, {fieldIndex});
      ++fieldIndex;
    }
    builder.CreateStore(aggregate, slot);
  }
  builder.CreateRet(builder.CreateLoad(lower(record), slot));
  return fn;
}

void IRGenerator::collectLambdas(const Expr& expr, std::vector<const LambdaExpr*>& out) {
  if (expr.kind() == NodeKind::LambdaExpr) {
    out.push_back(static_cast<const LambdaExpr*>(&expr));
  }
  switch (expr.kind()) {
  case NodeKind::CallExpr: {
    const auto& call = static_cast<const CallExpr&>(expr);
    collectLambdas(call.callee(), out);
    for (const std::unique_ptr<Expr>& arg : call.arguments()) {
      collectLambdas(*arg, out);
    }
    break;
  }
  case NodeKind::MemberExpr:
    collectLambdas(static_cast<const MemberExpr&>(expr).object(), out);
    break;
  case NodeKind::BinaryExpr:
    collectLambdas(static_cast<const BinaryExpr&>(expr).left(), out);
    collectLambdas(static_cast<const BinaryExpr&>(expr).right(), out);
    break;
  case NodeKind::UnaryExpr:
    collectLambdas(static_cast<const UnaryExpr&>(expr).operand(), out);
    break;
  case NodeKind::CastExpr:
    collectLambdas(static_cast<const CastExpr&>(expr).value(), out);
    break;
  case NodeKind::IndexExpr: {
    const auto& index = static_cast<const IndexExpr&>(expr);
    collectLambdas(index.object(), out);
    if (index.start() != nullptr) {
      collectLambdas(*index.start(), out);
    }
    if (index.stop() != nullptr) {
      collectLambdas(*index.stop(), out);
    }
    break;
  }
  case NodeKind::ListLiteral:
    for (const std::unique_ptr<Expr>& item : static_cast<const ListLiteral&>(expr).elements()) {
      collectLambdas(*item, out);
    }
    break;
  case NodeKind::DictLiteral: {
    const auto& dict = static_cast<const DictLiteral&>(expr);
    for (std::size_t index = 0; index < dict.keys().size(); ++index) {
      collectLambdas(*dict.keys()[index], out);
      collectLambdas(*dict.values()[index], out);
    }
    break;
  }
  case NodeKind::TernaryExpr: {
    const auto& ternary = static_cast<const TernaryExpr&>(expr);
    collectLambdas(ternary.thenValue(), out);
    collectLambdas(ternary.condition(), out);
    collectLambdas(ternary.elseValue(), out);
    break;
  }
  case NodeKind::TupleExpr:
    for (const std::unique_ptr<Expr>& item : static_cast<const TupleExpr&>(expr).elements()) {
      collectLambdas(*item, out);
    }
    break;
  case NodeKind::WalrusExpr:
    collectLambdas(static_cast<const WalrusExpr&>(expr).value(), out);
    break;
  case NodeKind::LambdaExpr:
    collectLambdas(static_cast<const LambdaExpr&>(expr).body(), out);
    break;
  default:
    break;
  }
}

void IRGenerator::collectLambdas(const Stmt& stmt, std::vector<const LambdaExpr*>& out) {
  switch (stmt.kind()) {
  case NodeKind::ExprStmt:
    collectLambdas(static_cast<const ExprStmt&>(stmt).expression(), out);
    break;
  case NodeKind::VarDecl:
    if (static_cast<const VarDecl&>(stmt).init() != nullptr) {
      collectLambdas(*static_cast<const VarDecl&>(stmt).init(), out);
    }
    break;
  case NodeKind::AssignStmt:
    collectLambdas(static_cast<const AssignStmt&>(stmt).target(), out);
    collectLambdas(static_cast<const AssignStmt&>(stmt).value(), out);
    break;
  case NodeKind::ReturnStmt:
    if (static_cast<const ReturnStmt&>(stmt).value() != nullptr) {
      collectLambdas(*static_cast<const ReturnStmt&>(stmt).value(), out);
    }
    break;
  case NodeKind::IfStmt:
    for (const IfBranch& branch : static_cast<const IfStmt&>(stmt).branches()) {
      if (branch.condition != nullptr) {
        collectLambdas(*branch.condition, out);
      }
      for (const std::unique_ptr<Stmt>& item : branch.body) {
        collectLambdas(*item, out);
      }
    }
    break;
  case NodeKind::WhileStmt:
    collectLambdas(static_cast<const WhileStmt&>(stmt).condition(), out);
    for (const std::unique_ptr<Stmt>& item : static_cast<const WhileStmt&>(stmt).body()) {
      collectLambdas(*item, out);
    }
    break;
  case NodeKind::ForStmt:
    collectLambdas(static_cast<const ForStmt&>(stmt).iterable(), out);
    for (const std::unique_ptr<Stmt>& item : static_cast<const ForStmt&>(stmt).body()) {
      collectLambdas(*item, out);
    }
    break;
  case NodeKind::FunctionDef:
    for (const std::unique_ptr<Stmt>& item : static_cast<const FunctionDef&>(stmt).body()) {
      collectLambdas(*item, out);
    }
    break;
  case NodeKind::ClassDef:
    for (const std::unique_ptr<FunctionDef>& method :
         static_cast<const ClassDef&>(stmt).methods()) {
      collectLambdas(*method, out);
    }
    break;
  case NodeKind::DeferStmt:
    for (const std::unique_ptr<Stmt>& item : static_cast<const DeferStmt&>(stmt).body()) {
      collectLambdas(*item, out);
    }
    break;
  case NodeKind::WithStmt:
    collectLambdas(static_cast<const WithStmt&>(stmt).context(), out);
    for (const std::unique_ptr<Stmt>& item : static_cast<const WithStmt&>(stmt).body()) {
      collectLambdas(*item, out);
    }
    break;
  default:
    break;
  }
}

void IRGenerator::declareLambdas(const Module& ast) {
  std::vector<const LambdaExpr*> lambdas;
  for (const std::unique_ptr<Stmt>& statement : ast.statements()) {
    collectLambdas(*statement, lambdas);
  }
  for (const LambdaExpr* lambda : lambdas) {
    if (lambda == nullptr || lambda->llvmName().empty() || lambda->resolvedType() == nullptr) {
      continue;
    }
    if (module_->getFunction(lambda->llvmName()) != nullptr) {
      continue;
    }
    llvm::FunctionType* type = llvmFunctionTypeFrom(lambda->resolvedType());
    llvm::Function* fn =
        llvm::Function::Create(type, llvm::Function::InternalLinkage, lambda->llvmName(), module_);
    functions_[lambda->llvmName()] = fn;
  }
}

bool IRGenerator::emitLambdaFunction(const LambdaExpr& expr) {
  llvm::Function* fn = module_->getFunction(expr.llvmName());
  if (fn == nullptr || !fn->empty() || expr.resolvedType() == nullptr) {
    return true;
  }
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
  llvm::IRBuilder<> builder(entry);
  auto savedLocals = locals_;
  auto savedDrops = dropStack_;
  const FunctionDef* savedFn = currentFunction_;
  currentFunction_ = nullptr;
  locals_.clear();
  dropStack_.clear();
  std::size_t index = 0;
  const Type* fnType = expr.resolvedType();
  for (llvm::Argument& arg : fn->args()) {
    if (index >= expr.params().size()) {
      break;
    }
    arg.setName(expr.params()[index].name);
    llvm::Value* slot = builder.CreateAlloca(arg.getType(), nullptr, expr.params()[index].name);
    builder.CreateStore(&arg, slot);
    const Type* paramType =
        index < fnType->paramTypes().size() ? fnType->paramTypes()[index] : nullptr;
    rememberLocal(expr.params()[index].name, slot, paramType);
    ++index;
  }
  llvm::Value* result = emitExpr(builder, expr.body());
  const Type* ret = fnType->returnType();
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    emitReturn(builder, result, ret);
  }
  locals_ = std::move(savedLocals);
  dropStack_ = std::move(savedDrops);
  currentFunction_ = savedFn;
  return true;
}

llvm::Value* IRGenerator::emitLambda(llvm::IRBuilder<>& builder, const LambdaExpr& expr) {
  const auto found = functions_.find(expr.llvmName());
  if (found == functions_.end()) {
    diagnostics_->error(expr.range(), "internal: missing lambda '" + expr.llvmName() + "'");
    return nullptr;
  }
  return packCallable(builder, found->second, nullptr);
}

llvm::Type* IRGenerator::callableFatType() {
  return llvm::StructType::get(
      *context_,
      {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)});
}

llvm::Value*
IRGenerator::packCallable(llvm::IRBuilder<>& builder, llvm::Value* fn, llvm::Value* env) {
  llvm::Type* fat = callableFatType();
  // Callable values can escape their defining frame (decorators and nested
  // functions both rely on this), so the pair must not live in a stack alloca.
  llvm::Function* allocate = runtimeDecl("sere_alloc", builder.getPtrTy(), {builder.getInt64Ty()});
  llvm::Value* slot = builder.CreateCall(allocate, {builder.getInt64(16)}, "cb.pack");
  llvm::Value* fnPtr = fn == nullptr ? llvm::ConstantPointerNull::get(builder.getPtrTy())
                                     : builder.CreateBitCast(fn, builder.getPtrTy());
  llvm::Value* envPtr = env == nullptr ? llvm::ConstantPointerNull::get(builder.getPtrTy())
                                       : builder.CreateBitCast(env, builder.getPtrTy());
  builder.CreateStore(fnPtr, builder.CreateStructGEP(fat, slot, 0));
  builder.CreateStore(envPtr, builder.CreateStructGEP(fat, slot, 1));
  return slot;
}

llvm::Value* IRGenerator::emitIndirectCallable(llvm::IRBuilder<>& builder,
                                               llvm::Value* callable,
                                               llvm::FunctionType* freeType,
                                               const std::vector<llvm::Value*>& args) {
  if (callable == nullptr || freeType == nullptr) {
    return nullptr;
  }
  if (callable->getType()->isPointerTy()) {
    callable = builder.CreateLoad(callableFatType(), callable, "cb.fat");
  }
  if (!callable->getType()->isStructTy()) {
    if (freeType->getReturnType()->isVoidTy()) {
      builder.CreateCall(freeType, callable, args);
      return nullptr;
    }
    return builder.CreateCall(freeType, callable, args);
  }
  llvm::Value* fn = builder.CreateExtractValue(callable, {0}, "cb.fn");
  llvm::Value* env = builder.CreateExtractValue(callable, {1}, "cb.env");
  llvm::Value* isBound =
      builder.CreateICmpNE(env, llvm::ConstantPointerNull::get(builder.getPtrTy()));
  llvm::Function* parent = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock* boundBlock = llvm::BasicBlock::Create(*context_, "cb.bound", parent);
  llvm::BasicBlock* freeBlock = llvm::BasicBlock::Create(*context_, "cb.free", parent);
  llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*context_, "cb.merge", parent);
  builder.CreateCondBr(isBound, boundBlock, freeBlock);

  std::vector<llvm::Type*> boundParams;
  boundParams.push_back(builder.getPtrTy());
  for (llvm::Type* param : freeType->params()) {
    boundParams.push_back(param);
  }
  llvm::FunctionType* boundType =
      llvm::FunctionType::get(freeType->getReturnType(), boundParams, false);

  builder.SetInsertPoint(boundBlock);
  std::vector<llvm::Value*> boundArgs;
  boundArgs.push_back(env);
  boundArgs.insert(boundArgs.end(), args.begin(), args.end());
  llvm::Value* boundRet = nullptr;
  if (boundType->getReturnType()->isVoidTy()) {
    builder.CreateCall(boundType, fn, boundArgs);
  } else {
    boundRet = builder.CreateCall(boundType, fn, boundArgs, "cb.bound.ret");
  }
  llvm::BasicBlock* boundEnd = builder.GetInsertBlock();
  builder.CreateBr(mergeBlock);

  builder.SetInsertPoint(freeBlock);
  llvm::Value* freeRet = nullptr;
  if (freeType->getReturnType()->isVoidTy()) {
    builder.CreateCall(freeType, fn, args);
  } else {
    freeRet = builder.CreateCall(freeType, fn, args, "cb.free.ret");
  }
  llvm::BasicBlock* freeEnd = builder.GetInsertBlock();
  builder.CreateBr(mergeBlock);

  builder.SetInsertPoint(mergeBlock);
  if (freeType->getReturnType()->isVoidTy()) {
    return nullptr;
  }
  llvm::PHINode* phi = builder.CreatePHI(freeType->getReturnType(), 2, "cb.ret");
  phi->addIncoming(boundRet, boundEnd);
  phi->addIncoming(freeRet, freeEnd);
  return phi;
}

bool IRGenerator::emitUnpack(llvm::IRBuilder<>& builder,
                             const TupleExpr& targets,
                             llvm::Value* value,
                             const Type* valueType) {
  if (value == nullptr || valueType == nullptr) {
    return false;
  }
  for (std::size_t index = 0; index < targets.elements().size(); ++index) {
    const Expr& item = *targets.elements()[index];
    const NameExpr* name = asName(item);
    if (name == nullptr) {
      return false;
    }
    const Type* elemType =
        index < valueType->args().size() ? valueType->args()[index] : item.resolvedType();
    if (!locals_.contains(name->name()) && !globals_.contains(name->name())) {
      createLocalSlot(builder, name->name(), elemType);
    }
    llvm::Value* slot =
        locals_.contains(name->name()) ? locals_[name->name()] : globals_[name->name()];
    llvm::Value* extracted = builder.CreateExtractValue(value, {static_cast<unsigned>(index)});
    if (slot != nullptr) {
      builder.CreateStore(extracted, slot);
    }
  }
  return true;
}

bool IRGenerator::emitDel(llvm::IRBuilder<>& builder, const DelStmt& statement) {
  if (statement.target().kind() != NodeKind::IndexExpr) {
    diagnostics_->error(statement.range(), "del lowering requires an index");
    return false;
  }
  const auto& index = static_cast<const IndexExpr&>(statement.target());
  const Type* objectType = index.object().resolvedType();
  llvm::Value* object = emitExpr(builder, index.object());
  if (object == nullptr || objectType == nullptr || !index.hasStart()) {
    return false;
  }
  if (objectType->isList()) {
    llvm::Value* at = emitIndexI64(builder, *index.start());
    llvm::Function* fn = runtimeDecl(
        "sere_list_remove", builder.getVoidTy(), {builder.getPtrTy(), builder.getInt64Ty()});
    builder.CreateCall(fn, {object, at});
    return true;
  }
  if (objectType->isDict()) {
    llvm::Value* key = emitExpr(builder, *index.start());
    llvm::Function* fn = runtimeDecl(
        "sere_dict_del", builder.getInt32Ty(), {builder.getPtrTy(), builder.getPtrTy()});
    builder.CreateCall(fn, {object, emitTempSlot(builder, key, objectType->dictKeyType())});
    return true;
  }
  diagnostics_->error(statement.range(), "del is not implemented for this type");
  return false;
}

bool IRGenerator::emitWithExit(llvm::IRBuilder<>& builder, const WithFrame& frame) {
  if (frame.stmt == nullptr || frame.self == nullptr) {
    return false;
  }
  const Type* record = frame.stmt->context().resolvedType() == nullptr
                           ? nullptr
                           : frame.stmt->context().resolvedType()->canonical();
  emitDunderOnSelf(builder, record, frame.self, "__exit__", {});
  return true;
}

bool IRGenerator::emitWith(llvm::IRBuilder<>& builder,
                           const WithStmt& statement,
                           const Type* returnType) {
  const Type* record = resolveType(statement.context().resolvedType());
  if (record == nullptr) {
    return false;
  }
  llvm::Value* self = emitAddress(builder, statement.context(), false);
  if (self == nullptr) {
    llvm::Value* value = emitExpr(builder, statement.context());
    if (value == nullptr) {
      return false;
    }
    self = builder.CreateAlloca(lower(record), nullptr, "with.self");
    builder.CreateStore(value, self);
  }
  llvm::Value* entered = emitDunderOnSelf(builder, record, self, "__enter__", {});
  if (!statement.name().empty()) {
    const Type* enteredType = record->dunderReturn("__enter__");
    if (enteredType == nullptr) {
      enteredType = record;
    }
    if (!locals_.contains(statement.name())) {
      createLocalSlot(builder, statement.name(), enteredType);
    }
    if (entered != nullptr && locals_.contains(statement.name())) {
      builder.CreateStore(entered, locals_[statement.name()]);
    }
  }
  withStack_.push_back(WithFrame{&statement, self});
  const bool ok = emitBlock(builder, statement.body(), returnType);
  if (builder.GetInsertBlock()->getTerminator() == nullptr) {
    (void)emitWithExit(builder, withStack_.back());
  }
  if (!withStack_.empty() && withStack_.back().stmt == &statement) {
    withStack_.pop_back();
  }
  return ok;
}

} // namespace sere
