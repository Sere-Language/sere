/// @file Registry.h
/// `sere login`, `sere logout`, and `sere publish` against the Sere package registry.
///
/// The token is a bearer credential created at https://sere-lang.com/developers.
/// `sere login` keeps it in a per-user credentials file; `SERE_TOKEN` overrides
/// that file so CI can publish without writing anything to disk.

#pragma once

#include "sere/driver/Options.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sere {

/// Environment variable that replaces the stored token.
inline constexpr const char* RegistryTokenEnv = "SERE_TOKEN";

/// Environment variable that replaces the stored credentials file (tests, CI).
inline constexpr const char* RegistryCredentialsEnv = "SERE_CREDENTIALS";

/// Environment variable that replaces the registry base URL (tests, self-hosting).
inline constexpr const char* RegistryUrlEnv = "SERE_REGISTRY_URL";

/// Default registry base URL, without a trailing slash.
inline constexpr const char* DefaultRegistryUrl = "https://sere-lang.com";

/// Per-user credentials file: `%LOCALAPPDATA%\sere\credentials.toml` on Windows,
/// `$XDG_CONFIG_HOME/sere/credentials.toml` (or `~/.config/...`) elsewhere.
[[nodiscard]] std::filesystem::path credentialsPath();

/// Registry base URL with any trailing slash removed.
[[nodiscard]] std::string registryBaseUrl();

/// One HTTP response from the registry.
struct RegistryResponse {
  /// False when the request never completed; see `error`.
  bool reached = false;
  int status = 0;
  /// Response bytes (JSON, an error page, or a downloaded archive).
  std::string body;
  /// Transport failure message, empty when `reached` is true.
  std::string error;
  /// Response header names lowercased, in the order they arrived.
  std::vector<std::pair<std::string, std::string>> headers;
};

/// Case-insensitive header lookup; nullptr when the header is absent.
[[nodiscard]] const std::string* registryHeader(const RegistryResponse& response,
                                                std::string_view name);

/// `GET url`. `maxBytes` caps the body (0 uses the default cap).
[[nodiscard]] RegistryResponse registryHttpGet(const std::string& url, std::size_t maxBytes = 0);

/// True when `token` looks like a registry publish token (`sere_<prefix>_<secret>`).
[[nodiscard]] bool isPublishTokenShape(std::string_view token);

/// Short display form of a token: `sere_4f0c9a21_...`.
[[nodiscard]] std::string maskPublishToken(std::string_view token);

/// `sere login [token]`: with a token, validate and store it; without one, report
/// the credential that a publish would use. `--token` sets the value too.
[[nodiscard]] int loginCommand(const CompilerOptions& options);

/// `sere logout`: forget the stored token. `SERE_TOKEN` is left alone.
[[nodiscard]] int logoutCommand();

/// `sere publish [--dry-run]`: pack this library project and POST it to the
/// registry. Applications are rejected; only `kind = "lib"` projects publish.
[[nodiscard]] int publishCommand(const CompilerOptions& options);

} // namespace sere
