/// @file Parse.h
/// Parse Sere expressions from captured macro source.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/lex/Token.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sere {

class DiagnosticEngine;

[[nodiscard]] std::unique_ptr<Expr> parseSereExpr(DiagnosticEngine& diagnostics,
                                                  std::string_view text,
                                                  SourceLocation base);

[[nodiscard]] std::vector<std::unique_ptr<Expr>> parseSereExprList(
    DiagnosticEngine& diagnostics, std::string_view text, SourceLocation base);

[[nodiscard]] std::unique_ptr<Expr> parseSereExprFromTokens(DiagnosticEngine& diagnostics,
                                                            std::vector<Token> tokens);

}  // namespace sere
