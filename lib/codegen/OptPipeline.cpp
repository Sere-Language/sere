/// @file OptPipeline.cpp
/// Sere's optimization switches, the release-mode rewrites they imply, and the
/// LLVM pipeline they select.

#include "sere/codegen/OptPipeline.h"

#include "sere/codegen/OptPasses.h"

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace sere {
namespace {

/// Coroutine splitting must run before any middle-end pass that could clone or
/// inline an unsplit coroutine.
constexpr std::string_view kCoroPipeline = "coro-early,coro-split,coro-cleanup";

/// Appends `pass` to a comma-separated pipeline, skipping empty ones.
void appendPass(std::string& pipeline, std::string_view pass) {
  if (pass.empty()) {
    return;
  }
  if (!pipeline.empty()) {
    pipeline += ',';
  }
  pipeline += pass;
}

/// Copies the switch state a preset implies, leaving identity fields alone.
void copyPreset(const OptimizationOptions& preset, OptimizationOptions& options) {
  options.level = preset.level;
  options.inlineExpansion = preset.inlineExpansion;
  options.inlineAll = preset.inlineAll;
  options.constFold = preset.constFold;
  options.deadCode = preset.deadCode;
  options.peephole = preset.peephole;
  options.cse = preset.cse;
  options.strengthReduce = preset.strengthReduce;
  options.loopUnroll = preset.loopUnroll;
  options.loopInvariantHoist = preset.loopInvariantHoist;
  options.vectorize = preset.vectorize;
  options.branchOpt = preset.branchOpt;
  options.tailCalls = preset.tailCalls;
}

/// The text `SERE_OPT_REPORT=1` prints.
void reportRewrites(const OptRewriteReport& report) {
  llvm::errs() << "[sere-opt] nonnull=" << report.annotatedNonNull
               << " nounwind=" << report.annotatedNoUnwind
               << " null-checks=" << report.nullChecksRemoved
               << " error-checks=" << report.errorChecksRemoved
               << " checks=" << report.checkBlocksRemoved
               << " stack=" << report.allocationsStackPromoted
               << " frees-elided=" << report.freesElided
               << " fast-math-fns=" << report.fastMathFunctions
               << " tailcall-fns=" << report.tailCallFunctions << '\n';
}

[[nodiscard]] bool optimizationReportEnabled() {
  const char* value = std::getenv("SERE_OPT_REPORT");
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

/// The pipeline built from the level preset and the individually enabled
/// passes.
///
/// Sere always composes its own pipeline instead of calling LLVM's level
/// defaults, so every switch in `OptimizationOptions` selects a real pass and
/// the pipeline is identical for `--emit-llvm`, `--emit-asm`, and linking.
[[nodiscard]] std::string buildFlagPipeline(const OptimizationOptions& options) {
  const bool aggressive = options.level == OptLevel::O3;
  const bool tuned = options.level != OptLevel::O0;

  std::string moduleStart;
  std::string functionPasses;
  std::string moduleMiddle;
  std::string cleanupPasses;
  std::string moduleEnd;

  // ----- before the function pipeline -------------------------------------
  if (options.inlineAll) {
    appendPass(moduleStart, "always-inline");
  }
  if (options.constFold) {
    appendPass(moduleStart, "globalopt");
    appendPass(moduleStart, "ipsccp");
  }

  // ----- per function -----------------------------------------------------
  if (tuned) {
    appendPass(functionPasses, "sroa");
    appendPass(functionPasses, "mem2reg");
  }
  if (options.constFold) {
    appendPass(functionPasses, "sccp");
  }
  if (options.peephole) {
    appendPass(functionPasses, "instcombine");
    appendPass(functionPasses, "dse");
    appendPass(functionPasses, "memcpyopt");
    appendPass(functionPasses, "mergeicmps");
  }
  if (options.cse) {
    appendPass(functionPasses, "early-cse<memssa>");
  }
  if (options.strengthReduce) {
    appendPass(functionPasses, "aggressive-instcombine");
    appendPass(functionPasses, "reassociate");
    appendPass(functionPasses, "div-rem-pairs");
    appendPass(functionPasses, "constraint-elimination");
  }
  if (options.cse) {
    appendPass(functionPasses, "gvn");
    appendPass(functionPasses, "gvn-hoist");
  }
  if (options.branchOpt) {
    appendPass(functionPasses, "simplifycfg");
    appendPass(functionPasses, "correlated-propagation");
    appendPass(functionPasses, "tailcallelim");
  }
  if (options.constFold) {
    appendPass(functionPasses, "float2int");
  }
  if (options.loopInvariantHoist) {
    appendPass(functionPasses, "loop-simplify");
    appendPass(functionPasses, "lcssa");
    appendPass(functionPasses, "loop-rotate");
    appendPass(functionPasses, "loop-idiom");
    appendPass(functionPasses, "licm");
    appendPass(functionPasses, "loop-deletion");
    appendPass(functionPasses, "loop-instsimplify");
  }
  if (options.loopUnroll) {
    appendPass(functionPasses, "loop-unroll");
  }
  if (options.vectorize) {
    appendPass(functionPasses, "loop-vectorize");
    appendPass(functionPasses, "slp-vectorizer");
    appendPass(functionPasses, "vector-combine");
  }
  if (options.peephole) {
    appendPass(functionPasses, "instcombine");
  }
  if (options.tailCalls) {
    appendPass(functionPasses, "tailcallelim");
  }
  if (options.deadCode) {
    appendPass(functionPasses, "adce");
    appendPass(functionPasses, "dce");
  }
  if (options.peephole || options.constFold) {
    appendPass(functionPasses, "instcombine");
  }

  // ----- between the two function rounds ----------------------------------
  if (options.inlineExpansion) {
    appendPass(moduleMiddle, "inline");
  }
  if (options.constFold) {
    appendPass(moduleMiddle, "ipsccp");
  }
  if (options.inlineExpansion || options.inlineAll) {
    appendPass(moduleMiddle, "function-attrs");
  }
  if (options.deadCode) {
    appendPass(moduleMiddle, "globaldce");
    appendPass(moduleMiddle, "strip-dead-prototypes");
  }
  if (options.inlineExpansion || options.inlineAll) {
    appendPass(moduleMiddle, "deadargelim");
  }

  // ----- cleanup ----------------------------------------------------------
  if (options.branchOpt) {
    appendPass(cleanupPasses, "jump-threading");
  }
  if (tuned) {
    appendPass(cleanupPasses, "simplifycfg");
    appendPass(cleanupPasses, "instcombine");
    appendPass(cleanupPasses, "adce");
  }
  if (options.deadCode) {
    appendPass(moduleEnd, "globaldce");
  }

  std::string pipeline;
  appendPass(pipeline, moduleStart);
  if (!functionPasses.empty()) {
    appendPass(pipeline, "function(" + functionPasses + ")");
  }
  appendPass(pipeline, moduleMiddle);
  // Each extra round sees what inlining and vectorization exposed. O3 runs the
  // function pipeline three times because on already-canonical IR the later
  // rounds are where the remaining loop and memory wins are.
  if (tuned && !functionPasses.empty()) {
    appendPass(pipeline, "function(" + functionPasses + ")");
  }
  if (aggressive && !functionPasses.empty()) {
    appendPass(pipeline, "function(" + functionPasses + ")");
  }
  if (!cleanupPasses.empty()) {
    appendPass(pipeline, "cgscc(" + cleanupPasses + ")");
  }
  appendPass(pipeline, moduleEnd);
  return pipeline;
}

} // namespace

OptimizationOptions optimizationPreset(OptLevel level) {
  OptimizationOptions preset;
  preset.level = level;
  switch (level) {
  case OptLevel::O0:
    break;
  case OptLevel::O1:
    preset.inlineExpansion = true;
    preset.constFold = true;
    preset.deadCode = true;
    preset.peephole = true;
    preset.cse = true;
    preset.strengthReduce = true;
    preset.loopInvariantHoist = true;
    preset.branchOpt = true;
    preset.tailCalls = true;
    break;
  case OptLevel::O2:
    preset = optimizationPreset(OptLevel::O1);
    preset.level = OptLevel::O2;
    preset.vectorize = true;
    preset.loopUnroll = true;
    break;
  case OptLevel::O3:
    preset = optimizationPreset(OptLevel::O2);
    preset.level = OptLevel::O3;
    preset.inlineAll = true;
    preset.loopUnroll = true;
    break;
  case OptLevel::Os:
    preset = optimizationPreset(OptLevel::O2);
    preset.level = OptLevel::Os;
    preset.loopUnroll = false;
    preset.inlineAll = false;
    break;
  case OptLevel::Oz:
    preset.level = OptLevel::Oz;
    preset.peephole = true;
    preset.constFold = true;
    preset.deadCode = true;
    preset.branchOpt = true;
    break;
  }
  return preset;
}

void applyOptLevel(OptimizationOptions& options, OptLevel level) {
  const OptimizationOptions preset = optimizationPreset(level);
  const bool levelWasExplicit = options.levelExplicit;
  const std::string passes = options.passes;
  copyPreset(preset, options);
  options.levelExplicit = levelWasExplicit;
  // A level names a whole pipeline, so per-pass switches named before it no
  // longer select the flag-composed pipeline; ones named after it still do.
  options.tailCallsExplicit = false;
  options.passFlagsExplicit = false;
  options.passes = passes;
}

bool OptimizationOptions::hasExplicitPassFlags() const { return passFlagsExplicit; }

bool parseOptLevel(std::string_view text, OptLevel& level, std::string& error) {
  if (text == "0" || text == "O0" || text == "-O0") {
    level = OptLevel::O0;
    return true;
  }
  if (text == "1" || text == "O1" || text == "-O1") {
    level = OptLevel::O1;
    return true;
  }
  if (text == "2" || text == "O2" || text == "-O2") {
    level = OptLevel::O2;
    return true;
  }
  if (text == "3" || text == "O3" || text == "-O3") {
    level = OptLevel::O3;
    return true;
  }
  if (text == "s" || text == "Os" || text == "-Os") {
    level = OptLevel::Os;
    return true;
  }
  if (text == "z" || text == "Oz" || text == "-Oz") {
    level = OptLevel::Oz;
    return true;
  }
  error = "invalid --opt level '" + std::string(text) + "' (expected O0, O1, O2, O3, Os, Oz)";
  return false;
}

bool parseOptimizationFlag(std::string_view argument,
                           OptimizationOptions& options,
                           bool& handled,
                           std::string& error) {
  handled = true;
  auto setPass = [&](bool& slot, bool value) {
    slot = value;
    options.passFlagsExplicit = true;
  };
  auto setFlagWithOff = [&](std::string_view on, std::string_view off, bool& slot) -> bool {
    if (argument == on) {
      setPass(slot, true);
      return true;
    }
    if (argument == off) {
      setPass(slot, false);
      return true;
    }
    return false;
  };

  // Aggregate levels, covering `-O2`, `--O2`, and a bare `-O`.
  {
    std::string_view body = argument;
    while (!body.empty() && body.front() == '-') {
      body.remove_prefix(1);
    }
    if (!body.empty() && body.front() == 'O') {
      const std::string_view suffix = body.substr(1);
      if (suffix.empty()) {
        applyOptLevel(options, OptLevel::O2);
        options.levelExplicit = true;
        return true;
      }
      OptLevel level = OptLevel::O0;
      if (!parseOptLevel(suffix, level, error)) {
        return false;
      }
      applyOptLevel(options, level);
      options.levelExplicit = true;
      return true;
    }
  }
  if (argument == "--release") {
    applyOptLevel(options, OptLevel::O3);
    options.levelExplicit = true;
    options.runtimeChecks = false;
    options.boundsChecks = false;
    options.nullChecks = false;
    options.stackAlloc = true;
    options.tailCalls = true;
    options.tailCallsExplicit = true;
    options.fastMath = true;
    return true;
  }
  if (argument == "--debug") {
    applyOptLevel(options, OptLevel::O0);
    options.levelExplicit = true;
    options.runtimeChecks = true;
    options.boundsChecks = true;
    options.nullChecks = true;
    options.stackAlloc = false;
    options.arenaAlloc = false;
    return true;
  }
  if (argument == "--fast-math") {
    options.fastMath = true;
    return true;
  }
  if (argument == "--no-fast-math") {
    options.fastMath = false;
    return true;
  }
  if (argument == "--inline-all" || argument == "--inline-all-no-limits") {
    setPass(options.inlineAll, true);
    options.inlineExpansion = true;
    return true;
  }
  if (argument == "--no-inline-all") {
    setPass(options.inlineAll, false);
    return true;
  }
  if (argument == "--inline" || argument == "--inline-expansion") {
    setPass(options.inlineExpansion, true);
    return true;
  }
  if (argument == "--no-inline") {
    setPass(options.inlineExpansion, false);
    options.inlineAll = false;
    return true;
  }
  if (setFlagWithOff("--const-fold", "--no-const-fold", options.constFold) ||
      setFlagWithOff("--constant-fold", "--no-constant-fold", options.constFold) ||
      setFlagWithOff("--dead-code", "--no-dead-code", options.deadCode) ||
      setFlagWithOff("--dce", "--no-dce", options.deadCode) ||
      setFlagWithOff("--peephole", "--no-peephole", options.peephole) ||
      setFlagWithOff("--cse", "--no-cse", options.cse) ||
      setFlagWithOff("--gvn", "--no-gvn", options.cse) ||
      setFlagWithOff("--strength-reduce", "--no-strength-reduce", options.strengthReduce) ||
      setFlagWithOff("--loop-unroll", "--no-loop-unroll", options.loopUnroll) ||
      setFlagWithOff("--unroll-loops", "--no-unroll-loops", options.loopUnroll) ||
      setFlagWithOff("--loop-invariant-hoist", "--no-loop-invariant-hoist",
                     options.loopInvariantHoist) ||
      setFlagWithOff("--hoist", "--no-hoist", options.loopInvariantHoist) ||
      setFlagWithOff("--licm", "--no-licm", options.loopInvariantHoist) ||
      setFlagWithOff("--vectorize", "--no-vectorize", options.vectorize) ||
      setFlagWithOff("--branch-opt", "--no-branch-opt", options.branchOpt) ||
      setFlagWithOff("--branch-optimize", "--no-branch-optimize", options.branchOpt)) {
    return true;
  }
  if (argument == "--tailcalls" || argument == "--tail-calls" || argument == "--tailcall-opt") {
    options.tailCalls = true;
    options.tailCallsExplicit = true;
    return true;
  }
  if (argument == "--no-tailcalls" || argument == "--no-tail-calls") {
    options.tailCalls = false;
    options.tailCallsExplicit = true;
    return true;
  }
  if (argument == "--lto" || argument == "--lto=full" || argument == "--lto=thin") {
    options.lto = true;
    return true;
  }
  if (argument == "--no-lto") {
    options.lto = false;
    return true;
  }
  if (argument == "--no-runtime-checks") {
    options.runtimeChecks = false;
    return true;
  }
  if (argument == "--runtime-checks") {
    options.runtimeChecks = true;
    return true;
  }
  if (argument == "--no-bounds-checks") {
    options.boundsChecks = false;
    return true;
  }
  if (argument == "--bounds-checks") {
    options.boundsChecks = true;
    return true;
  }
  if (argument == "--no-null-checks") {
    options.nullChecks = false;
    return true;
  }
  if (argument == "--null-checks") {
    options.nullChecks = true;
    return true;
  }
  if (argument == "--stack-alloc") {
    options.stackAlloc = true;
    return true;
  }
  if (argument == "--no-stack-alloc") {
    options.stackAlloc = false;
    return true;
  }
  if (argument == "--arena-alloc") {
    options.arenaAlloc = true;
    return true;
  }
  if (argument == "--no-arena-alloc") {
    options.arenaAlloc = false;
    return true;
  }
  handled = false;
  return true;
}

std::vector<std::string> clangCodegenFlags(const OptimizationOptions& options) {
  std::vector<std::string> flags;
  switch (options.level) {
  case OptLevel::O0:
    flags.emplace_back("-O0");
    break;
  case OptLevel::O1:
    flags.emplace_back("-O1");
    break;
  case OptLevel::O2:
    flags.emplace_back("-O2");
    break;
  case OptLevel::O3:
    flags.emplace_back("-O3");
    break;
  case OptLevel::Os:
    flags.emplace_back("-Os");
    break;
  case OptLevel::Oz:
    flags.emplace_back("-Oz");
    break;
  }
  if (options.level != OptLevel::O0) {
    flags.emplace_back("-fomit-frame-pointer");
    // Small pools and generated code: the unwind tables Sere never unwinds
    // through cost load time and binary size.
    flags.emplace_back("-fno-asynchronous-unwind-tables");
    flags.emplace_back("-fno-unwind-tables");
    // Tune the backend to the machine doing the build, the way a native
    // toolchain would.
    flags.emplace_back("-march=native");
    flags.emplace_back("-mtune=native");
  }
  if (options.fastMath) {
    flags.emplace_back("-ffast-math");
    flags.emplace_back("-fno-math-errno");
    flags.emplace_back("-fno-trapping-math");
    flags.emplace_back("-fno-signed-zeros");
    flags.emplace_back("-ffp-contract=fast");
  }
  if (options.loopUnroll) {
    flags.emplace_back("-funroll-loops");
  }
  if (options.vectorize) {
    flags.emplace_back("-fvectorize");
    flags.emplace_back("-fslp-vectorize");
  }
  if (options.lto) {
    flags.emplace_back("-flto");
  }
  if (!options.runtimeChecks && !options.boundsChecks) {
    // The checks are gone from the IR; keep the backend from re-introducing
    // traps on the arithmetic they guarded.
    flags.emplace_back("-fwrapv");
  }
  return flags;
}

std::string describeOptimization(const OptimizationOptions& options) {
  std::string text;
  switch (options.level) {
  case OptLevel::O0:
    text = "O0";
    break;
  case OptLevel::O1:
    text = "O1";
    break;
  case OptLevel::O2:
    text = "O2";
    break;
  case OptLevel::O3:
    text = "O3";
    break;
  case OptLevel::Os:
    text = "Os";
    break;
  case OptLevel::Oz:
    text = "Oz";
    break;
  }
  auto note = [&text](std::string_view name, bool on) {
    if (on) {
      text += ',';
      text += name;
    }
  };
  note("inline", options.inlineExpansion);
  note("inline-all", options.inlineAll);
  note("const-fold", options.constFold);
  note("dead-code", options.deadCode);
  note("peephole", options.peephole);
  note("cse", options.cse);
  note("strength-reduce", options.strengthReduce);
  note("loop-unroll", options.loopUnroll);
  note("licm", options.loopInvariantHoist);
  note("tailcalls", options.tailCalls);
  note("fast-math", options.fastMath);
  note("vectorize", options.vectorize);
  note("branch-opt", options.branchOpt);
  note("lto", options.lto);
  note("stack-alloc", options.stackAlloc);
  note("arena-alloc", options.arenaAlloc);
  if (!options.runtimeChecks) {
    text += ",no-runtime-checks";
  }
  if (!options.boundsChecks) {
    text += ",no-bounds-checks";
  }
  if (!options.nullChecks) {
    text += ",no-null-checks";
  }
  return text;
}

bool runOptPipeline(llvm::Module& module,
                    const OptimizationOptions& options,
                    std::string& error) {
  if (std::getenv("SERE_DUMP_RAW_IR") != nullptr) {
    std::error_code dumpEc;
    llvm::raw_fd_ostream dumpOut("sere-coro-before.ll", dumpEc, llvm::sys::fs::OF_Text);
    if (!dumpEc) {
      module.print(dumpOut, nullptr);
    }
  }

  const OptRewriteReport report = runPrePipelinePasses(module, options);
  if (optimizationReportEnabled()) {
    reportRewrites(report);
    llvm::errs() << "[sere-opt] pipeline: " << describeOptimization(options) << '\n';
  }

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

  llvm::OptimizationLevel llvmLevel = llvm::OptimizationLevel::O1;
  switch (options.level) {
  case OptLevel::O0:
    llvmLevel = llvm::OptimizationLevel::O0;
    break;
  case OptLevel::O1:
    llvmLevel = llvm::OptimizationLevel::O1;
    break;
  case OptLevel::O2:
    llvmLevel = llvm::OptimizationLevel::O2;
    break;
  case OptLevel::O3:
    llvmLevel = llvm::OptimizationLevel::O3;
    break;
  case OptLevel::Os:
    llvmLevel = llvm::OptimizationLevel::Os;
    break;
  case OptLevel::Oz:
    llvmLevel = llvm::OptimizationLevel::Oz;
    break;
  }

  llvm::ModulePassManager pipeline;
  if (!options.passes.empty()) {
    if (auto failed = builder.parsePassPipeline(pipeline, options.passes)) {
      error = llvm::toString(std::move(failed));
      return false;
    }
  } else if (options.lto) {
    // A prelink pipeline is what `-flto` expects: it keeps the cross-module
    // surface visible while still specializing what it can.
    pipeline = builder.buildLTOPreLinkDefaultPipeline(llvmLevel);
  } else {
    // Every level is composed here, so a switch that is off really removes its
    // pass instead of being re-added by a preset pipeline.
    const std::string text = buildFlagPipeline(options);
    if (!text.empty()) {
      if (auto failed = builder.parsePassPipeline(pipeline, text)) {
        error = "cannot build pipeline '" + text + "': " + llvm::toString(std::move(failed));
        return false;
      }
    }
  }

  // Coroutine lowering must run before any middle-end pass that could clone or
  // inline an unsplit coroutine. Parsing the textual pipeline mirrors what the
  // standalone `opt` driver does and is proven to split Sere's coroutine shape.
  llvm::ModulePassManager coroPipeline;
  if (auto failed = builder.parsePassPipeline(coroPipeline, std::string(kCoroPipeline))) {
    error = llvm::toString(std::move(failed));
    return false;
  }
  coroPipeline.run(module, modules);
  pipeline.run(module, modules);
  return true;
}

} // namespace sere
