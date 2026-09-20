/// @file OptPasses.cpp
/// Sere-specific IR lowering passes that run before the LLVM pipeline.

#include "sere/codegen/OptPasses.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/ModRef.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace sere {
namespace {

/// Stack promotion refuses sizes above this, so a huge allocation cannot blow
/// the stack. `SERE_STACK_ALLOC_MAX` overrides it.
constexpr std::uint64_t kDefaultStackAllocMax = 1u << 20;

[[nodiscard]] std::uint64_t stackAllocMax() {
  if (const char* text = std::getenv("SERE_STACK_ALLOC_MAX"); text != nullptr && *text != '\0') {
    const unsigned long long parsed = std::strtoull(text, nullptr, 10);
    if (parsed != 0) {
      return parsed;
    }
  }
  return kDefaultStackAllocMax;
}

/// Names whose result the runtime never returns as null. Anything with `try`,
/// `find`, `search`, `maybe`, or `optional` in it is a lookup that may miss and
/// is therefore left alone.
[[nodiscard]] bool isNonNullReturning(std::string_view name) {
  if (!name.starts_with("sere_")) {
    return false;
  }
  for (const std::string_view miss : {"try", "find", "search", "maybe", "optional", "scan"}) {
    if (name.find(miss) != std::string_view::npos) {
      return false;
    }
  }
  for (const std::string_view hit : {"alloc", "_new", "copy", "item", "slice", "_data", "_repr",
                                     "concat", "join", "repeat", "replace", "argv", "arena",
                                     "pool", "shared", "reader", "string"}) {
    if (name.find(hit) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

/// Runtime accessors the release-mode `--no-bounds-checks` promise extends to:
/// out-of-range access no longer unwinds, so the calls can be hoisted, CSEd,
/// and vectorized like a raw load.
[[nodiscard]] bool isBoundsCheckedAccessor(std::string_view name) {
  return llvm::StringSwitch<bool>(name)
      .Cases("sere_list_item", "sere_list_pop_at", "sere_list_insert", "sere_list_remove", true)
      .Cases("sere_str_index", "sere_str_slice", "sere_list_slice", true)
      .Cases("sere_dict_get", "sere_dict_set", "sere_dict_del", true)
      .Default(false);
}

/// Pure readers: the whole call is a function of its arguments.
[[nodiscard]] bool isPureReader(std::string_view name) {
  return llvm::StringSwitch<bool>(name)
      .Cases("sere_list_len", "sere_list_index_of", "sere_list_count", true)
      .Cases("sere_dict_len", "sere_dict_has", true)
      .Cases("sere_str_contains", "sere_str_eq", "sere_str_cmp", true)
      .Cases("sere_gc_bytes_in_use", "sere_gc_bytes_allocated", "sere_gc_live_blocks", true)
      .Cases("sere_gc_collections", "sere_async_now_ms", true)
      .Default(false);
}

[[nodiscard]] bool isAllocatingDeclaration(std::string_view name) {
  return llvm::StringSwitch<bool>(name)
      .Cases("sere_alloc", "sere_gc_alloc", "sere_arena_alloc", "sere_pool_alloc", true)
      .Cases("sere_shared_new", "sere_list_new", "sere_array_new", "sere_dict_new", true)
      .Cases("sere_list_copy", "sere_list_slice", "sere_list_item", "sere_dict_copy", true)
      .Cases("sere_arena_new", "sere_pool_new", true)
      .Default(false);
}

/// The global a string literal pointer refers to, or null when the pointer is
/// not a literal.
[[nodiscard]] const llvm::GlobalVariable* literalGlobal(const llvm::Value* value) {
  if (value == nullptr) {
    return nullptr;
  }
  value = value->stripPointerCasts();
  return llvm::dyn_cast<llvm::GlobalVariable>(value);
}

/// The text of a string literal, or empty when it is not one.
[[nodiscard]] std::string literalText(const llvm::Value* value) {
  const llvm::GlobalVariable* global = literalGlobal(value);
  if (global == nullptr || !global->hasInitializer()) {
    return {};
  }
  const auto* data = llvm::dyn_cast<llvm::ConstantDataArray>(global->getInitializer());
  if (data == nullptr || !data->isString()) {
    return {};
  }
  return data->getAsString().str();
}

/// True when `value` came from something the runtime guarantees is non-null.
[[nodiscard]] bool isProvablyNonNull(const llvm::Value* value) {
  if (value == nullptr) {
    return false;
  }
  value = value->stripPointerCasts();
  if (llvm::isa<llvm::AllocaInst>(value) || llvm::isa<llvm::GlobalVariable>(value) ||
      llvm::isa<llvm::Function>(value)) {
    return true;
  }
  const auto* call = llvm::dyn_cast<llvm::CallBase>(value);
  if (call == nullptr) {
    return false;
  }
  const llvm::Function* callee = call->getCalledFunction();
  if (callee == nullptr) {
    return false;
  }
  if (!callee->getReturnType()->isPointerTy()) {
    return false;
  }
  if (callee->hasRetAttribute(llvm::Attribute::NonNull)) {
    return true;
  }
  return callee->isDeclaration() && isNonNullReturning(callee->getName());
}

/// A block a conditional branch can skip: it does nothing but report a
/// compiler-inserted failure, so removing it only removes a check.
enum class CheckKind {
  None,
  Runtime,
  Bounds,
};

/// Reads the check a block performs, or `None` when the block must stay.
[[nodiscard]] CheckKind classifyCheckBlock(const llvm::BasicBlock& block) {
  const auto* terminator = llvm::dyn_cast<llvm::UnreachableInst>(block.getTerminator());
  if (terminator == nullptr) {
    return CheckKind::None;
  }
  std::string message;
  bool sawPanic = false;
  for (const llvm::Instruction& instruction : block) {
    const auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
    if (call == nullptr) {
      // A phi would make the block reachable from several places and a store
      // would make it do real work; both disqualify it.
      return CheckKind::None;
    }
    const llvm::Function* callee = call->getCalledFunction();
    if (callee == nullptr) {
      return CheckKind::None;
    }
    const std::string_view name = callee->getName();
    if (name == "sere_panic") {
      sawPanic = true;
      if (call->arg_size() > 0) {
        message = literalText(call->getArgOperand(0));
      }
      continue;
    }
    // `sere_raise` carries user exceptions that `try`/`except` must still see.
    if (name == "sere_raise" || name == "sere_error_isa" || name == "sere_has_error") {
      return CheckKind::None;
    }
    if (!callee->isDeclaration()) {
      return CheckKind::None;
    }
  }
  if (!sawPanic) {
    return CheckKind::None;
  }
  if (message.find("index") != std::string::npos || message.find("range") != std::string::npos ||
      message.find("bound") != std::string::npos || message.find("IndexError") != std::string::npos) {
    return CheckKind::Bounds;
  }
  return CheckKind::Runtime;
}

/// True when only `branch` can reach `block`.
[[nodiscard]] bool hasSinglePredecessor(const llvm::BasicBlock& block,
                                        const llvm::BranchInst& branch) {
  return block.getSinglePredecessor() == branch.getParent();
}

/// Every use of `value` stays in the function: no store captures it, no call
/// receives it, and nothing derives a value the pass cannot follow.
[[nodiscard]] bool doesNotEscape(const llvm::CallInst& allocation) {
  llvm::SmallVector<const llvm::Value*, 16> worklist{&allocation};
  while (!worklist.empty()) {
    const llvm::Value* value = worklist.pop_back_val();
    for (const llvm::User* user : value->users()) {
      if (const auto* load = llvm::dyn_cast<llvm::LoadInst>(user)) {
        if (load->getPointerOperand() == value) {
          continue;
        }
        return false;
      }
      if (const auto* store = llvm::dyn_cast<llvm::StoreInst>(user)) {
        if (store->getPointerOperand() == value) {
          continue;
        }
        // The pointer itself is written somewhere: it outlives the frame.
        return false;
      }
      if (const auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(user)) {
        worklist.push_back(gep);
        continue;
      }
      if (llvm::isa<llvm::BitCastInst>(user) || llvm::isa<llvm::AddrSpaceCastInst>(user) ||
          llvm::isa<llvm::ICmpInst>(user)) {
        continue;
      }
      // Calls, returns, phis, and selects all hand the pointer to code this
      // pass cannot see.
      return false;
    }
  }
  return true;
}

} // namespace

std::uint32_t annotateRuntimeDeclarations(llvm::Module& module,
                                          const OptimizationOptions& options) {
  std::uint32_t annotated = 0;
  if (options.boundsChecks && options.nullChecks && options.runtimeChecks) {
    return 0;
  }
  for (llvm::Function& function : module) {
    if (!function.isDeclaration()) {
      continue;
    }
    const std::string_view name = function.getName();
    if (!options.boundsChecks && isBoundsCheckedAccessor(name)) {
      // Out-of-range access no longer unwinds, so the call is a raw load the
      // middle end may hoist, CSE, and vectorize.
      function.addFnAttr(llvm::Attribute::NoUnwind);
      if (isPureReader(name)) {
        function.setMemoryEffects(llvm::MemoryEffects::readOnly());
      }
      ++annotated;
    }
    if ((!options.nullChecks || !options.runtimeChecks) &&
        function.getReturnType()->isPointerTy() &&
        (isAllocatingDeclaration(name) || isNonNullReturning(name)) &&
        !function.hasRetAttribute(llvm::Attribute::NonNull)) {
      function.addRetAttr(llvm::Attribute::NonNull);
      ++annotated;
    }
    if (!options.runtimeChecks && !options.nullChecks && isPureReader(name)) {
      function.setMemoryEffects(llvm::MemoryEffects::readOnly());
    }
  }
  return annotated;
}

std::uint32_t eliminateNullChecks(llvm::Module& module) {
  std::uint32_t removed = 0;
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    std::vector<llvm::BasicBlock*> touched;
    for (llvm::BasicBlock& block : function) {
      for (llvm::Instruction& instruction : llvm::make_early_inc_range(block)) {
        auto* compare = llvm::dyn_cast<llvm::ICmpInst>(&instruction);
        if (compare == nullptr) {
          continue;
        }
        if (compare->getPredicate() != llvm::CmpInst::ICMP_EQ &&
            compare->getPredicate() != llvm::CmpInst::ICMP_NE) {
          continue;
        }
        if (!compare->getOperand(0)->getType()->isPointerTy()) {
          continue;
        }
        const bool leftNull = llvm::isa<llvm::ConstantPointerNull>(compare->getOperand(0));
        const bool rightNull = llvm::isa<llvm::ConstantPointerNull>(compare->getOperand(1));
        if (leftNull == rightNull) {
          continue;
        }
        const llvm::Value* candidate = leftNull ? compare->getOperand(1) : compare->getOperand(0);
        if (!isProvablyNonNull(candidate)) {
          continue;
        }
        // `eq null` is false, `ne null` is true.
        const bool isEqual = compare->getPredicate() == llvm::CmpInst::ICMP_EQ;
        compare->replaceAllUsesWith(
            llvm::ConstantInt::get(llvm::Type::getInt1Ty(module.getContext()), !isEqual));
        compare->eraseFromParent();
        if (std::find(touched.begin(), touched.end(), &block) == touched.end()) {
          touched.push_back(&block);
        }
        ++removed;
      }
    }
    for (llvm::BasicBlock* block : touched) {
      (void)llvm::ConstantFoldTerminator(block, /*DeleteDeadConditions=*/false);
    }
    if (!touched.empty()) {
      (void)llvm::removeUnreachableBlocks(function);
    }
  }
  return removed;
}

std::uint32_t eliminateCheckBlocks(llvm::Module& module, bool removeRuntimeChecks, bool removeBoundsChecks) {
  if (!removeRuntimeChecks && !removeBoundsChecks) {
    return 0;
  }
  std::uint32_t removed = 0;
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    bool changed = false;
    for (llvm::BasicBlock& block : function) {
      auto* branch = llvm::dyn_cast<llvm::BranchInst>(block.getTerminator());
      if (branch == nullptr || !branch->isConditional()) {
        continue;
      }
      llvm::BasicBlock* removable = nullptr;
      bool takeFalse = false;
      for (unsigned index = 0; index < 2; ++index) {
        llvm::BasicBlock* successor = branch->getSuccessor(index);
        if (index == 1 && removable != nullptr) {
          // Both arms report a check: there is no good path to keep.
          removable = nullptr;
          break;
        }
        const CheckKind kind = classifyCheckBlock(*successor);
        const bool wanted = kind == CheckKind::Bounds ? removeBoundsChecks
                                                      : kind == CheckKind::Runtime && removeRuntimeChecks;
        if (!wanted || !hasSinglePredecessor(*successor, *branch)) {
          continue;
        }
        removable = successor;
        takeFalse = index == 0;
      }
      if (removable == nullptr) {
        continue;
      }
      branch->setCondition(llvm::ConstantInt::get(llvm::Type::getInt1Ty(module.getContext()),
                                                  !takeFalse));
      (void)llvm::ConstantFoldTerminator(&block, /*DeleteDeadConditions=*/false);
      ++removed;
      changed = true;
    }
    if (changed) {
      (void)llvm::removeUnreachableBlocks(function);
    }
  }
  return removed;
}

std::uint32_t promoteNonEscapingAllocations(llvm::Module& module) {
  const std::uint64_t limit = stackAllocMax();
  std::uint32_t promoted = 0;
  for (llvm::Function& function : module) {
    if (function.isDeclaration() || function.isVarArg()) {
      continue;
    }
    llvm::BasicBlock* entry = &function.getEntryBlock();
    for (llvm::Instruction& instruction : llvm::make_early_inc_range(llvm::instructions(function))) {
      auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr || call->arg_size() != 1 || !call->getType()->isPointerTy()) {
        continue;
      }
      const llvm::Function* callee = call->getCalledFunction();
      if (callee == nullptr || !callee->isDeclaration()) {
        continue;
      }
      const std::string_view name = callee->getName();
      if (name != "sere_alloc" && name != "sere_gc_alloc" && name != "sere_arena_alloc") {
        continue;
      }
      auto* size = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(0));
      if (size == nullptr || size->isNegative() || size->getZExtValue() == 0 ||
          size->getZExtValue() > limit) {
        continue;
      }
      if (!doesNotEscape(*call)) {
        continue;
      }
      llvm::IRBuilder<> builder(entry, entry->getFirstInsertionPt());
      llvm::AllocaInst* slot =
          builder.CreateAlloca(llvm::Type::getInt8Ty(module.getContext()),
                               llvm::ConstantInt::get(size->getType(), size->getZExtValue()),
                               call->getName());
      slot->setAlignment(llvm::Align(16));
      call->replaceAllUsesWith(slot);
      call->eraseFromParent();
      ++promoted;
    }
  }
  return promoted;
}

std::uint32_t elideIndividualFrees(llvm::Module& module) {
  std::uint32_t elided = 0;
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    for (llvm::Instruction& instruction : llvm::make_early_inc_range(llvm::instructions(function))) {
      auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr) {
        continue;
      }
      const llvm::Function* callee = call->getCalledFunction();
      if (callee == nullptr || !callee->isDeclaration()) {
        continue;
      }
      const std::string_view name = callee->getName();
      if (name != "sere_free" && name != "sere_gc_free") {
        continue;
      }
      if (!call->use_empty()) {
        continue;
      }
      call->eraseFromParent();
      ++elided;
    }
  }
  return elided;
}

std::uint32_t applyFastMathAttributes(llvm::Module& module) {
  std::uint32_t count = 0;
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    function.addFnAttr("unsafe-fp-math", "true");
    function.addFnAttr("no-nans-fp-math", "true");
    function.addFnAttr("no-infs-fp-math", "true");
    function.addFnAttr("no-signed-zeros-fp-math", "true");
    function.addFnAttr("fp-contract", "fast");
    function.addFnAttr("less-precise-fpmad", "true");
    for (llvm::Instruction& instruction : llvm::instructions(function)) {
      auto* operation = llvm::dyn_cast<llvm::FPMathOperator>(&instruction);
      if (operation == nullptr || !operation->getType()->isFloatingPointTy()) {
        continue;
      }
      operation->setFast(true);
    }
    ++count;
  }
  return count;
}

std::uint32_t applyTailCallAttributes(llvm::Module& module, bool allowTailCalls) {
  std::uint32_t count = 0;
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    function.addFnAttr("disable-tail-calls", allowTailCalls ? "false" : "true");
    ++count;
  }
  return count;
}

void markAlwaysInline(llvm::Module& module) {
  for (llvm::Function& function : module) {
    if (function.isDeclaration() || function.isVarArg()) {
      continue;
    }
    const std::string_view name = function.getName();
    if (name == "main" || name == "sere_main" || name.contains("coro")) {
      continue;
    }
    function.addFnAttr(llvm::Attribute::AlwaysInline);
  }
}

void stampLoweringFlags(llvm::Module& module, const OptimizationOptions& options) {
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    if (!options.runtimeChecks) {
      function.addFnAttr("sere.no-runtime-checks", "true");
    }
    if (!options.boundsChecks) {
      function.addFnAttr("sere.no-bounds-checks", "true");
    }
    if (!options.nullChecks) {
      function.addFnAttr("sere.no-null-checks", "true");
    }
    if (options.stackAlloc) {
      function.addFnAttr("sere.stack-alloc", "true");
    }
    if (options.arenaAlloc) {
      function.addFnAttr("sere.arena-alloc", "true");
    }
    if (options.fastMath) {
      function.addFnAttr("sere.fast-math", "true");
    }
  }
}

OptRewriteReport runPrePipelinePasses(llvm::Module& module,
                                      const OptimizationOptions& options) {
  OptRewriteReport report;
  report.annotatedNonNull = annotateRuntimeDeclarations(module, options);
  if (options.fastMath) {
    report.fastMathFunctions = applyFastMathAttributes(module);
  }
  if (options.tailCallsExplicit) {
    report.tailCallFunctions = applyTailCallAttributes(module, options.tailCalls);
  }
  if (!options.nullChecks || !options.runtimeChecks) {
    report.nullChecksRemoved = eliminateNullChecks(module);
  }
  report.checkBlocksRemoved =
      eliminateCheckBlocks(module, !options.runtimeChecks, !options.boundsChecks);
  if (options.arenaAlloc) {
    report.freesElided = elideIndividualFrees(module);
  }
  if (options.stackAlloc) {
    report.allocationsStackPromoted = promoteNonEscapingAllocations(module);
  }
  if (options.inlineAll) {
    markAlwaysInline(module);
  }
  stampLoweringFlags(module, options);
  return report;
}

} // namespace sere
