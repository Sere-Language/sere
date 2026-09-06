/// @file Compiler.h
/// Orchestrates lex, parse, LLVM IR emission, and linking.

#pragma once

#include "sere/driver/Options.h"

namespace sere {

class Compiler {
public:
  [[nodiscard]] int run(const CompilerOptions& options);
};

[[nodiscard]] int compileInput(const CompilerOptions& options);

} // namespace sere
