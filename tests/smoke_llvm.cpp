/// @file smoke_llvm.cpp
/// Verifies that the process can construct LLVM IR and print it.

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

int main() {
  llvm::LLVMContext context;
  auto module = std::make_unique<llvm::Module>("sere_smoke", context);
  llvm::FunctionType* type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false);
  llvm::Function* function =
      llvm::Function::Create(type, llvm::Function::ExternalLinkage, "sere_smoke", module.get());
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", function);
  llvm::IRBuilder<> builder(entry);
  builder.CreateRet(builder.getInt32(42));

  std::string errors;
  llvm::raw_string_ostream stream(errors);
  if (llvm::verifyModule(*module, &stream)) {
    llvm::errs() << "LLVM smoke test failed: " << stream.str() << '\n';
    return 1;
  }
  module->print(llvm::outs(), nullptr);
  return 0;
}
