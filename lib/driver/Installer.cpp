/// @file Installer.cpp
/// Stages the Sere toolchain and compiles a Windows Inno Setup installer.

#include "sere/driver/Installer.h"

#include "sere/ToolchainPaths.h"
#include "sere/Version.h"
#include "sere/driver/Toolchain.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace sere {
int buildInstaller(const CompilerOptions& options) {
#ifndef _WIN32
  llvm::errs() << "error: use releases/stage.sh to package Linux releases\n";
  return 1;
#else
  const auto root = std::filesystem::path(SERE_SOURCE_DIR);
  const auto script = root / "releases" / "stage.ps1";
  if (!std::filesystem::is_regular_file(script)) {
    llvm::errs() << "error: release packaging requires a Sere source checkout\n";
    return 1;
  }
  const auto shell = llvm::sys::findProgramByName("powershell.exe");
  if (!shell)
    return 1;
  std::vector<std::string> owned{
      *shell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script.string()};
  if (!options.outputPath.empty()) {
    owned.push_back("-InstallerOutput");
    owned.push_back(std::filesystem::absolute(options.outputPath).string());
  }
  std::vector<llvm::StringRef> args;
  for (const auto& arg : owned)
    args.push_back(arg);
  return llvm::sys::ExecuteAndWait(*shell, args);
#endif
}
} // namespace sere
