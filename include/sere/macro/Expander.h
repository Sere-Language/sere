/// @file Expander.h
/// Expands MacroInvoke nodes using registered MacroDef items.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/macro/Quote.h"
#include "sere/lex/Token.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sere {

class DiagnosticEngine;

class MacroEnv {
public:
  bool add(const MacroDef& def);
  [[nodiscard]] const MacroDef* find(const std::string& name) const;
  void addModule(Module& module);

private:
  std::unordered_map<std::string, const MacroDef*> macros_{};
};

class MacroExpander {
public:
  MacroExpander(DiagnosticEngine& diagnostics, MacroEnv& env, std::uint32_t fuel = 128);

  [[nodiscard]] bool expandModule(Module& module);

private:
  bool expandStmtList(std::vector<std::unique_ptr<Stmt>>& statements);
  bool expandInside(Stmt& statement);
  std::unique_ptr<Expr> expandExpr(const Expr& expr);
  std::vector<std::unique_ptr<Stmt>> expandInvokeStmt(const MacroInvokeStmt& invoke);
  std::unique_ptr<Expr> expandInvokeExpr(const MacroInvokeExpr& invoke);
  std::unique_ptr<Expr> expandDef(const MacroDef& def,
                                  const std::string& rawText,
                                  SourceRange rawRange,
                                  const std::vector<Token>& tokens,
                                  SourceRange callSite,
                                  MacroDelimiter delimiter);
  std::unique_ptr<Expr> expandQuote(const MacroDef& def,
                                    MacroBindings env,
                                    SourceRange callSite);
  std::unique_ptr<Expr> expandMatch(const MacroDef& def,
                                    const std::vector<Token>& tokens,
                                    SourceRange callSite);
  std::unique_ptr<Expr> expandRaw(const MacroDef& def,
                                  const std::string& rawText,
                                  SourceRange rawRange,
                                  SourceRange callSite);
  std::unique_ptr<Expr> expandPipeline(const std::string& rawText, SourceRange callSite);
  std::unique_ptr<Expr> wrapResult(const MacroDef& def, std::unique_ptr<Expr> value,
                                   SourceRange callSite);

  DiagnosticEngine* diagnostics_;
  MacroEnv* env_;
  std::uint32_t fuel_;
  std::uint32_t nextMark_ = 1;
};

}  // namespace sere
