/// @file Options.h
/// Command-line options for the sere driver.

#pragma once

#include "sere/codegen/OptPipeline.h"
#include "sere/diag/DiagnosticEngine.h"

#include <filesystem>
#include <string>
#include <vector>

namespace sere {

enum class ProjectCommand {
  None = 0,
  Init,
  Build,
  Run,
  Clean,
  Shell,
  BuildInstaller,
};

struct CompilerOptions {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  bool emitLlvm = false;
  bool dumpTokens = false;
  bool analyze = false;
  bool lsp = false;
  bool help = false;
  bool version = false;
  bool optOverridden = false;
  ProjectCommand projectCommand = ProjectCommand::None;
  std::filesystem::path initName;
  std::string shellHost;
  std::vector<std::string> programArgs;
  std::vector<std::filesystem::path> linkLibraries;
  ColorMode colorMode = ColorMode::Auto;
  OptLevel optLevel = OptLevel::O0;
  std::string passes;
};

[[nodiscard]] bool parseCommandLine(int argc, char** argv, CompilerOptions& options,
                                    std::string& error);

}  // namespace sere
