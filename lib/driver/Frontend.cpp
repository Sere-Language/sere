/// @file Frontend.cpp
/// Runs lexer, parser, prelude load, imports, and type checking.

#include "sere/driver/Frontend.h"

#include "sere/ast/Query.h"
#include "sere/driver/ImportPath.h"
#include "sere/driver/Prelude.h"
#include "sere/driver/Project.h"
#include "sere/lex/Lexer.h"
#include "sere/macro/Expander.h"
#include "sere/parse/Parser.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace sere {
namespace {

/// Points diagnostics at an imported source, then restores the previous file.
class DiagnosticSourceScope {
public:
  DiagnosticSourceScope(DiagnosticEngine& diagnostics, const SourceManager* source)
      : diagnostics_(&diagnostics), previous_(diagnostics.source()) {
    diagnostics.setSource(source);
  }

  DiagnosticSourceScope(const DiagnosticSourceScope&) = delete;
  DiagnosticSourceScope& operator=(const DiagnosticSourceScope&) = delete;

  ~DiagnosticSourceScope() { diagnostics_->setSource(previous_); }

private:
  DiagnosticEngine* diagnostics_;
  const SourceManager* previous_;
};

[[nodiscard]] std::string absolutePath(const std::string& path) {
  std::error_code error;
  const std::filesystem::path abs = std::filesystem::absolute(path, error);
  if (error) {
    return path;
  }
  return abs.string();
}

[[nodiscard]] std::string moduleDocstring(const Module& module) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->fromPrelude()) {
      continue;
    }
    if (statement->kind() == NodeKind::ExprStmt) {
      const Expr& expr = static_cast<const ExprStmt&>(*statement).expression();
      if (expr.kind() == NodeKind::StringLiteral) {
        return static_cast<const StringLiteral&>(expr).value();
      }
    }
    break;
  }
  return {};
}

[[nodiscard]] std::string joinPath(const std::vector<std::string>& parts) {
  std::string text;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index != 0) {
      text += ".";
    }
    text += parts[index];
  }
  return text;
}

void collectImportStmts(const Module& module, std::vector<const ImportStmt*>& out) {
  for (const std::unique_ptr<Stmt>& statement : module.statements()) {
    if (statement->kind() == NodeKind::ImportStmt) {
      out.push_back(static_cast<const ImportStmt*>(statement.get()));
    }
  }
}

[[nodiscard]] std::string exportedName(const Stmt& item) {
  if (item.kind() == NodeKind::FunctionDef) {
    return static_cast<const FunctionDef&>(item).name();
  }
  if (item.kind() == NodeKind::ClassDef) {
    return static_cast<const ClassDef&>(item).name();
  }
  if (item.kind() == NodeKind::EnumDef) {
    return static_cast<const EnumDef&>(item).name();
  }
  if (item.kind() == NodeKind::TypeAlias) {
    return static_cast<const TypeAlias&>(item).name();
  }
  if (item.kind() == NodeKind::MacroDef) {
    return static_cast<const MacroDef&>(item).name();
  }
  if (item.kind() == NodeKind::VarDecl) {
    return static_cast<const VarDecl&>(item).name();
  }
  return {};
}

[[nodiscard]] bool isPrivateNamed(const Module& module, std::string_view name) {
  for (const std::unique_ptr<Stmt>& item : module.statements()) {
    if (item != nullptr && item->isPrivate() && exportedName(*item) == name) {
      return true;
    }
  }
  return false;
}

void bindModuleExports(TypeChecker& checker,
                       TypeContext& types,
                       Module& module,
                       const std::string& moduleName,
                       const ImportStmt& statement,
                       DiagnosticEngine& diagnostics) {
  std::vector<RecordField> exports;
  for (std::unique_ptr<Stmt>& item : module.statements()) {
    if (item->fromPrelude()) {
      continue;
    }
    if (item->kind() == NodeKind::FunctionDef) {
      auto& function = static_cast<FunctionDef&>(*item);
      function.setModulePrefix(moduleName);
      RecordField field;
      field.name = function.name();
      field.type = function.resolvedType();
      field.isPublic = !item->isPrivate();
      field.llvmName =
          function.isExtern() ? function.externName() : moduleName + "_" + function.name();
      for (const ParamDecl& param : function.params()) {
        field.paramNames.push_back(param.name);
        if (param.defaultValue == nullptr) {
          field.requiredArgs += 1;
        }
      }
      exports.push_back(field);
    } else if (item->kind() == NodeKind::ClassDef || item->kind() == NodeKind::EnumDef) {
      RecordField field;
      field.name = item->kind() == NodeKind::ClassDef
                       ? static_cast<ClassDef&>(*item).name()
                       : static_cast<EnumDef&>(*item).name();
      field.type = item->resolvedType();
      field.isPublic = !item->isPrivate();
      exports.push_back(field);
    } else if (item->kind() == NodeKind::TypeAlias) {
      auto& alias = static_cast<TypeAlias&>(*item);
      RecordField field;
      field.name = alias.name();
      field.type = alias.resolvedType();
      field.isPublic = !item->isPrivate();
      exports.push_back(field);
    } else if (item->kind() == NodeKind::MacroDef) {
      RecordField field;
      field.name = static_cast<MacroDef&>(*item).name();
      field.isPublic = !item->isPrivate();
      exports.push_back(field);
    } else if (item->kind() == NodeKind::VarDecl) {
      auto& decl = static_cast<VarDecl&>(*item);
      RecordField field;
      field.name = decl.name();
      field.type = decl.resolvedType();
      field.isPublic = !item->isPrivate();
      exports.push_back(field);
    }
  }
  const Type* moduleType = types.defineModule(moduleName, std::move(exports));
  if (!statement.isFrom()) {
    Symbol symbol;
    symbol.kind = SymbolKind::Module;
    symbol.type = moduleType;
    checker.importSymbol(statement.alias().empty() ? moduleName : statement.alias(), symbol,
                         statement.range().start);
    return;
  }
  if (!statement.star()) {
    for (const std::string& name : statement.names()) {
      const RecordField* found = nullptr;
      for (const RecordField& field : moduleType->fields()) {
        if (field.name == name) {
          found = &field;
          break;
        }
      }
      if (found != nullptr && found->isPublic) {
        continue;
      }
      if (found != nullptr || isPrivateNamed(module, name)) {
        diagnostics.error(statement.range(), "'" + name + "' is private and is not exported");
      } else {
        diagnostics.error(statement.range(),
                          "cannot import name '" + name + "' from '" + moduleName + "'");
      }
    }
  }
  for (const RecordField& field : moduleType->fields()) {
    const bool wanted = statement.star();
    bool named = false;
    for (const std::string& name : statement.names()) {
      named = named || name == field.name;
    }
    if (!wanted && !named) {
      continue;
    }
    if (!field.isPublic) {
      continue;
    }
    Symbol symbol;
    symbol.type = field.type;
    symbol.kind = field.type != nullptr && field.type->isRecord() ? SymbolKind::Class
                  : field.type != nullptr && field.type->kind() == TypeKind::Function
                      ? SymbolKind::Function
                  : field.type != nullptr && field.type->kind() == TypeKind::Alias
                      ? SymbolKind::Type
                      : SymbolKind::Variable;
    if (symbol.kind == SymbolKind::Variable) {
      for (std::unique_ptr<Stmt>& item : module.statements()) {
        if (item->kind() == NodeKind::MacroDef &&
            static_cast<MacroDef&>(*item).name() == field.name) {
          const auto& macro = static_cast<MacroDef&>(*item);
          symbol.kind = SymbolKind::Macro;
          symbol.paramNames = macro.params();
          symbol.typeDisplay = formatMacro(macro);
          symbol.snippet = macroSnippet(macro);
          break;
        }
      }
    }
    if (symbol.kind == SymbolKind::Function) {
      for (std::unique_ptr<Stmt>& item : module.statements()) {
        if (item->kind() == NodeKind::FunctionDef &&
            static_cast<FunctionDef&>(*item).name() == field.name) {
          symbol.function = static_cast<FunctionDef*>(item.get());
          break;
        }
      }
    }
    checker.importSymbol(field.name, symbol, statement.range().start);
  }
}

void bindPreludeExports(TypeChecker& checker, Module& prelude) {
  for (std::unique_ptr<Stmt>& item : prelude.statements()) {
    if (item == nullptr) {
      continue;
    }
    Symbol symbol;
    if (item->kind() == NodeKind::ClassDef) {
      auto& classDef = static_cast<ClassDef&>(*item);
      symbol.kind = SymbolKind::Class;
      symbol.type = classDef.resolvedType();
      (void)checker.importSymbol(classDef.name(), symbol, classDef.range().start);
    } else if (item->kind() == NodeKind::EnumDef) {
      auto& enumDef = static_cast<EnumDef&>(*item);
      symbol.kind = SymbolKind::Class;
      symbol.type = enumDef.resolvedType();
      (void)checker.importSymbol(enumDef.name(), symbol, enumDef.range().start);
    } else if (item->kind() == NodeKind::TypeAlias) {
      auto& alias = static_cast<TypeAlias&>(*item);
      symbol.kind = SymbolKind::Type;
      symbol.type = alias.resolvedType();
      (void)checker.importSymbol(alias.name(), symbol, alias.range().start);
    } else if (item->kind() == NodeKind::FunctionDef) {
      auto& function = static_cast<FunctionDef&>(*item);
      symbol.kind = SymbolKind::Function;
      symbol.type = function.resolvedType();
      symbol.function = &function;
      (void)checker.importSymbol(function.name(), symbol, function.range().start);
    } else if (item->kind() == NodeKind::MacroDef) {
      auto& macro = static_cast<MacroDef&>(*item);
      symbol.kind = SymbolKind::Macro;
      symbol.paramNames = macro.params();
      symbol.typeDisplay = formatMacro(macro);
      symbol.snippet = macroSnippet(macro);
      (void)checker.importSymbol(macro.name(), symbol, macro.range().start);
    }
  }
}

}  // namespace

bool Frontend::analyze(const std::string& path,
                       const std::string& text,
                       const std::filesystem::path& stdlibDir,
                       const SourceOverlay* overlay) {
  diagnostics_ = DiagnosticEngine();
  ast_.reset();
  types_.reset();
  checker_.reset();
  imported_.clear();
  importSources_.clear();
  importPaths_.clear();
  importNames_.clear();
  importIndex_.clear();
  macroUses_.clear();
  overlay_ = overlay;
  source_ = std::make_unique<SourceManager>(path, text);
  diagnostics_.setSource(source_.get());
  const LanguageContext context = resolveLanguageContext(path);
  const std::filesystem::path stdlib = !stdlibDir.empty() ? stdlibDir : context.stdlib;
  std::cerr << "sere-debug: lex/parse user module\n";
  Lexer lexer(*source_, diagnostics_);
  Parser parser(diagnostics_, lexer.tokenizeAll(), source_.get());
  ast_ = parser.parseModule();
  std::cerr << "sere-debug: parsed user module\n";
  if (ast_ == nullptr) {
    ast_ = std::make_unique<Module>(SourceRange{}, std::vector<std::unique_ptr<Stmt>>{});
    return false;
  }
  if (!loadImports(path, stdlib)) {
    std::cerr << "sere-debug: loadImports failed\n";
    return false;
  }
  std::cerr << "sere-debug: imports loaded count=" << imported_.size() << "\n";
  if (!stdlib.empty() && !loadPrelude(*ast_, diagnostics_, stdlib, overlay_)) {
    std::cerr << "sere-debug: loadPrelude failed\n";
    return false;
  }
  std::cerr << "sere-debug: prelude loaded\n";
  collectMacroUses(*ast_, macroUses_);
  MacroEnv macros;
  for (std::unique_ptr<Module>& imported : imported_) {
    macros.addModule(*imported);
  }
  macros.addModule(*ast_);
  MacroExpander expander(diagnostics_, macros);
  for (std::size_t index = 0; index < imported_.size(); ++index) {
    DiagnosticSourceScope scope(diagnostics_, importSources_[index].get());
    (void)expander.expandModule(*imported_[index]);
  }
  std::cerr << "sere-debug: expanded imports\n";
  (void)expander.expandModule(*ast_);
  std::cerr << "sere-debug: expanded user module\n";
  types_ = std::make_unique<TypeContext>();
  std::unique_ptr<Module> preludeChecked;
  if (!stdlib.empty()) {
    preludeChecked = parsePrelude(diagnostics_, stdlib, overlay_);
    if (preludeChecked == nullptr) {
      std::cerr << "sere-debug: parsePrelude failed\n";
      return false;
    }
    std::cerr << "sere-debug: typecheck prelude\n";
    TypeChecker preludeChecker(*types_, diagnostics_);
    preludeChecker.setModuleInfo((stdlib / "prelude.sere").string(), "prelude", "", "", true);
    if (!preludeChecker.check(*preludeChecked)) {
      std::cerr << "sere-debug: prelude check failed\n";
      return false;
    }
  }
  std::cerr << "sere-debug: typecheck imported\n";
  (void)typecheckImported(preludeChecked.get());
  std::cerr << "sere-debug: typecheck main\n";
  checker_ = std::make_unique<TypeChecker>(*types_, diagnostics_);
  checker_->setModuleInfo(absolutePath(path), "__main__", "", moduleDocstring(*ast_), true);
  std::vector<const ImportStmt*> imports;
  collectImportStmts(*ast_, imports);
  for (const ImportStmt* statement : imports) {
    const std::string key = joinPath(statement->modulePath());
    const auto found = importIndex_.find(key);
    if (found == importIndex_.end()) {
      continue;
    }
    bindModuleExports(*checker_, *types_, *imported_[found->second],
                      importNames_[found->second], *statement, diagnostics_);
  }
  (void)checker_->check(*ast_);
  std::cerr << "sere-debug: analyze done\n";
  return !diagnostics_.hasErrors();
}

bool Frontend::loadImports(const std::filesystem::path& origin,
                           const std::filesystem::path& stdlibDir) {
  if (ast_ == nullptr) {
    return true;
  }
  std::vector<const ImportStmt*> pending;
  collectImportStmts(*ast_, pending);
  std::error_code pathError;
  const std::filesystem::path originPath = std::filesystem::absolute(origin, pathError);
  const std::filesystem::path originDir =
      std::filesystem::is_regular_file(originPath, pathError) ? originPath.parent_path()
                                                             : originPath;
  const LanguageContext context = resolveLanguageContext(originPath);
  std::vector<std::filesystem::path> searchDirs = importSearchDirs(originDir, stdlibDir);
  appendLanguageContextDirs(searchDirs, context);
  std::vector<std::filesystem::path> visiting;
  while (!pending.empty()) {
    const ImportStmt* statement = pending.back();
    pending.pop_back();
    const std::string key = joinPath(statement->modulePath());
    if (importIndex_.contains(key)) {
      continue;
    }
    std::string resolveError;
    const std::filesystem::path file =
        resolveImportFile(searchDirs, statement->modulePath(), originPath, &resolveError);
    if (file.empty()) {
      if (!resolveError.empty()) {
        diagnostics_.error(statement->range(), resolveError);
      } else {
        diagnostics_.error(statement->range(), "cannot find module '" + key + "'");
      }
      return false;
    }
    const std::optional<std::string> text = readSourceFile(file, overlay_);
    if (!text.has_value()) {
      diagnostics_.error(statement->range(), "cannot read module '" + file.string() + "'");
      return false;
    }
    auto source = std::make_unique<SourceManager>(file.string(), *text);
    std::unique_ptr<Module> module;
    {
      DiagnosticSourceScope scope(diagnostics_, source.get());
      Lexer lexer(*source, diagnostics_);
      Parser parser(diagnostics_, lexer.tokenizeAll(), source.get());
      const std::size_t errorCount = diagnostics_.diagnostics().size();
      module = parser.parseModule();
      if (module == nullptr || diagnostics_.diagnostics().size() > errorCount) {
        return false;
      }
    }
    collectImportStmts(*module, pending);
    importIndex_[key] = imported_.size();
    importPaths_.push_back(file);
    importNames_.push_back(statement->modulePath().empty() ? file.stem().string()
                                                           : statement->modulePath().back());
    imported_.push_back(std::move(module));
    importSources_.push_back(std::move(source));
  }
  return true;
}

bool Frontend::importsReady(std::size_t index, const std::vector<char>& done) const {
  std::vector<const ImportStmt*> imports;
  collectImportStmts(*imported_[index], imports);
  for (const ImportStmt* statement : imports) {
    const auto found = importIndex_.find(joinPath(statement->modulePath()));
    if (found != importIndex_.end() && found->second != index && done[found->second] == 0) {
      return false;
    }
  }
  return true;
}

bool Frontend::typecheckOneImported(std::size_t index, Module* prelude) {
  DiagnosticSourceScope scope(diagnostics_, importSources_[index].get());
  TypeChecker checker(*types_, diagnostics_);
  const std::string file = absolutePath(importPaths_[index].string());
  const std::string name = importNames_[index];
  checker.setModuleInfo(file, name, "", moduleDocstring(*imported_[index]), true);
  if (prelude != nullptr) {
    bindPreludeExports(checker, *prelude);
  }
  std::vector<const ImportStmt*> imports;
  collectImportStmts(*imported_[index], imports);
  for (const ImportStmt* statement : imports) {
    const auto found = importIndex_.find(joinPath(statement->modulePath()));
    if (found == importIndex_.end()) {
      continue;
    }
    bindModuleExports(checker, *types_, *imported_[found->second], importNames_[found->second],
                      *statement, diagnostics_);
  }
  if (!checker.check(*imported_[index])) {
    return false;
  }
  for (std::unique_ptr<Stmt>& statement : imported_[index]->statements()) {
    if (statement->kind() == NodeKind::FunctionDef) {
      static_cast<FunctionDef&>(*statement).setModulePrefix(name);
    }
  }
  return true;
}

bool Frontend::typecheckImported(Module* prelude) {
  std::vector<char> done(imported_.size(), 0);
  std::size_t remaining = imported_.size();
  while (remaining > 0) {
    bool progressed = false;
    for (std::size_t index = 0; index < imported_.size(); ++index) {
      if (done[index] != 0 || !importsReady(index, done)) {
        continue;
      }
      if (!typecheckOneImported(index, prelude)) {
        return false;
      }
      done[index] = 1;
      remaining -= 1;
      progressed = true;
    }
    if (progressed) {
      continue;
    }
    for (std::size_t index = 0; index < imported_.size(); ++index) {
      if (done[index] != 0) {
        continue;
      }
      if (!typecheckOneImported(index, prelude)) {
        return false;
      }
      done[index] = 1;
      remaining -= 1;
    }
  }
  return true;
}

DiagnosticEngine& Frontend::diagnostics() { return diagnostics_; }

const DiagnosticEngine& Frontend::diagnostics() const { return diagnostics_; }

SourceManager* Frontend::source() { return source_.get(); }

const SourceManager* Frontend::source() const { return source_.get(); }

Module* Frontend::module() { return ast_.get(); }

const Module* Frontend::module() const { return ast_.get(); }

TypeContext* Frontend::types() { return types_.get(); }

const TypeContext* Frontend::types() const { return types_.get(); }

TypeChecker* Frontend::checker() { return checker_.get(); }

const TypeChecker* Frontend::checker() const { return checker_.get(); }

const std::vector<std::unique_ptr<Module>>& Frontend::importedModules() const { return imported_; }

const std::vector<std::filesystem::path>& Frontend::importedModulePaths() const {
  return importPaths_;
}

std::vector<std::string> Frontend::importedModuleNames() const {
  std::vector<std::string> names;
  names.reserve(importIndex_.size());
  for (const auto& entry : importIndex_) {
    names.push_back(entry.first);
  }
  return names;
}

const std::vector<MacroUse>& Frontend::macroUses() const { return macroUses_; }

}  // namespace sere
