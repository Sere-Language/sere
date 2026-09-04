/// @file ProjectInit.h
/// Creates a Sere project layout: src, libs, bin, and a local venv.

#pragma once

#include <filesystem>
#include <string>

namespace sere {

[[nodiscard]] int initSereProject(const std::filesystem::path& name,
                                  const std::filesystem::path& compilerDir,
                                  std::string& error);

[[nodiscard]] int initSereLibrary(const std::filesystem::path& name, std::string& error);

/// Copies compiler, runtime, headers, and stdlib into a project venv/bin.
void copyProjectToolchain(const std::filesystem::path& compilerDir,
                          const std::filesystem::path& root);

/// Copies this running compiler into %LOCALAPPDATA%\\Programs\\Sere when that
/// install is a different binary or version string. Then syncs a project venv
/// if `sere.toml` is found. Leaves src/ and libs/ alone.
[[nodiscard]] int updateSereEnvironment(const std::filesystem::path& start, std::string& error);

/// Rewrites venv/shell.* so `sere shell` never sources a user profile.
void writeProjectShellRc(const std::filesystem::path& root);

}  // namespace sere
