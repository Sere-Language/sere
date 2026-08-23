/// @file SourceOverlay.cpp
/// Canonical path keys and disk fallback for language-server overlays.

#include "sere/driver/SourceOverlay.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace sere {

std::string SourceOverlay::normalize(const std::filesystem::path& path) {
  if (path.empty()) {
    return {};
  }
  std::error_code error;
  std::filesystem::path abs = std::filesystem::absolute(path, error);
  if (error) {
    abs = path;
  }
  const std::filesystem::path canonical = std::filesystem::weakly_canonical(abs, error);
  if (error) {
    return abs.lexically_normal().generic_string();
  }
  return canonical.generic_string();
}

void SourceOverlay::set(const std::filesystem::path& path, std::string text) {
  const std::string key = normalize(path);
  if (key.empty()) {
    return;
  }
  files_[key] = std::move(text);
}

void SourceOverlay::remove(const std::filesystem::path& path) {
  files_.erase(normalize(path));
}

void SourceOverlay::clear() { files_.clear(); }

bool SourceOverlay::empty() const { return files_.empty(); }

bool SourceOverlay::contains(const std::filesystem::path& path) const {
  return files_.contains(normalize(path));
}

std::optional<std::string> SourceOverlay::read(const std::filesystem::path& path) const {
  const auto found = files_.find(normalize(path));
  if (found == files_.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::optional<std::string> readSourceFile(const std::filesystem::path& path,
                                          const SourceOverlay* overlay) {
  if (overlay != nullptr) {
    if (std::optional<std::string> text = overlay->read(path); text.has_value()) {
      return text;
    }
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

}  // namespace sere
