/// @file SeremLLVMBackend.h
/// Lowers Serem SSA IR to LLVM IR for the native Sere toolchain.

#pragma once

#include "sere/codegen/Serem.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace llvm {
class BasicBlock;
class Function;
class LLVMContext;
class Module;
class StructType;
class Type;
class Value;
} // namespace llvm

namespace sere {

class DiagnosticEngine;

class SeremLLVMBackend {
public:
  SeremLLVMBackend(llvm::LLVMContext& context, DiagnosticEngine& diagnostics);
  ~SeremLLVMBackend();

  [[nodiscard]] std::unique_ptr<llvm::Module>
  emit(const serem::IRModule& module, const std::string& moduleName);

private:
  [[nodiscard]] llvm::Type* lowerType(const serem::IRType& type);
  [[nodiscard]] llvm::Value* lowerValue(const serem::ValuePtr& value);
  [[nodiscard]] llvm::Value* lowerOperation(const serem::Operation& operation);
  /// Defines the platform entry point. The Sere `main` follows the language's
  /// signature (`main(argv: list[str])`), so the process arguments have to be
  /// turned into a list before the user function runs.
  void emitEntryPoint(const serem::IRFunction& userMain, llvm::Function* userEntry);
  [[nodiscard]] llvm::BasicBlock* blockFor(std::string_view name) const;
  [[nodiscard]] llvm::Function* functionFor(const serem::FunctionRef& function);
  [[nodiscard]] llvm::Function* ensureExternal(const serem::FunctionRef& function);
  [[nodiscard]] std::string attribute(const serem::Operation& operation,
                                      std::string_view name) const;
  void report(std::string message);

  llvm::LLVMContext* context_;
  DiagnosticEngine* diagnostics_;
  std::unique_ptr<llvm::Module> module_;
  std::unordered_map<std::string, llvm::StructType*> structs_;
  std::unordered_map<std::string, llvm::Function*> functions_;
  std::unordered_map<std::string, llvm::BasicBlock*> blocks_;
  std::unordered_map<const serem::Value*, llvm::Value*> values_;
  std::string currentFunctionName_;
  class IRBuilderHolder;
  std::unique_ptr<IRBuilderHolder> builder_;
};

} // namespace sere
