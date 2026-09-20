/// @file LLVMTransform.cpp
/// Always-on cleanup passes over the generated LLVM module.
///
/// Each stage mirrors a Serem transformer stage, so `--emit-llvm` shows the same
/// program `--emit-serem` does: `print(1 + 1)` is folded rather than added at
/// runtime, and a prelude helper the program never reaches is dropped instead of
/// being lowered alongside the code that runs.

#include "sere/codegen/LLVMTransform.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/ConstantFolding.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils/Local.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

namespace {

/// Names the module starts executing from. `sere_main` is what the generator
/// calls the program's entry point; a wrapper named `main` calls it. Either may
/// be absent, so both are roots whenever they are defined.
constexpr std::string_view kEntryPoint = "sere_main";
constexpr std::string_view kEntryWrapper = "main";

/// Stage name reported when the folding stage cannot be built for a module.
constexpr std::string_view kConstantFoldStage = "constant-fold";

/// Upper bound on the fixed-point rounds. Every stage is monotone, so the
/// pipeline settles long before this; the bound only stops a buggy stage from
/// spinning forever.
constexpr int kMaxRounds = 8;

/// One rewrite over a whole module.
///
/// `run` reports whether the module changed so the pipeline can iterate to a
/// fixed point without any stage having to know about the others.
class TransformStage {
public:
  virtual ~TransformStage() = default;
  [[nodiscard]] virtual std::string_view name() const = 0;
  virtual bool run(llvm::Module& module) = 0;
};

/// Folds arithmetic, comparisons, casts, and constant branches.
///
/// The generator writes `1 + 1` as an `add` because it never inspects operand
/// values, so the module only loses those operations here.
class ConstantFoldStage final : public TransformStage {
public:
  ConstantFoldStage(const llvm::DataLayout& layout, const llvm::TargetLibraryInfo& libraryInfo)
      : layout_(layout), libraryInfo_(libraryInfo) {}

  [[nodiscard]] std::string_view name() const override { return kConstantFoldStage; }

  bool run(llvm::Module& module) override {
    bool changed = false;
    for (llvm::Function& function : module) {
      if (function.isDeclaration()) {
        continue;
      }
      for (llvm::BasicBlock& block : function) {
        // Instructions are visited in program order, so a value folded early
        // feeds the instruction that consumes it without another round.
        for (llvm::Instruction& instruction : llvm::make_early_inc_range(block)) {
          // A terminator folds into a CFG rewrite rather than a value
          // replacement, and a phi has no single predecessor to read.
          if (instruction.getType()->isVoidTy() || instruction.isTerminator() ||
              llvm::isa<llvm::PHINode>(instruction)) {
            continue;
          }
          llvm::Constant* folded =
              llvm::ConstantFoldInstruction(&instruction, layout_, &libraryInfo_);
          if (folded == nullptr || llvm::isa<llvm::UndefValue>(folded)) {
            // Folding gave up, or decided the operation has no value at all,
            // such as a shift past the operand width. Printing `undef` there
            // would hide the computation the program still performs.
            continue;
          }
          instruction.replaceAllUsesWith(folded);
          instruction.eraseFromParent();
          changed = true;
        }
      }
      // A branch on a constant is decided at compile time. The condition is kept
      // so the printed IR still shows what the program tests.
      for (llvm::BasicBlock& block : function) {
        changed =
            llvm::ConstantFoldTerminator(&block, /*DeleteDeadConditions=*/false, &libraryInfo_) ||
            changed;
      }
    }
    return changed;
  }

private:
  const llvm::DataLayout& layout_;
  const llvm::TargetLibraryInfo& libraryInfo_;
};

/// Keeps only the definitions the module entry point can reach.
///
/// The generator materialises every function the program declares, including
/// each prelude helper, so a program that calls none of `clamp`, `min`, or
/// `sign` still carries them. Reachability starts at the entry point and follows
/// instruction operands, which covers a function whose address is taken as much
/// as one that is called directly.
class DeadCodeStage final : public TransformStage {
public:
  [[nodiscard]] std::string_view name() const override { return "dead-code"; }

  bool run(llvm::Module& module) override {
    llvm::SmallPtrSet<llvm::Function*, 2> roots;
    for (const std::string_view name : {kEntryPoint, kEntryWrapper}) {
      if (llvm::Function* function = module.getFunction(name);
          function != nullptr && !function->isDeclaration()) {
        roots.insert(function);
      }
    }
    if (roots.empty()) {
      // No entry point means a library module, where every definition is part of
      // the published surface.
      return false;
    }
    bool changed = false;
    bool progress = true;
    // Removing a definition drops the only reference to the helpers it called,
    // so sweep until no definition loses its last use.
    while (progress) {
      progress = false;
      std::vector<llvm::Function*> dead;
      for (llvm::Function& function : module) {
        if (!roots.contains(&function) && function.use_empty()) {
          dead.push_back(&function);
        }
      }
      for (llvm::Function* function : dead) {
        function->eraseFromParent();
        progress = true;
      }
      changed = changed || !dead.empty();
    }
    return changed;
  }
};

/// Removes blocks nothing can branch to.
///
/// Folding a branch on a constant leaves the arm it can never take behind, and
/// printing that arm suggests the program still tests something at runtime.
class UnreachableBlockStage final : public TransformStage {
public:
  [[nodiscard]] std::string_view name() const override { return "unreachable-blocks"; }

  bool run(llvm::Module& module) override {
    bool changed = false;
    for (llvm::Function& function : module) {
      if (function.isDeclaration()) {
        continue;
      }
      changed = llvm::removeUnreachableBlocks(function) || changed;
    }
    return changed;
  }
};

/// Drops globals no surviving code references. String literals are private
/// globals, so a folded branch or a removed function leaves them behind.
class UnusedGlobalStage final : public TransformStage {
public:
  [[nodiscard]] std::string_view name() const override { return "unused-globals"; }

  bool run(llvm::Module& module) override {
    bool changed = false;
    bool progress = true;
    while (progress) {
      progress = false;
      std::vector<llvm::GlobalVariable*> dead;
      for (llvm::GlobalVariable& global : module.globals()) {
        // Only a global this module owns can disappear: an externally linked one
        // is a symbol another object file may still name, and an appending one
        // is the module's own table rather than a value no code reads.
        if (global.isDeclaration() || !global.hasLocalLinkage() ||
            global.hasAppendingLinkage() || !global.use_empty()) {
          continue;
        }
        dead.push_back(&global);
      }
      for (llvm::GlobalVariable* global : dead) {
        global->eraseFromParent();
        progress = true;
      }
      changed = changed || !dead.empty();
    }
    return changed;
  }
};

/// The pipeline, in the order the stages must run: dead code first so the later
/// stages do not spend time on definitions that are about to disappear.
[[nodiscard]] std::vector<std::unique_ptr<TransformStage>>
defaultTransformStages(const llvm::Module& module, const llvm::TargetLibraryInfo& libraryInfo) {
  std::vector<std::unique_ptr<TransformStage>> stages;
  stages.push_back(std::make_unique<DeadCodeStage>());
  stages.push_back(std::make_unique<ConstantFoldStage>(module.getDataLayout(), libraryInfo));
  stages.push_back(std::make_unique<UnreachableBlockStage>());
  stages.push_back(std::make_unique<UnusedGlobalStage>());
  return stages;
}

/// Target the folding stage reads its constants at.
///
/// Folding needs a data layout to know how wide a constant is, and a module that
/// names no triple is the one the generator builds before it stamps a target on
/// it, so the host's triple stands in.
[[nodiscard]] bool resolveTarget(const llvm::Module& module, llvm::Triple& target,
                                 std::string& error) {
  target = module.getTargetTriple();
  if (target.getTriple().empty()) {
    target = llvm::Triple(llvm::sys::getDefaultTargetTriple());
  }
  if (target.getArch() == llvm::Triple::UnknownArch) {
    error = "stage '" + std::string(kConstantFoldStage) + "' cannot be built for target '" +
            target.getTriple() + "'";
    return false;
  }
  return true;
}

} // namespace

int runLLVMTransformers(llvm::Module& module, std::string& error) {
  llvm::Triple target;
  if (!resolveTarget(module, target, error)) {
    return -1;
  }
  // The library info is what tells folding whether a call is a known function it
  // may evaluate at compile time, so both outlive the stages that read them.
  const llvm::TargetLibraryInfoImpl libraryInfoImpl(target);
  const llvm::TargetLibraryInfo libraryInfo(libraryInfoImpl);

  const std::vector<std::unique_ptr<TransformStage>> stages =
      defaultTransformStages(module, libraryInfo);

  int ran = 0;
  for (int round = 0; round < kMaxRounds; ++round) {
    bool changed = false;
    for (const std::unique_ptr<TransformStage>& stage : stages) {
      ++ran;
      changed = stage->run(module) || changed;
    }
    if (!changed) {
      break;
    }
  }
  return ran;
}

} // namespace sere
