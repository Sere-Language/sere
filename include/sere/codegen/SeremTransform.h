/// @file SeremTransform.h
/// Rewrite passes over the target-independent Serem IR.

#pragma once

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

/// The pipeline `runTransformers` applies, in the order the passes must run:
/// dead code first so the later passes do not spend time on unreachable work.
[[nodiscard]] std::vector<std::unique_ptr<TransformPass>> defaultTransformPasses();

/// Applies `defaultTransformPasses()` until they stop changing the module and
/// returns the number of passes that reported a change.
int runTransformers(IRModule& module);

} // namespace sere::serem
