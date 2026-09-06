/// @file IgnoreDirective.cpp
/// Scans source comments for `# type: ignore` suppression directives.

#include "sere/diag/IgnoreDirective.h"

#include <cctype>
#include <optional>

namespace sere {
namespace {

[[nodiscard]] bool isSpace(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] std::string_view trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && isSpace(text[start])) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start && isSpace(text[end - 1])) {
    --end;
  }
  return text.substr(start, end - start);
}

[[nodiscard]] bool consume(std::string_view& text, std::string_view prefix) {
  if (!text.starts_with(prefix)) {
    return false;
  }
  text.remove_prefix(prefix.size());
  return true;
}

void skipSpaces(std::string_view& text) {
  while (!text.empty() && isSpace(text.front())) {
    text.remove_prefix(1);
  }
}

[[nodiscard]] std::string_view commentSuffix(std::string_view line) {
  bool inSingle = false;
  bool inDouble = false;
  bool inBacktick = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char ch = line[index];
    if (ch == '\\' && (inSingle || inDouble || inBacktick)) {
      ++index;
      continue;
    }
    if (!inDouble && !inBacktick && ch == '\'') {
      inSingle = !inSingle;
      continue;
    }
    if (!inSingle && !inBacktick && ch == '"') {
      inDouble = !inDouble;
      continue;
    }
    if (!inSingle && !inDouble && ch == '`') {
      inBacktick = !inBacktick;
      continue;
    }
    if (!inSingle && !inDouble && !inBacktick && ch == '#') {
      return line.substr(index + 1);
    }
  }
  return {};
}

[[nodiscard]] bool isCommentOnlyLine(std::string_view line) {
  const std::string_view trimmed = trim(line);
  return trimmed.empty() || trimmed.starts_with('#');
}

struct ParsedIgnore {
  bool present = false;
  bool all = false;
  std::vector<std::string> names{};
};

[[nodiscard]] std::vector<std::string> splitCodes(std::string_view body) {
  std::vector<std::string> names;
  std::string current;
  for (const char ch : body) {
    if (ch == ',') {
      const std::string_view token = trim(current);
      if (!token.empty()) {
        names.emplace_back(token);
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  const std::string_view token = trim(current);
  if (!token.empty()) {
    names.emplace_back(token);
  }
  return names;
}

[[nodiscard]] ParsedIgnore parseIgnoreComment(std::string_view comment) {
  ParsedIgnore parsed;
  std::string_view text = trim(comment);
  if (!consume(text, "type")) {
    return parsed;
  }
  skipSpaces(text);
  if (consume(text, ":")) {
    skipSpaces(text);
    if (!consume(text, "ignore")) {
      return parsed;
    }
    parsed.present = true;
    skipSpaces(text);
    if (consume(text, "[")) {
      const std::size_t close = text.find(']');
      if (close == std::string_view::npos) {
        return parsed;
      }
      parsed.names = splitCodes(text.substr(0, close));
      parsed.all = parsed.names.empty();
      return parsed;
    }
    parsed.all = true;
    return parsed;
  }
  if (!consume(text, "[")) {
    return parsed;
  }
  const std::size_t close = text.find(']');
  if (close == std::string_view::npos) {
    return parsed;
  }
  parsed.names = splitCodes(text.substr(0, close));
  text.remove_prefix(close + 1);
  skipSpaces(text);
  if (!consume(text, ":")) {
    return parsed;
  }
  skipSpaces(text);
  if (!consume(text, "ignore")) {
    return parsed;
  }
  parsed.present = true;
  parsed.all = parsed.names.empty();
  return parsed;
}

void addCodes(std::vector<DiagnosticCode>& codes, DiagnosticCode code) {
  for (const DiagnosticCode existing : codes) {
    if (existing == code) {
      return;
    }
  }
  codes.push_back(code);
}

void applyNames(const ParsedIgnore& parsed,
                std::uint32_t line,
                bool fileScope,
                bool commentOnly,
                IgnoreSet& ignores,
                std::vector<UnknownIgnoreName>& unknownNames) {
  if (!parsed.present) {
    return;
  }
  if (!fileScope && commentOnly) {
    ignores.coversNextLine[line] = true;
  }
  if (parsed.all) {
    if (fileScope) {
      ignores.ignoreAllFile = true;
    } else {
      ignores.lineIgnoreAll[line] = true;
    }
  }
  std::vector<DiagnosticCode> codes;
  for (const std::string& name : parsed.names) {
    const std::optional<DiagnosticCode> code = parseDiagnosticCode(name);
    if (!code.has_value()) {
      unknownNames.push_back(UnknownIgnoreName{line, name});
      continue;
    }
    if (*code == DiagnosticCode::Exception) {
      if (fileScope) {
        ignores.ignoreAllFile = true;
      } else {
        ignores.lineIgnoreAll[line] = true;
      }
      continue;
    }
    addCodes(codes, *code);
  }
  if (codes.empty()) {
    return;
  }
  if (fileScope) {
    for (const DiagnosticCode code : codes) {
      addCodes(ignores.fileCodes, code);
    }
    return;
  }
  std::vector<DiagnosticCode>& target = ignores.lineCodes[line];
  for (const DiagnosticCode code : codes) {
    addCodes(target, code);
  }
}

[[nodiscard]] bool containsCode(const std::vector<DiagnosticCode>& codes, DiagnosticCode code) {
  for (const DiagnosticCode existing : codes) {
    if (existing == code || existing == DiagnosticCode::Exception) {
      return true;
    }
  }
  return false;
}

} // namespace

IgnoreSet parseIgnoreDirectives(std::string_view source,
                                std::vector<UnknownIgnoreName>& unknownNames) {
  IgnoreSet ignores;
  bool inHeader = true;
  std::uint32_t line = 1;
  std::size_t start = 0;
  while (start <= source.size()) {
    std::size_t end = start;
    while (end < source.size() && source[end] != '\n') {
      ++end;
    }
    const std::string_view raw = source.substr(start, end - start);
    std::string_view lineText = raw;
    if (!lineText.empty() && lineText.back() == '\r') {
      lineText.remove_suffix(1);
    }
    const bool commentOnly = isCommentOnlyLine(lineText);
    if (inHeader && !commentOnly) {
      inHeader = false;
    }
    const ParsedIgnore parsed = parseIgnoreComment(commentSuffix(lineText));
    if (parsed.present) {
      applyNames(parsed, line, inHeader, commentOnly, ignores, unknownNames);
    }
    if (end >= source.size()) {
      break;
    }
    start = end + 1;
    ++line;
  }
  return ignores;
}

bool diagnosticIsIgnored(const IgnoreSet& ignores, std::uint32_t line, DiagnosticCode code) {
  if (ignores.ignoreAllFile) {
    return true;
  }
  if (containsCode(ignores.fileCodes, code)) {
    return true;
  }
  const auto allHere = ignores.lineIgnoreAll.find(line);
  if (allHere != ignores.lineIgnoreAll.end() && allHere->second) {
    return true;
  }
  const auto codesHere = ignores.lineCodes.find(line);
  if (codesHere != ignores.lineCodes.end() && containsCode(codesHere->second, code)) {
    return true;
  }
  if (line > 1) {
    const auto covers = ignores.coversNextLine.find(line - 1);
    if (covers != ignores.coversNextLine.end() && covers->second) {
      const auto allPrev = ignores.lineIgnoreAll.find(line - 1);
      if (allPrev != ignores.lineIgnoreAll.end() && allPrev->second) {
        return true;
      }
      const auto codesPrev = ignores.lineCodes.find(line - 1);
      if (codesPrev != ignores.lineCodes.end() && containsCode(codesPrev->second, code)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace sere
