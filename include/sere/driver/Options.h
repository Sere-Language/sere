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
  InitLib,
  Build,
  Pack,
  Run,
  Clean,
  Shell,
  BuildInstaller,
  RefreshBin,
  Update,
  UpdateLocal,
  Login,
  Logout,
  Publish,
};

struct CompilerOptions {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  bool emitLlvm = false;
  bool emitAsm = false;
  bool dumpTokens = false;
  bool dumpAst = false;
  bool dumpSymbols = false;
  bool dumpLlvmIrRaw = false;
  bool analyze = false;
  bool lsp = false;
  bool help = false;
  bool version = false;
  bool printEnv = false;
  bool optOverridden = false;
  ProjectCommand projectCommand = ProjectCommand::None;
  std::filesystem::path initName;
  bool initLibrary = false;
  std::string shellHost;
  /// Registry publish token: `sere login <token>` or `--token`.
  std::string authToken;
  /// `sere publish --dry-run`: describe the upload without sending it.
  bool dryRun = false;
  std::vector<std::string> programArgs;
  std::vector<std::filesystem::path> linkLibraries;
  ColorMode colorMode = ColorMode::Auto;
  OptLevel optLevel = OptLevel::O0;
  std::string passes;
};

[[nodiscard]] bool
parseCommandLine(int argc, char** argv, CompilerOptions& options, std::string& error);

} // namespace sere
