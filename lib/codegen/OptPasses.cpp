/// @file OptPasses.cpp
/// Sere-specific IR lowering passes that run before the LLVM pipeline.

#include "sere/codegen/OptPasses.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Analysis/ConstantFolding.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Support/ModRef.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <optional>
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

/// True when `name` is one of `names`.
[[nodiscard]] bool inSet(std::string_view name, std::initializer_list<std::string_view> names) {
  for (const std::string_view candidate : names) {
    if (name == candidate) {
      return true;
    }
  }
  return false;
}

/// Declarations whose result the runtime may return as null, so `nonnull` must
/// not be claimed for them.
[[nodiscard]] bool mayReturnNull(std::string_view name) {
  return inSet(name,
               {"sere_list_item", "sere_dict_get", "sere_str_index", "sere_list_pop_at",
                "sere_str_concat_data", "sere_str_repr_data", "sere_str_i32_data",
                "sere_str_i64_data", "sere_str_bool_data", "sere_str_ptr_data",
                "sere_str_f64_data", "sere_format_value", "sere_input"});
}

/// Names whose result the runtime never returns as null. Anything with `try`,
/// `find`, `search`, `maybe`, or `optional` in it is a lookup that may miss and
/// is therefore left alone.
[[nodiscard]] bool isNonNullReturning(std::string_view name) {
  if (!name.starts_with("sere_") || mayReturnNull(name)) {
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

/// Pure readers: the whole call is a function of its arguments.
[[nodiscard]] bool isPureReader(std::string_view name) {
  return inSet(name,
               {"sere_list_len", "sere_list_index_of", "sere_list_count", "sere_dict_len",
                "sere_dict_has", "sere_str_contains", "sere_str_eq", "sere_str_cmp",
                "sere_gc_bytes_in_use", "sere_gc_bytes_allocated", "sere_gc_live_blocks",
                "sere_gc_collections", "sere_async_now_ms"});
}

/// Runtime accessors an out-of-range argument would have panicked on.
[[nodiscard]] bool isBoundsCheckedAccessor(std::string_view name) {
  return inSet(name,
               {"sere_list_item", "sere_list_pop_at", "sere_list_insert", "sere_list_remove",
                "sere_str_index", "sere_str_slice", "sere_list_slice", "sere_dict_get",
                "sere_dict_set", "sere_dict_del"});
}

[[nodiscard]] bool isAllocatingDeclaration(std::string_view name) {
  return inSet(name,
               {"sere_alloc", "sere_gc_alloc", "sere_arena_alloc", "sere_pool_alloc",
                "sere_shared_new", "sere_list_new", "sere_array_new", "sere_dict_new",
                "sere_list_copy", "sere_list_slice", "sere_list_item", "sere_dict_copy",
                "sere_arena_new", "sere_pool_new"});
}

/// Allocating declarations return fresh memory: the pointer is never null and
/// never aliases anything else the program can already name. An accessor that
/// reads an existing object is not here, because its result aliases its
/// argument and `noalias` would be a lie.
[[nodiscard]] bool isFreshAllocation(std::string_view name) {
  return isAllocatingDeclaration(name) &&
         !inSet(name, {"sere_list_item", "sere_list_from_argv", "sere_pool_alloc"});
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
    if (instruction.isTerminator()) {
      continue;
    }
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

/// Folds every instruction whose operands are constant and then every branch
/// whose condition became constant, so the blocks a rewrite orphaned can be
/// collected.
void foldConstants(llvm::Function& function,
                   const llvm::DataLayout& layout,
                   const llvm::TargetLibraryInfo& libraryInfo) {
  for (llvm::BasicBlock& block : function) {
    for (llvm::Instruction& instruction : llvm::make_early_inc_range(block)) {
      if (instruction.getType()->isVoidTy() || instruction.isTerminator() ||
          llvm::isa<llvm::PHINode>(instruction)) {
        continue;
      }
      llvm::Constant* folded = llvm::ConstantFoldInstruction(&instruction, layout, &libraryInfo);
      if (folded == nullptr || llvm::isa<llvm::UndefValue>(folded)) {
        continue;
      }
      instruction.replaceAllUsesWith(folded);
      instruction.eraseFromParent();
    }
  }
  // Terminators are folded after the value pass so a compare that folded to a
  // constant already decided its branch.
  for (llvm::BasicBlock& block : function) {
    (void)llvm::ConstantFoldTerminator(&block, /*DeleteDeadConditions=*/false);
  }
}

/// True when the call only inspects or clears the pending error state.
[[nodiscard]] bool isErrorStateCall(std::string_view name) {
  return inSet(name,
               {"sere_has_error", "sere_clear_error", "sere_release_current_error",
                "sere_error_isa", "sere_error_message", "sere_error_name", "sere_reraise"});
}

/// Target library info for folding, built from the module's triple.
[[nodiscard]] std::unique_ptr<llvm::TargetLibraryInfoImpl>
libraryInfoFor(const llvm::Module& module) {
  llvm::Triple target = module.getTargetTriple();
  if (target.getTriple().empty()) {
    target = llvm::Triple(llvm::sys::getDefaultTargetTriple());
  }
  return std::make_unique<llvm::TargetLibraryInfoImpl>(target);
}

} // namespace

std::uint32_t annotateRuntimeDeclarations(llvm::Module& module,
                                          const OptimizationOptions& options) {
  std::uint32_t annotated = 0;
  for (llvm::Function& function : module) {
    if (!function.isDeclaration()) {
      continue;
    }
    const std::string_view name = function.getName();
    if (!name.starts_with("sere_")) {
      continue;
    }
    // The runtime is C: nothing in it can unwind through the caller's frame,
    // and every entry point either returns or terminates. Claiming both lets
    // the middle end sink and hoist calls and enables tail-call formation.
    bool changed = false;
    if (!function.hasFnAttribute(llvm::Attribute::NoUnwind)) {
      function.addFnAttr(llvm::Attribute::NoUnwind);
      changed = true;
    }
    if (isPureReader(name)) {
      function.setMemoryEffects(llvm::MemoryEffects::readOnly());
      function.addFnAttr(llvm::Attribute::WillReturn);
      function.addFnAttr("sere.pure", "true");
    } else if (isBoundsCheckedAccessor(name) && !options.boundsChecks) {
      // No check can fire, so the accessor returns whenever it is called. Its
      // memory effects stay unknown because the dictionary and list mutators
      // are in this set.
      function.addFnAttr(llvm::Attribute::WillReturn);
    }
    if (options.boundsChecks) {
      // With checks on, `sere_list_len` and the other readers still cannot
      // fault, but an out-of-range access can, so only the readers are
      // promised a return.
      if (isPureReader(name)) {
        function.addFnAttr(llvm::Attribute::WillReturn);
      }
    }
    if ((!options.nullChecks || !options.runtimeChecks) &&
        function.getReturnType()->isPointerTy() && isFreshAllocation(name)) {
      if (!function.hasRetAttribute(llvm::Attribute::NonNull)) {
        function.addRetAttr(llvm::Attribute::NonNull);
        function.addRetAttr(llvm::Attribute::NoAlias);
        function.addRetAttr(llvm::Attribute::NoUndef);
        changed = true;
      }
    }
    if (inSet(name, {"sere_alloc", "sere_gc_alloc", "sere_shared_new"}) &&
        function.arg_size() > 0 && function.getArg(0)->getType()->isIntegerTy()) {
      // `allocsize` lets LLVM fold two allocation sizes into one and lets it
      // know the block is at least that large.
      if (!function.hasFnAttribute(llvm::Attribute::AllocSize)) {
        function.addFnAttr(llvm::Attribute::getWithAllocSizeArgs(
            module.getContext(), /*ElemSizeArg=*/0, std::nullopt));
        changed = true;
      }
    }
    if (changed) {
      ++annotated;
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
        const CheckKind kind = classifyCheckBlock(*successor);
        const bool wanted = kind == CheckKind::Bounds ? removeBoundsChecks
                                                      : kind == CheckKind::Runtime && removeRuntimeChecks;
        if (!wanted || !hasSinglePredecessor(*successor, *branch)) {
          continue;
        }
        if (removable != nullptr) {
          // Both arms report a check: there is no good path to keep.
          removable = nullptr;
          break;
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

std::uint32_t stripErrorStateChecks(llvm::Module& module) {
  std::uint32_t removed = 0;
  const std::unique_ptr<llvm::TargetLibraryInfoImpl> libraryInfoImpl = libraryInfoFor(module);
  const llvm::TargetLibraryInfo libraryInfo(*libraryInfoImpl);
  const llvm::DataLayout& layout = module.getDataLayout();
  for (llvm::Function& function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    bool touched = false;
    for (llvm::Instruction& instruction :
         llvm::make_early_inc_range(llvm::instructions(function))) {
      auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr) {
        continue;
      }
      const llvm::Function* callee = call->getCalledFunction();
      if (callee == nullptr || !callee->isDeclaration() || !isErrorStateCall(callee->getName())) {
        continue;
      }
      // A query of the pending error reports "none": nothing raised, so a real
      // raise cannot reach the runtime either once the checks are gone. A
      // clear has no observable effect without a reader, and a re-raise is
      // only reachable from an error path that is about to disappear.
      if (!call->getType()->isVoidTy()) {
        call->replaceAllUsesWith(llvm::Constant::getNullValue(call->getType()));
      }
      call->eraseFromParent();
      ++removed;
      touched = true;
    }
    if (touched) {
      foldConstants(function, layout, libraryInfo);
      (void)llvm::removeUnreachableBlocks(function);
    }
  }
  return removed;
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
  llvm::FastMathFlags flags;
  flags.setFast();
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
      if (!llvm::isa<llvm::FPMathOperator>(instruction)) {
        continue;
      }
      instruction.setFastMathFlags(flags);
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
    if (name == "main" || name == "sere_main" || name.find("coro") != std::string_view::npos) {
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
  if (!options.runtimeChecks) {
    // Dropping the error-state checks first turns the exception paths into
    // unreachable blocks, so the check-block pass then only has the panics to
    // consider.
    report.errorChecksRemoved = stripErrorStateChecks(module);
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
