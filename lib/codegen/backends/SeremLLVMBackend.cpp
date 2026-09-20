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

/// True when the Serem integer kind is unsigned, so a widening cast zero-extends
/// instead of sign-extending it.
[[nodiscard]] bool isUnsignedKind(serem::IRType::Kind kind) {
  switch (kind) {
  case serem::IRType::Kind::U8:
  case serem::IRType::Kind::U16:
  case serem::IRType::Kind::U32:
  case serem::IRType::Kind::U64: return true;
  default: return false;
  }
}

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

llvm::Function* SeremLLVMBackend::coroutineIntrinsic(unsigned id) {
  return llvm::Intrinsic::getOrInsertDeclaration(
      module_.get(), static_cast<llvm::Intrinsic::ID>(id));
}

void SeremLLVMBackend::resetCoroutine() {
  coroPromise_ = nullptr;
  coroId_ = nullptr;
  coroHdl_ = nullptr;
  coroMem_ = nullptr;
  coroIterator_ = nullptr;
  coroCleanup_ = nullptr;
  coroSuspendBlock_ = nullptr;
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
  /// Storage for a `@static` field. Statics have no slot in the instance layout,
  /// so every one of them lives in a word-sized module global keyed by the
  /// field path the generator names.
  auto staticSlot = [&](const std::string& field) -> llvm::GlobalVariable* {
    const std::string name = "sere.static." + field;
    if (llvm::GlobalVariable* existing = module_->getGlobalVariable(name)) {
      return existing;
    }
    return new llvm::GlobalVariable(
        *module_, ir.getInt64Ty(), /*isConstant=*/false, llvm::GlobalValue::InternalLinkage,
        llvm::ConstantInt::get(ir.getInt64Ty(), 0), name);
  };
  /// Bits a value travels through a static slot as.
  auto toWord = [&](llvm::Value* value) -> llvm::Value* {
    if (value == nullptr) {
      return llvm::ConstantInt::get(ir.getInt64Ty(), 0);
    }
    llvm::Type* valueType = value->getType();
    if (valueType->isPointerTy()) {
      return ir.CreatePtrToInt(value, ir.getInt64Ty());
    }
    if (valueType->isDoubleTy()) {
      return ir.CreateBitCast(value, ir.getInt64Ty());
    }
    if (valueType->isFloatTy()) {
      return ir.CreateBitCast(ir.CreateFPExt(value, ir.getDoubleTy()), ir.getInt64Ty());
    }
    if (valueType->isIntegerTy(1)) {
      return ir.CreateZExt(value, ir.getInt64Ty());
    }
    if (valueType->isIntegerTy() && valueType->getIntegerBitWidth() < 64) {
      return ir.CreateSExt(value, ir.getInt64Ty());
    }
    return value;
  };
  /// Inverse of `toWord` for a known destination type.
  auto fromWord = [&](llvm::Value* word, llvm::Type* wanted) -> llvm::Value* {
    if (wanted->isPointerTy()) {
      return ir.CreateIntToPtr(word, wanted);
    }
    if (wanted->isDoubleTy()) {
      return ir.CreateBitCast(word, wanted);
    }
    if (wanted->isFloatTy()) {
      return ir.CreateFPTrunc(ir.CreateBitCast(word, ir.getDoubleTy()), wanted);
    }
    if (wanted->isIntegerTy() && wanted->getIntegerBitWidth() < 64) {
      return ir.CreateTrunc(word, wanted);
    }
    return word;
  };
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
  // Converts both operands to a shared type before a binary operation. Integer
  // operands widen to the larger width, mixed integer/float operands promote the
  // integer to the float, and mixed float widths promote to the wider one. Each
  // operand's own Serem type decides whether an integer extension sign- or
  // zero-extends.
  auto binary = [&](std::size_t leftIndex,
                    std::size_t rightIndex) -> std::pair<llvm::Value*, llvm::Value*> {
    llvm::Value* left = operand(leftIndex);
    llvm::Value* right = operand(rightIndex);
    if (left == nullptr || right == nullptr || left->getType() == right->getType()) {
      return {left, right};
    }
    const bool leftUnsigned =
        leftIndex < operands.size() && isUnsignedKind(operands[leftIndex]->type().kind());
    const bool rightUnsigned =
        rightIndex < operands.size() && isUnsignedKind(operands[rightIndex]->type().kind());
    llvm::Type* leftType = left->getType();
    llvm::Type* rightType = right->getType();
    if (leftType->isIntegerTy() && rightType->isIntegerTy()) {
      llvm::Type* common = ir.getIntNTy(std::max(leftType->getIntegerBitWidth(),
                                                 rightType->getIntegerBitWidth()));
      left = convert(left, common, !leftUnsigned);
      right = convert(right, common, !rightUnsigned);
    } else if (leftType->isFloatingPointTy() && rightType->isFloatingPointTy()) {
      llvm::Type* common = leftType->isDoubleTy() || rightType->isDoubleTy() ? ir.getDoubleTy()
                                                                             : ir.getFloatTy();
      left = convert(left, common, true);
      right = convert(right, common, true);
    } else if (leftType->isIntegerTy() && rightType->isFloatingPointTy()) {
      left = convert(left, rightType, !leftUnsigned);
    } else if (leftType->isFloatingPointTy() && rightType->isIntegerTy()) {
      right = convert(right, leftType, !rightUnsigned);
    }
    return {left, right};
  };
  if (opcode == "heap.alloc") {
    auto alloc = module_->getOrInsertFunction("sere_alloc", ir.getPtrTy(), ir.getInt64Ty());
    result = ir.CreateCall(alloc, {llvm::ConstantExpr::getSizeOf(lowerType(*operation.type().pointee()))});
  } else if (opcode == "heap.free") {
    ir.CreateCall(module_->getOrInsertFunction("sere_free", ir.getVoidTy(), ir.getPtrTy()), {operand(0)});
  } else if (opcode.starts_with("any.")) {
    auto* boxType = llvm::StructType::get(*context_, {ir.getInt32Ty(), ir.getPtrTy(), ir.getPtrTy()});
    std::uint32_t tag = 0;
    const std::string tagText = attribute(operation, "tag");
    (void)std::from_chars(tagText.data(), tagText.data() + tagText.size(), tag);
    if (opcode == "any.box") {
      auto alloc = module_->getOrInsertFunction("sere_alloc", ir.getPtrTy(), ir.getInt64Ty());
      llvm::Value* value = operand(0);
      llvm::Value* data = ir.CreateCall(alloc, {llvm::ConstantExpr::getSizeOf(value->getType())});
      ir.CreateStore(value, data);
      result = ir.CreateCall(alloc, {llvm::ConstantExpr::getSizeOf(boxType)});
      ir.CreateStore(ir.getInt32(tag), ir.CreateStructGEP(boxType, result, 0));
      ir.CreateStore(data, ir.CreateStructGEP(boxType, result, 1));
      ir.CreateStore(ir.CreateGlobalString(attribute(operation, "name")), ir.CreateStructGEP(boxType, result, 2));
    } else if (opcode == "any.is") {
      result = ir.CreateICmpEQ(ir.CreateLoad(ir.getInt32Ty(), ir.CreateStructGEP(boxType, operand(0), 0)), ir.getInt32(tag));
    } else if (opcode == "any.name") {
      result = ir.CreateLoad(ir.getPtrTy(), ir.CreateStructGEP(boxType, operand(0), 2));
    } else {
      llvm::Value* data = ir.CreateLoad(ir.getPtrTy(), ir.CreateStructGEP(boxType, operand(0), 1));
      result = ir.CreateLoad(type, data);
    }
  } else if (opcode == "static.get" || opcode == "static.set") {
    const std::string name = "sere.static." + attribute(operation, "symbol");
    llvm::Type* fieldType = opcode == "static.get" ? type : lowerType(operands[0]->type());
    // Include internal globals in the lookup: every read and write must use the
    // same storage, including when a read is emitted before the first write.
    llvm::GlobalVariable* slot = module_->getGlobalVariable(name, true);
    if (slot == nullptr) {
      slot = new llvm::GlobalVariable(*module_, fieldType, /*isConstant=*/false,
                                      llvm::GlobalValue::InternalLinkage,
                                      llvm::Constant::getNullValue(fieldType), name);
    }
    if (opcode == "static.get") {
      result = ir.CreateLoad(fieldType, slot);
    } else {
      ir.CreateStore(operand(0), slot);
    }
  }
  else if (opcode == "pointer.null") result = llvm::ConstantPointerNull::get(ir.getPtrTy());
  else if (opcode == "pointer.is_null") {
    result = ir.CreateIsNull(operand(0));
    if (attribute(operation, "negated") == "true") result = ir.CreateNot(result);
  }
  else if (opcode == "add" || opcode == "fadd" || opcode == "sub" || opcode == "fsub" ||
           opcode == "mul" || opcode == "fmul" || opcode == "div" || opcode == "fdiv" ||
           opcode == "rem" || opcode == "and" || opcode == "or" || opcode == "xor" ||
           opcode == "shl" || opcode == "shr") {
    auto [left, right] = binary(0, 1);
    if (left != nullptr && right != nullptr) {
      const bool floating = left->getType()->isFloatingPointTy();
      if (opcode == "add" || opcode == "fadd")
        result = floating ? builder_->builder.CreateFAdd(left, right)
                          : builder_->builder.CreateAdd(left, right);
      else if (opcode == "sub" || opcode == "fsub")
        result = floating ? builder_->builder.CreateFSub(left, right)
                          : builder_->builder.CreateSub(left, right);
      else if (opcode == "mul" || opcode == "fmul")
        result = floating ? builder_->builder.CreateFMul(left, right)
                          : builder_->builder.CreateMul(left, right);
      else if (opcode == "div" || opcode == "fdiv")
        result = floating ? builder_->builder.CreateFDiv(left, right)
                          : builder_->builder.CreateSDiv(left, right);
      else if (opcode == "rem")
        result = floating ? builder_->builder.CreateFRem(left, right)
                          : builder_->builder.CreateSRem(left, right);
      else if (opcode == "and") result = builder_->builder.CreateAnd(left, right);
      else if (opcode == "or") result = builder_->builder.CreateOr(left, right);
      else if (opcode == "xor") result = builder_->builder.CreateXor(left, right);
      else if (opcode == "shl") result = builder_->builder.CreateShl(left, right);
      else if (opcode == "shr") result = builder_->builder.CreateAShr(left, right);
    }
  }
  else if (opcode.starts_with("cmp.")) {
    const std::string predicate = opcode.substr(4);
    auto [left, right] = binary(0, 1);
    if (operands[0]->type().kind() == serem::IRType::Kind::String &&
        operands[1]->type().kind() == serem::IRType::Kind::String) {
      auto compare = module_->getOrInsertFunction("strcmp", ir.getInt32Ty(), ir.getPtrTy(), ir.getPtrTy());
      left = ir.CreateCall(compare, {left, right});
      right = ir.getInt32(0);
    }
    if (left != nullptr && right != nullptr) {
      // `None` lowers to a zero of its own type, so a null test against a
      // pointer arrives as a pointer/integer pair. Compare them as pointers.
      if (left->getType()->isPointerTy() != right->getType()->isPointerTy()) {
        if (left->getType()->isPointerTy()) {
          right = ir.CreateIntToPtr(convert(right, ir.getInt64Ty()), left->getType());
        } else {
          left = ir.CreateIntToPtr(convert(left, ir.getInt64Ty()), right->getType());
        }
      }
      const bool floating = left->getType()->isFloatingPointTy();
      llvm::CmpInst::Predicate comparison =
          floating ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ;
      if (predicate == "ne")
        comparison = floating ? llvm::CmpInst::FCMP_UNE : llvm::CmpInst::ICMP_NE;
      else if (predicate == "lt")
        comparison = floating ? llvm::CmpInst::FCMP_OLT : llvm::CmpInst::ICMP_SLT;
      else if (predicate == "le")
        comparison = floating ? llvm::CmpInst::FCMP_OLE : llvm::CmpInst::ICMP_SLE;
      else if (predicate == "gt")
        comparison = floating ? llvm::CmpInst::FCMP_OGT : llvm::CmpInst::ICMP_SGT;
      else if (predicate == "ge")
        comparison = floating ? llvm::CmpInst::FCMP_OGE : llvm::CmpInst::ICMP_SGE;
      result = floating ? builder_->builder.CreateFCmp(comparison, left, right)
                        : builder_->builder.CreateICmp(comparison, left, right);
    }
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
    llvm::Function* write = module_->getFunction("sere_write");
    if (write == nullptr) {
      llvm::FunctionType* writeType = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*context_),
          {llvm::PointerType::getUnqual(*context_), llvm::Type::getInt64Ty(*context_)}, false);
      write = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                     "sere_write", module_.get());
    }
    llvm::Function* length = module_->getFunction("strlen");
    if (length == nullptr) {
      llvm::FunctionType* lengthType = llvm::FunctionType::get(
          llvm::Type::getInt64Ty(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
      length = llvm::Function::Create(lengthType, llvm::Function::ExternalLinkage,
                                      "strlen", module_.get());
    }
    // Arguments are separated by a single space and the line ends with one
    // newline, matching the direct backend's print().
    for (std::size_t index = 0; index < operands.size(); ++index) {
      llvm::Value* value = operand(index);
      if (value == nullptr) continue;
      if (index != 0) {
        builder_->builder.CreateCall(
            write, {builder_->builder.CreateGlobalString(" ", "", 0, module_.get()),
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 1)});
      }
      if (value->getType()->isPointerTy()) {
        if (operands[index]->type().kind() == serem::IRType::Kind::String) {
          builder_->builder.CreateCall(write,
                                       {value, builder_->builder.CreateCall(length, {value})});
        } else {
          llvm::Function* ptrWrite = module_->getFunction("sere_write_ptr");
          if (ptrWrite == nullptr) {
            llvm::FunctionType* ptrWriteType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*context_), {llvm::PointerType::getUnqual(*context_)}, false);
            ptrWrite = llvm::Function::Create(ptrWriteType, llvm::Function::ExternalLinkage,
                                              "sere_write_ptr", module_.get());
          }
          builder_->builder.CreateCall(ptrWrite, {value});
        }
      } else if (value->getType()->isIntegerTy(32)) {
        llvm::Function* writeI32 = module_->getFunction("sere_write_i32");
        if (writeI32 == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt32Ty(*context_)}, false);
          writeI32 = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                            "sere_write_i32", module_.get());
        }
        builder_->builder.CreateCall(writeI32, {value});
      } else if (value->getType()->isIntegerTy(64)) {
        llvm::Function* writeI64 = module_->getFunction("sere_write_i64");
        if (writeI64 == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt64Ty(*context_)}, false);
          writeI64 = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                            "sere_write_i64", module_.get());
        }
        builder_->builder.CreateCall(writeI64, {value});
      } else if (value->getType()->isIntegerTy(1)) {
        llvm::Function* writeBool = module_->getFunction("sere_write_bool");
        if (writeBool == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getInt8Ty(*context_)}, false);
          writeBool = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                             "sere_write_bool", module_.get());
        }
        builder_->builder.CreateCall(
            writeBool, {builder_->builder.CreateZExt(value, llvm::Type::getInt8Ty(*context_))});
      } else if (value->getType()->isFloatTy() || value->getType()->isDoubleTy()) {
        llvm::Function* writeF64 = module_->getFunction("sere_write_f64");
        if (writeF64 == nullptr) {
          llvm::FunctionType* writeType = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*context_), {llvm::Type::getDoubleTy(*context_)}, false);
          writeF64 = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage,
                                            "sere_write_f64", module_.get());
        }
        if (value->getType()->isFloatTy()) {
          value = builder_->builder.CreateFPExt(value, llvm::Type::getDoubleTy(*context_));
        }
        builder_->builder.CreateCall(writeF64, {value});
      }
    }
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
  else if (opcode == "runtime.format") {
    // `f"{value:spec}"`: the runtime parses the spec so both backends render a
    // formatted value identically. Kinds match runtime/sere_rt.c: 0 int,
    // 1 float, 2 str, 3 bool; only the matching argument is read.
    const std::string kindText = attribute(operation, "kind");
    const std::int32_t kind =
        kindText.empty() ? 2 : static_cast<std::int32_t>(kindText[0] - '0');
    const std::string spec = attribute(operation, "spec");
    llvm::Value* intValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
    llvm::Value* floatValue =
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), 0.0);
    llvm::Value* data = llvm::ConstantPointerNull::get(ir.getPtrTy());
    llvm::Value* length = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
    llvm::Value* value = operand(0);
    if (kind == 1) {
      floatValue = value;
    } else if (kind == 0 || kind == 3) {
      intValue = value;
    } else {
      data = value;
      llvm::Function* lengthFn = module_->getFunction("strlen");
      if (lengthFn == nullptr) {
        lengthFn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt64Ty(*context_), {ir.getPtrTy()}, false),
            llvm::Function::ExternalLinkage, "strlen", module_.get());
      }
      length = ir.CreateCall(lengthFn, {value});
    }
    auto format = module_->getOrInsertFunction(
        "sere_format_value", ir.getPtrTy(), llvm::Type::getInt32Ty(*context_),
        llvm::Type::getInt64Ty(*context_), llvm::Type::getDoubleTy(*context_), ir.getPtrTy(),
        llvm::Type::getInt64Ty(*context_), ir.getPtrTy(), llvm::Type::getInt64Ty(*context_),
        ir.getPtrTy());
    llvm::AllocaInst* outLength = ir.CreateAlloca(llvm::Type::getInt64Ty(*context_));
    // The formatter returns NUL-terminated text, which is how Serem strings are
    // represented, so the pointer is the result.
    result = ir.CreateCall(
        format, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), kind), intValue,
                 floatValue, data, length, ir.CreateGlobalString(spec, "", 0, module_.get()),
                 llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_),
                                        static_cast<std::uint64_t>(spec.size())),
                 outLength});
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
  else if (opcode == "extract") {
    unsigned index = 0;
    const std::string indexText = attribute(operation, "index");
    (void)std::from_chars(indexText.data(), indexText.data() + indexText.size(), index);
    llvm::Value* aggregate = operand(0);
    if (aggregate != nullptr && aggregate->getType()->isStructTy()) {
      result = ir.CreateExtractValue(aggregate, {index});
    } else if (aggregate != nullptr) {
      result = aggregate;
    }
  }
  else if (opcode == "insert") {
    unsigned index = 0;
    const std::string indexText = attribute(operation, "index");
    (void)std::from_chars(indexText.data(), indexText.data() + indexText.size(), index);
    llvm::Value* aggregate = operand(0);
    if (aggregate != nullptr && aggregate->getType()->isStructTy() &&
        index < aggregate->getType()->getStructNumElements()) {
      result = ir.CreateInsertValue(
          aggregate, convert(operand(1), aggregate->getType()->getStructElementType(index)),
          {index});
    } else if (aggregate != nullptr) {
      result = aggregate;
    }
  }
  else if (opcode == "phi") {
    // The generator only emits a phi for a branch target it could not split, so
    // the incoming values are already available in the current block.
    result = operands.empty() ? nullptr : operand(0);
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
  else if (opcode == "list.equal") {
    std::int32_t kind = 0;
    const std::string kindText = attribute(operation, "kind");
    if (!kindText.empty()) {
      (void)std::from_chars(kindText.data(), kindText.data() + kindText.size(), kind);
    }
    llvm::FunctionCallee equal = module_->getOrInsertFunction(
        "sere_list_equal", ir.getInt32Ty(), ir.getPtrTy(), ir.getPtrTy(), ir.getInt32Ty());
    result = ir.CreateICmpNE(ir.CreateCall(equal, {operand(0), operand(1), ir.getInt32(kind)}),
                             ir.getInt32(0));
    if (attribute(operation, "negated") == "true") {
      result = ir.CreateNot(result);
    }
  }
  else if (opcode == "list.concat") {
    llvm::FunctionCallee concat = module_->getOrInsertFunction("sere_list_concat", ir.getPtrTy(),
                                                          ir.getPtrTy(), ir.getPtrTy());
    result = ir.CreateCall(concat, {operand(0), operand(1)});
  }
  else if (opcode == "list.repeat") {
    llvm::FunctionCallee repeat = module_->getOrInsertFunction("sere_list_repeat", ir.getPtrTy(),
                                                          ir.getPtrTy(), ir.getInt64Ty());
    result = ir.CreateCall(repeat, {operand(0), convert(operand(1), ir.getInt64Ty())});
  }
  else if (opcode == "string.repeat") {
    llvm::FunctionCallee repeat = module_->getOrInsertFunction(
        "sere_str_repeat", ir.getVoidTy(), ir.getPtrTy(), ir.getInt64Ty(), ir.getInt64Ty(),
        ir.getPtrTy(), ir.getPtrTy());
    llvm::FunctionCallee length =
        module_->getOrInsertFunction("strlen", ir.getInt64Ty(), ir.getPtrTy());
    llvm::Value* data = ir.CreateAlloca(ir.getPtrTy());
    llvm::Value* dataLength = ir.CreateAlloca(ir.getInt64Ty());
    ir.CreateCall(repeat, {operand(0), ir.CreateCall(length, {operand(0)}),
                           convert(operand(1), ir.getInt64Ty()), data, dataLength});
    result = ir.CreateLoad(ir.getPtrTy(), data);
  }
  else if (opcode == "slice") {
    // `xs[a:b]`, `s[a:]` and `s[:b]` differ only by which bounds are present.
    const bool hasStart = attribute(operation, "has.start") == "true";
    const bool hasStop = attribute(operation, "has.stop") == "true";
    const std::size_t startIndex = 1;
    const std::size_t stopIndex = hasStart ? 2 : 1;
    const bool isString = operands[0]->type().kind() == serem::IRType::Kind::String;
    llvm::Value* start = hasStart ? convert(operand(startIndex), ir.getInt64Ty()) : ir.getInt64(0);
    llvm::Value* stop = hasStop ? convert(operand(stopIndex), ir.getInt64Ty()) : ir.getInt64(0);
    if (isString) {
      llvm::FunctionCallee sliceFn = module_->getOrInsertFunction(
          "sere_str_slice", ir.getVoidTy(), ir.getPtrTy(), ir.getInt64Ty(), ir.getInt64Ty(),
          ir.getInt64Ty(), ir.getInt32Ty(), ir.getInt32Ty(), ir.getPtrTy(), ir.getPtrTy());
      llvm::FunctionCallee length =
          module_->getOrInsertFunction("strlen", ir.getInt64Ty(), ir.getPtrTy());
      llvm::Value* data = ir.CreateAlloca(ir.getPtrTy());
      llvm::Value* dataLength = ir.CreateAlloca(ir.getInt64Ty());
      ir.CreateCall(sliceFn,
                    {operand(0), ir.CreateCall(length, {operand(0)}), start, stop,
                     ir.getInt32(hasStart ? 1 : 0), ir.getInt32(hasStop ? 1 : 0), data, dataLength});
      result = ir.CreateLoad(ir.getPtrTy(), data);
    } else {
      llvm::FunctionCallee sliceFn =
          module_->getOrInsertFunction("sere_list_slice", ir.getPtrTy(), ir.getPtrTy(),
                                       ir.getInt64Ty(), ir.getInt64Ty(), ir.getInt32Ty(),
                                       ir.getInt32Ty());
      result = ir.CreateCall(
          sliceFn, {operand(0), start, stop, ir.getInt32(hasStart ? 1 : 0),
                    ir.getInt32(hasStop ? 1 : 0)});
    }
  }
  else if (opcode == "assert") {
    // The backend mirrors the LLVM generator: a failed assertion raises
    // `AssertionError` and, when nothing catches it, reports and exits.
    llvm::Value* condition = operand(0);
    llvm::Function* function = ir.GetInsertBlock()->getParent();
    llvm::BasicBlock* failBlock = llvm::BasicBlock::Create(*context_, "assert.fail", function);
    llvm::BasicBlock* okBlock = llvm::BasicBlock::Create(*context_, "assert.ok", function);
    ir.CreateCondBr(condition, okBlock, failBlock);
    ir.SetInsertPoint(failBlock);
    llvm::Function* raise = module_->getFunction("sere_raise");
    if (raise == nullptr) {
      raise = llvm::Function::Create(
          llvm::FunctionType::get(ir.getVoidTy(), {ir.getPtrTy(), ir.getPtrTy(), ir.getInt64Ty()},
                                  false),
          llvm::Function::ExternalLinkage, "sere_raise", module_.get());
    }
    llvm::Value* message = operands.size() > 1 ? operand(1) : nullptr;
    llvm::Value* messageLength = ir.getInt64(0);
    if (message != nullptr) {
      llvm::Function* length = module_->getFunction("strlen");
      if (length == nullptr) {
        length = llvm::Function::Create(
            llvm::FunctionType::get(ir.getInt64Ty(), {ir.getPtrTy()}, false),
            llvm::Function::ExternalLinkage, "strlen", module_.get());
      }
      messageLength = ir.CreateCall(length, {message});
    } else {
      message = ir.CreateGlobalString("assertion failed");
      messageLength = ir.getInt64(16);
    }
    ir.CreateCall(raise, {builder_->builder.CreateGlobalString("AssertionError;Exception"), message,
                          messageLength});
    // A surrounding `try` dispatches when one is recorded, exactly like `throw`;
    // otherwise the function returns and the entry wrapper reports the error.
    const std::string handler = attribute(operation, "handler");
    if (!handler.empty() && blockFor(handler) != nullptr) {
      ir.CreateBr(blockFor(handler));
    } else if (function->getReturnType()->isVoidTy()) {
      ir.CreateRetVoid();
    } else {
      ir.CreateRet(llvm::Constant::getNullValue(function->getReturnType()));
    }
    ir.SetInsertPoint(okBlock);
  }
  else if (opcode == "construct") {
    if (type->isPointerTy() && operation.type().pointee() != nullptr &&
        operation.type().pointee()->kind() == serem::IRType::Kind::Struct) {
      llvm::Type* record = lowerType(*operation.type().pointee());
      auto alloc = module_->getOrInsertFunction("sere_alloc", ir.getPtrTy(), ir.getInt64Ty());
      result = ir.CreateCall(alloc, {llvm::ConstantExpr::getSizeOf(record)});
      ir.CreateStore(llvm::Constant::getNullValue(record), result);
      // A class records its concrete type in the first word, which is what a
      // runtime `is` test against a subclass reads back.
      const std::string typeIdText = attribute(operation, "type.id");
      if (!typeIdText.empty() && record->getStructNumElements() > 0 &&
          record->getStructElementType(0)->isIntegerTy(32)) {
        std::int32_t typeId = 0;
        (void)std::from_chars(typeIdText.data(), typeIdText.data() + typeIdText.size(), typeId);
        ir.CreateStore(
            llvm::ConstantInt::get(record->getStructElementType(0), typeId),
            ir.CreateStructGEP(record, result, 0));
      }
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
        // An enum's first word is the variant discriminant; the generator tags
        // the constructor with the variant's index, not its arm position.
        std::int32_t tag = 0;
        const std::string tagText = attribute(operation, "tag");
        if (!tagText.empty()) {
          (void)std::from_chars(tagText.data(), tagText.data() + tagText.size(), tag);
        }
        result = builder_->builder.CreateInsertValue(
            result, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), tag), {0});
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
      // A `@static` field, or a nested class reached through one. The slot is a
      // module global: a pointer result asks for its address, because the access
      // is the base of a static store, and anything else asks for its value.
      llvm::GlobalVariable* slot = staticSlot(attribute(operation, "field"));
      result = type->isPointerTy()
                   ? static_cast<llvm::Value*>(slot)
                   : fromWord(ir.CreateLoad(ir.getInt64Ty(), slot), type);
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
    if (indexText == "-1") {
      // A `@static` write: the base is the receiver, and the storage is the
      // field's own module global.
      ir.CreateStore(toWord(operand(1)), staticSlot(attribute(operation, "field")));
    } else if (operands[0]->type().pointee() != nullptr) {
      llvm::Type* record = lowerType(*operands[0]->type().pointee());
      if (!record->isStructTy()) {
        report("Serem field assignment requires a record type");
      } else if (index >= record->getStructNumElements()) {
        report("Serem field assignment index is out of range");
      } else {
        ir.CreateStore(convert(operand(1), record->getStructElementType(index)),
                       ir.CreateStructGEP(record, operand(0), index));
      }
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
  else if (opcode == "throw") {
    llvm::Function* function = ir.GetInsertBlock()->getParent();
    if (operands.empty() || operand(0) == nullptr) {
      llvm::Function* rer = module_->getFunction("sere_reraise");
      if (rer == nullptr) {
        rer = llvm::Function::Create(llvm::FunctionType::get(ir.getVoidTy(), false),
                                     llvm::Function::ExternalLinkage, "sere_reraise",
                                     module_.get());
      }
      ir.CreateCall(rer, {});
    } else {
      llvm::Value* object = operand(0);
      llvm::Type* record = object->getType()->isPointerTy() && operands[0]->type().pointee() != nullptr
                               ? lowerType(*operands[0]->type().pointee())
                               : nullptr;
      llvm::Value* messagePtr = nullptr;
      llvm::Value* messageLen = ir.getInt64(0);
      unsigned field = 0;
      const std::string fieldText = attribute(operation, "field");
      if (!fieldText.empty() && fieldText != "-1") {
        (void)std::from_chars(fieldText.data(), fieldText.data() + fieldText.size(), field);
      }
      if (record != nullptr && record->isStructTy() && field < record->getStructNumElements() &&
          object->getType()->isPointerTy()) {
        // `str` is a null-terminated `char*` in the Serem ABI, so the message
        // field is the pointer itself and its length comes from `strlen`.
        messagePtr = ir.CreateExtractValue(ir.CreateLoad(record, object), {field});
        llvm::Function* length = module_->getFunction("strlen");
        if (length == nullptr) {
          length = llvm::Function::Create(
              llvm::FunctionType::get(ir.getInt64Ty(), {ir.getPtrTy()}, false),
              llvm::Function::ExternalLinkage, "strlen", module_.get());
        }
        messageLen = ir.CreateCall(length, {messagePtr});
      }
      llvm::Function* raise = module_->getFunction("sere_raise");
      if (raise == nullptr) {
        raise = llvm::Function::Create(
            llvm::FunctionType::get(ir.getVoidTy(), {ir.getPtrTy(), ir.getPtrTy(), ir.getInt64Ty()},
                                    false),
            llvm::Function::ExternalLinkage, "sere_raise", module_.get());
      }
      if (messagePtr == nullptr) messagePtr = llvm::ConstantPointerNull::get(ir.getPtrTy());
      ir.CreateCall(raise, {builder_->builder.CreateGlobalString(attribute(operation, "type")),
                            messagePtr, messageLen});
      if (record != nullptr && record->isStructTy() && object->getType()->isPointerTy()) {
        // The exception object is a class reference: store the pointer itself so
        // `except ... as e` can recover the instance and read its message field.
        llvm::Value* slot = ir.CreateAlloca(ir.getPtrTy());
        ir.CreateStore(object, slot);
        llvm::Function* setObject = module_->getFunction("sere_error_set_object");
        if (setObject == nullptr) {
          setObject = llvm::Function::Create(
              llvm::FunctionType::get(ir.getVoidTy(), {ir.getPtrTy(), ir.getInt64Ty()}, false),
              llvm::Function::ExternalLinkage, "sere_error_set_object", module_.get());
        }
        ir.CreateCall(setObject,
                      {slot, ir.getInt64(module_->getDataLayout().getTypeAllocSize(ir.getPtrTy()))});
      }
    }
    const std::string handler = attribute(operation, "handler");
    if (!handler.empty() && blockFor(handler) != nullptr) {
      ir.CreateBr(blockFor(handler));
    } else if (function->getReturnType()->isVoidTy()) {
      ir.CreateRetVoid();
    } else {
      ir.CreateRet(llvm::Constant::getNullValue(function->getReturnType()));
    }
  }
  else if (opcode == "error.isa") {
    llvm::Function* isa = module_->getFunction("sere_error_isa");
    if (isa == nullptr) {
      isa = llvm::Function::Create(llvm::FunctionType::get(ir.getInt32Ty(), {ir.getPtrTy()}, false),
                                   llvm::Function::ExternalLinkage, "sere_error_isa",
                                   module_.get());
    }
    result = ir.CreateICmpNE(
        ir.CreateCall(isa, {builder_->builder.CreateGlobalString(attribute(operation, "type"))}),
        ir.getInt32(0));
  }
  else if (opcode == "error.bind") {
    // The stored object is the exception instance pointer; recover it so the
    // bound name is the same class reference `raise` stored.
    llvm::Value* slot = ir.CreateAlloca(ir.getPtrTy());
    llvm::Function* copy = module_->getFunction("sere_error_copy_object");
    if (copy == nullptr) {
      copy = llvm::Function::Create(
          llvm::FunctionType::get(ir.getVoidTy(), {ir.getPtrTy(), ir.getInt64Ty()}, false),
          llvm::Function::ExternalLinkage, "sere_error_copy_object", module_.get());
    }
    ir.CreateCall(copy,
                  {slot, ir.getInt64(module_->getDataLayout().getTypeAllocSize(ir.getPtrTy()))});
    result = ir.CreateLoad(ir.getPtrTy(), slot);
  }
  else if (opcode == "error.enter") {
    llvm::Function* enter = module_->getFunction("sere_error_enter");
    if (enter == nullptr) {
      enter = llvm::Function::Create(llvm::FunctionType::get(ir.getVoidTy(), false),
                                     llvm::Function::ExternalLinkage, "sere_error_enter",
                                     module_.get());
    }
    ir.CreateCall(enter, {});
  }
  else if (opcode == "error.leave") {
    llvm::Function* leave = module_->getFunction("sere_error_leave");
    if (leave == nullptr) {
      leave = llvm::Function::Create(
          llvm::FunctionType::get(ir.getVoidTy(), {ir.getInt32Ty()}, false),
          llvm::Function::ExternalLinkage, "sere_error_leave", module_.get());
    }
    ir.CreateCall(leave, {ir.getInt32(attribute(operation, "restore") == "true" ? 1 : 0)});
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
    auto parseTag = [](const std::string& text) {
      int tag = -1;
      if (text.empty()) return tag;
      (void)std::from_chars(text.data(), text.data() + text.size(), tag);
      return tag;
    };
    llvm::Value* stored = ir.CreateExtractValue(operand(0), {0});
    // `tags` lists several acceptable members; `tag` is the single-member form.
    const std::string tags = attribute(operation, "tags");
    llvm::Value* result_ = nullptr;
    if (!tags.empty()) {
      std::string current;
      for (std::size_t index = 0; index <= tags.size(); ++index) {
        const char character = index == tags.size() ? ',' : tags[index];
        if (character != ',') {
          current.push_back(character);
          continue;
        }
        llvm::Value* test = ir.CreateICmpEQ(stored, ir.getInt32(parseTag(current)));
        result_ = result_ == nullptr ? test : ir.CreateOr(result_, test);
        current.clear();
      }
    }
    if (result_ == nullptr) {
      result_ = ir.CreateICmpEQ(stored, ir.getInt32(parseTag(attribute(operation, "tag"))));
    }
    // A class the union does not list is stored under the member it derives
    // from, so the tag has to be confirmed by the record's own type id.
    const std::string typeIdText = attribute(operation, "type.id");
    if (!typeIdText.empty() && stored->getType()->isIntegerTy(32)) {
      std::int32_t typeId = 0;
      (void)std::from_chars(typeIdText.data(), typeIdText.data() + typeIdText.size(), typeId);
      llvm::Value* boxed = ir.CreateIntToPtr(ir.CreateExtractValue(operand(0), {1}),
                                             llvm::PointerType::getUnqual(*context_));
      result_ = ir.CreateAnd(
          result_, ir.CreateICmpEQ(ir.CreateLoad(llvm::Type::getInt32Ty(*context_), boxed),
                                   ir.getInt32(typeId)));
    }
    result = result_;
    if (attribute(operation, "negated") == "true") result = ir.CreateNot(result);
  } else if (opcode == "object.isa") {
    // A class object records its concrete type id in its first word, so an
    // `isinstance` test reads that back and accepts every listed class, which
    // the generator fills with the target and its subclasses.
    auto parseInt = [](const std::string& text) {
      std::int32_t value = 0;
      (void)std::from_chars(text.data(), text.data() + text.size(), value);
      return value;
    };
    llvm::Value* object = operand(0);
    llvm::Value* matched = nullptr;
    if (object != nullptr && object->getType()->isPointerTy()) {
      llvm::Value* identity = ir.CreateLoad(ir.getInt32Ty(), object);
      std::string current;
      const std::string ids = attribute(operation, "ids") + ",";
      for (const char character : ids) {
        if (character != ',') {
          current.push_back(character);
          continue;
        }
        if (current.empty()) {
          continue;
        }
        llvm::Value* test = ir.CreateICmpEQ(identity, ir.getInt32(parseInt(current)));
        matched = matched == nullptr ? test : ir.CreateOr(matched, test);
        current.clear();
      }
    }
    if (matched == nullptr) {
      report("Serem type test needs a class object");
      result = nullptr;
    } else {
      result = attribute(operation, "negated") == "true" ? ir.CreateNot(matched) : matched;
    }
  } else if (opcode == "await") {
    result = operand(0);
  } else if (opcode == "coro.begin") {
    if (attribute(operation, "generator") == "true") {
      // A generator coroutine: a typed promise, a lazy initial suspend, and an
      // iterator slot that `for-in` resumes through the coroutine intrinsics.
      llvm::Function* function = ir.GetInsertBlock()->getParent();
      llvm::Type* elementType = operation.type().pointee() != nullptr
                                    ? lowerType(*operation.type().pointee())
                                    : nullptr;
      if (elementType == nullptr || elementType->isVoidTy()) {
        elementType = llvm::Type::getInt32Ty(*context_);
      }
      llvm::PointerType* ptrTy = ir.getPtrTy();
      coroPromise_ = ir.CreateAlloca(elementType, nullptr, "coro.promise");
      coroId_ = ir.CreateCall(coroutineIntrinsic(llvm::Intrinsic::coro_id),
                              {ir.getInt32(0), coroPromise_,
                               llvm::ConstantPointerNull::get(ptrTy),
                               llvm::ConstantPointerNull::get(ptrTy)},
                              "coro.id");
      llvm::Function* sizeFn = llvm::Intrinsic::getOrInsertDeclaration(
          module_.get(), llvm::Intrinsic::coro_size, {ir.getInt64Ty()});
      llvm::Value* size = ir.CreateCall(sizeFn, {}, "coro.size");
      llvm::Function* alloc = module_->getFunction("sere_alloc");
      if (alloc == nullptr) {
        alloc = llvm::Function::Create(
            llvm::FunctionType::get(ptrTy, {ir.getInt64Ty()}, false),
            llvm::Function::ExternalLinkage, "sere_alloc", module_.get());
      }
      coroMem_ = ir.CreateCall(alloc, {size}, "coro.alloc");
      coroHdl_ = ir.CreateCall(coroutineIntrinsic(llvm::Intrinsic::coro_begin),
                               {coroId_, coroMem_}, "coro.hdl");
      coroIterator_ = ir.CreateCall(alloc, {ir.getInt64(8)}, "iterator");
      ir.CreateStore(coroHdl_, coroIterator_);

      // The cleanup and suspend blocks only need values available here, so they
      // can be filled in before the body is lowered.
      coroCleanup_ = llvm::BasicBlock::Create(*context_, "coro.cleanup", function);
      coroSuspendBlock_ = llvm::BasicBlock::Create(*context_, "coro.suspend", function);
      {
        llvm::IRBuilder<> cb(coroCleanup_);
        llvm::Value* freeMem = cb.CreateCall(coroutineIntrinsic(llvm::Intrinsic::coro_free),
                                             {coroId_, coroHdl_}, "coro.free");
        llvm::Function* freeFn = module_->getFunction("sere_free");
        if (freeFn == nullptr) {
          freeFn = llvm::Function::Create(
              llvm::FunctionType::get(cb.getVoidTy(), {ptrTy}, false),
              llvm::Function::ExternalLinkage, "sere_free", module_.get());
        }
        cb.CreateCall(freeFn, {freeMem});
        cb.CreateBr(coroSuspendBlock_);
      }
      {
        llvm::IRBuilder<> sb(coroSuspendBlock_);
        sb.CreateCall(coroutineIntrinsic(llvm::Intrinsic::coro_end),
                      {coroHdl_, sb.getFalse(), llvm::ConstantTokenNone::get(*context_)});
        sb.CreateRet(coroIterator_);
      }

      // The ramp suspends immediately, so the first resume runs the body.
      llvm::BasicBlock* body = llvm::BasicBlock::Create(*context_, "coro.body", function);
      llvm::Value* initSuspend = ir.CreateCall(
          coroutineIntrinsic(llvm::Intrinsic::coro_suspend),
          {llvm::ConstantTokenNone::get(*context_), ir.getFalse()}, "coro.init.suspend");
      llvm::SwitchInst* initSwitch = ir.CreateSwitch(initSuspend, coroSuspendBlock_, 2);
      initSwitch->addCase(ir.getInt8(0), body);
      initSwitch->addCase(ir.getInt8(1), coroCleanup_);
      ir.SetInsertPoint(body);
      function->addFnAttr(llvm::Attribute::PresplitCoroutine);
      result = coroIterator_;
    } else {
      // Async functions run to completion: `await` is a no-op and the body
      // lowers linearly, so the token is never dereferenced.
      result = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type));
    }
  }
  else if (opcode == "yield") {
    if (coroPromise_ != nullptr && operand(0) != nullptr) {
      llvm::Value* value = operand(0);
      llvm::Type* elementType = llvm::cast<llvm::AllocaInst>(coroPromise_)->getAllocatedType();
      if (value->getType() != elementType) {
        value = convert(value, elementType);
      }
      ir.CreateStore(value, coroPromise_);
    }
  }
  else if (opcode == "coro.suspend") {
    if (coroHdl_ != nullptr && coroCleanup_ != nullptr && coroSuspendBlock_ != nullptr) {
      llvm::Value* suspended = ir.CreateCall(
          coroutineIntrinsic(llvm::Intrinsic::coro_suspend),
          {llvm::ConstantTokenNone::get(*context_), ir.getFalse()}, "coro.suspend");
      llvm::BasicBlock* resume = llvm::BasicBlock::Create(
          *context_, "coro.resume", ir.GetInsertBlock()->getParent());
      llvm::SwitchInst* sw = ir.CreateSwitch(suspended, coroSuspendBlock_, 2);
      sw->addCase(ir.getInt8(0), resume);
      sw->addCase(ir.getInt8(1), coroCleanup_);
      ir.SetInsertPoint(resume);
    }
  }
  else if (opcode == "coro.end") {
    if (coroHdl_ != nullptr && coroCleanup_ != nullptr && coroSuspendBlock_ != nullptr) {
      llvm::Value* suspended = ir.CreateCall(
          coroutineIntrinsic(llvm::Intrinsic::coro_suspend),
          {llvm::ConstantTokenNone::get(*context_), ir.getTrue()}, "coro.final.suspend");
      llvm::BasicBlock* trap = llvm::BasicBlock::Create(
          *context_, "coro.trap", ir.GetInsertBlock()->getParent());
      llvm::SwitchInst* sw = ir.CreateSwitch(suspended, coroSuspendBlock_, 2);
      sw->addCase(ir.getInt8(0), trap);
      sw->addCase(ir.getInt8(1), coroCleanup_);
      llvm::IRBuilder<> tb(trap);
      tb.CreateCall(llvm::Intrinsic::getOrInsertDeclaration(module_.get(), llvm::Intrinsic::trap));
      tb.CreateUnreachable();
    }
  }
  else if (opcode == "coro.resume" || opcode == "coro.done" || opcode == "coro.destroy" ||
           opcode == "coro.promise") {
    llvm::Function* resumeFn = coroutineIntrinsic(llvm::Intrinsic::coro_resume);
    llvm::Function* doneFn = coroutineIntrinsic(llvm::Intrinsic::coro_done);
    llvm::Function* promiseFn = coroutineIntrinsic(llvm::Intrinsic::coro_promise);
    llvm::Function* destroyFn = coroutineIntrinsic(llvm::Intrinsic::coro_destroy);
    llvm::Value* handle = ir.CreateLoad(ir.getPtrTy(), operand(0));
    if (opcode == "coro.resume") {
      ir.CreateCall(resumeFn, {handle});
    } else if (opcode == "coro.done") {
      result = ir.CreateCall(doneFn, {handle});
    } else if (opcode == "coro.destroy") {
      ir.CreateCall(destroyFn, {handle});
    } else {
      llvm::Type* elementType = operation.type().pointee() != nullptr
                                    ? lowerType(*operation.type().pointee())
                                    : llvm::Type::getInt32Ty(*context_);
      const unsigned align =
          module_->getDataLayout().getABITypeAlign(elementType).value();
      result = ir.CreateCall(promiseFn, {handle, ir.getInt32(align), ir.getFalse()});
    }
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
  // An uncaught exception prints its message and exits nonzero, matching the
  // direct backend's entry wrapper.
  llvm::Function* unhandled = module_->getFunction("sere_error_unhandled");
  if (unhandled == nullptr) {
    unhandled = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), false),
        llvm::Function::ExternalLinkage, "sere_error_unhandled", module_.get());
  }
  builder_->builder.CreateCall(unhandled, {});
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
    resetCoroutine();
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
