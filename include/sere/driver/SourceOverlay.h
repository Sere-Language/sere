/// @file SourceOverlay.h
/// In-memory file contents that override disk when the language server analyzes.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace sere {

/// Maps canonical source paths to unsaved or freshly edited buffers.
class SourceOverlay {
public:
  void set(const std::filesystem::path& path, std::string text);
  void remove(const std::filesystem::path& path);
  void clear();

  [[nodiscard]] bool empty() const;
  [[nodiscard]] bool contains(const std::filesystem::path& path) const;
  [[nodiscard]] std::optional<std::string> read(const std::filesystem::path& path) const;
  [[nodiscard]] static std::string normalize(const std::filesystem::path& path);

private:
  std::unordered_map<std::string, std::string> files_{};
};

/// Reads `path` from `overlay` when present, otherwise from disk.
[[nodiscard]] std::optional<std::string> readSourceFile(const std::filesystem::path& path,
                                                        const SourceOverlay* overlay = nullptr);

}  // namespace sere
