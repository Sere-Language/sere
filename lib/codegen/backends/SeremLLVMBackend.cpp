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
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <charconv>
#include <string_view>
#include <utility>

namespace sere {

namespace {

/// Storage layout for one list element kind: the LLVM type actually stored and
/// the byte stride the runtime uses to index items. Both must agree with the
/// runtime formatter, which reads the same kind code.
[[nodiscard]] std::pair<llvm::Type*, std::int64_t>
listElementLayout(llvm::LLVMContext& context, std::int32_t kind) {
  switch (kind) {
  case 1: return {llvm::Type::getInt32Ty(context), 4};
  case 2: return {llvm::Type::getInt64Ty(context), 8};
  case 3: return {llvm::Type::getDoubleTy(context), 8};
  case 4: return {llvm::Type::getFloatTy(context), 4};
  case 7:
  case 9: return {llvm::Type::getInt8Ty(context), 1};
  case 8:
  case 10: return {llvm::Type::getInt16Ty(context), 2};
  case 5: return {llvm::Type::getInt8Ty(context), 1};
  case 0: return {llvm::PointerType::getUnqual(context), 16};
  default: return {llvm::PointerType::getUnqual(context), 8};
  }
}

/// Reads a list element kind attribute; unknown or absent codes fall back to the
/// pointer layout instead of guessing a numeric width.
[[nodiscard]] std::int32_t elementKindCode(const std::string& text) {
  std::int32_t code = 6;
  if (text.empty()) return code;
  (void)std::from_chars(text.data(), text.data() + text.size(), code);
  return code;
}

/// Symbol the language-level `main` is emitted under while the platform entry
/// point takes the `main` symbol.
constexpr const char* kEntrySymbol = "sere_main";

} // namespace

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
  if (externalSymbols_.contains(function.name())) {
    std::vector<llvm::Type*> params;
    for (const auto& param : function.type().parameters()) {
      params.push_back(lowerType(param));
      if (param.kind() == serem::IRType::Kind::String) params.push_back(builder_->builder.getInt64Ty());
    }
    llvm::Type* resultType = lowerType(*function.type().pointee());
    if (function.type().pointee()->kind() == serem::IRType::Kind::String) {
      resultType = builder_->builder.getVoidTy();
      params.push_back(builder_->builder.getPtrTy());
      params.push_back(builder_->builder.getPtrTy());
    }
    functionType = llvm::FunctionType::get(resultType, params, false);
  }
  llvm::Function* result = module_->getFunction(function.name());
  if (result == nullptr) {
    result = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage,
                                    function.name(), module_.get());
  } else if (result->getFunctionType() != functionType) {
    report("conflicting Serem external signature for " + function.name());
    return nullptr;
  }
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
  auto& ir = builder_->builder;
  auto convert = [&](llvm::Value* value, llvm::Type* target, bool isSigned = true) -> llvm::Value* {
    llvm::Type* source = value->getType();
    if (source == target) return value;
    if (target->isIntegerTy(1)) {
      if (source->isIntegerTy() || source->isPointerTy()) return ir.CreateIsNotNull(value);
      if (source->isFloatingPointTy()) return ir.CreateFCmpUNE(value, llvm::ConstantFP::get(source, 0));
    }
    if (source->isIntegerTy() && target->isIntegerTy()) return ir.CreateIntCast(value, target, isSigned);
    if (source->isFloatingPointTy() && target->isFloatingPointTy()) return ir.CreateFPCast(value, target);
    if (source->isIntegerTy() && target->isFloatingPointTy())
      return isSigned ? ir.CreateSIToFP(value, target) : ir.CreateUIToFP(value, target);
    if (source->isFloatingPointTy() && target->isIntegerTy())
      return isSigned ? ir.CreateFPToSI(value, target) : ir.CreateFPToUI(value, target);
    if (source->isPointerTy() && target->isPointerTy()) return value;
    report("unsupported Serem value conversion");
    return llvm::UndefValue::get(target);
  };
  if (opcode == "pointer.null") result = llvm::ConstantPointerNull::get(ir.getPtrTy());
  else if (opcode == "pointer.is_null") {
    result = ir.CreateIsNull(operand(0));
    if (attribute(operation, "negated") == "true") result = ir.CreateNot(result);
  }
  else if (opcode == "add") result = builder_->builder.CreateAdd(operand(0), operand(1));
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
    llvm::Value* left = operand(0);
    llvm::Value* right = operand(1);
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
      llvm::Type* common = ir.getIntNTy(std::max(left->getType()->getIntegerBitWidth(),
                                                right->getType()->getIntegerBitWidth()));
      left = convert(left, common);
      right = convert(right, common);
    }
    result = builder_->builder.CreateICmp(comparison, left, right);
  } else if (opcode == "neg") result = builder_->builder.CreateNeg(operand(0));
  else if (opcode == "not" || opcode == "invert") result = builder_->builder.CreateNot(operand(0));
  else if (opcode == "alloca") {
    const serem::IRType* element = operation.type().pointee();
    result = element == nullptr ? nullptr : builder_->builder.CreateAlloca(lowerType(*element));
  }
  else if (opcode == "load") result = builder_->builder.CreateLoad(type, operand(0));
  else if (opcode == "store") builder_->builder.CreateStore(operand(0), operand(1));
  else if (opcode == "store.indirect") builder_->builder.CreateStore(operand(0), operand(1));
  else if (opcode == "deref") result = builder_->builder.CreateLoad(type, operand(0));
  else if (opcode == "address.of") result = operand(0);
  else if (opcode == "select") result = builder_->builder.CreateSelect(operand(0), operand(1), operand(2));
  else if (opcode == "index" || opcode == "index.address" || opcode == "index.set") {
    auto item = module_->getOrInsertFunction("sere_list_item", ir.getPtrTy(), ir.getPtrTy(), ir.getInt64Ty());
    llvm::Value* address = ir.CreateCall(item, {operand(0), convert(operand(1), ir.getInt64Ty())});
    if (opcode == "index.address") result = address;
    else if (opcode == "index.set") ir.CreateStore(operand(2), address);
    else result = ir.CreateLoad(type, address);
  }
  else if (opcode == "aggregate.array") {
    auto create = module_->getOrInsertFunction("sere_array_new", ir.getPtrTy(), ir.getInt64Ty(), ir.getInt64Ty());
    const auto layout = listElementLayout(*context_, elementKindCode(attribute(operation, "element.kind")));
    result = ir.CreateCall(create, {ir.getInt64(layout.second), ir.getInt64(operands.size())});
    auto item = module_->getOrInsertFunction("sere_list_item", ir.getPtrTy(), ir.getPtrTy(), ir.getInt64Ty());
    for (std::size_t index = 0; index < operands.size(); ++index) {
      llvm::Value* address = ir.CreateCall(item, {result, ir.getInt64(index)});
      if (elementKindCode(attribute(operation, "element.kind")) == 0) {
        auto pushString = module_->getOrInsertFunction("strlen", ir.getInt64Ty(), ir.getPtrTy());
        llvm::Type* stringType = llvm::StructType::get(*context_, {ir.getPtrTy(), ir.getInt64Ty()});
        ir.CreateStore(operand(index), ir.CreateStructGEP(stringType, address, 0));
        ir.CreateStore(ir.CreateCall(pushString, {operand(index)}), ir.CreateStructGEP(stringType, address, 1));
      } else {
        ir.CreateStore(convert(operand(index), layout.first), address);
      }
    }
  }
  else if (opcode == "aggregate.range") {
    const auto layout = listElementLayout(*context_, elementKindCode(attribute(operation, "element.kind")));
    llvm::Value* start = operands.size() > 1 ? convert(operand(0), ir.getInt64Ty()) : ir.getInt64(0);
    llvm::Value* stop = convert(operand(operands.size() > 1 ? 1 : 0), ir.getInt64Ty());
    llvm::Value* step = operands.size() > 2 ? convert(operand(2), ir.getInt64Ty()) : ir.getInt64(1);
    auto create = module_->getOrInsertFunction("sere_list_new", ir.getPtrTy(), ir.getInt64Ty());
    auto push = module_->getOrInsertFunction("sere_list_push", ir.getVoidTy(), ir.getPtrTy(), ir.getPtrTy());
    result = ir.CreateCall(create, {ir.getInt64(layout.second)});
    llvm::Value* counter = ir.CreateAlloca(ir.getInt64Ty());
    llvm::Value* item = ir.CreateAlloca(layout.first);
    ir.CreateStore(start, counter);
    llvm::Function* function = ir.GetInsertBlock()->getParent();
    auto* condition = llvm::BasicBlock::Create(*context_, "range.cond", function);
    auto* body = llvm::BasicBlock::Create(*context_, "range.body", function);
    auto* end = llvm::BasicBlock::Create(*context_, "range.end", function);
    ir.CreateBr(condition);
    ir.SetInsertPoint(condition);
    llvm::Value* current = ir.CreateLoad(ir.getInt64Ty(), counter);
    llvm::Value* active = ir.CreateSelect(ir.CreateICmpSGT(step, ir.getInt64(0)),
        ir.CreateICmpSLT(current, stop),
        ir.CreateAnd(ir.CreateICmpSLT(step, ir.getInt64(0)), ir.CreateICmpSGT(current, stop)));
    ir.CreateCondBr(active, body, end);
    ir.SetInsertPoint(body);
    ir.CreateStore(convert(current, layout.first), item);
    ir.CreateCall(push, {result, item});
    ir.CreateStore(ir.CreateAdd(current, step), counter);
    ir.CreateBr(condition);
    ir.SetInsertPoint(end);
  }
  else if (opcode == "iter.begin") {
    result = builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
    builder_->builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0), result);
  }
  else if (opcode == "iter.has_next") {
    llvm::Function* listLength = module_->getFunction("sere_list_len");
    if (listLength == nullptr) {
      listLength = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt64Ty(*context_),
                                  {llvm::PointerType::getUnqual(*context_)}, false),
          llvm::Function::ExternalLinkage, "sere_list_len", module_.get());
    }
    result = builder_->builder.CreateICmpSLT(
        builder_->builder.CreateLoad(llvm::Type::getInt64Ty(*context_), operand(1)),
        builder_->builder.CreateCall(listLength, {operand(0)}));
  }
  else if (opcode == "iter.next") {
    llvm::Type* indexType = llvm::Type::getInt64Ty(*context_);
    llvm::Function* itemFn = module_->getFunction("sere_list_item");
    if (itemFn == nullptr) {
      itemFn = llvm::Function::Create(
          llvm::FunctionType::get(llvm::PointerType::getUnqual(*context_),
                                  {llvm::PointerType::getUnqual(*context_), indexType}, false),
          llvm::Function::ExternalLinkage, "sere_list_item", module_.get());
    }
    llvm::Value* index = builder_->builder.CreateLoad(indexType, operand(1));
    llvm::Value* slot = builder_->builder.CreateCall(itemFn, {operand(0), index});
    builder_->builder.CreateStore(
        builder_->builder.CreateAdd(index, llvm::ConstantInt::get(indexType, 1)), operand(1));
    if (attribute(operation, "element") == "str") {
      llvm::StructType* stringType = llvm::StructType::get(
          *context_, {llvm::PointerType::getUnqual(*context_), indexType});
      result = builder_->builder.CreateExtractValue(
          builder_->builder.CreateLoad(stringType, slot), {0});
    } else {
      result = builder_->builder.CreateLoad(type, slot);
    }
  }
  else if (opcode == "contains") {
    llvm::Value* left = operand(0);
    llvm::Value* right = operand(1);
    llvm::Value* contained = nullptr;
    if (operands[0]->type().kind() == serem::IRType::Kind::String) {
      llvm::Function* stringLength = module_->getFunction("strlen");
      if (stringLength == nullptr) {
        stringLength = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt64Ty(*context_),
                                    {llvm::PointerType::getUnqual(*context_)}, false),
            llvm::Function::ExternalLinkage, "strlen", module_.get());
      }
      llvm::Function* contains = module_->getFunction("sere_str_contains");
      if (contains == nullptr) {
        contains = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(*context_),
                                    {llvm::PointerType::getUnqual(*context_),
                                     llvm::Type::getInt64Ty(*context_),
                                     llvm::PointerType::getUnqual(*context_),
                                     llvm::Type::getInt64Ty(*context_)}, false),
            llvm::Function::ExternalLinkage, "sere_str_contains", module_.get());
      }
      contained = builder_->builder.CreateICmpNE(
          builder_->builder.CreateCall(contains, {right, builder_->builder.CreateCall(stringLength, {right}),
                                                  left, builder_->builder.CreateCall(stringLength, {left})}),
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
    } else {
      llvm::Function* contains = module_->getFunction("sere_list_contains");
      if (contains == nullptr) {
        contains = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(*context_),
                                    {llvm::PointerType::getUnqual(*context_),
                                     llvm::PointerType::getUnqual(*context_)}, false),
            llvm::Function::ExternalLinkage, "sere_list_contains", module_.get());
      }
      llvm::Value* storage = nullptr;
      if (attribute(operation, "element") == "str") {
        llvm::StructType* stringType = llvm::StructType::get(
            *context_, {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
        storage = builder_->builder.CreateAlloca(stringType);
        builder_->builder.CreateStore(left, builder_->builder.CreateStructGEP(stringType, storage, 0));
        llvm::Function* stringLength = module_->getFunction("strlen");
        if (stringLength == nullptr) {
          stringLength = llvm::Function::Create(
              llvm::FunctionType::get(llvm::Type::getInt64Ty(*context_),
                                      {llvm::PointerType::getUnqual(*context_)}, false),
              llvm::Function::ExternalLinkage, "strlen", module_.get());
        }
        builder_->builder.CreateStore(
            builder_->builder.CreateCall(stringLength, {left}),
            builder_->builder.CreateStructGEP(stringType, storage, 1));
      } else {
        storage = builder_->builder.CreateAlloca(left->getType());
        builder_->builder.CreateStore(left, storage);
      }
      contained = builder_->builder.CreateICmpNE(
          builder_->builder.CreateCall(contains, {right, storage}),
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
    }
    result = attribute(operation, "negated") == "true"
                 ? builder_->builder.CreateNot(contained)
                 : contained;
  }
  else if (opcode == "runtime.input") {
    llvm::Function* input = module_->getFunction("sere_input");
    if (input == nullptr) {
      llvm::FunctionType* inputType = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)},
          false);
      input = llvm::Function::Create(inputType, llvm::Function::ExternalLinkage,
                                     "sere_input", module_.get());
    }
    llvm::Function* length = module_->getFunction("strlen");
    if (length == nullptr) {
      llvm::FunctionType* lengthType = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
      length = llvm::Function::Create(lengthType, llvm::Function::ExternalLinkage,
                                      "strlen", module_.get());
    }
    llvm::Value* prompt = operand(0);
    llvm::Value* data = builder_->builder.CreateAlloca(llvm::PointerType::getUnqual(*context_));
    llvm::Value* lengthValue = builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
    builder_->builder.CreateCall(input, {prompt, builder_->builder.CreateCall(length, {prompt}),
                                         data, lengthValue});
    result = builder_->builder.CreateLoad(llvm::PointerType::getUnqual(*context_), data);
  }
  else if (opcode == "runtime.len") {
    const std::string kind = attribute(operation, "kind");
    const char* runtimeName = kind == "str" ? "strlen" :
                              kind.find("dict[") == 0 ? "sere_dict_len" : "sere_list_len";
    llvm::Function* length = module_->getFunction(runtimeName);
    if (length == nullptr) {
      length = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt64Ty(*context_),
                                  {llvm::PointerType::getUnqual(*context_)}, false),
          llvm::Function::ExternalLinkage, runtimeName, module_.get());
    }
    result = builder_->builder.CreateCall(length, {operand(0)});
  }
  else if (opcode == "builtin.method") {
    const std::string name = attribute(operation, "name");
    llvm::Value* value = operand(0);
    auto runtime = [&](const char* functionName, llvm::Type* returnType,
                       std::initializer_list<llvm::Type*> params) {
      llvm::Function* function = module_->getFunction(functionName);
      if (function == nullptr) {
        function = llvm::Function::Create(
            llvm::FunctionType::get(returnType, params, false), llvm::Function::ExternalLinkage,
            functionName, module_.get());
      }
      return function;
    };
    if (name.starts_with("list.")) {
      auto slot = [&](llvm::Value* item) {
        if (attribute(operation, "element") == "str") {
          llvm::StructType* stringType = llvm::StructType::get(
              *context_, {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
          llvm::Value* storage = builder_->builder.CreateAlloca(stringType);
          builder_->builder.CreateStore(item, builder_->builder.CreateStructGEP(stringType, storage, 0));
          llvm::Function* stringLength = runtime(
              "strlen", llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)});
          builder_->builder.CreateStore(
              builder_->builder.CreateCall(stringLength, {item}),
              builder_->builder.CreateStructGEP(stringType, storage, 1));
          return storage;
        }
        llvm::AllocaInst* storage = builder_->builder.CreateAlloca(item->getType());
        builder_->builder.CreateStore(item, storage);
        return static_cast<llvm::Value*>(storage);
      };
      llvm::Value* item = operands.size() > 1 ? operand(1) : nullptr;
      if (name == "list.append" || name == "list.push") {
        if (item != nullptr) builder_->builder.CreateCall(
            runtime("sere_list_push", llvm::Type::getVoidTy(*context_),
                    {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)}),
            {value, slot(item)});
        return nullptr;
      }
      if (name == "list.clear" || name == "list.reverse") {
        builder_->builder.CreateCall(
            runtime(name == "list.clear" ? "sere_list_clear" : "sere_list_reverse",
                    llvm::Type::getVoidTy(*context_), {llvm::PointerType::getUnqual(*context_)}),
            {value});
        return nullptr;
      }
      if (name == "list.extend") {
        builder_->builder.CreateCall(
            runtime("sere_list_extend", llvm::Type::getVoidTy(*context_),
                    {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)}),
            {value, operand(1)});
        return nullptr;
      }
      if (name == "list.copy" || name == "list.clone") {
        return builder_->builder.CreateCall(
            runtime("sere_list_copy", llvm::PointerType::getUnqual(*context_),
                    {llvm::PointerType::getUnqual(*context_)}), {value});
      }
      if (name == "list.insert") {
        builder_->builder.CreateCall(
            runtime("sere_list_insert", llvm::Type::getVoidTy(*context_),
                    {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
                     llvm::PointerType::getUnqual(*context_)}),
            {value, operand(1), slot(operand(2))});
        return nullptr;
      }
      if (name == "list.remove" || name == "list.find" || name == "list.index" ||
          name == "list.count" || name == "list.contains" || name == "list.has") {
        const char* functionName = name == "list.remove" ? "sere_list_remove_value"
                                  : name == "list.find" || name == "list.index" ? "sere_list_index_of"
                                  : name == "list.count" ? "sere_list_count" : "sere_list_contains";
        llvm::Type* returnType = name == "list.remove" || name == "list.contains" || name == "list.has"
                                     ? llvm::Type::getInt32Ty(*context_)
                                     : llvm::Type::getInt64Ty(*context_);
        llvm::Value* call = builder_->builder.CreateCall(
            runtime(functionName, returnType,
                    {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)}),
            {value, slot(item)});
        return (name == "list.remove" || name == "list.contains" || name == "list.has")
                   ? builder_->builder.CreateICmpNE(call, llvm::ConstantInt::get(returnType, 0))
                   : call;
      }
      if (name == "list.pop") {
        llvm::AllocaInst* output = builder_->builder.CreateAlloca(type);
        if (operands.size() == 1) {
          builder_->builder.CreateCall(
              runtime("sere_list_pop", llvm::Type::getVoidTy(*context_),
                      {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)}),
              {value, output});
        } else {
          builder_->builder.CreateCall(
              runtime("sere_list_pop_at", llvm::Type::getVoidTy(*context_),
                      {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
                       llvm::PointerType::getUnqual(*context_)}),
              {value, operand(1), output});
        }
        return builder_->builder.CreateLoad(type, output);
      }
    }
    llvm::Function* length = module_->getFunction("strlen");
    if (length == nullptr) {
      llvm::FunctionType* lengthType = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
      length = llvm::Function::Create(lengthType, llvm::Function::ExternalLinkage,
                                      "strlen", module_.get());
    }
    llvm::Value* valueLength = builder_->builder.CreateCall(length, {value});
    auto outputString = [&](llvm::Function* function, std::vector<llvm::Value*> args) {
      llvm::Value* data = builder_->builder.CreateAlloca(
          llvm::PointerType::getUnqual(*context_));
      llvm::Value* outputLength =
          builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
      args.push_back(data);
      args.push_back(outputLength);
      builder_->builder.CreateCall(function, args);
      return builder_->builder.CreateLoad(llvm::PointerType::getUnqual(*context_), data);
    };
    auto stringArgument = [&](std::size_t index) {
      llvm::Value* argument = operand(index);
      return std::array<llvm::Value*, 2>{argument,
                                         builder_->builder.CreateCall(length, {argument})};
    };
    if (name == "str.upper" || name == "str.lower" || name == "str.strip" ||
        name == "str.lstrip" || name == "str.rstrip" || name == "str.capitalize" ||
        name == "str.title") {
      const std::string runtimeName = "sere_string_" + name.substr(4);
      llvm::Function* function = runtime(
          runtimeName.c_str(), llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)});
      result = outputString(function, {value, valueLength});
    } else if (name == "str.starts_with" || name == "str.startswith" ||
               name == "str.ends_with" || name == "str.endswith" ||
               name == "str.contains" || name == "str.has") {
      const auto argument = stringArgument(1);
      const std::string runtimeName =
          name == "str.contains" || name == "str.has"
              ? "sere_str_contains"
              : (name.find("ends") != std::string::npos ? "sere_string_ends_with"
                                                           : "sere_string_starts_with");
      llvm::Function* function = runtime(
          runtimeName.c_str(), llvm::Type::getInt32Ty(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
      result = builder_->builder.CreateICmpNE(
          builder_->builder.CreateCall(function, {value, valueLength, argument[0], argument[1]}),
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
    } else if (name == "str.find" || name == "str.rfind" || name == "str.count") {
      const auto argument = stringArgument(1);
      const char* runtimeName = name == "str.find" ? "sere_string_find"
                              : name == "str.rfind" ? "sere_string_rfind"
                                                     : "sere_string_count";
      llvm::Function* function = runtime(
          runtimeName, llvm::Type::getInt64Ty(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
      result = builder_->builder.CreateCall(function, {value, valueLength, argument[0], argument[1]});
    } else if (name == "str.replace") {
      const auto oldString = stringArgument(1);
      const auto newString = stringArgument(2);
      llvm::Function* function = runtime(
          "sere_string_replace", llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)});
      result = outputString(function, {value, valueLength, oldString[0], oldString[1],
                                       newString[0], newString[1]});
    } else if (name == "str.split") {
      const auto separator = stringArgument(1);
      llvm::Function* function = runtime(
          "sere_string_split", llvm::PointerType::getUnqual(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
      result = builder_->builder.CreateCall(
          function, {value, valueLength, separator[0], separator[1]});
    } else if (name == "str.join") {
      llvm::Function* function = runtime(
          "sere_string_join", llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_),
           llvm::PointerType::getUnqual(*context_)});
      result = outputString(function, {value, valueLength, operand(1)});
    } else if (name == "str.repeat") {
      llvm::Function* function = runtime(
          "sere_string_repeat", llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_),
           llvm::Type::getInt64Ty(*context_), llvm::PointerType::getUnqual(*context_),
           llvm::PointerType::getUnqual(*context_)});
      llvm::Value* count = operand(1);
      if (count->getType() != llvm::Type::getInt64Ty(*context_)) {
        count = builder_->builder.CreateSExt(count, llvm::Type::getInt64Ty(*context_));
      }
      result = outputString(function, {value, valueLength, count});
    } else if (name == "str.is_empty" || name == "str.is_digit" ||
               name == "str.is_alpha" || name == "str.is_space") {
      const std::string runtimeName = "sere_string_" + name.substr(4);
      llvm::Function* function = runtime(
          runtimeName.c_str(), llvm::Type::getInt32Ty(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)});
      result = builder_->builder.CreateICmpNE(
          builder_->builder.CreateCall(function, {value, valueLength}),
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
    } else {
      report("unsupported Serem string builtin method: " + name);
    }
  }
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
        if (operands[index]->type().kind() == serem::IRType::Kind::String) {
          builder_->builder.CreateCall(print, {value, builder_->builder.CreateCall(length, {value})});
          stringPrinted = true;
        } else {
          llvm::Function* write = module_->getFunction("sere_write_ptr");
          if (write == nullptr) {
            llvm::FunctionType* writeType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
            write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                           "sere_write_ptr", module_.get());
          }
          builder_->builder.CreateCall(write, {value});
        }
      } else if (value->getType()->isIntegerTy(32)) {
        llvm::Function* write = module_->getFunction("sere_write_i32");
        if (write == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt32Ty(*context_)}, false);
          write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                         "sere_write_i32", module_.get());
        }
        builder_->builder.CreateCall(write, {value});
      } else if (value->getType()->isIntegerTy(64)) {
        llvm::Function* write = module_->getFunction("sere_write_i64");
        if (write == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt64Ty(*context_)}, false);
          write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                         "sere_write_i64", module_.get());
        }
        builder_->builder.CreateCall(write, {value});
      } else if (value->getType()->isIntegerTy(1)) {
        llvm::Function* write = module_->getFunction("sere_write_bool");
        if (write == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt8Ty(*context_)}, false);
          write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                         "sere_write_bool", module_.get());
        }
        builder_->builder.CreateCall(
            write, {builder_->builder.CreateZExt(value, llvm::Type::getInt8Ty(*context_))});
      } else if (value->getType()->isFloatTy() || value->getType()->isDoubleTy()) {
        llvm::Function* write = module_->getFunction("sere_write_f64");
        if (write == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getDoubleTy(*context_)}, false);
          write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                         "sere_write_f64", module_.get());
        }
        if (value->getType()->isFloatTy()) {
          value = builder_->builder.CreateFPExt(value, llvm::Type::getDoubleTy(*context_));
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
  else if (opcode == "shared.new" || opcode == "unique.new") {
    llvm::Function* alloc = module_->getFunction("sere_alloc");
    if (alloc == nullptr) {
      llvm::FunctionType* allocType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context_), {llvm::Type::getInt64Ty(*context_)}, false);
      alloc = llvm::Function::Create(allocType, llvm::Function::ExternalLinkage,
                                     "sere_alloc", module_.get());
    }
    result = builder_->builder.CreateCall(
        alloc, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 8)});
    if (!operands.empty()) builder_->builder.CreateStore(operand(0), result);
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
      if (!value->getType()->isPointerTy()) {
        llvm::Function* stringify = nullptr;
        if (value->getType()->isIntegerTy(32)) {
          stringify = module_->getFunction("sere_str_i32_data");
          if (stringify == nullptr) {
            llvm::FunctionType* type = llvm::FunctionType::get(
                llvm::PointerType::getUnqual(*context_),
                {llvm::Type::getInt32Ty(*context_), llvm::PointerType::getUnqual(*context_)}, false);
            stringify = llvm::Function::Create(type, llvm::Function::ExternalLinkage,
                                               "sere_str_i32_data", module_.get());
          }
        } else if (value->getType()->isIntegerTy(64)) {
          stringify = module_->getFunction("sere_str_i64_data");
          if (stringify == nullptr) {
            llvm::FunctionType* type = llvm::FunctionType::get(
                llvm::PointerType::getUnqual(*context_),
                {llvm::Type::getInt64Ty(*context_), llvm::PointerType::getUnqual(*context_)}, false);
            stringify = llvm::Function::Create(type, llvm::Function::ExternalLinkage,
                                               "sere_str_i64_data", module_.get());
          }
        }
        if (stringify != nullptr) {
          llvm::AllocaInst* lengthSlot =
              builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
          value = builder_->builder.CreateCall(stringify, {value, lengthSlot});
          llvm::Value* valueLength =
              builder_->builder.CreateLoad(llvm::Type::getInt64Ty(*context_), lengthSlot);
          if (current == nullptr) {
            current = value;
            currentLength = valueLength;
          } else {
            llvm::AllocaInst* outLength =
                builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
            current = builder_->builder.CreateCall(
                concat, {current, currentLength, value, valueLength, outLength});
            currentLength = builder_->builder.CreateLoad(
                llvm::Type::getInt64Ty(*context_), outLength);
          }
          continue;
        }
      }
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
  else if (opcode == "value.repr") {
    if (!attribute(operation, "element.name").empty()) {
      llvm::Value* callback = llvm::ConstantPointerNull::get(ir.getPtrTy());
      const auto repr = functions_.find(attribute(operation, "element.repr"));
      if (repr != functions_.end()) callback = repr->second;
      auto format = module_->getOrInsertFunction("sere_list_object_repr_data", ir.getPtrTy(),
                                                ir.getPtrTy(), ir.getPtrTy(), ir.getPtrTy());
      result = ir.CreateCall(format, {operand(0), callback,
                                     ir.CreateGlobalString(attribute(operation, "element.name"))});
      values_[&operation] = result;
      return result;
    }
    llvm::Function* repr = module_->getFunction("sere_list_repr_data");
    if (repr == nullptr) {
      llvm::FunctionType* reprType = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt32Ty(*context_),
           llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)},
          false);
      repr = llvm::Function::Create(reprType, llvm::Function::ExternalLinkage,
                                    "sere_list_repr_data", module_.get());
    }
    llvm::Value* data = builder_->builder.CreateAlloca(llvm::PointerType::getUnqual(*context_));
    llvm::Value* length = builder_->builder.CreateAlloca(llvm::Type::getInt64Ty(*context_));
    builder_->builder.CreateCall(
        repr, {operand(0),
               llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_),
                                      elementKindCode(attribute(operation, "element.kind"))),
               data, length});
    result = builder_->builder.CreateLoad(llvm::PointerType::getUnqual(*context_), data);
  }
  else if (opcode == "aggregate.list") {
    const std::int32_t elementKind = elementKindCode(attribute(operation, "element.kind"));
    const std::pair<llvm::Type*, std::int64_t> layout = listElementLayout(*context_, elementKind);
    llvm::Function* create = module_->getFunction("sere_list_new");
    if (create == nullptr) {
      llvm::FunctionType* createType = llvm::FunctionType::get(
          llvm::PointerType::getUnqual(*context_), {llvm::Type::getInt64Ty(*context_)}, false);
      create = llvm::Function::Create(createType, llvm::Function::ExternalLinkage,
                                      "sere_list_new", module_.get());
    }
    result = builder_->builder.CreateCall(
        create, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), layout.second)});
    if (elementKind == 0) {
      llvm::Function* push = module_->getFunction("sere_list_str_push");
      if (push == nullptr) {
        llvm::FunctionType* pushType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context_),
            {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_),
             llvm::Type::getInt64Ty(*context_)},
            false);
        push = llvm::Function::Create(pushType, llvm::Function::ExternalLinkage,
                                      "sere_list_str_push", module_.get());
      }
      llvm::Function* length = module_->getFunction("strlen");
      if (length == nullptr) {
        llvm::FunctionType* lengthType = llvm::FunctionType::get(
            llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
        length = llvm::Function::Create(lengthType, llvm::Function::ExternalLinkage,
                                        "strlen", module_.get());
      }
      for (std::size_t index = 0; index < operands.size(); ++index) {
        llvm::Value* item = operand(index);
        if (item == nullptr) continue;
        builder_->builder.CreateCall(push, {result, item, builder_->builder.CreateCall(length, {item})});
      }
    } else if (!operands.empty()) {
      llvm::Function* push = module_->getFunction("sere_list_push");
      if (push == nullptr) {
        llvm::FunctionType* pushType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context_),
            {llvm::PointerType::getUnqual(*context_), llvm::PointerType::getUnqual(*context_)},
            false);
        push = llvm::Function::Create(pushType, llvm::Function::ExternalLinkage,
                                      "sere_list_push", module_.get());
      }
      llvm::Type* elementType = layout.first;
      for (std::size_t index = 0; index < operands.size(); ++index) {
        llvm::Value* item = operand(index);
        if (item == nullptr) continue;
        if (item->getType()->isIntegerTy(1) && elementType->isIntegerTy(8)) {
          item = builder_->builder.CreateZExt(item, elementType);
        } else if (item->getType()->isIntegerTy() && elementType->isIntegerTy() &&
                   item->getType() != elementType) {
          item = builder_->builder.CreateIntCast(item, elementType, true);
        } else if (item->getType()->isFloatingPointTy() && elementType->isFloatingPointTy() &&
                   item->getType() != elementType) {
          item = elementType->isDoubleTy() ? builder_->builder.CreateFPExt(item, elementType)
                                           : builder_->builder.CreateFPTrunc(item, elementType);
        }
        llvm::AllocaInst* slot = builder_->builder.CreateAlloca(elementType);
        builder_->builder.CreateStore(item, slot);
        builder_->builder.CreateCall(push, {result, slot});
      }
    }
  }
  else if (opcode == "construct") {
    if (type->isPointerTy() && operation.type().pointee() != nullptr &&
        operation.type().pointee()->kind() == serem::IRType::Kind::Struct) {
      llvm::Type* record = lowerType(*operation.type().pointee());
      auto alloc = module_->getOrInsertFunction("sere_alloc", ir.getPtrTy(), ir.getInt64Ty());
      result = ir.CreateCall(alloc, {llvm::ConstantExpr::getSizeOf(record)});
      ir.CreateStore(llvm::Constant::getNullValue(record), result);
      const auto init = functions_.find(attribute(operation, "init"));
      if (init != functions_.end()) {
        std::vector<llvm::Value*> args{result};
        for (std::size_t index = 0; index < operands.size(); ++index) {
          args.push_back(convert(operand(index), init->second->getFunctionType()->getParamType(index + 1)));
        }
        ir.CreateCall(init->second, args);
      }
    } else if (type->isStructTy()) {
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
      } else if (object != nullptr && operands[0]->type().pointee() != nullptr) {
        llvm::Type* record = lowerType(*operands[0]->type().pointee());
        result = ir.CreateLoad(type, ir.CreateStructGEP(record, object, index));
      }
    }
  } else if (opcode == "member.set") {
    unsigned index = 0;
    const std::string indexText = attribute(operation, "index");
    (void)std::from_chars(indexText.data(), indexText.data() + indexText.size(), index);
    if (operands[0]->type().pointee() != nullptr) {
      llvm::Type* record = lowerType(*operands[0]->type().pointee());
      ir.CreateStore(convert(operand(1), record->getStructElementType(index)),
                     ir.CreateStructGEP(record, operand(0), index));
    } else {
      report("Serem field assignment requires object storage");
    }
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
    llvm::Value* externalString = nullptr;
    if (function != nullptr && externalSymbols_.contains(function->getName().str())) {
      std::vector<llvm::Value*> expanded;
      const auto& signature = operands[0]->type();
      for (std::size_t index = 0; index < arguments.size(); ++index) {
        expanded.push_back(arguments[index]);
        if (index < signature.parameters().size() &&
            signature.parameters()[index].kind() == serem::IRType::Kind::String) {
          auto length = module_->getOrInsertFunction("strlen", ir.getInt64Ty(), ir.getPtrTy());
          expanded.push_back(ir.CreateCall(length, {arguments[index]}));
        }
      }
      if (operation.type().kind() == serem::IRType::Kind::String) {
        externalString = ir.CreateAlloca(ir.getPtrTy());
        expanded.push_back(externalString);
        expanded.push_back(ir.CreateAlloca(ir.getInt64Ty()));
      }
      arguments = std::move(expanded);
    }
    if (function != nullptr) {
      if (arguments.size() != function->arg_size()) {
        report("Serem call argument count mismatch for " + function->getName().str());
      } else {
        for (std::size_t index = 0; index < arguments.size(); ++index)
          arguments[index] = convert(arguments[index], function->getFunctionType()->getParamType(index));
        result = builder_->builder.CreateCall(function, arguments);
        if (externalString != nullptr) result = ir.CreateLoad(ir.getPtrTy(), externalString);
      }
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
    if (operands.empty() && currentFunctionName_ == "main") {
      builder_->builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
    } else if (operands.empty()) builder_->builder.CreateRetVoid();
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
    result = convert(operand(0), type, attribute(operation, "unsigned") != "true");
  } else if (opcode == "union.pack") {
    int tag = 0;
    const std::string text = attribute(operation, "tag");
    (void)std::from_chars(text.data(), text.data() + text.size(), tag);
    llvm::Value* payload = operand(0);
    if (payload->getType()->isPointerTy()) payload = ir.CreatePtrToInt(payload, ir.getInt64Ty());
    else if (payload->getType()->isIntegerTy()) payload = ir.CreateZExtOrTrunc(payload, ir.getInt64Ty());
    else if (payload->getType()->isFloatingPointTy()) {
      payload = ir.CreateBitCast(payload, ir.getIntNTy(payload->getType()->getPrimitiveSizeInBits()));
      payload = ir.CreateZExtOrTrunc(payload, ir.getInt64Ty());
    } else {
      auto alloc = module_->getOrInsertFunction("sere_alloc", ir.getPtrTy(), ir.getInt64Ty());
      llvm::Value* memory = ir.CreateCall(alloc, {llvm::ConstantExpr::getSizeOf(payload->getType())});
      ir.CreateStore(payload, memory);
      payload = ir.CreatePtrToInt(memory, ir.getInt64Ty());
    }
    result = ir.CreateInsertValue(llvm::UndefValue::get(type), ir.getInt32(tag), {0});
    result = ir.CreateInsertValue(result, payload, {1});
  } else if (opcode == "union.extract") {
    llvm::Value* payload = ir.CreateExtractValue(operand(0), {1});
    if (type->isPointerTy()) result = ir.CreateIntToPtr(payload, type);
    else if (type->isIntegerTy()) result = ir.CreateTruncOrBitCast(payload, type);
    else if (type->isFloatingPointTy()) {
      result = ir.CreateBitCast(ir.CreateTruncOrBitCast(payload,
          ir.getIntNTy(type->getPrimitiveSizeInBits())), type);
    } else result = ir.CreateLoad(type, ir.CreateIntToPtr(payload, ir.getPtrTy()));
  } else if (opcode == "union.is") {
    int tag = -1;
    const std::string text = attribute(operation, "tag");
    (void)std::from_chars(text.data(), text.data() + text.size(), tag);
    result = ir.CreateICmpEQ(ir.CreateExtractValue(operand(0), {0}), ir.getInt32(tag));
    if (attribute(operation, "negated") == "true") result = ir.CreateNot(result);
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
  if (result == nullptr && !operation.type().isVoid()) {
    report("unsupported Serem operation: " + opcode);
    result = llvm::UndefValue::get(type);
  }
  if (!operation.resultName().empty()) values_[&operation] = result;
  return result;
}

void SeremLLVMBackend::emitEntryPoint(const serem::IRFunction& userMain,
                                      llvm::Function* userEntry) {
  if (userEntry == nullptr) {
    return;
  }
  if (userMain.parameters().size() > 1) {
    report("Serem entry point accepts at most one list[str] parameter");
    return;
  }
  llvm::Type* pointerType = llvm::PointerType::getUnqual(*context_);
  llvm::Type* countType = llvm::Type::getInt32Ty(*context_);
  llvm::Function* wrapper = llvm::Function::Create(
      llvm::FunctionType::get(countType, {countType, pointerType}, false),
      llvm::Function::ExternalLinkage, "main", module_.get());
  llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*context_, "entry", wrapper);
  builder_->builder.SetInsertPoint(entryBlock);
  std::vector<llvm::Value*> arguments;
  // `main(argv: list[str])` receives the process arguments as a list, which the
  // runtime builds from the platform's argc/argv pair.
  if (!userMain.parameters().empty()) {
    llvm::Function* fromArgv = module_->getFunction("sere_list_from_argv");
    if (fromArgv == nullptr) {
      fromArgv = llvm::Function::Create(
          llvm::FunctionType::get(pointerType, {countType, pointerType}, false),
          llvm::Function::ExternalLinkage, "sere_list_from_argv", module_.get());
    }
    arguments.push_back(builder_->builder.CreateCall(
        fromArgv, {wrapper->getArg(0), wrapper->getArg(1)}));
  }
  llvm::Value* result = builder_->builder.CreateCall(userEntry, arguments);
  if (result->getType()->isVoidTy()) {
    builder_->builder.CreateRet(llvm::ConstantInt::get(countType, 0));
  } else {
    builder_->builder.CreateRet(result);
  }
  functions_.insert_or_assign("main", wrapper);
}

std::unique_ptr<llvm::Module> SeremLLVMBackend::emit(const serem::IRModule& module,
                                                     const std::string& moduleName) {
  module_ = std::make_unique<llvm::Module>(moduleName, *context_);
  structs_.clear();
  functions_.clear();
  blocks_.clear();
  values_.clear();
  externalSymbols_.clear();
  for (const auto& function : module.functions())
    if (function->isExternal()) externalSymbols_.insert(function->name());
  for (const auto& type : module.types()) (void)lowerType(type->type());
  // The user's `main` is renamed so the synthesised platform entry point can
  // own the `main` symbol, the same split the LLVM backend performs in
  // emitCMainWrapper.
  const serem::IRFunction* userMain = nullptr;
  for (const auto& function : module.functions()) {
    if (!function->isExternal() && function->name() == "main") {
      userMain = function.get();
      break;
    }
  }
  for (const auto& function : module.functions()) {
    // External declarations are materialized lazily by ensureExternal when a
    // call actually references them, so unrelated externs (such as runtime
    // intrinsics the frontend lowers to dedicated operations) never emit a
    // declaration whose signature could clash with the runtime library.
    if (function->isExternal()) continue;
    std::vector<llvm::Type*> params;
    for (const serem::IRType& param : function->parameters()) params.push_back(lowerType(param));
    llvm::Type* resultType = lowerType(function->resultType());
    if (function->name() == "main" && function->resultType().isVoid()) {
      resultType = llvm::Type::getInt32Ty(*context_);
    }
    const std::string symbol =
        userMain != nullptr && function->name() == "main" ? kEntrySymbol : function->name();
    auto* functionType = llvm::FunctionType::get(resultType, params, false);
    functions_[function->name()] = llvm::Function::Create(
        functionType, llvm::Function::ExternalLinkage, symbol, module_.get());
  }
  for (const auto& function : module.functions()) {
    if (function->isExternal()) continue;
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
      for (const auto& operation : block->operations()) {
        (void)lowerOperation(*operation);
        if (builder_->builder.GetInsertBlock()->getTerminator() != nullptr) break;
      }
      if (builder_->builder.GetInsertBlock()->getTerminator() == nullptr && block->isTerminated()) {
        builder_->builder.CreateUnreachable();
      } else if (builder_->builder.GetInsertBlock()->getTerminator() == nullptr) {
        if (function->name() == "main" && function->resultType().isVoid()) {
          builder_->builder.CreateRet(
              llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
        } else if (function->resultType().isVoid()) builder_->builder.CreateRetVoid();
        else builder_->builder.CreateRet(llvm::UndefValue::get(lowerType(function->resultType())));
      }
    }
  }
  if (userMain != nullptr) {
    emitEntryPoint(*userMain, functions_["main"]);
  }
  std::string verificationError;
  llvm::raw_string_ostream errors(verificationError);
  if (llvm::verifyModule(*module_, &errors)) {
    report("invalid Serem LLVM module: " + verificationError);
    return nullptr;
  }
  return std::move(module_);
}

} // namespace sere
