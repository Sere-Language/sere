/// @file SeremTransform.h
/// Rewrite passes over the target-independent Serem IR.

#pragma once

#include "sere/codegen/OptPipeline.h"
#include "sere/codegen/Serem.h"

#include <memory>
#include <string_view>
#include <vector>

namespace sere::serem {

/// One rewrite over a whole module.
///
/// `run` reports whether the module changed so the driver can iterate a
/// pipeline to a fixed point without any pass having to know about the others.
class TransformPass {
public:
  virtual ~TransformPass() = default;
  [[nodiscard]] virtual std::string_view name() const = 0;
  virtual bool run(IRModule& module) = 0;
};

/// Folds arithmetic, comparisons, casts, and constant branches into constants.
[[nodiscard]] std::unique_ptr<TransformPass> makeConstantFoldPass();

/// Keeps only the definitions reachable from the module entry point.
[[nodiscard]] std::unique_ptr<TransformPass> makeDeadCodePass();

/// Removes blocks no branch can reach, such as the arm of a folded branch.
[[nodiscard]] std::unique_ptr<TransformPass> makeUnreachableBlockPass();

/// Drops string literals no surviving function still references.
[[nodiscard]] std::unique_ptr<TransformPass> makeUnusedGlobalPass();

/// Identity and absorbing rewrites plus strength reduction: `x * 8` becomes
/// `x << 3`, unsigned `x / 8` becomes `x >> 3`, and `x & 15` replaces
/// unsigned `x % 16`.
[[nodiscard]] std::unique_ptr<TransformPass> makeStrengthReducePass();

/// Replaces repeated pure computations in a block with the first result.
[[nodiscard]] std::unique_ptr<TransformPass> makeCommonSubexpressionPass();

/// Removes the pending-error bookkeeping (`error.enter`, `error.leave`,
/// `error.bind`) and turns the handler dispatch (`error.isa`) into `false`, so
/// the exception machinery disappears from a release build.
[[nodiscard]] std::unique_ptr<TransformPass> makeRuntimeCheckStripPass();

/// The pipeline `runTransformers` applies, in the order the passes must run:
/// dead code first so the later passes do not spend time on unreachable work.
[[nodiscard]] std::vector<std::unique_ptr<TransformPass>> defaultTransformPasses();

/// The pipeline the optimizer switches select. Every level keeps the baseline
/// cleanups; the switches add the passes they name.
[[nodiscard]] std::vector<std::unique_ptr<TransformPass>>
transformPasses(const OptimizationOptions& options);

/// Runs the switch-selected pipeline to a fixed point.
int runOptimizingTransformers(IRModule& module, const OptimizationOptions& options);

/// Applies `defaultTransformPasses()` until they stop changing the module and
/// returns the number of passes that reported a change.
int runTransformers(IRModule& module);

} // namespace sere::serem
