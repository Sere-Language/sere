/// @file Prelude.h
/// Discovers and parses the Sere standard library prelude.

#pragma once

#include "sere/ast/Syntax.h"

#include <filesystem>
#include <memory>

namespace sere {

class DiagnosticEngine;
class SourceOverlay;

[[nodiscard]] std::filesystem::path findStdlibDirectory(const std::filesystem::path& compilerDir);

[[nodiscard]] std::unique_ptr<Module> parsePrelude(DiagnosticEngine& diagnostics,
                                                   const std::filesystem::path& stdlibDir,
                                                   const SourceOverlay* overlay = nullptr);

[[nodiscard]] bool loadPrelude(Module& userModule,
                               DiagnosticEngine& diagnostics,
                               const std::filesystem::path& stdlibDir,
                               const SourceOverlay* overlay = nullptr);

}  // namespace sere
