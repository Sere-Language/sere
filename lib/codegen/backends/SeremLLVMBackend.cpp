/// @file SeremLLVMBackend.cpp
/// Serem SSA to LLVM lowering.

#include "sere/codegen/backends/SeremLLVMBackend.h"

#include "sere/diag/DiagnosticEngine.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <cstdint>
#include <charconv>
#include <string_view>
#include <utility>

namespace sere {

class SeremLLVMBackend::IRBuilderHolder {
public:
  explicit IRBuilderHolder(llvm::LLVMContext& context) : builder(context) {}
  llvm::IRBuilder<> builder;
};

SeremLLVMBackend::SeremLLVMBackend(llvm::LLVMContext& context, DiagnosticEngine& diagnostics)
    : context_(&context), diagnostics_(&diagnostics), builder_(std::make_unique<IRBuilderHolder>(context)) {
}

SeremLLVMBackend::~SeremLLVMBackend() = default;

void SeremLLVMBackend::report(std::string message) {
  diagnostics_->error(std::move(message));
}

llvm::Type* SeremLLVMBackend::lowerType(const serem::IRType& type) {
  switch (type.kind()) {
  case serem::IRType::Kind::Void: return llvm::Type::getVoidTy(*context_);
  case serem::IRType::Kind::Bool: return llvm::Type::getInt1Ty(*context_);
  case serem::IRType::Kind::I8:
  case serem::IRType::Kind::U8: return llvm::Type::getInt8Ty(*context_);
  case serem::IRType::Kind::I16:
  case serem::IRType::Kind::U16: return llvm::Type::getInt16Ty(*context_);
  case serem::IRType::Kind::I32:
  case serem::IRType::Kind::U32: return llvm::Type::getInt32Ty(*context_);
  case serem::IRType::Kind::I64:
  case serem::IRType::Kind::U64: return llvm::Type::getInt64Ty(*context_);
  case serem::IRType::Kind::F32: return llvm::Type::getFloatTy(*context_);
  case serem::IRType::Kind::F64: return llvm::Type::getDoubleTy(*context_);
  case serem::IRType::Kind::Ptr:
  case serem::IRType::Kind::String: return llvm::PointerType::getUnqual(*context_);
  case serem::IRType::Kind::Array:
    return llvm::ArrayType::get(lowerType(type.elements().front()), type.length());
  case serem::IRType::Kind::Function: {
    std::vector<llvm::Type*> params;
    for (const serem::IRType& param : type.parameters()) params.push_back(lowerType(param));
    return llvm::FunctionType::get(lowerType(*type.pointee()), params, false);
  }
  case serem::IRType::Kind::Struct: {
    const auto found = structs_.find(type.name());
    if (found != structs_.end()) return found->second;
    llvm::StructType* result = llvm::StructType::create(*context_, type.name());
    structs_.insert_or_assign(type.name(), result);
    std::vector<llvm::Type*> fields;
    for (const serem::IRType& field : type.elements()) fields.push_back(lowerType(field));
    result->setBody(fields);
    return result;
  }
  case serem::IRType::Kind::Label: return llvm::Type::getLabelTy(*context_);
  }
  return llvm::Type::getVoidTy(*context_);
}

llvm::BasicBlock* SeremLLVMBackend::blockFor(std::string_view name) const {
  auto found = blocks_.find(std::string(name));
  if (found != blocks_.end()) return found->second;
  found = blocks_.find(currentFunctionName_ + ":" + std::string(name));
  return found == blocks_.end() ? nullptr : found->second;
}

std::string SeremLLVMBackend::attribute(const serem::Operation& operation,
                                        std::string_view name) const {
  const auto found = operation.attributes().find(std::string(name));
  return found == operation.attributes().end() ? std::string{} : found->second;
}

llvm::Function* SeremLLVMBackend::functionFor(const serem::FunctionRef& function) {
  const auto found = functions_.find(function.name());
  return found == functions_.end() ? ensureExternal(function) : found->second;
}

llvm::Function* SeremLLVMBackend::ensureExternal(const serem::FunctionRef& function) {
  auto* functionType = llvm::dyn_cast<llvm::FunctionType>(lowerType(function.type()));
  if (functionType == nullptr) {
    report("Serem function reference '" + function.name() + "' has a non-function type");
    return nullptr;
  }
  llvm::Function* result = llvm::Function::Create(
      functionType, llvm::Function::ExternalLinkage, function.name(), module_.get());
  functions_.insert_or_assign(function.name(), result);
  return result;
}

llvm::Value* SeremLLVMBackend::lowerValue(const serem::ValuePtr& value) {
  if (value == nullptr) return nullptr;
  const auto found = values_.find(value.get());
  if (found != values_.end()) return found->second;
  switch (value->valueKind()) {
  case serem::ValueKind::ConstantInt: {
    const auto* constant = static_cast<const serem::ConstantInt*>(value.get());
    return llvm::ConstantInt::get(lowerType(value->type()), constant->value(), true);
  }
  case serem::ValueKind::ConstantFloat: {
    const auto* constant = static_cast<const serem::ConstantFloat*>(value.get());
    return llvm::ConstantFP::get(lowerType(value->type()), constant->value());
  }
  case serem::ValueKind::ConstantString: {
    const auto* constant = static_cast<const serem::ConstantString*>(value.get());
    llvm::GlobalVariable* global = builder_->builder.CreateGlobalString(constant->value());
    return builder_->builder.CreatePointerCast(global, llvm::PointerType::getUnqual(*context_));
  }
  case serem::ValueKind::Argument:
    report("Serem argument was not attached to an LLVM function");
    return nullptr;
  case serem::ValueKind::FunctionRef:
    return functionFor(*static_cast<const serem::FunctionRef*>(value.get()));
  case serem::ValueKind::Operation:
    return lowerOperation(*static_cast<const serem::Operation*>(value.get()));
  }
  return nullptr;
}

llvm::Value* SeremLLVMBackend::lowerOperation(const serem::Operation& operation) {
  const auto operands = operation.operands();
  auto operand = [&](std::size_t index) -> llvm::Value* {
    return index < operands.size() ? lowerValue(operands[index]) : nullptr;
  };
  llvm::Value* result = nullptr;
  const std::string& opcode = operation.opcode();
  llvm::Type* type = lowerType(operation.type());
  if (opcode == "add") result = builder_->builder.CreateAdd(operand(0), operand(1));
  else if (opcode == "sub") result = builder_->builder.CreateSub(operand(0), operand(1));
  else if (opcode == "mul") result = builder_->builder.CreateMul(operand(0), operand(1));
  else if (opcode == "div") result = builder_->builder.CreateSDiv(operand(0), operand(1));
  else if (opcode == "rem") result = builder_->builder.CreateSRem(operand(0), operand(1));
  else if (opcode == "fadd") result = builder_->builder.CreateFAdd(operand(0), operand(1));
  else if (opcode == "fsub") result = builder_->builder.CreateFSub(operand(0), operand(1));
  else if (opcode == "fmul") result = builder_->builder.CreateFMul(operand(0), operand(1));
  else if (opcode == "fdiv") result = builder_->builder.CreateFDiv(operand(0), operand(1));
  else if (opcode == "and") result = builder_->builder.CreateAnd(operand(0), operand(1));
  else if (opcode == "or") result = builder_->builder.CreateOr(operand(0), operand(1));
  else if (opcode == "xor") result = builder_->builder.CreateXor(operand(0), operand(1));
  else if (opcode == "shl") result = builder_->builder.CreateShl(operand(0), operand(1));
  else if (opcode == "shr") result = builder_->builder.CreateAShr(operand(0), operand(1));
  else if (opcode.starts_with("cmp.")) {
    const std::string predicate = opcode.substr(4);
    llvm::CmpInst::Predicate comparison = llvm::CmpInst::ICMP_EQ;
    if (predicate == "ne") comparison = llvm::CmpInst::ICMP_NE;
    else if (predicate == "lt") comparison = llvm::CmpInst::ICMP_SLT;
    else if (predicate == "le") comparison = llvm::CmpInst::ICMP_SLE;
    else if (predicate == "gt") comparison = llvm::CmpInst::ICMP_SGT;
    else if (predicate == "ge") comparison = llvm::CmpInst::ICMP_SGE;
    result = builder_->builder.CreateICmp(comparison, operand(0), operand(1));
  } else if (opcode == "neg") result = builder_->builder.CreateNeg(operand(0));
  else if (opcode == "not" || opcode == "invert") result = builder_->builder.CreateNot(operand(0));
  else if (opcode == "alloca") {
    const serem::IRType* element = operation.type().pointee();
    result = element == nullptr ? nullptr : builder_->builder.CreateAlloca(lowerType(*element));
  }
  else if (opcode == "load") result = builder_->builder.CreateLoad(type, operand(0));
  else if (opcode == "store") builder_->builder.CreateStore(operand(0), operand(1));
  else if (opcode == "select") result = builder_->builder.CreateSelect(operand(0), operand(1), operand(2));
  else if (opcode == "runtime.print") {
    llvm::Function* print = module_->getFunction("sere_print_str");
    if (print == nullptr) {
      llvm::FunctionType* printType = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)}, false);
      print = llvm::Function::Create(printType, llvm::Function::ExternalLinkage,
                                     "sere_print_str", module_.get());
    }
    llvm::Function* length = module_->getFunction("strlen");
    if (length == nullptr) {
      llvm::FunctionType* lengthType = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
      length = llvm::Function::Create(lengthType, llvm::Function::ExternalLinkage,
                                      "strlen", module_.get());
    }
    bool stringPrinted = false;
    for (std::size_t index = 0; index < operands.size(); ++index) {
      llvm::Value* value = operand(index);
      if (value == nullptr) continue;
      if (value->getType()->isPointerTy()) {
        builder_->builder.CreateCall(print, {value, builder_->builder.CreateCall(length, {value})});
        stringPrinted = true;
      } else if (value->getType()->isIntegerTy(32)) {
        llvm::Function* write = module_->getFunction("sere_write_i32");
        if (write == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt32Ty(*context_)}, false);
          write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                         "sere_write_i32", module_.get());
        }
        builder_->builder.CreateCall(write, {value});
      }
    }
    if (!stringPrinted) {
      if (llvm::Function* newline = module_->getFunction("sere_write_nl")) {
        builder_->builder.CreateCall(newline);
      } else {
        llvm::FunctionType* newlineType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context_), false);
        llvm::Function* newlineFn = llvm::Function::Create(
            newlineType, llvm::Function::ExternalLinkage, "sere_write_nl", module_.get());
        builder_->builder.CreateCall(newlineFn);
      }
    }
  }
  else if (opcode == "string.concat") {
    llvm::Function* concat = module_->getFunction("sere_str_concat_data");
    if (concat == nullptr) {
      llvm::FunctionType* concatType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_)}, false);
      concat = llvm::Function::Create(concatType, llvm::Function::ExternalLinkage,
                                      "sere_str_concat_data", module_.get());
    }
    llvm::Function* length = module_->getFunction("strlen");
    if (length == nullptr) {
      llvm::FunctionType* lengthType = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
      length = llvm::Function::Create(lengthType, llvm::Function::ExternalLinkage,
                                      "strlen", module_.get());
    }
    llvm::Value* current = nullptr;
    llvm::Value* currentLength = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
    for (std::size_t index = 0; index < operands.size(); ++index) {
      llvm::Value* value = operand(index);
      if (value == nullptr) continue;
      llvm::Value* valueLength = builder_->builder.CreateCall(length, {value});
      if (current == nullptr) {
        current = value;
        currentLength = valueLength;
      } else {
        llvm::AllocaInst* outLength = builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
        current = builder_->builder.CreateCall(concat,
                                               {current, currentLength, value, valueLength, outLength});
        currentLength = builder_->builder.CreateLoad(llvm::Type::getInt64Ty(*context_), outLength);
      }
    }
    result = current;
  }
  else if (opcode == "construct") {
    if (type->isStructTy()) {
      result = llvm::UndefValue::get(type);
      unsigned field = 0;
      if (type->getStructNumElements() == 2 && operands.size() == 1) {
        result = builder_->builder.CreateInsertValue(
            result, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), {0});
        field = 1;
      }
      for (std::size_t index = 0; index < operands.size(); ++index) {
        result = builder_->builder.CreateInsertValue(result, operand(index),
                                                     {field + static_cast<unsigned>(index)});
      }
    } else if (!operands.empty()) {
      result = operand(0);
    }
  } else if (opcode == "member.get") {
    unsigned index = 0;
    const std::string indexText = attribute(operation, "index");
    (void)std::from_chars(indexText.data(), indexText.data() + indexText.size(), index);
    if (indexText == "-1") {
      const std::string field = attribute(operation, "field");
      if (field == "name") {
        result = operand(0);
      } else {
        llvm::GlobalVariable* global = builder_->builder.CreateGlobalString(field);
        result = builder_->builder.CreatePointerCast(
          global, llvm::PointerType::getUnqual(*context_));
      }
    } else {
      llvm::Value* object = operand(0);
      if (object != nullptr && object->getType()->isStructTy()) {
        result = builder_->builder.CreateExtractValue(object, {index});
      } else {
        result = object;
      }
    }
  } else if (opcode == "member.set") {
    // Value aggregates are immutable in SSA; mutable object lowering will add
    // an address-producing member operation in the next dialect revision.
  }
  else if (opcode == "enum.tag") {
    llvm::Value* object = operand(0);
    if (object != nullptr && object->getType()->isStructTy()) {
      result = builder_->builder.CreateExtractValue(object, {0});
    } else {
      result = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0);
    }
  } else if (opcode == "enum.payload") {
    llvm::Value* object = operand(0);
    if (object != nullptr && object->getType()->isStructTy()) {
      result = builder_->builder.CreateExtractValue(object, {1});
    } else {
      result = operand(0);
    }
  }
  else if (opcode == "call" || opcode == "invoke") {
    llvm::Function* function = nullptr;
    if (!operands.empty() && operands[0] != nullptr &&
        operands[0]->valueKind() == serem::ValueKind::FunctionRef) {
      function = functionFor(*static_cast<const serem::FunctionRef*>(operands[0].get()));
    }
    std::vector<llvm::Value*> arguments;
    for (std::size_t index = 1; index < operands.size(); ++index) {
      arguments.push_back(lowerValue(operands[index]));
    }
    if (function != nullptr) {
      result = builder_->builder.CreateCall(function, arguments);
    } else if (!operands.empty()) {
      llvm::Value* callee = lowerValue(operands[0]);
      if (callee != nullptr && callee->getType()->isPointerTy()) {
        std::vector<llvm::Type*> parameterTypes;
        for (llvm::Value* argument : arguments) parameterTypes.push_back(argument->getType());
        llvm::FunctionType* indirectType = llvm::FunctionType::get(type, parameterTypes, false);
        result = builder_->builder.CreateCall(indirectType, callee, arguments);
      } else {
        report("Serem call lowering requires a direct or indirect callable");
      }
    } else {
      report("Serem call lowering requires a callable");
    }
  } else if (opcode == "return") {
    if (operands.empty()) builder_->builder.CreateRetVoid();
    else builder_->builder.CreateRet(operand(0));
  } else if (opcode == "unreachable") builder_->builder.CreateUnreachable();
  else if (opcode == "branch") {
    if (llvm::BasicBlock* target = blockFor(attribute(operation, "target"))) {
      builder_->builder.CreateBr(target);
    }
  } else if (opcode == "cond_branch") {
    llvm::BasicBlock* ifTrue = blockFor(attribute(operation, "true"));
    llvm::BasicBlock* ifFalse = blockFor(attribute(operation, "false"));
    if (ifTrue != nullptr && ifFalse != nullptr) builder_->builder.CreateCondBr(operand(0), ifTrue, ifFalse);
  } else if (opcode == "cast.value") {
    result = builder_->builder.CreateBitCast(operand(0), type);
  } else if (opcode == "await") {
    result = operand(0);
  } else if (opcode == "coro.begin") {
    result = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
  }
  else if (opcode == "decorated.call") {
    if (operands.size() >= 2) {
      llvm::Value* decorator = lowerValue(operands[0]);
      llvm::Value* target = lowerValue(operands[1]);
      std::vector<llvm::Value*> decoratorArgs{target};
      llvm::FunctionType* decoratorType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context_),
          {llvm::PointerType::getUnqual(*context_)}, false);
      llvm::Value* wrapped = builder_->builder.CreateCall(decoratorType, decorator, decoratorArgs);
      std::vector<llvm::Value*> arguments;
      for (std::size_t index = 2; index < operands.size(); ++index) {
        arguments.push_back(lowerValue(operands[index]));
      }
      std::vector<llvm::Type*> parameterTypes;
      for (llvm::Value* argument : arguments) parameterTypes.push_back(argument->getType());
      llvm::FunctionType* wrappedType = llvm::FunctionType::get(type, parameterTypes, false);
      result = builder_->builder.CreateCall(wrappedType, wrapped, arguments);
    }
  }
  if (result == nullptr && !operation.type().isVoid()) result = llvm::UndefValue::get(type);
  if (!operation.resultName().empty()) values_[&operation] = result;
  return result;
}

std::unique_ptr<llvm::Module> SeremLLVMBackend::emit(const serem::IRModule& module,
                                                     const std::string& moduleName) {
  module_ = std::make_unique<llvm::Module>(moduleName, *context_);
  structs_.clear();
  functions_.clear();
  blocks_.clear();
  values_.clear();
  for (const auto& type : module.types()) (void)lowerType(type->type());
  for (const auto& function : module.functions()) {
    std::vector<llvm::Type*> params;
    for (const serem::IRType& param : function->parameters()) params.push_back(lowerType(param));
    auto* functionType = llvm::FunctionType::get(lowerType(function->resultType()), params, false);
    functions_[function->name()] = llvm::Function::Create(
        functionType, llvm::Function::ExternalLinkage, function->name(), module_.get());
  }
  for (const auto& function : module.functions()) {
    currentFunctionName_ = function->name();
    llvm::Function* llvmFunction = functions_[function->name()];
    for (std::size_t index = 0; index < function->arguments().size(); ++index) {
      values_[function->arguments()[index].get()] = llvmFunction->getArg(index);
    }
    for (std::size_t index = 0; index < function->blocks().size(); ++index) {
      llvm::BasicBlock* block = llvm::BasicBlock::Create(
          *context_, function->blocks()[index]->label(), llvmFunction);
      blocks_.insert_or_assign(function->name() + ":" + function->blocks()[index]->label(), block);
    }
    for (const auto& block : function->blocks()) {
      builder_->builder.SetInsertPoint(blockFor(function->name() + ":" + block->label()));
      for (const auto& operation : block->operations()) (void)lowerOperation(*operation);
      if (builder_->builder.GetInsertBlock()->getTerminator() == nullptr && block->isTerminated()) {
        builder_->builder.CreateUnreachable();
      } else if (builder_->builder.GetInsertBlock()->getTerminator() == nullptr) {
        if (function->resultType().isVoid()) builder_->builder.CreateRetVoid();
        else builder_->builder.CreateRet(llvm::UndefValue::get(lowerType(function->resultType())));
      }
    }
  }
  return std::move(module_);
}

} // namespace sere
