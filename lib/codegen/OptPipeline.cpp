/// @file OptPipeline.cpp
/// Builds default or custom LLVM pass pipelines via PassBuilder.

#include "sere/codegen/OptPipeline.h"

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Support/Error.h>
#include <llvm/Transforms/Coroutines/CoroCleanup.h>
#include <llvm/Transforms/Coroutines/CoroEarly.h>
#include <llvm/Transforms/Coroutines/CoroSplit.h>

namespace sere {

// Async functions are lowered with LLVM's coroutine intrinsics. The splitting
// passes must run at *every* optimization level (including O0, where the rest
// of the pipeline is skipped) or the coroutine frames are never materialized.
static void addCoroutinePasses(llvm::ModulePassManager& mpm) {
  mpm.addPass(llvm::CoroEarlyPass());
  mpm.addPass(llvm::createModuleToPostOrderCGSCCPassAdaptor(llvm::CoroSplitPass()));
  mpm.addPass(llvm::CoroCleanupPass());
}

bool parseOptLevel(std::string_view text, OptLevel& level, std::string& error) {
  if (text == "0" || text == "O0") {
    level = OptLevel::O0;
    return true;
  }
  if (text == "1" || text == "O1") {
    level = OptLevel::O1;
    return true;
  }
  if (text == "2" || text == "O2") {
    level = OptLevel::O2;
    return true;
  }
  if (text == "3" || text == "O3") {
    level = OptLevel::O3;
    return true;
  }
  if (text == "s" || text == "Os") {
    level = OptLevel::Os;
    return true;
  }
  if (text == "z" || text == "Oz") {
    level = OptLevel::Oz;
    return true;
  }
  error = "invalid --opt level '" + std::string(text) + "' (expected O0, O1, O2, O3, Os, Oz)";
  return false;
}

bool runOptPipeline(llvm::Module& module,
                    OptLevel level,
                    std::string_view passes,
                    std::string& error) {
  llvm::LoopAnalysisManager loops;
  llvm::FunctionAnalysisManager functions;
  llvm::CGSCCAnalysisManager cgscc;
  llvm::ModuleAnalysisManager modules;
  llvm::PassBuilder builder;
  builder.registerModuleAnalyses(modules);
  builder.registerCGSCCAnalyses(cgscc);
  builder.registerFunctionAnalyses(functions);
  builder.registerLoopAnalyses(loops);
  builder.crossRegisterProxies(loops, functions, cgscc, modules);

  llvm::ModulePassManager pipeline;
  if (!passes.empty()) {
    if (auto failed = builder.parsePassPipeline(pipeline, passes)) {
      error = llvm::toString(std::move(failed));
      return false;
    }
    addCoroutinePasses(pipeline);
  } else if (level == OptLevel::O0) {
    addCoroutinePasses(pipeline);
  } else {
    llvm::OptimizationLevel llvmLevel = llvm::OptimizationLevel::O1;
    if (level == OptLevel::O2) {
      llvmLevel = llvm::OptimizationLevel::O2;
    } else if (level == OptLevel::O3) {
      llvmLevel = llvm::OptimizationLevel::O3;
    } else if (level == OptLevel::Os) {
      llvmLevel = llvm::OptimizationLevel::Os;
    } else if (level == OptLevel::Oz) {
      llvmLevel = llvm::OptimizationLevel::Oz;
    }
    pipeline = builder.buildPerModuleDefaultPipeline(llvmLevel);
    addCoroutinePasses(pipeline);
  }
  pipeline.run(module, modules);
  return true;
}

} // namespace sere
