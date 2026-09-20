/// @file smoke_serem.cpp
/// Verifies the target-independent Serem value and builder model.

#include "sere/codegen/Serem.h"

#include <cassert>
#include <memory>
#include <string>

int main() {
  using namespace sere::serem;

  IRModule module("smoke");
  auto function = std::make_unique<IRFunction>("add", std::vector<IRType>{IRType::i32(), IRType::i32()},
                                               IRType::i32());
  IRFunction& add = module.addFunction(std::move(function));
  IRBuilder builder(add);
  auto result = builder.add(add.argument(0), add.argument(1), IRType::i32());
  builder.ret(result);

  const std::string text = module.display();
  assert(text.find("module @smoke") != std::string::npos);
  assert(text.find("%0 = add i32 %arg0, %arg1") != std::string::npos);
  assert(text.find("return %0") != std::string::npos);
  assert(result->type().display() == "i32");
  assert(add.blocks().front()->isTerminated());

  // String literals are emitted as named globals, and their uses reference the
  // global instead of repeating the literal inline.
  IRModule stringModule("strings");
  (void)stringModule.addGlobal(
      std::make_unique<GlobalConstant>("str.0", IRType::stringType(), ConstantString("hi").display()));
  auto stringFunction =
      std::make_unique<IRFunction>("greet", std::vector<IRType>{}, IRType::voidType());
  IRFunction& greet = stringModule.addFunction(std::move(stringFunction));
  IRBuilder stringBuilder(greet);
  (void)stringBuilder.operation("runtime.print", IRType::voidType(),
                                {std::make_shared<ConstantString>("hi", "str.0")});
  const std::string stringText = stringModule.display();
  assert(stringText.find("global @str.0 = str \"hi\"") != std::string::npos);
  assert(stringText.find("runtime.print @str.0") != std::string::npos);
  assert(stringText.find("runtime.print \"hi\"") == std::string::npos);
  return 0;
}
