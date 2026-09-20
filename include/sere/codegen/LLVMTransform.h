/// @file LLVMTransform.h
/// Always-on cleanup passes for the LLVM module.
///
/// Sibling of `SeremTransform`: at the default `--opt=O0` the module reaches the
/// printer exactly as the generator wrote it, so `print(1 + 1)` stays a runtime
/// add and every prelude helper the program never calls still appears in the
/// output. These passes are the LLVM-side equivalents of the Serem transformer
/// stages, and running them for the direct and the Serem backend alike keeps
/// `--emit-llvm` showing the same program `--emit-serem` does.

#pragma once

#include <string>

namespace llvm {
class Module;
}

namespace sere {

/// Applies the transformer pipeline to `module`.
///
/// Returns the number of stages that ran, or -1 with `error` set when a stage
/// could not be constructed.
[[nodiscard]] int runLLVMTransformers(llvm::Module& module, std::string& error);

} // namespace sere
