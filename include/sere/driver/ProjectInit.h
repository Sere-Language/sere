/// @file ProjectInit.h
/// Creates a Sere project layout: src, libs, bin, and a local venv.

#pragma once

#include <filesystem>
#include <string>

namespace sere {

[[nodiscard]] int initSereProject(const std::filesystem::path& name,
                                  const std::filesystem::path& compilerDir,
                                  std::string& error);

/// Rewrites venv/shell.* so `sere shell` never sources a user profile.
void writeProjectShellRc(const std::filesystem::path& root);

}  // namespace sere
