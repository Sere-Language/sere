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
#include <string>
#include <string_view>
#include <system_error>

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

[[nodiscard]] bool stdlibHasPrelude(const std::filesystem::path& directory) {
  std::error_code error;
  return !directory.empty() && std::filesystem::exists(directory / "prelude.sere", error);
}

[[nodiscard]] std::string trimCfg(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
    text.remove_suffix(1);
  }
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
    text.remove_prefix(1);
    text.remove_suffix(1);
  }
  return std::string(text);
}

[[nodiscard]] std::filesystem::path homeFromSereCfg(const std::filesystem::path& cfgPath) {
  std::ifstream input(cfgPath, std::ios::binary);
  if (!input) {
    return {};
  }
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    if (trimCfg(line.substr(0, eq)) != "home") {
      continue;
    }
    return std::filesystem::path(trimCfg(line.substr(eq + 1)));
  }
  return {};
}

}  // namespace

std::filesystem::path findStdlibDirectory(const std::filesystem::path& compilerDir) {
  if (const char* fromEnv = std::getenv("SERE_STDLIB");
      fromEnv != nullptr && fromEnv[0] != '\0' && stdlibHasPrelude(fromEnv)) {
    return fromEnv;
  }
  const std::filesystem::path nextToCompiler = compilerDir / "stdlib";
  if (stdlibHasPrelude(nextToCompiler)) {
    return nextToCompiler;
  }
  const std::filesystem::path venvStdlib = compilerDir.parent_path() / "stdlib";
  if (stdlibHasPrelude(venvStdlib)) {
    return venvStdlib;
  }
  const std::filesystem::path toolchainHome =
      homeFromSereCfg(compilerDir.parent_path() / "sere.cfg");
  if (stdlibHasPrelude(toolchainHome / "stdlib")) {
    return toolchainHome / "stdlib";
  }
  if (stdlibHasPrelude(toolchainHome)) {
    return toolchainHome;
  }
  const std::filesystem::path projectVenv =
      std::filesystem::current_path() / "venv" / "stdlib";
  if (stdlibHasPrelude(projectVenv)) {
    return projectVenv;
  }
  const std::filesystem::path sourceTree = std::filesystem::current_path() / "stdlib";
  if (stdlibHasPrelude(sourceTree)) {
    return sourceTree;
  }
  return nextToCompiler;
}

std::unique_ptr<Module> parsePrelude(DiagnosticEngine& diagnostics,
                                     const std::filesystem::path& stdlibDir) {
  const std::filesystem::path preludePath = stdlibDir / "prelude.sere";
  const std::optional<std::string> text = readAll(preludePath);
  if (!text.has_value()) {
    diagnostics.error("cannot load standard library prelude from '" + preludePath.string() + "'");
    diagnostics.help("set SERE_STDLIB or keep stdlib/ next to sere");
    return nullptr;
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
    return nullptr;
  }
  return prelude;
}

bool loadPrelude(Module& userModule,
                 DiagnosticEngine& diagnostics,
                 const std::filesystem::path& stdlibDir) {
  std::unique_ptr<Module> prelude = parsePrelude(diagnostics, stdlibDir);
  if (prelude == nullptr) {
    return false;
  }
  userModule.insertFront(std::move(prelude->statements()));
  return true;
}

}  // namespace sere
