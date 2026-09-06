/// @file Quote.h
/// AST clone and splice substitution for macro templates.

#pragma once

#include "sere/ast/Syntax.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sere {

struct MacroBindings {
  std::unordered_map<std::string, std::vector<std::unique_ptr<Expr>>> exprs;
  std::unique_ptr<Expr> input;
};

[[nodiscard]] std::unique_ptr<TypeExpr> cloneTypeExpr(const TypeExpr& expr);
[[nodiscard]] std::unique_ptr<Expr> cloneExpr(const Expr& expr);
[[nodiscard]] std::unique_ptr<Stmt> cloneStmt(const Stmt& stmt);

[[nodiscard]] std::unique_ptr<Expr>
substExpr(const Expr& tmpl, const MacroBindings& env, std::uint32_t mark, SourceRange callSite);
[[nodiscard]] std::vector<std::unique_ptr<Stmt>>
substStmts(const std::vector<std::unique_ptr<Stmt>>& body,
           const MacroBindings& env,
           std::uint32_t mark,
           SourceRange callSite);

void applyHygiene(Stmt& stmt, std::uint32_t mark);
void applyHygiene(Expr& expr, std::uint32_t mark);

} // namespace sere
