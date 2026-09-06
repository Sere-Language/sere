/// @file DiagnosticEngine.cpp
/// Pretty rustc-style diagnostics with source snippets, carets, and color.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "sere/diag/DiagnosticEngine.h"

#include "sere/diag/DiagnosticCode.h"
#include "sere/diag/IgnoreDirective.h"
#include "sere/source/SourceManager.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace sere {
namespace {

constexpr std::size_t kMaxDiagnostics = 32;

constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kRed = "\033[31m";
constexpr const char* kYellow = "\033[33m";
constexpr const char* kCyan = "\033[36m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kBlue = "\033[34m";

[[nodiscard]] bool stderrIsTty() {
#ifdef _WIN32
  return _isatty(_fileno(stderr)) != 0;
#else
  return isatty(STDERR_FILENO) != 0;
#endif
}

void enableVirtualTerminal() {
#ifdef _WIN32
  HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD mode = 0;
  if (GetConsoleMode(handle, &mode) == 0) {
    return;
  }
  SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
  (void)0;
#endif
}

[[nodiscard]] std::string_view severityName(DiagnosticSeverity severity) {
  switch (severity) {
  case DiagnosticSeverity::Note:
    return "note";
  case DiagnosticSeverity::Warning:
    return "warning";
  case DiagnosticSeverity::Error:
    return "error";
  }
  return "error";
}

[[nodiscard]] const char* severityColor(DiagnosticSeverity severity) {
  switch (severity) {
  case DiagnosticSeverity::Note:
    return kCyan;
  case DiagnosticSeverity::Warning:
    return kYellow;
  case DiagnosticSeverity::Error:
    return kRed;
  }
  return kRed;
}

[[nodiscard]] std::string paint(bool color, const char* code, std::string_view text) {
  if (!color) {
    return std::string(text);
  }
  std::string out = code;
  out += text;
  out += kReset;
  return out;
}

[[nodiscard]] SourceRange pointRange(SourceLocation location) {
  return SourceRange{location, location};
}

[[nodiscard]] std::string_view lineFor(const Diagnostic& diagnostic, const SourceManager* source) {
  if (!diagnostic.lineText.empty()) {
    return diagnostic.lineText;
  }
  if (source == nullptr || diagnostic.range.start.line == 0) {
    return {};
  }
  return source->lineText(diagnostic.range.start.line);
}

[[nodiscard]] std::string fileFor(const Diagnostic& diagnostic, const SourceManager* source) {
  if (!diagnostic.file.empty()) {
    return diagnostic.file;
  }
  if (source != nullptr) {
    return source->path();
  }
  return {};
}

[[nodiscard]] std::size_t caretCount(const Diagnostic& diagnostic, std::string_view line) {
  const SourceRange range = diagnostic.range;
  if (range.end.offset <= range.start.offset || range.end.line != range.start.line) {
    return 1;
  }
  std::size_t count = 1;
  if (range.end.column > range.start.column) {
    count = static_cast<std::size_t>(range.end.column - range.start.column);
  }
  const std::size_t maxCount =
      line.size() + 1 >= range.start.column ? line.size() + 1 - range.start.column : 1;
  return std::max<std::size_t>(1, (std::min)(count, maxCount));
}

void appendSnippet(std::ostringstream& out,
                   const Diagnostic& diagnostic,
                   const SourceManager* source,
                   bool color) {
  if (diagnostic.range.start.line == 0) {
    return;
  }
  const std::string_view line = lineFor(diagnostic, source);
  const std::string file = fileFor(diagnostic, source);
  if (line.empty() && file.empty()) {
    return;
  }
  const std::uint32_t lineNo = diagnostic.range.start.line;
  const std::uint32_t column =
      diagnostic.range.start.column == 0 ? 1 : diagnostic.range.start.column;
  const std::string lineLabel = std::to_string(lineNo);
  const std::string gutter(lineLabel.size(), ' ');
  const std::string bar = paint(color, kBlue, "|");
  out << " " << paint(color, kBlue, "-->") << " " << file << ':' << lineNo << ':' << column << '\n';
  out << " " << gutter << " " << bar << '\n';
  out << " " << paint(color, kBlue, lineLabel) << " " << bar << " " << line << '\n';
  if (line.empty()) {
    return;
  }
  const std::size_t indent = column == 0 ? 0 : static_cast<std::size_t>(column - 1);
  const std::size_t carets = caretCount(diagnostic, line);
  out << " " << gutter << " " << bar << " " << std::string(indent, ' ')
      << paint(color, severityColor(diagnostic.severity), std::string(carets, '^')) << '\n';
}

void appendDiagnostic(std::ostringstream& out,
                      const Diagnostic& diagnostic,
                      const SourceManager* source,
                      bool color) {
  if (color) {
    out << kBold << severityColor(diagnostic.severity) << severityName(diagnostic.severity) << "["
        << diagnosticCodeName(diagnostic.code) << "]" << kReset << kBold << ": " << kReset;
  } else {
    out << severityName(diagnostic.severity) << "[" << diagnosticCodeName(diagnostic.code) << "]: ";
  }
  out << diagnostic.message << '\n';
  appendSnippet(out, diagnostic, source, color);
  if (!diagnostic.help.empty()) {
    if (color) {
      out << " " << paint(color, kGreen, "=") << " " << kBold << kGreen << "help" << kReset << kBold
          << ": " << kReset;
    } else {
      out << " = help: ";
    }
    out << diagnostic.help << '\n';
  }
}

} // namespace

void DiagnosticEngine::setSource(const SourceManager* source) {
  source_ = source;
  ignoreSource_ = nullptr;
  ensureIgnores();
}

const SourceManager* DiagnosticEngine::source() const {
  return source_;
}

void DiagnosticEngine::setColorMode(ColorMode mode) {
  colorMode_ = mode;
}

void DiagnosticEngine::snapshot(Diagnostic& diagnostic) const {
  if (source_ == nullptr || diagnostic.range.start.line == 0) {
    return;
  }
  diagnostic.file = source_->path();
  diagnostic.lineText = std::string(source_->lineText(diagnostic.range.start.line));
}

void DiagnosticEngine::ensureIgnores() {
  if (source_ == ignoreSource_) {
    return;
  }
  ignoreSource_ = source_;
  ignores_ = {};
  if (source_ == nullptr) {
    return;
  }
  std::vector<UnknownIgnoreName> unknown;
  ignores_ = parseIgnoreDirectives(source_->text(), unknown);
  if (!ignoreParsedSources_.insert(source_).second) {
    return;
  }
  for (const UnknownIgnoreName& entry : unknown) {
    SourceRange range{};
    range.start.line = entry.line;
    range.start.column = 1;
    range.start.offset = source_->offsetAt(entry.line, 1);
    range.end = range.start;
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.code = DiagnosticCode::ValueError;
    diagnostic.range = range;
    diagnostic.message = "unknown diagnostic exception '" + entry.name + "'";
    diagnostic.help = "available exceptions: " + diagnosticCodeCatalogText();
    if (!diagnosticIsIgnored(ignores_, entry.line, diagnostic.code)) {
      snapshot(diagnostic);
      diagnostics_.push_back(std::move(diagnostic));
      lastWasSuppressed_ = false;
    }
  }
}

void DiagnosticEngine::add(DiagnosticSeverity severity, SourceRange range, std::string message) {
  ensureIgnores();
  const DiagnosticCode code = inferDiagnosticCode(message);
  if (diagnosticIsIgnored(ignores_, range.start.line, code)) {
    lastWasSuppressed_ = true;
    return;
  }
  if (diagnostics_.size() >= kMaxDiagnostics) {
    if (!emittedLimit_) {
      emittedLimit_ = true;
      Diagnostic limit;
      limit.severity = DiagnosticSeverity::Error;
      limit.code = DiagnosticCode::RuntimeError;
      limit.range.start.line = 0;
      limit.range.start.column = 0;
      limit.range.end = limit.range.start;
      limit.message = "too many errors, stopping";
      diagnostics_.push_back(std::move(limit));
    }
    lastWasSuppressed_ = false;
    return;
  }
  Diagnostic diagnostic;
  diagnostic.severity = severity;
  diagnostic.code = code;
  diagnostic.range = range;
  diagnostic.message = std::move(message);
  snapshot(diagnostic);
  diagnostics_.push_back(std::move(diagnostic));
  lastWasSuppressed_ = false;
}

void DiagnosticEngine::error(std::string message) {
  SourceRange range{};
  range.start.line = 0;
  range.start.column = 0;
  range.end = range.start;
  add(DiagnosticSeverity::Error, range, std::move(message));
}

void DiagnosticEngine::error(SourceLocation location, std::string message) {
  add(DiagnosticSeverity::Error, pointRange(location), std::move(message));
}

void DiagnosticEngine::error(SourceRange range, std::string message) {
  add(DiagnosticSeverity::Error, range, std::move(message));
}

void DiagnosticEngine::warn(SourceLocation location, std::string message) {
  add(DiagnosticSeverity::Warning, pointRange(location), std::move(message));
}

void DiagnosticEngine::warn(SourceRange range, std::string message) {
  add(DiagnosticSeverity::Warning, range, std::move(message));
}

void DiagnosticEngine::note(SourceLocation location, std::string message) {
  add(DiagnosticSeverity::Note, pointRange(location), std::move(message));
}

void DiagnosticEngine::note(SourceRange range, std::string message) {
  add(DiagnosticSeverity::Note, range, std::move(message));
}

void DiagnosticEngine::help(std::string text) {
  if (lastWasSuppressed_ || diagnostics_.empty()) {
    return;
  }
  diagnostics_.back().help = std::move(text);
}

bool DiagnosticEngine::hasErrors() const {
  for (const Diagnostic& diagnostic : diagnostics_) {
    if (diagnostic.severity == DiagnosticSeverity::Error) {
      return true;
    }
  }
  return false;
}

const std::vector<Diagnostic>& DiagnosticEngine::diagnostics() const {
  return diagnostics_;
}

bool DiagnosticEngine::shouldColor() const {
  if (colorMode_ == ColorMode::Never) {
    return false;
  }
  if (colorMode_ == ColorMode::Always) {
    return true;
  }
#ifdef _WIN32
  if (GetEnvironmentVariableA("NO_COLOR", nullptr, 0) > 0) {
    return false;
  }
#else
  if (std::getenv("NO_COLOR") != nullptr) {
    return false;
  }
#endif
  return stderrIsTty();
}

std::string DiagnosticEngine::format(bool color) const {
  std::ostringstream out;
  for (std::size_t index = 0; index < diagnostics_.size(); ++index) {
    if (index != 0) {
      out << '\n';
    }
    appendDiagnostic(out, diagnostics_[index], source_, color);
  }
  return out.str();
}

std::string DiagnosticEngine::format(const SourceManager& source, bool color) const {
  std::ostringstream out;
  for (std::size_t index = 0; index < diagnostics_.size(); ++index) {
    if (index != 0) {
      out << '\n';
    }
    appendDiagnostic(out, diagnostics_[index], &source, color);
  }
  return out.str();
}

void DiagnosticEngine::printAll() const {
  if (source_ == nullptr) {
    if (shouldColor()) {
      enableVirtualTerminal();
    }
    std::cerr << format(shouldColor());
    if (!diagnostics_.empty()) {
      std::cerr << '\n';
    }
    return;
  }
  printAll(*source_);
}

void DiagnosticEngine::printAll(const SourceManager& source) const {
  if (shouldColor()) {
    enableVirtualTerminal();
  }
  std::cerr << format(source, shouldColor());
  if (!diagnostics_.empty()) {
    std::cerr << '\n';
  }
}

} // namespace sere
