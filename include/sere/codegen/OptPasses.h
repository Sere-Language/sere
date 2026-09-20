/// @file OptPasses.h
/// Sere-specific IR lowering passes that run before the LLVM pipeline.
///
/// The passes read the same `OptimizationOptions` the CLI filled in, so the
/// release-mode switches (`--no-runtime-checks`, `--stack-alloc`,
/// `--arena-alloc`, ...) are decided in one place. They never run the generic
/// LLVM pipeline; they only rewrite the module the generator produced.

#pragma once

#include "sere/codegen/OptPipeline.h"

#include <cstdint>

namespace llvm {
class Module;
}

namespace sere {

/// What the pre-pipeline passes changed, for `SERE_OPT_REPORT=1`.
struct OptRewriteReport {
  std::uint32_t annotatedNonNull = 0;
  std::uint32_t annotatedNoUnwind = 0;
  std::uint32_t nullChecksRemoved = 0;
  std::uint32_t checkBlocksRemoved = 0;
  std::uint32_t boundsCheckBlocksRemoved = 0;
  std::uint32_t blocksSimplified = 0;
  std::uint32_t allocationsStackPromoted = 0;
  std::uint32_t freesElided = 0;
  std::uint32_t fastMathFunctions = 0;
  std::uint32_t tailCallFunctions = 0;
};

/// Applies every release-mode rewrite `options` asks for, in dependency order:
/// annotations first (so the check eliminators know what is provably non-null),
/// then check elimination, then allocation shaping, then attributes.
[[nodiscard]] OptRewriteReport runPrePipelinePasses(llvm::Module& module,
                                                    const OptimizationOptions& options);

/// Marks allocation results `nonnull` and runtime accessors `nounwind`.
[[nodiscard]] std::uint32_t annotateRuntimeDeclarations(llvm::Module& module,
                                                        const OptimizationOptions& options);

/// Folds `icmp eq/ne ptr %p, null` when `%p` is provably non-null.
[[nodiscard]] std::uint32_t eliminateNullChecks(llvm::Module& module);

/// Rewrites conditional branches whose failure arm only reports a check.
[[nodiscard]] std::uint32_t
eliminateCheckBlocks(llvm::Module& module, bool removeRuntimeChecks, bool removeBoundsChecks);

/// Promotes non-escaping `sere_alloc`/`sere_gc_alloc` calls to stack slots.
[[nodiscard]] std::uint32_t promoteNonEscapingAllocations(llvm::Module& module);

/// Elides `sere_free`/`sere_gc_free` calls so the collector frees in bulk.
[[nodiscard]] std::uint32_t elideIndividualFrees(llvm::Module& module);

/// Relaxes floating point on every definition.
[[nodiscard]] std::uint32_t applyFastMathAttributes(llvm::Module& module);

/// Sets `disable-tail-calls` on every definition.
[[nodiscard]] std::uint32_t applyTailCallAttributes(llvm::Module& module, bool allowTailCalls);

/// Marks every non-entry definition `alwaysinline` for `--inline-all`.
void markAlwaysInline(llvm::Module& module);

/// Records the release switches on the module so the emitted IR states them.
void stampLoweringFlags(llvm::Module& module, const OptimizationOptions& options);

} // namespace sere
