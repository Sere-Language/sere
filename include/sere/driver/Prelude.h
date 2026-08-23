/// @file Prelude.h
/// Discovers and parses the Sere standard library prelude.

#pragma once

#include "sere/ast/Syntax.h"

#include <filesystem>

namespace sere {

class DiagnosticEngine;

[[nodiscard]] std::filesystem::path findStdlibDirectory(const std::filesystem::path& compilerDir);

[[nodiscard]] bool loadPrelude(Module& userModule,
                               DiagnosticEngine& diagnostics,
                               const std::filesystem::path& stdlibDir);

}  // namespace sere
