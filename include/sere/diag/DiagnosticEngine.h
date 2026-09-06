/// @file DiagnosticEngine.h
/// Collects compiler diagnostics. The toolchain does not use C++ exceptions.

#pragma once

#include "sere/diag/DiagnosticCode.h"
#include "sere/diag/IgnoreDirective.h"
#include "sere/source/SourceLocation.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace sere {

class SourceManager;

enum class DiagnosticSeverity {
  Note,
  Warning,
  Error,
};

enum class ColorMode {
  Auto,
  Always,
  Never,
};

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  DiagnosticCode code = DiagnosticCode::TypeError;
  SourceRange range{};
  std::string message;
  std::string help;
  std::string file;
  std::string lineText;
};

class DiagnosticEngine {
public:
  void setSource(const SourceManager* source);
  [[nodiscard]] const SourceManager* source() const;
  void setColorMode(ColorMode mode);

  void error(std::string message);
  void error(SourceLocation location, std::string message);
  void error(SourceRange range, std::string message);
  void warn(SourceLocation location, std::string message);
  void warn(SourceRange range, std::string message);
  void note(SourceLocation location, std::string message);
  void note(SourceRange range, std::string message);
  void help(std::string text);

  [[nodiscard]] bool hasErrors() const;
  [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const;
  [[nodiscard]] std::string format(bool color) const;
  [[nodiscard]] std::string format(const SourceManager& source, bool color) const;
  void printAll() const;
  void printAll(const SourceManager& source) const;

private:
  void add(DiagnosticSeverity severity, SourceRange range, std::string message);
  void ensureIgnores();
  [[nodiscard]] bool shouldColor() const;
  void snapshot(Diagnostic& diagnostic) const;

  const SourceManager* source_ = nullptr;
  const SourceManager* ignoreSource_ = nullptr;
  IgnoreSet ignores_{};
  std::unordered_set<const SourceManager*> ignoreParsedSources_{};
  ColorMode colorMode_ = ColorMode::Auto;
  std::vector<Diagnostic> diagnostics_{};
  bool emittedLimit_ = false;
  bool lastWasSuppressed_ = false;
};

} // namespace sere
