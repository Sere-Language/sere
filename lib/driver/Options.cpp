/// @file Options.cpp
/// Parses sere command-line flags. Keep this table-driven and allocation-light.

#include "sere/driver/Options.h"

#include <string>
#include <string_view>

namespace sere {
namespace {

void printUsage(std::string& error) {
  const std::string usage =
      "Usage: sere <command> [options]\n"
      "       sere [options] <file.sere>\n"
      "\n"
      "Project commands:\n"
      "  init <name>         Create a Sere project (src, libs, bin, venv, scripts)\n"
      "  build               Compile the project in the current directory\n"
      "  run [-- <args>]     Build and run the project executable\n"
      "  clean               Remove bin/ artifacts\n"
      "  shell               Enter the Sere project shell\n"
      "  build-installer     Package a Windows installer (compiler, LLVM, stdlib, editor)\n"
      "\n"
      "After init:\n"
      "  ./<name>/scripts/activate     enter the project shell\n"
      "  sere build                    compile src/main.sere\n"
      "  sere run                      build and run\n"
      "\n"
      "Compiler options:\n"
      "  --help              Show this help\n"
      "  --version           Show version\n"
      "  --emit-llvm         Write LLVM IR instead of linking an executable\n"
      "  --dump-tokens       Print lexer tokens\n"
      "  --analyze           Print JSON diagnostics and stop\n"
      "  --lsp               Run the language server on stdin/stdout\n"
      "  --build-installer   Same as build-installer\n"
      "  --init <name>       Same as init\n"
      "  --host <shell>      Shell to nest: powershell, cmd, bash (shell command)\n"
      "  --link <lib>        Link an extra native C/C++ library into the program\n"
      "  --color=<mode>      Color diagnostics: auto, always, never\n"
      "  --no-color          Disable color (same as --color=never)\n"
      "  --opt=<level>       LLVM optimization: O0, O1, O2, O3, Os, Oz\n"
      "  --passes=<pipeline> Custom LLVM pass pipeline (PassBuilder syntax)\n"
      "  -o <path>           Output path\n";

  if (error.empty()) {
    error = usage;
  } else {
    error += "\n" + usage;
  }
}

[[nodiscard]] bool parseProjectCommand(std::string_view argument, ProjectCommand& command) {
  if (argument == "init") {
    command = ProjectCommand::Init;
    return true;
  }
  if (argument == "build") {
    command = ProjectCommand::Build;
    return true;
  }
  if (argument == "run") {
    command = ProjectCommand::Run;
    return true;
  }
  if (argument == "clean") {
    command = ProjectCommand::Clean;
    return true;
  }
  if (argument == "shell" || argument == "activate") {
    command = ProjectCommand::Shell;
    return true;
  }
  if (argument == "build-installer") {
    command = ProjectCommand::BuildInstaller;
    return true;
  }
  return false;
}

}  // namespace

bool parseCommandLine(int argc, char** argv, CompilerOptions& options, std::string& error) {
  bool endOfFlags = false;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (endOfFlags) {
      options.programArgs.emplace_back(argument);
      continue;
    }
    if (argument == "--") {
      endOfFlags = true;
      continue;
    }
    if (argument == "--help" || argument == "-h" || argument == "help") {
      options.help = true;
      printUsage(error);
      return true;
    }
    if (argument == "--version" || argument == "version") {
      options.version = true;
      continue;
    }
    if (argument == "--emit-llvm") {
      options.emitLlvm = true;
      continue;
    }
    if (argument == "--dump-tokens") {
      options.dumpTokens = true;
      continue;
    }
    if (argument == "--analyze") {
      options.analyze = true;
      continue;
    }
    if (argument == "--lsp") {
      options.lsp = true;
      continue;
    }
    if (argument == "--build-installer") {
      options.projectCommand = ProjectCommand::BuildInstaller;
      continue;
    }
    if (argument == "--init") {
      options.projectCommand = ProjectCommand::Init;
      if (index + 1 < argc && argv[index + 1][0] != '-') {
        ++index;
        options.initName = argv[index];
      } else {
        options.initName = "sere-project";
      }
      continue;
    }
    if (argument == "--host") {
      if (index + 1 >= argc) {
        error = "missing shell name after --host";
        return false;
      }
      ++index;
      options.shellHost = argv[index];
      continue;
    }
    if (argument == "--link") {
      if (index + 1 >= argc) {
        error = "missing library path after --link";
        return false;
      }
      ++index;
      options.linkLibraries.emplace_back(argv[index]);
      continue;
    }
    if (argument == "--no-color") {
      options.colorMode = ColorMode::Never;
      continue;
    }
    if (argument == "--color") {
      error = "missing color mode; use --color=auto, --color=always, or --color=never";
      return false;
    }
    if (argument.starts_with("--color=")) {
      const std::string_view mode = argument.substr(8);
      if (mode == "always") {
        options.colorMode = ColorMode::Always;
      } else if (mode == "never") {
        options.colorMode = ColorMode::Never;
      } else if (mode == "auto") {
        options.colorMode = ColorMode::Auto;
      } else {
        error = "invalid --color mode '" + std::string(mode) +
                "' (expected auto, always, or never)";
        return false;
      }
      continue;
    }
    if (argument.starts_with("--opt=")) {
      if (!parseOptLevel(argument.substr(6), options.optLevel, error)) {
        return false;
      }
      options.optOverridden = true;
      continue;
    }
    if (argument == "--opt") {
      if (index + 1 >= argc) {
        error = "missing optimization level after --opt";
        return false;
      }
      ++index;
      if (!parseOptLevel(argv[index], options.optLevel, error)) {
        return false;
      }
      options.optOverridden = true;
      continue;
    }
    if (argument.starts_with("--passes=")) {
      options.passes = std::string(argument.substr(9));
      continue;
    }
    if (argument == "--passes") {
      if (index + 1 >= argc) {
        error = "missing pipeline after --passes";
        return false;
      }
      ++index;
      options.passes = argv[index];
      continue;
    }
    if (argument == "-o") {
      if (index + 1 >= argc) {
        error = "missing path after -o";
        return false;
      }
      ++index;
      options.outputPath = argv[index];
      continue;
    }
    if (argument.starts_with('-')) {
      error = "unknown option: " + std::string(argument);
      return false;
    }
    ProjectCommand command = ProjectCommand::None;
    if (options.projectCommand == ProjectCommand::None && parseProjectCommand(argument, command)) {
      options.projectCommand = command;
      if (command == ProjectCommand::Init) {
        if (index + 1 < argc && argv[index + 1][0] != '-') {
          ++index;
          options.initName = argv[index];
        } else {
          options.initName = "sere-project";
        }
      }
      continue;
    }
    if (options.projectCommand == ProjectCommand::Run) {
      options.programArgs.emplace_back(argument);
      continue;
    }
    if (options.projectCommand != ProjectCommand::None) {
      error = "unexpected argument '" + std::string(argument) + "'";
      return false;
    }
    if (!options.inputPath.empty()) {
      error = "multiple input files are not supported yet";
      return false;
    }
    options.inputPath = std::string(argument);
  }
  if (options.projectCommand == ProjectCommand::Init && options.initName.empty()) {
    options.initName = "sere-project";
  }
  if (!options.help && !options.version && !options.lsp &&
      options.projectCommand == ProjectCommand::None && options.inputPath.empty()) {
    error = "missing input file or project command";
    printUsage(error);
    return false;
  }
  return true;
}

}  // namespace sere
