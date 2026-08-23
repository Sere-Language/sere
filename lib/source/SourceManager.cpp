/// @file SourceManager.cpp
/// Loads source text and maps byte offsets to line/column.

#include "sere/source/SourceManager.h"

#include <algorithm>

namespace sere {
namespace {

void stripUtf8Bom(std::string& text) {
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.erase(0, 3);
  }
}

void buildLineOffsets(const std::string& text, std::vector<std::uint32_t>& offsets) {
  offsets.clear();
  offsets.push_back(0);
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\n') {
      offsets.push_back(static_cast<std::uint32_t>(index + 1));
    }
  }
}

}  // namespace

SourceManager::SourceManager(std::string path, std::string text)
    : path_(std::move(path)), text_(std::move(text)) {
  stripUtf8Bom(text_);
  buildLineOffsets(text_, lineOffsets_);
}

const std::string& SourceManager::path() const { return path_; }

std::string_view SourceManager::text() const { return text_; }

std::size_t SourceManager::size() const { return text_.size(); }

char SourceManager::charAt(std::size_t offset) const {
  if (offset >= text_.size()) {
    return '\0';
  }
  return text_[offset];
}

SourceLocation SourceManager::location(std::size_t offset) const {
  const std::uint32_t clamped =
      static_cast<std::uint32_t>(std::min(offset, text_.size()));
  const auto lineIt =
      std::upper_bound(lineOffsets_.begin(), lineOffsets_.end(), clamped);
  const std::size_t lineIndex =
      static_cast<std::size_t>(std::distance(lineOffsets_.begin(), lineIt)) - 1;
  const std::uint32_t lineStart = lineOffsets_[lineIndex];
  SourceLocation loc;
  loc.offset = clamped;
  loc.line = static_cast<std::uint32_t>(lineIndex + 1);
  loc.column = clamped - lineStart + 1;
  return loc;
}

std::uint32_t SourceManager::offsetAt(std::uint32_t line, std::uint32_t column) const {
  if (line == 0 || lineOffsets_.empty()) {
    return 0;
  }
  const std::size_t lineIndex = static_cast<std::size_t>(line - 1);
  if (lineIndex >= lineOffsets_.size()) {
    return static_cast<std::uint32_t>(text_.size());
  }
  const std::uint32_t lineStart = lineOffsets_[lineIndex];
  const std::uint32_t lineEnd = lineIndex + 1 < lineOffsets_.size()
                                    ? lineOffsets_[lineIndex + 1]
                                    : static_cast<std::uint32_t>(text_.size());
  const std::uint32_t columnIndex = column == 0 ? 0 : column - 1;
  const std::uint32_t offset = lineStart + columnIndex;
  if (offset > lineEnd) {
    return lineEnd;
  }
  return offset;
}

std::string_view SourceManager::slice(SourceRange range) const {
  const std::size_t start = range.start.offset;
  const std::size_t end = range.end.offset;
  if (start >= text_.size() || end < start) {
    return {};
  }
  const std::size_t clampedEnd = std::min(end, text_.size());
  return std::string_view(text_).substr(start, clampedEnd - start);
}

std::string_view SourceManager::lineText(std::uint32_t line) const {
  if (line == 0 || lineOffsets_.empty()) {
    return {};
  }
  const std::size_t lineIndex = static_cast<std::size_t>(line - 1);
  if (lineIndex >= lineOffsets_.size()) {
    return {};
  }
  const std::uint32_t start = lineOffsets_[lineIndex];
  std::uint32_t end = lineIndex + 1 < lineOffsets_.size()
                          ? lineOffsets_[lineIndex + 1]
                          : static_cast<std::uint32_t>(text_.size());
  while (end > start && (text_[end - 1] == '\n' || text_[end - 1] == '\r')) {
    --end;
  }
  return std::string_view(text_).substr(start, end - start);
}

}  // namespace sere
