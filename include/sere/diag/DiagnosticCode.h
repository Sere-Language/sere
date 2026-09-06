/// @file DiagnosticCode.h
/// Named diagnostic exceptions used in messages and `# type[NameError]: ignore`.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

enum class DiagnosticCode {
  Exception,
  SyntaxError,
  IndentationError,
  NameError,
  AttributeError,
  TypeError,
  IndexError,
  ImportError,
  ValueError,
  AssertionError,
  PermissionError,
  RuntimeError,
  RecursionError,
  NotImplementedError,
};

struct DiagnosticCodeInfo {
  DiagnosticCode code = DiagnosticCode::TypeError;
  std::string_view name;
  std::string_view description;
};

[[nodiscard]] std::string_view diagnosticCodeName(DiagnosticCode code);

[[nodiscard]] std::string_view diagnosticCodeDescription(DiagnosticCode code);

[[nodiscard]] std::optional<DiagnosticCode> parseDiagnosticCode(std::string_view name);

[[nodiscard]] const std::vector<DiagnosticCodeInfo>& diagnosticCodeCatalog();

[[nodiscard]] std::string diagnosticCodeCatalogText();

[[nodiscard]] DiagnosticCode inferDiagnosticCode(std::string_view message);

} // namespace sere
