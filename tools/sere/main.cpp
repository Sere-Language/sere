/// @file main.cpp
/// sere entry point. Argument parsing lives in the driver library.

#include "sere/driver/Compiler.h"
#include "sere/driver/Options.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  sere::CompilerOptions options;
  std::string error;
  if (!sere::parseCommandLine(argc, argv, options, error)) {
    std::cerr << "sere: " << error << '\n';
    return 1;
  }
  if (options.help) {
    std::cout << error;
    return 0;
  }
  sere::Compiler compiler;
  return compiler.run(options);
}
