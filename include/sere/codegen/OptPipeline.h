/// @file OptPipeline.h
/// Configurable LLVM optimization pipeline for sere.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace llvm {
class Module;
}

namespace sere {

enum class OptLevel {
  O0,
  O1,
  O2,
  O3,
  Os,
  Oz,
};

/// Every optimization switch the driver understands.
///
/// The struct is the single source of truth for the optimizer: command-line
/// parsing fills it in, `runOptPipeline` reads it, and the native toolchain
/// flags are derived from it so the IR pipeline and the backend agree on what
/// was requested.
struct OptimizationOptions {
  /// Aggregate level. Presets fan out into the individual switches below.
  OptLevel level = OptLevel::O0;
  /// True when the user named a level (`--O2`, `--opt=O3`, ...).
  bool levelExplicit = false;
  /// Raw PassBuilder pipeline; when set it replaces the built-in one.
  std::string passes;

  // ----- middle-end passes -------------------------------------------------
  /// `--inline` / `--no-inline`.
  bool inlineExpansion = false;
  /// `--inline-all`: every non-entry function is marked `alwaysinline`.
  bool inlineAll = false;
  /// `--const-fold` / `--no-const-fold`: constant propagation and SCCP.
  bool constFold = false;
  /// `--dead-code` / `--no-dead-code`: aggressive DCE plus DCE.
  bool deadCode = false;
  /// `--peephole` / `--no-peephole`: instruction combining.
  bool peephole = false;
  /// `--cse` / `--no-cse`: early CSE plus GVN.
  bool cse = false;
  /// `--strength-reduce` / `--no-strength-reduce`.
  bool strengthReduce = false;
  /// `--loop-unroll` / `--no-loop-unroll`.
  bool loopUnroll = false;
  /// `--loop-invariant-hoist` / `--no-loop-invariant-hoist`.
  bool loopInvariantHoist = false;
  /// `--tailcalls` / `--no-tailcalls`.
  bool tailCalls = false;
  /// True when `--tailcalls`/`--no-tailcalls` was given explicitly.
  bool tailCallsExplicit = false;
  /// `--fast-math`: relaxed floating point on every definition.
  bool fastMath = false;
  /// `--vectorize` / `--no-vectorize`.
  bool vectorize = false;
  /// `--branch-opt` / `--no-branch-opt`.
  bool branchOpt = false;
  /// `--lto`: prelink pipeline plus `-flto` on the native toolchain.
  bool lto = false;

  // ----- release-mode lowering --------------------------------------------
  /// `--no-runtime-checks`: drop every compiler-inserted check.
  bool runtimeChecks = true;
  /// `--no-bounds-checks`: drop indexing checks and mark accessors side-effect free.
  bool boundsChecks = true;
  /// `--no-null-checks`: assume allocation results are never null.
  bool nullChecks = true;

  // ----- allocation --------------------------------------------------------
  /// `--stack-alloc`: promote non-escaping allocations to stack slots.
  bool stackAlloc = false;
  /// `--arena-alloc`: never return single blocks; let the collector bulk-free.
  bool arenaAlloc = false;

  /// True once a per-pass switch (`--inline`, `--cse`, ...) was named, which
  /// selects the flag-composed pipeline over the level's default one.
  bool passFlagsExplicit = false;

  /// True when any per-pass switch differs from where the level left it.
  [[nodiscard]] bool hasExplicitPassFlags() const;
};

/// The switch state a named level implies.
[[nodiscard]] OptimizationOptions optimizationPreset(OptLevel level);

/// Applies `level`'s preset to `options`, keeping nothing else.
void applyOptLevel(OptimizationOptions& options, OptLevel level);

/// Parses a level name: `0`, `O2`, `s`, `Oz`, ... into `level`.
[[nodiscard]] bool parseOptLevel(std::string_view text, OptLevel& level, std::string& error);

/// Handles one optimization flag. `handled` is false when `argument` is not an
/// optimization flag, so the caller can keep looking.
[[nodiscard]] bool parseOptimizationFlag(std::string_view argument,
                                         OptimizationOptions& options,
                                         bool& handled,
                                         std::string& error);

/// Runs the Sere pre-pipeline passes followed by the LLVM pipeline.
[[nodiscard]] bool
runOptPipeline(llvm::Module& module, const OptimizationOptions& options, std::string& error);

/// Flags for the native toolchain, so the backend runs the same level.
[[nodiscard]] std::vector<std::string> clangCodegenFlags(const OptimizationOptions& options);

/// One-line summary of the enabled switches, for `--print-env` and diagnostics.
[[nodiscard]] std::string describeOptimization(const OptimizationOptions& options);

} // namespace sere
