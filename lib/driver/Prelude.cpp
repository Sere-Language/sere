/// @file Prelude.cpp
/// Loads stdlib/prelude.sere into the front of a user module.

#include "sere/driver/Prelude.h"

#include "sere/diag/DiagnosticEngine.h"
#include "sere/lex/Lexer.h"
#include "sere/parse/Parser.h"
#include "sere/source/SourceManager.h"

#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

namespace sere {
namespace {

[[nodiscard]] std::optional<std::string> readAll(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace

std::filesystem::path findStdlibDirectory(const std::filesystem::path& compilerDir) {
  if (const char* fromEnv = std::getenv("SERE_STDLIB")) {
    return fromEnv;
  }
  const std::filesystem::path nextToCompiler = compilerDir / "stdlib";
  if (std::filesystem::exists(nextToCompiler / "prelude.sere")) {
    return nextToCompiler;
  }
  const std::filesystem::path venvStdlib = compilerDir.parent_path() / "stdlib";
  if (std::filesystem::exists(venvStdlib / "prelude.sere")) {
    return venvStdlib;
  }
  const std::filesystem::path projectVenv =
      std::filesystem::current_path() / "venv" / "stdlib";
  if (std::filesystem::exists(projectVenv / "prelude.sere")) {
    return projectVenv;
  }
  const std::filesystem::path sourceTree = std::filesystem::current_path() / "stdlib";
  return sourceTree;
}

bool loadPrelude(Module& userModule,
                 DiagnosticEngine& diagnostics,
                 const std::filesystem::path& stdlibDir) {
  const std::filesystem::path preludePath = stdlibDir / "prelude.sere";
  const std::optional<std::string> text = readAll(preludePath);
  if (!text.has_value()) {
    diagnostics.error("cannot load standard library prelude from '" + preludePath.string() + "'");
    diagnostics.help("set SERE_STDLIB or keep stdlib/ next to sere");
    return false;
  }
  SourceManager source(preludePath.string(), *text);
  const SourceManager* previous = diagnostics.source();
  const std::size_t errorCount = diagnostics.diagnostics().size();
  diagnostics.setSource(&source);
  Lexer lexer(source, diagnostics);
  Parser parser(diagnostics, lexer.tokenizeAll());
  std::unique_ptr<Module> prelude = parser.parseModule();
  diagnostics.setSource(previous);
  if (prelude == nullptr || diagnostics.diagnostics().size() > errorCount) {
    return false;
  }
  userModule.insertFront(std::move(prelude->statements()));
  return true;
}

}  // namespace sere
