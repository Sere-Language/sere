/// @file SeremGenerator.h
/// Lowers the typed Sere AST into the target-independent Serem IR.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/codegen/Serem.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace sere {

class DiagnosticEngine;
class TypeContext;

class SeremGenerator {
public:
  SeremGenerator(DiagnosticEngine& diagnostics, TypeContext& types);

  [[nodiscard]] std::unique_ptr<serem::IRModule> emit(const Module& module,
                                                      std::string moduleName);

private:
  [[nodiscard]] serem::IRType lowerType(const Type* type) const;
  [[nodiscard]] bool emitFunction(const FunctionDef& function);
  [[nodiscard]] bool emitStatement(const Stmt& statement);
  [[nodiscard]] serem::ValuePtr emitExpression(const Expr& expression);
  [[nodiscard]] serem::ValuePtr emitName(const NameExpr& expression);
  [[nodiscard]] serem::ValuePtr emitBinary(const BinaryExpr& expression);
  [[nodiscard]] serem::ValuePtr emitCall(const CallExpr& expression);
  [[nodiscard]] serem::ValuePtr emitUnary(const UnaryExpr& expression);
  [[nodiscard]] bool bindLocal(const std::string& name, serem::ValuePtr value);
  [[nodiscard]] serem::ValuePtr local(const std::string& name) const;
  [[nodiscard]] bool unsupported(const Node& node, std::string_view feature);
  [[nodiscard]] std::string functionName(const FunctionDef& function) const;

  DiagnosticEngine* diagnostics_;
  std::unique_ptr<serem::IRModule> module_;
  serem::IRFunction* function_ = nullptr;
  std::unique_ptr<serem::IRBuilder> builder_;
  std::unordered_map<std::string, serem::ValuePtr> locals_;
  std::unordered_map<std::string, serem::IRType> functions_;
};

} // namespace sere
