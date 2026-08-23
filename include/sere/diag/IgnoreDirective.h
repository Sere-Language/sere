/// @file IgnoreDirective.h
/// Parses `# type: ignore` and file-level `# type[NameError]: ignore` comments.

#pragma once

#include "sere/diag/DiagnosticCode.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sere {

struct UnknownIgnoreName {
  std::uint32_t line = 1;
  std::string name;
};

struct IgnoreSet {
  bool ignoreAllFile = false;
  std::vector<DiagnosticCode> fileCodes{};
  std::unordered_map<std::uint32_t, bool> lineIgnoreAll{};
  std::unordered_map<std::uint32_t, std::vector<DiagnosticCode>> lineCodes{};
  std::unordered_map<std::uint32_t, bool> coversNextLine{};
};

[[nodiscard]] IgnoreSet parseIgnoreDirectives(std::string_view source,
                                              std::vector<UnknownIgnoreName>& unknownNames);

[[nodiscard]] bool diagnosticIsIgnored(const IgnoreSet& ignores,
                                       std::uint32_t line,
                                       DiagnosticCode code);

}  // namespace sere
