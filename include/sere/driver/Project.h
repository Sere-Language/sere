/// @file Project.h
/// Project manifest, discovery, and build/run/clean commands.

#pragma once

#include "sere/codegen/OptPipeline.h"
#include "sere/driver/Options.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sere {

struct ProjectManifest {
  std::string name = "sere-project";
  std::filesystem::path root;
  std::filesystem::path src;
  std::filesystem::path entry;
  std::filesystem::path libs;
  std::filesystem::path stdlib;
  std::filesystem::path output;
  OptLevel optLevel = OptLevel::O0;
  bool native = false;
};

/// Stdlib and import roots for a file, preferring an init'd project over a global env.
struct LanguageContext {
  std::optional<ProjectManifest> project;
  std::filesystem::path stdlib;
};

[[nodiscard]] std::optional<std::filesystem::path> findProjectRoot(
    const std::filesystem::path& start);

[[nodiscard]] bool loadProjectManifest(const std::filesystem::path& root,
                                       ProjectManifest& manifest,
                                       std::string& error);

/// Resolves the Sere project and stdlib that should analyze `start`.
///
/// Preference: `sere.toml` walking from `start`, then `SERE_PROJECT_ROOT` when that
/// tree exists, then `SERE_STDLIB` / the compiler stdlib. An activated shell
/// (`SERE_ACTIVE`) is trusted only when `SERE_PROJECT_ROOT` matches this project.
[[nodiscard]] LanguageContext resolveLanguageContext(const std::filesystem::path& start);

/// Appends the project's `libs`, `src`, and root to an import search list.
void appendLanguageContextDirs(std::vector<std::filesystem::path>& dirs,
                               const LanguageContext& context);

[[nodiscard]] int buildProject(const CompilerOptions& options);
[[nodiscard]] int runProject(const CompilerOptions& options);
[[nodiscard]] int cleanProject(const CompilerOptions& options);

}  // namespace sere
