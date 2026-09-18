/// @file Packages.h
/// `sere add`: resolves a published package, verifies it, and installs it into
/// the project's `libs/` directory.

#pragma once

#include "sere/driver/Options.h"

namespace sere {

/// `sere add [--force] [--dry-run] <name>[@<version>]`
///
/// Downloads are public, so no token is needed. The archive checksum reported by
/// the registry is verified before anything is written to disk.
[[nodiscard]] int addCommand(const CompilerOptions& options);

} // namespace sere
