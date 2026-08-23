/// @file Toolchain.h
/// Locates clang, lld, and the Sere runtime without requiring env.ps1.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace sere {

[[nodiscard]] std::filesystem::path compilerDirectory();
[[nodiscard]] std::optional<std::filesystem::path> llvmToolsDirectory();
[[nodiscard]] std::optional<std::string> findClang();
[[nodiscard]] std::optional<std::string> findLld();
[[nodiscard]] std::optional<std::filesystem::path> findRuntimeLibrary();
[[nodiscard]] std::optional<std::filesystem::path> findNativeLibrary(std::string_view stem);
void prependLlvmToolsToPath();
void prependToPath(const std::filesystem::path& directory);
void setEnvironmentVariable(std::string_view name, std::string_view value);
/// Puts MSVC and Windows SDK lib dirs on LIB so clang can link without vcvars.
void applyHostLinkEnvironment();
[[nodiscard]] std::optional<std::filesystem::path> findSystemLibrary(std::string_view name);

}  // namespace sere
