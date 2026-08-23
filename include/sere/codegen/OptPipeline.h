/// @file OptPipeline.h
/// Configurable LLVM optimization pipeline for sere.

#pragma once

#include <string>
#include <string_view>

namespace llvm {
class Module;
}

namespace sere {

enum class OptLevel {
  O0,
  O1,
  O2,
  O3,
  Os,
  Oz,
};

[[nodiscard]] bool parseOptLevel(std::string_view text, OptLevel& level, std::string& error);
[[nodiscard]] bool runOptPipeline(llvm::Module& module,
                                  OptLevel level,
                                  std::string_view passes,
                                  std::string& error);

}  // namespace sere
