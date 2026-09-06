/// @file IRGenerator.h
/// Lowers a typed Sere module to LLVM IR without hardcoded language functions.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/types/TypeContext.h"

#include <llvm/IR/IRBuilder.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace llvm {
class BasicBlock;
class Function;
class LLVMContext;
class Module;
class Type;
class Value;
} // namespace llvm

namespace sere {

class DiagnosticEngine;

class IRGenerator {
public:
  IRGenerator(llvm::LLVMContext& context, DiagnosticEngine& diagnostics, TypeContext& types);

  [[nodiscard]] std::unique_ptr<llvm::Module>
  emit(const Module& ast,
       const std::string& moduleName,
       const std::vector<const Module*>* imported = nullptr);

private:
  llvm::Type* lower(const Type* type);
  llvm::Value* emitAnyTypeMatch(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* target);
  std::uint64_t valueSize(const Type* type);
  llvm::Function*
  runtimeDecl(const char* name, llvm::Type* returnType, const std::vector<llvm::Type*>& params);
  llvm::FunctionType* llvmFunctionType(const FunctionDef& function);
  std::unordered_map<std::string, llvm::Function*> functions_{};
  std::unordered_map<std::string, const FunctionDef*> functionDefs_{};
  std::unordered_map<std::string, bool> externFunctions_{};
  std::unordered_set<std::string> reachable_{};
  std::vector<const Type*> classTypes_{};
  void collectReachable(const std::vector<const Module*>& modules);
  void collectClassTypes(const Module& ast);
  [[nodiscard]] bool shouldEmit(const FunctionDef& function) const;
  void declareFunctions(const Module& ast);
  void declareGlobals(const Module& ast);
  llvm::Value* declareGlobal(const std::string& name, const Type* type);
  void
  rememberStaticDecl(const std::string& name, const FunctionDef& function, const VarDecl& decl);
  void emitModuleInitFn(const Module& ast, const std::vector<const Module*>* imported = nullptr);
  llvm::Value* emitIndex(llvm::IRBuilder<>& builder, const IndexExpr& expr);
  llvm::Value* emitListLiteral(llvm::IRBuilder<>& builder, const ListLiteral& expr);
  llvm::Value* emitDictLiteral(llvm::IRBuilder<>& builder, const DictLiteral& expr);
  llvm::Value* emitCollectionNew(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitAppend(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitBuiltinMethod(llvm::IRBuilder<>& builder, const CallExpr& expr);
  bool emitDictAssign(llvm::IRBuilder<>& builder, const IndexExpr& target, const Expr& value);
  llvm::Value* emitTempSlot(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* type);
  llvm::Value* emitIndexI64(llvm::IRBuilder<>& builder, const Expr& index);
  [[nodiscard]] static int dictKeyKind(const Type* keyType);
  [[nodiscard]] static std::string classStaticName(const Type* record, std::string_view field);
  [[nodiscard]] static std::string functionStaticName(const FunctionDef& function,
                                                      std::string_view name);
  bool emitFunction(const FunctionDef& function, const std::string& overrideName = {});
  bool setupAsyncCoroutine(llvm::IRBuilder<>& builder,
                           llvm::Function* llvmFn,
                           const Type* returnType);
  void buildAsyncTail(llvm::IRBuilder<>& builder, llvm::Function* llvmFn);
  void declareInstantiations();
  bool emitInstantiations(const std::vector<const Module*>& modules);
  bool emitCMainWrapper(llvm::Function* userMain);
  bool emitStatement(llvm::IRBuilder<>& builder, const Stmt& statement, const Type* returnType);
  llvm::Value* emitExpr(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitAwait(llvm::IRBuilder<>& builder, const AwaitExpr& expr);
  llvm::Value* emitTaskBox(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* inner);
  llvm::Value* emitTaskUnbox(llvm::IRBuilder<>& builder, llvm::Value* task, const Type* inner);
  llvm::Value* emitCall(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitConstruct(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitInitConstruct(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitMethodCall(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitIntrinsic(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitPrint(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitCast(llvm::IRBuilder<>& builder, const CastExpr& expr);
  llvm::Value* emitCastValue(llvm::IRBuilder<>& builder,
                             const Expr& value,
                             const Type* from,
                             const Type* to,
                             SourceRange range);
  llvm::Value*
  emitNumericCast(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* from, const Type* to);
  llvm::Value* emitStrLiteral(llvm::IRBuilder<>& builder, std::string_view text);
  llvm::Value* emitStrFromC(llvm::IRBuilder<>& builder, const char* fnName, llvm::Value* value);
  llvm::Value* emitStrConcat(llvm::IRBuilder<>& builder, llvm::Value* left, llvm::Value* right);
  llvm::Value* emitToStr(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitScalarToStr(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* type);
  llvm::Value* emitValueRepr(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* type);
  llvm::Value* emitBuiltinExpr(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitBuiltinDefault(llvm::IRBuilder<>& builder, const Type* type);
  void emitAnyRepr();
  llvm::Value* emitUnionStr(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitRecordStr(llvm::IRBuilder<>& builder, const Expr& object);
  llvm::Value* emitEnumStr(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitPointerStr(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitListStr(llvm::IRBuilder<>& builder, const Expr& expr);
  void emitWriteStr(llvm::IRBuilder<>& builder, llvm::Value* str);
  void emitWriteValue(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitInterpolated(llvm::IRBuilder<>& builder, const InterpolatedStringExpr& expr);
  llvm::Value* emitLogical(llvm::IRBuilder<>& builder, const BinaryExpr& expr);
  llvm::Value* emitBinary(llvm::IRBuilder<>& builder, const BinaryExpr& expr);
  llvm::Value* emitUnary(llvm::IRBuilder<>& builder, const UnaryExpr& expr);
  bool emitIf(llvm::IRBuilder<>& builder, const IfStmt& statement, const Type* returnType);
  bool emitWhile(llvm::IRBuilder<>& builder, const WhileStmt& statement, const Type* returnType);
  bool emitFor(llvm::IRBuilder<>& builder, const ForStmt& statement, const Type* returnType);
  bool emitAssert(llvm::IRBuilder<>& builder, const AssertStmt& statement);
  llvm::Value* emitRange(llvm::IRBuilder<>& builder, const CallExpr& expr);
  llvm::Value* emitParse(llvm::IRBuilder<>& builder, const CallExpr& expr, bool optional);
  bool emitBlock(llvm::IRBuilder<>& builder,
                 const std::vector<std::unique_ptr<Stmt>>& body,
                 const Type* returnType);
  llvm::Value* emitDefault(const Type* type);
  llvm::Value*
  emitCoerce(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* from, const Type* to);
  void emitReturn(llvm::IRBuilder<>& builder, llvm::Value* value, const Type* returnType);
  llvm::Value*
  emitStrCompare(llvm::IRBuilder<>& builder, BinaryOp op, llvm::Value* left, llvm::Value* right);
  void widenIntegerPair(llvm::IRBuilder<>& builder,
                        llvm::Value*& left,
                        llvm::Value*& right,
                        const Type* leftType,
                        const Type* rightType);
  llvm::Value*
  createLocalSlot(llvm::IRBuilder<>& builder, const std::string& name, const Type* type);
  [[nodiscard]] static unsigned enumTagFromField(const RecordField* field);
  llvm::Value*
  emitEnumSwitchStr(llvm::IRBuilder<>& builder, llvm::Value* tag, const Type* type, bool qualified);
  llvm::Value* emitComprehension(llvm::IRBuilder<>& builder, const ComprehensionExpr& expr);
  llvm::Value* emitTernary(llvm::IRBuilder<>& builder, const TernaryExpr& expr);
  llvm::Value* emitTuple(llvm::IRBuilder<>& builder, const TupleExpr& expr);
  llvm::Value* emitWalrus(llvm::IRBuilder<>& builder, const WalrusExpr& expr);
  llvm::Value* emitLambda(llvm::IRBuilder<>& builder, const LambdaExpr& expr);
  bool emitLambdaFunction(const LambdaExpr& expr);
  void declareLambdas(const Module& ast);
  void declareNestedFunctions(const FunctionDef& function);
  bool emitNestedFunctions(const FunctionDef& function);
  void collectLambdas(const Expr& expr, std::vector<const LambdaExpr*>& out);
  void collectLambdas(const Stmt& stmt, std::vector<const LambdaExpr*>& out);
  llvm::FunctionType* llvmFunctionTypeFrom(const Type* type);
  llvm::FunctionType* llvmFunctionTypeFromCall(const CallExpr& expr);
  llvm::Type* callableFatType();
  llvm::Value* packCallable(llvm::IRBuilder<>& builder, llvm::Value* fn, llvm::Value* env);
  llvm::Value* emitIndirectCallable(llvm::IRBuilder<>& builder,
                                    llvm::Value* callable,
                                    llvm::FunctionType* freeType,
                                    const std::vector<llvm::Value*>& args);
  llvm::Value* emitConstructorThunk(const Type* record);
  bool emitDel(llvm::IRBuilder<>& builder, const DelStmt& statement);
  struct WithFrame {
    const WithStmt* stmt = nullptr;
    llvm::Value* self = nullptr;
  };
  bool emitWith(llvm::IRBuilder<>& builder, const WithStmt& statement, const Type* returnType);
  bool emitWithExit(llvm::IRBuilder<>& builder, const WithFrame& frame);
  llvm::Value* emitDunderOnSelf(llvm::IRBuilder<>& builder,
                                const Type* record,
                                llvm::Value* self,
                                std::string_view name,
                                const std::vector<llvm::Value*>& extra);
  void emitDeferred(llvm::IRBuilder<>& builder, const Type* returnType);
  bool emitUnpack(llvm::IRBuilder<>& builder,
                  const TupleExpr& targets,
                  llvm::Value* value,
                  const Type* valueType);
  llvm::Value* packStr(llvm::IRBuilder<>& builder, llvm::Value* data, llvm::Value* len);
  llvm::Value* emitEnumTag(llvm::IRBuilder<>& builder, llvm::Value* value);
  llvm::Value* emitEnumUnit(llvm::IRBuilder<>& builder, const Type* type, unsigned tag);
  llvm::Value* emitEnumName(llvm::IRBuilder<>& builder, const Expr& expr);
  llvm::Value* emitNamesList(llvm::IRBuilder<>& builder, const std::vector<std::string>& names);
  llvm::Value* emitDunderCall(llvm::IRBuilder<>& builder,
                              const Expr& object,
                              std::string_view name,
                              const std::vector<llvm::Value*>& extra);
  llvm::Value*
  emitObjectPointer(llvm::IRBuilder<>& builder, const Expr& object, const Type* record);
  llvm::Value* emitNamedMethod(llvm::IRBuilder<>& builder,
                               const std::string& llvmName,
                               llvm::Value* self,
                               const std::vector<llvm::Value*>& extra);
  bool emitTry(llvm::IRBuilder<>& builder, const TryStmt& statement, const Type* returnType);
  bool emitRaise(llvm::IRBuilder<>& builder, const RaiseStmt& statement);
  bool emitMatch(llvm::IRBuilder<>& builder, const MatchStmt& statement, const Type* returnType);
  void emitErrorCheck(llvm::IRBuilder<>& builder);
  void appendDefaultArgs(llvm::IRBuilder<>& builder,
                         std::vector<llvm::Value*>& args,
                         const FunctionDef& function,
                         std::size_t provided,
                         std::size_t skip);
  void appendBoundCallArgs(llvm::IRBuilder<>& builder,
                           const CallExpr& expr,
                           const FunctionDef& function,
                           const Type* fnType,
                           std::vector<llvm::Value*>& args,
                           std::size_t skipParams = 0);
  llvm::Value* emitAddress(llvm::IRBuilder<>& builder, const Expr& expr, bool required = true);
  void emitDrops(llvm::IRBuilder<>& builder);
  void rememberLocal(const std::string& name, llvm::Value* allocaInst, const Type* type);

  llvm::LLVMContext* context_;
  DiagnosticEngine* diagnostics_;
  TypeContext* types_;
  llvm::Module* module_ = nullptr;
  std::vector<const Type*> boxedTypes_{};
  std::unordered_map<const Type*, llvm::Type*> lowered_{};
  std::unordered_map<std::string, llvm::Value*> locals_{};
  std::unordered_map<std::string, llvm::Value*> globals_{};
  llvm::Function* moduleInitFn_ = nullptr;
  std::unordered_map<std::string, llvm::Value*> decoratorSlots_{};
  const FunctionDef* currentFunction_ = nullptr;
  std::vector<std::pair<llvm::Value*, const Type*>> dropStack_{};
  std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loops_{};
  std::vector<llvm::BasicBlock*> tryHandlers_{};
  int tryDepth_ = 0;
  const FunctionDef* userMain_ = nullptr;
  std::unordered_map<std::string, const Type*> subst_{};
  std::vector<const DeferStmt*> defers_{};
  std::vector<WithFrame> withStack_{};
  // Coroutine lowering state for the async function currently being emitted.
  bool asyncFn_ = false;
  llvm::Value* asyncId_ = nullptr;
  llvm::Value* asyncHdl_ = nullptr;
  llvm::Value* asyncMem_ = nullptr;
  llvm::Value* asyncPromise_ = nullptr;
  llvm::Type* asyncResultTy_ = nullptr;
  llvm::BasicBlock* asyncFinal_ = nullptr;
  llvm::BasicBlock* asyncCleanup_ = nullptr;
  llvm::BasicBlock* asyncSuspend_ = nullptr;
  llvm::Function* asyncCoroIdFn_ = nullptr;
  llvm::Function* asyncCoroSizeFn_ = nullptr;
  llvm::Function* asyncCoroBeginFn_ = nullptr;
  llvm::Function* asyncCoroSuspendFn_ = nullptr;
  llvm::Function* asyncCoroFreeFn_ = nullptr;
  llvm::Function* asyncCoroEndFn_ = nullptr;
  llvm::Function* asyncCoroPromiseFn_ = nullptr;
  llvm::Function* asyncCoroDoneFn_ = nullptr;
  llvm::Function* asyncCoroResumeFn_ = nullptr;
  llvm::Function* asyncCoroDestroyFn_ = nullptr;
};

} // namespace sere
