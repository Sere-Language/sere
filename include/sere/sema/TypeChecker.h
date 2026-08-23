/// @file TypeChecker.h
/// Resolves type expressions, checks declarations, and types expressions.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/types/Intrinsic.h"
#include "sere/types/TypeContext.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sere {

class DiagnosticEngine;

enum class SymbolKind {
  Variable,
  Function,
  Class,
  Intrinsic,
  Type,
  Module,
  Macro,
};

struct Symbol {
  SymbolKind kind = SymbolKind::Variable;
  const Type* type = nullptr;
  IntrinsicKind intrinsic = IntrinsicKind::None;
  const FunctionDef* function = nullptr;
  bool readonly = false;
  std::string compileTimeText{};
  bool hasCompileTimeBool = false;
  bool compileTimeBool = false;
  std::vector<std::string> paramNames{};
  std::string typeDisplay{};
  std::string snippet{};
};

struct SemanticSymbol {
  std::string name;
  std::string kind;
  std::string typeDisplay;
  std::string container;
  std::vector<std::string> paramNames{};
  std::vector<std::string> paramTypes{};
  std::string returnType{};
  SourceLocation location{};
  SourceRange range{};
  bool navigable = true;
  std::string snippet{};
};

class TypeChecker {
public:
  TypeChecker(TypeContext& types, DiagnosticEngine& diagnostics);

  [[nodiscard]] bool check(Module& module);
  void setModuleInfo(std::string file, std::string name, std::string package, std::string doc,
                     bool debug);
  [[nodiscard]] const std::vector<SemanticSymbol>& symbols() const;
  bool importSymbol(const std::string& name, Symbol symbol, SourceLocation location);

private:
  void pushScope();
  void popScope();
  [[nodiscard]] bool declare(const std::string& name,
                             Symbol symbol,
                             SourceLocation location,
                             bool navigable = true);
  [[nodiscard]] Symbol* lookup(const std::string& name);
  void registerBuiltins();
  void injectModuleGlobals();
  bool collectClassNames(Module& module);
  bool collectEnums(Module& module);
  bool collectAliases(Module& module);
  bool flattenClass(ClassDef& classDef);
  bool collectClassFields(Module& module);
  bool collectFunctions(Module& module);
  bool collectMacros(Module& module);
  bool collectMethods(Module& module);
  void inheritBaseMethods(const Type* record, const Type* base);
  bool checkBodies(Module& module);
  bool checkFunctionBody(FunctionDef& function);
  bool checkStatement(Stmt& statement, const Type* expectedReturn);
  bool checkVarDecl(VarDecl& decl);
  bool checkAssign(AssignStmt& statement);
  bool bindCallableAlias(AssignStmt& statement, NameExpr& target, const Symbol& source);
  bool checkReturn(ReturnStmt& statement, const Type* expectedReturn);
  [[nodiscard]] const Type* resolveTypeExpr(const TypeExpr& expr);
  [[nodiscard]] const Type* resolveNamedType(const std::string& name,
                                             const std::vector<std::unique_ptr<TypeExpr>>& args,
                                             SourceRange range,
                                             bool reportMissing);
  [[nodiscard]] const Type* resolveParamType(const FunctionDef& function,
                                             std::size_t index);
  [[nodiscard]] const Type* resolveParamDeclType(const ParamDecl& param);
  [[nodiscard]] bool validateParamList(const std::vector<ParamDecl>& params, SourceRange range);
  [[nodiscard]] bool checkFunctionArguments(CallExpr& expr,
                                            const std::vector<ParamDecl>& params,
                                            const std::vector<const Type*>& paramTypes,
                                            std::string_view calleeLabel,
                                            std::size_t selfSkip = 0);
  [[nodiscard]] FunctionDef* findMethodDef(const Type* record, std::string_view methodName);
  [[nodiscard]] const Type* checkExpr(Expr& expr);
  [[nodiscard]] const Type* checkName(NameExpr& expr);
  [[nodiscard]] const Type* checkCall(CallExpr& expr);
  [[nodiscard]] const Type* checkCast(CastExpr& expr);
  [[nodiscard]] const Type* checkCastValue(Expr& value, const Type* target, SourceRange range);
  [[nodiscard]] const Type* checkConstructor(CallExpr& expr, const Type* record);
  [[nodiscard]] const Type* checkMethodCall(CallExpr& expr);
  [[nodiscard]] const Type* checkMember(MemberExpr& expr);
  [[nodiscard]] const Type* checkIndex(IndexExpr& expr);
  [[nodiscard]] const Type* checkListLiteral(ListLiteral& expr);
  [[nodiscard]] const Type* checkDictLiteral(DictLiteral& expr);
  [[nodiscard]] const Type* checkComprehension(ComprehensionExpr& expr);
  [[nodiscard]] const Type* checkInterpolated(InterpolatedStringExpr& expr);
  [[nodiscard]] const Type* iterableElementType(Expr& iterable);
  bool bindCollectionInit(Expr& init, const Type* dest);
  [[nodiscard]] bool isClassName(const Expr& expr);
  [[nodiscard]] const Type* rewriteDunderBinary(BinaryExpr& expr, const Type* left,
                                                const Type* right);
  [[nodiscard]] const Type* checkBinary(BinaryExpr& expr);
  [[nodiscard]] const Type* checkUnary(UnaryExpr& expr);
  [[nodiscard]] const Type* checkDeref(UnaryExpr& expr, const Type* operand);
  [[nodiscard]] const Type* checkAddrOf(UnaryExpr& expr, const Type* operand);
  [[nodiscard]] const Type* checkIntrinsicCall(CallExpr& expr, IntrinsicKind kind);
  [[nodiscard]] const Type* checkSuperCall(CallExpr& expr);
  [[nodiscard]] const Type* resolveSuperType(SourceRange range);
  [[nodiscard]] const Type* checkTernary(TernaryExpr& expr);
  [[nodiscard]] const Type* checkTuple(TupleExpr& expr);
  [[nodiscard]] const Type* checkWalrus(WalrusExpr& expr);
  [[nodiscard]] const Type* checkLambda(LambdaExpr& expr);
  [[nodiscard]] const Type* checkIndirectCall(CallExpr& expr, const Type* functionType);
  bool checkWith(WithStmt& statement, const Type* expectedReturn);
  [[nodiscard]] std::optional<bool> constBool(const Expr& expr) const;
  bool checkIf(IfStmt& statement, const Type* expectedReturn);
  bool checkWhile(WhileStmt& statement, const Type* expectedReturn);
  bool checkFor(ForStmt& statement, const Type* expectedReturn);
  bool checkAssert(AssertStmt& statement);
  bool checkRaise(RaiseStmt& statement);
  bool checkTry(TryStmt& statement, const Type* expectedReturn);
  bool checkMatch(MatchStmt& statement, const Type* expectedReturn);
  bool checkDel(DelStmt& statement);
  bool checkDefer(DeferStmt& statement, const Type* expectedReturn);
  bool checkBreak(const BreakStmt& statement);
  bool checkContinue(const ContinueStmt& statement);
  [[nodiscard]] bool isAssignable(const Type* from, const Type* to) const;
  [[nodiscard]] bool canCast(const Type* from, const Type* to) const;
  [[nodiscard]] bool isPrintable(const Type* type) const;
  [[nodiscard]] bool isVoidLike(const Type* type) const;
  [[nodiscard]] bool declareInferred(NameExpr& name, const Type* type, SourceLocation location);
  [[nodiscard]] std::string suggestName(const std::string& name) const;
  void reportUnknown(SourceRange range, const std::string& kind, const std::string& name);
  void recordSymbol(const std::string& name,
                    const std::string& kind,
                    const Type* type,
                    SourceLocation location,
                    std::string container,
                    std::vector<std::string> paramNames = {});
  [[nodiscard]] const Type* specializeCall(CallExpr& expr, const Symbol& symbol);

  TypeContext* types_;
  DiagnosticEngine* diagnostics_;
  std::vector<std::unordered_map<std::string, Symbol>> scopes_{};
  std::vector<SemanticSymbol> symbols_{};
  int loopDepth_ = 0;
  std::string currentClass_{};
  std::unordered_map<std::string, ClassDef*> classes_{};
  std::unordered_map<std::string, bool> flattened_{};
  std::string moduleFile_{};
  std::string moduleName_{"__main__"};
  std::string modulePackage_{};
  std::string moduleDoc_{};
  bool moduleDebug_ = true;
  std::string currentFunctionName_{};
  int lambdaDepth_ = 0;
  int lambdaCounter_ = 0;
};

}  // namespace sere
