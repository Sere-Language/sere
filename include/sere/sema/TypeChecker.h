/// @file TypeChecker.h
/// Resolves type expressions, checks declarations, and types expressions.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/types/Intrinsic.h"
#include "sere/types/TypeContext.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
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
  SourceLocation declarationLocation{};
  IntrinsicKind intrinsic = IntrinsicKind::None;
  const FunctionDef* function = nullptr;
  bool readonly = false;
  bool staticStorage = false;
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
  const Type* type = nullptr;
  std::string container;
  std::vector<std::string> paramNames{};
  std::vector<std::string> paramTypes{};
  std::string returnType{};
  SourceLocation location{};
  SourceRange range{};
  SourceRange scopeRange{};
  std::size_t scopeDepth = 0;
  bool navigable = true;
  std::string snippet{};
};

class TypeChecker {
public:
  TypeChecker(TypeContext& types, DiagnosticEngine& diagnostics);

  [[nodiscard]] bool check(Module& module);
  void setModuleInfo(
      std::string file, std::string name, std::string package, std::string doc, bool debug);
  [[nodiscard]] const std::vector<SemanticSymbol>& symbols() const;
  [[nodiscard]] std::vector<const SemanticSymbol*> visibleSymbolsAt(std::uint32_t offset) const;
  // Keep source signatures for imported constructors and methods, including defaults.
  void importMethod(const Type* record, FunctionDef& method);
  bool importSymbol(const std::string& name, Symbol symbol, SourceLocation location);
  [[nodiscard]] const Type* typeOfName(std::string_view name) const;
  [[nodiscard]] const Type* typeOfPath(const std::vector<std::string>& parts) const;

private:
  void pushScope(SourceRange range = {});
  void popScope();
  [[nodiscard]] bool
  declare(const std::string& name, Symbol symbol, SourceLocation location, bool navigable = true);
  [[nodiscard]] Symbol* lookup(const std::string& name);
  void registerBuiltins();
  void injectModuleGlobals();
  bool collectClassNames(Module& module);
  bool collectEnumNames(Module& module);
  bool collectEnums(Module& module);
  bool collectAliases(Module& module);
  bool flattenClass(ClassDef& classDef);
  bool collectClassFields(Module& module);
  bool collectFunctions(Module& module);
  bool collectMacros(Module& module);
  bool collectMethods(Module& module);
  bool bindLocalClassImports(Module& module);
  bool collectExports(Module& module);
  bool applyDecorators(Module& module);
  bool applyFunctionDecorators(FunctionDef& function);
  bool applyClassDecorators(ClassDef& classDef);
  [[nodiscard]] const Type* applyDecoratorChain(std::vector<std::unique_ptr<Expr>>& exprs,
                                                const Type* target,
                                                SourceRange range);
  [[nodiscard]] const Type*
  callDecorator(const Type* wrapper, const Type* target, SourceRange range);
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
  [[nodiscard]] const Type* resolveTypeFromExpr(Expr& expr, bool reportMissing);
  [[nodiscard]] const Type* classType(const std::string& name) const;
  [[nodiscard]] const Type* resolveParamType(const FunctionDef& function, std::size_t index);
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
  [[nodiscard]] const Type*
  checkBuiltinMethod(CallExpr& expr, const Type* objectType, const std::string& name);
  [[nodiscard]] const Type* checkMember(MemberExpr& expr);
  [[nodiscard]] const Type* checkIndex(IndexExpr& expr);
  [[nodiscard]] const Type* checkListLiteral(ListLiteral& expr);
  [[nodiscard]] const Type* checkDictLiteral(DictLiteral& expr);
  [[nodiscard]] const Type* checkComprehension(ComprehensionExpr& expr);
  [[nodiscard]] const Type* checkInterpolated(InterpolatedStringExpr& expr);
  [[nodiscard]] const Type* iterableElementType(Expr& iterable);
  bool bindCollectionInit(Expr& init, const Type* dest);
  [[nodiscard]] bool isClassName(const Expr& expr);
  [[nodiscard]] const Type*
  rewriteDunderBinary(BinaryExpr& expr, const Type* left, const Type* right);
  [[nodiscard]] const Type* checkBinary(BinaryExpr& expr);
  [[nodiscard]] const Type* checkAwait(AwaitExpr& expr);
  [[nodiscard]] const Type* taskType(const Type* inner) const;
  [[nodiscard]] const Type* unwrapTask(const Type* task) const;
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
  [[nodiscard]] const Type* checkCallableCall(CallExpr& expr, const Type* constraint);
  [[nodiscard]] bool callableSatisfies(const Type* from, const Type* to) const;
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
  [[nodiscard]] bool ensureLiteralFits(const Expr& expr, const Type* dest);
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
  bool resolveTypeConstraints(const std::vector<std::unique_ptr<TypeExpr>>& constraints);
  bool ensureRecordConstraints(const Type* record);
  bool checkTypeConstraints(const std::vector<std::string>& names,
                            const std::vector<const Type*>& constraints,
                            const std::vector<const Type*>& args,
                            SourceRange range);
  [[nodiscard]] bool satisfiesTypeConstraint(const Type* argument, const Type* constraint) const;
  [[nodiscard]] const Type* specializeCall(CallExpr& expr, const Symbol& symbol);

  std::unordered_map<std::string, const Type*> activeTypeConstraints_{};
  std::unordered_map<const Type*, const std::vector<std::unique_ptr<TypeExpr>>*>
      recordConstraintExprs_{};
  std::vector<const Type*> resolvingConstraints_{};
  TypeContext* types_;
  DiagnosticEngine* diagnostics_;
  std::vector<std::unordered_map<std::string, Symbol>> scopes_{};
  std::vector<SourceRange> scopeRanges_{};
  std::vector<SemanticSymbol> symbols_{};
  int loopDepth_ = 0;
  std::string currentClass_{};
  [[nodiscard]] bool canAccessPrivate(const Type* owner) const;
  Symbol* lookupAssignment(const std::string& name);
  std::vector<std::pair<FunctionDef*, std::size_t>> enclosingFunctions_{};
  std::unordered_map<std::string, ClassDef*> classes_{};
  std::unordered_map<const Type*, std::unordered_map<std::string, FunctionDef*>> importedMethods_{};
  std::unordered_map<std::string, bool> flattened_{};
  std::string moduleFile_{};
  std::string moduleName_{"__main__"};
  std::string modulePackage_{};
  std::string moduleDoc_{};
  bool moduleDebug_ = true;
  std::string currentFunctionName_{};
  unsigned yieldCleanupDepth_ = 0;
  bool currentFunctionIsGenerator_ = false;
  bool currentFunctionIsAsync_ = false;
  std::string currentPropertyName_{};
  int lambdaDepth_ = 0;
  int lambdaCounter_ = 0;
  int nestedFunctionCounter_ = 0;
  FunctionDef* nestedFunction_ = nullptr;
  std::vector<const Type*>* inferredNestedReturns_ = nullptr;
  std::size_t nestedOuterScope_ = 0;
};

} // namespace sere
