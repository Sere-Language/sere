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
                                                      std::string moduleName,
                                                      const std::vector<const Module*>* imported = nullptr,
                                                      const std::vector<std::string>* importedNames = nullptr);

private:
  [[nodiscard]] serem::IRType lowerType(const Type* type) const;
  [[nodiscard]] bool emitFunction(const FunctionDef& function);
  [[nodiscard]] bool emitStatement(const Stmt& statement);
  [[nodiscard]] bool emitBlock(const std::vector<std::unique_ptr<Stmt>>& statements);
  [[nodiscard]] bool emitIf(const IfStmt& statement);
  [[nodiscard]] bool emitWhile(const WhileStmt& statement);
  [[nodiscard]] bool emitFor(const ForStmt& statement);
  void declareTypes(const Module& module);
  void declareClass(const ClassDef& classDef);
  void declareEnum(const EnumDef& enumDef);
  [[nodiscard]] serem::ValuePtr emitExpression(const Expr& expression);
  [[nodiscard]] serem::ValuePtr emitName(const NameExpr& expression);
  [[nodiscard]] serem::ValuePtr emitBinary(const BinaryExpr& expression);
  [[nodiscard]] serem::ValuePtr emitCall(const CallExpr& expression);
  [[nodiscard]] serem::ValuePtr emitUnary(const UnaryExpr& expression);
  [[nodiscard]] serem::ValuePtr emitMember(const MemberExpr& expression);
  [[nodiscard]] serem::ValuePtr emitIndex(const IndexExpr& expression);
  [[nodiscard]] serem::ValuePtr emitAggregate(const Expr& expression);
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
  std::unordered_map<const FunctionDef*, std::string> functionNames_;
  std::unordered_map<std::string, std::string> decorators_;
  std::unordered_map<std::string, std::string> classBases_;
  std::unordered_map<std::string, std::vector<serem::IRType>> classFields_;
  std::string currentOwnerClass_;
  std::vector<serem::BasicBlock*> breakTargets_;
  std::vector<serem::BasicBlock*> continueTargets_;
  serem::ValuePtr coroutineToken_;
};

} // namespace sere
