/// @file ProjectShell.h
/// Nested interactive shell with sere build / sere run on PATH.

#pragma once

#include "sere/driver/Options.h"

namespace sere {

[[nodiscard]] int enterProjectShell(const CompilerOptions& options);

}  // namespace sere
