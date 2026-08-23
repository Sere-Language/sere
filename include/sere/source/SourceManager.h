/// @file SourceManager.h
/// Owns a single translation unit's path and UTF-8 source text.

#pragma once

#include "sere/source/SourceLocation.h"

#include <string>
#include <string_view>
#include <vector>

namespace sere {

class SourceManager {
public:
  SourceManager(std::string path, std::string text);

  [[nodiscard]] const std::string& path() const;
  [[nodiscard]] std::string_view text() const;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] char charAt(std::size_t offset) const;
  [[nodiscard]] SourceLocation location(std::size_t offset) const;
  [[nodiscard]] std::uint32_t offsetAt(std::uint32_t line, std::uint32_t column) const;
  [[nodiscard]] std::string_view slice(SourceRange range) const;
  [[nodiscard]] std::string_view lineText(std::uint32_t line) const;

private:
  std::string path_;
  std::string text_;
  std::vector<std::uint32_t> lineOffsets_;
};

}  // namespace sere
