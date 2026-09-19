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
  return 0;
}
