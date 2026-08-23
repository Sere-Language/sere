/// @file Frontend.h
/// Shared lex/parse/typecheck pipeline used by the compiler and language server.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/ast/Query.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/sema/TypeChecker.h"
#include "sere/source/SourceManager.h"
#include "sere/types/TypeContext.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sere {

class Frontend {
public:
  [[nodiscard]] bool analyze(const std::string& path,
                             const std::string& text,
                             const std::filesystem::path& stdlibDir);

  [[nodiscard]] DiagnosticEngine& diagnostics();
  [[nodiscard]] const DiagnosticEngine& diagnostics() const;
  [[nodiscard]] SourceManager* source();
  [[nodiscard]] const SourceManager* source() const;
  [[nodiscard]] Module* module();
  [[nodiscard]] const Module* module() const;
  [[nodiscard]] TypeContext* types();
  [[nodiscard]] const TypeContext* types() const;
  [[nodiscard]] TypeChecker* checker();
  [[nodiscard]] const TypeChecker* checker() const;
  [[nodiscard]] const std::vector<std::unique_ptr<Module>>& importedModules() const;
  [[nodiscard]] std::vector<std::string> importedModuleNames() const;
  [[nodiscard]] const std::vector<MacroUse>& macroUses() const;

private:
  bool loadImports(const std::filesystem::path& origin, const std::filesystem::path& stdlibDir);
  [[nodiscard]] bool importsReady(std::size_t index, const std::vector<char>& done) const;
  [[nodiscard]] bool typecheckOneImported(std::size_t index, Module* prelude);
  bool typecheckImported(Module* prelude);
  DiagnosticEngine diagnostics_{};
  std::unique_ptr<SourceManager> source_{};
  std::unique_ptr<Module> ast_{};
  std::unique_ptr<TypeContext> types_{};
  std::unique_ptr<TypeChecker> checker_{};
  std::vector<std::unique_ptr<SourceManager>> importSources_{};
  std::vector<std::unique_ptr<Module>> imported_{};
  std::vector<std::filesystem::path> importPaths_{};
  std::unordered_map<std::string, std::size_t> importIndex_{};
  std::vector<MacroUse> macroUses_{};
};

}  // namespace sere
