/// @file Installer.h
/// Builds a Windows setup exe that installs the full Sere toolchain.

#pragma once

#include "sere/driver/Options.h"

namespace sere {

/// Stage compiler, stdlib, LLVM, runtime, and editor bits, then compile an
/// Inno Setup installer. Windows only.
[[nodiscard]] int buildInstaller(const CompilerOptions& options);

}  // namespace sere
