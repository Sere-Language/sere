/// @file SeremGenerator.h
/// Lowers the typed Sere AST into the target-independent Serem IR.

#pragma once

#include "sere/ast/Syntax.h"
#include "sere/codegen/Serem.h"

#include <memory>
#include <string>
#include <string_view>
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
  [[nodiscard]] const Type* resolveType(const Type* type) const;
  [[nodiscard]] bool emitFunction(const FunctionDef& function, std::string symbol = {});
  [[nodiscard]] bool emitStatement(const Stmt& statement);
  [[nodiscard]] bool emitBlock(const std::vector<std::unique_ptr<Stmt>>& statements);
  /// Lowers a lambda into a standalone function and yields its address. Lambdas
  /// capture nothing, so a plain function pointer is the whole value.
  [[nodiscard]] bool emitLambdaFunction(const LambdaExpr& expression);
  /// Lowers an inner `def` into a standalone function, so a callable value can
  /// name it like any other function.
  [[nodiscard]] bool emitNestedFunction(const FunctionDef& function);
  /// Emits a thunk that binds `receiver` to `owner::method`'s first parameter,
  /// so `obj.method` is callable as a value.
  [[nodiscard]] serem::ValuePtr
  emitBoundMethod(const Type* owner, const std::string& method, const Expr& object);
  /// Per-function emission state, swapped out while a nested body is lowered
  /// into its own frame.
  struct FunctionState {
    std::unique_ptr<serem::IRBuilder> builder;
    serem::IRFunction* function = nullptr;
    std::unordered_map<std::string, serem::ValuePtr> locals;
    std::unordered_map<std::string, const Type*> localTypes;
    std::unordered_map<std::string, std::string> statics;
    std::unordered_map<std::string, const Type*> staticTypes;
    const Type* returnType = nullptr;
    std::string ownerClass;
    std::vector<std::string> tryHandlers;
    std::vector<const DeferStmt*> defers;
    std::vector<serem::BasicBlock*> breakTargets;
    std::vector<serem::BasicBlock*> continueTargets;
    serem::ValuePtr coroutine;
    std::unordered_map<std::string, std::string> captures;
  };
  void pushFunctionState();
  void popFunctionState();
  [[nodiscard]] bool emitIf(const IfStmt& statement);
  [[nodiscard]] serem::ValuePtr emitDo(const DoExpr& expression);
  /// Emits a statement list and yields the value of its trailing expression
  /// (or of a trailing `if`/`else`).
  [[nodiscard]] serem::ValuePtr emitBlockValue(const std::vector<std::unique_ptr<Stmt>>& body,
                                               const Type* resultType);
  [[nodiscard]] serem::ValuePtr emitIfValue(const IfStmt& statement, const Type* resultType);
  [[nodiscard]] bool emitWhile(const WhileStmt& statement);
  [[nodiscard]] bool emitFor(const ForStmt& statement);
  void declareTypes(const Module& module);
  void declareClass(const ClassDef& classDef);
  /// Declares a Serem record type for every instantiated generic class, so the
  /// layout of `Box[i32]` matches the specialised method bodies.
  void declareInstances(const std::vector<const Module*>& modules);
  void declareEnum(const EnumDef& enumDef);
  [[nodiscard]] serem::ValuePtr emitExpression(const Expr& expression);
  /// Converts one lowered value between Sere types: numeric widths, a base-class
  /// view of a record, or a member of a union. Returns the value unchanged when
  /// both types share a representation.
  [[nodiscard]] serem::ValuePtr coerce(serem::ValuePtr value, const Type* from, const Type* to);
  /// Converts a value to its textual form: `__str__` for a record, the class
  /// name when the record has no `__str__`, and a formatted rendering for a
  /// container.
  [[nodiscard]] serem::ValuePtr printable(serem::ValuePtr value, const Type* type);
  /// Renders an enum's variant name from its discriminant, qualified as
  /// `Type.Variant` when the caller asks for the long form.
  [[nodiscard]] serem::ValuePtr
  enumNameValue(const Type* record, serem::ValuePtr value, bool qualified);
  /// Lowers `f"{value:spec}"` to the runtime formatter. The kind code is the
  /// one `sere_format_value` expects: 0 int, 1 float, 2 str, 3 bool.
  [[nodiscard]] serem::ValuePtr formatValue(const Expr& value, const std::string& spec);
  /// Serem symbol of `name` implemented by `record` or one of its base classes,
  /// or an empty string when the class does not implement it.
  [[nodiscard]] std::string methodSymbol(const Type* record, std::string_view name) const;
  /// Serem symbol used to render a record nested in a container: `__repr__` when
  /// it exists, then `__str__`. Empty when the class has neither.
  [[nodiscard]] std::string renderSymbol(const Type* record) const;
  /// Calls a method on a record value, converting the receiver and arguments to
  /// the declared parameter types. Returns nullptr when the method is missing.
  [[nodiscard]] serem::ValuePtr callMethod(const Type* record,
                                           const std::string& name,
                                           serem::ValuePtr self,
                                           const std::vector<serem::ValuePtr>& arguments);
  void appendDefaults(const std::string& symbol, std::vector<serem::ValuePtr>& arguments);
  [[nodiscard]] serem::ValuePtr emitName(const NameExpr& expression);
  [[nodiscard]] serem::ValuePtr emitBinary(const BinaryExpr& expression);
  [[nodiscard]] serem::ValuePtr emitUnionEquality(serem::ValuePtr left,
                                                  serem::ValuePtr right,
                                                  const Type* leftType,
                                                  const Type* rightType);
  [[nodiscard]] serem::ValuePtr emitCall(const CallExpr& expression);
  [[nodiscard]] serem::ValuePtr emitUnary(const UnaryExpr& expression);
  [[nodiscard]] serem::ValuePtr emitMember(const MemberExpr& expression);
  [[nodiscard]] serem::ValuePtr emitIndex(const IndexExpr& expression);
  [[nodiscard]] serem::ValuePtr emitAggregate(const Expr& expression);
  /// Lowers `[element for name in iterable]` to a loop that appends to a fresh
  /// list.
  [[nodiscard]] serem::ValuePtr emitComprehension(const ComprehensionExpr& expression);
  [[nodiscard]] serem::ValuePtr stringValue(std::string_view value);
  /// Binds `name` to a storage slot and records the type the slot holds, so a
  /// later read loads the slot with its real type rather than the (possibly
  /// narrowed) type of the reading expression.
  [[nodiscard]] bool
  bindLocal(const std::string& name, serem::ValuePtr value, const Type* type = nullptr);
  [[nodiscard]] serem::ValuePtr local(const std::string& name) const;
  [[nodiscard]] bool unsupported(const Node& node, std::string_view feature);
  [[nodiscard]] std::string functionName(const FunctionDef& function) const;
  /// Emits every pending `defer` body, innermost first, before a scope exits.
  void emitDeferred();

  DiagnosticEngine* diagnostics_;
  TypeContext* types_;
  std::unique_ptr<serem::IRModule> module_;
  serem::IRFunction* function_ = nullptr;
  std::unique_ptr<serem::IRBuilder> builder_;
  std::unordered_map<std::string, serem::ValuePtr> locals_;
  std::unordered_map<std::string, const Type*> localTypes_;
  /// `static` locals of the function being lowered: name to the Serem symbol of
  /// the module global that backs it.
  std::unordered_map<std::string, std::string> functionStatics_;
  std::unordered_map<std::string, const Type*> staticTypes_;
  const Type* returnType_ = nullptr;
  std::unordered_map<std::string, serem::IRType> functions_;
  std::unordered_map<std::string, const FunctionDef*> definitions_;
  std::unordered_map<std::string, std::string> functionSymbols_;
  /// "Class::method" to the Serem symbol that implements it, so a dunder call
  /// finds the class that declares the method even when it is inherited.
  std::unordered_map<std::string, std::string> methodSymbols_;
  std::unordered_map<const FunctionDef*, std::string> functionNames_;
  std::unordered_map<std::string, std::string> decorators_;
  std::unordered_map<std::string, std::string> classBases_;
  std::unordered_map<std::string, std::vector<serem::IRType>> classFields_;
  /// Every declared class, so `isinstance` can list the classes a value of a
  /// given static type may hold at runtime.
  std::vector<const Type*> classes_;
  std::string currentOwnerClass_;
  std::vector<serem::BasicBlock*> breakTargets_;
  std::vector<serem::BasicBlock*> continueTargets_;
  serem::ValuePtr coroutineToken_;
  /// Innermost exception dispatch block, so a `raise` branches to the handler
  /// that catches it. Parallel to `defers_` below.
  std::vector<std::string> tryHandlers_;
  /// Pending `defer` bodies, innermost last; run in reverse order on scope exit.
  std::vector<const DeferStmt*> defers_;
  /// Type-parameter substitution active while a generic instantiation is
  /// emitted, mapping each parameter name to the caller's concrete type.
  std::unordered_map<std::string, const Type*> subst_;
  /// Receiver type of the method being lowered: the instantiated class when a
  /// generic class is specialised, so the body reads the instance's layout.
  const Type* receiverOverride_ = nullptr;
  /// Saved frames of the bodies currently being lowered inside another one.
  std::vector<FunctionState> savedStates_;
  /// Captured outer locals the body being lowered reads through a global, keyed
  /// by the name the body uses for them.
  std::unordered_map<std::string, std::string> captureSymbols_;
  /// Number of bound-method thunks emitted, used to name each one uniquely.
  std::size_t boundThunks_ = 0;
};

} // namespace sere
