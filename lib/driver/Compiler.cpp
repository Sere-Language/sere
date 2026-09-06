/// @file Compiler.cpp
/// Runs the frontend pipeline and writes LLVM IR, native assembly, or a linked executable.

#include "sere/driver/Compiler.h"

#include "sere/Version.h"
#include "sere/codegen/IRGenerator.h"
#include "sere/codegen/OptPipeline.h"
#include "sere/diag/DiagnosticEngine.h"
#include "sere/driver/Frontend.h"
#include "sere/driver/Installer.h"
#include "sere/driver/Library.h"
#include "sere/driver/Prelude.h"
#include "sere/driver/Project.h"
#include "sere/driver/ProjectInit.h"
#include "sere/driver/ProjectShell.h"
#include "sere/driver/Toolchain.h"
#include "sere/lex/Lexer.h"
#include "sere/lex/Token.h"
#include "sere/lsp/LanguageServer.h"
#include "sere/source/SourceManager.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace sere {
namespace {

[[nodiscard]] std::optional<std::string> readFile(const std::filesystem::path& path,
                                                  std::string& error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "cannot open input file '" + path.string() + "'";
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void dumpTokens(const std::vector<Token>& tokens) {
  for (const Token& token : tokens) {
    std::cout << tokenKindName(token.kind()) << " '" << token.spelling() << "' @ "
              << token.location().line << ':' << token.location().column << '\n';
  }
}

[[nodiscard]] std::filesystem::path defaultOutput(const CompilerOptions& options) {
  if (!options.outputPath.empty()) {
    return options.outputPath;
  }
  std::filesystem::path output = options.inputPath;
  if (options.emitLlvm) {
    return output.replace_extension(".ll");
  }
  if (options.emitAsm) {
    return output.replace_extension(".s");
  }
#ifdef _WIN32
  return output.replace_extension(".exe");
#else
  if (output.extension() == ".sere") {
    output.replace_extension();
  }
  return output;
#endif
}

[[nodiscard]] bool
writeIr(const llvm::Module& module, const std::filesystem::path& path, std::string& error) {
  const std::filesystem::path tempPath = std::filesystem::path(path.string() + ".tmp");
  std::error_code errorCode;
  {
    llvm::raw_fd_ostream output(tempPath.string(), errorCode, llvm::sys::fs::OF_None);
    if (errorCode) {
      error = "cannot write LLVM IR to '" + path.string() + "': " + errorCode.message();
      return false;
    }
    module.print(output, nullptr);
    output.flush();
    if (output.has_error()) {
      error = "cannot write LLVM IR to '" + path.string() + "'";
      return false;
    }
  }
  std::filesystem::rename(tempPath, path, errorCode);
  if (!errorCode) {
    return true;
  }
  std::filesystem::copy_file(
      tempPath, path, std::filesystem::copy_options::overwrite_existing, errorCode);
  std::filesystem::remove(tempPath);
  if (errorCode) {
    error = "cannot write LLVM IR to '" + path.string() + "': " + errorCode.message();
    return false;
  }
  return true;
}

[[nodiscard]] bool importsModule(const std::vector<std::string>& names, std::string_view want) {
  for (const std::string& name : names) {
    if (name == want) {
      return true;
    }
  }
  return false;
}

void copyBesideOutput(const std::filesystem::path& from, const std::filesystem::path& outputPath) {
  if (!std::filesystem::exists(from) || !std::filesystem::is_regular_file(from)) {
    return;
  }
  std::filesystem::path destDir = outputPath.parent_path();
  if (destDir.empty()) {
    destDir = ".";
  }
  std::error_code error;
  std::filesystem::copy_file(
      from, destDir / from.filename(), std::filesystem::copy_options::overwrite_existing, error);
}

[[nodiscard]] int emitAssembly(const std::filesystem::path& irPath,
                               const std::filesystem::path& outputPath) {
  const std::optional<std::string> clang = findClang();
  if (!clang.has_value()) {
    llvm::errs() << "error: clang not found; set SERE_LLVM_DIR or re-run the Sere installer\n";
    return 1;
  }
  prependLlvmToolsToPath();
  const std::string clangPath = *clang;
  const std::vector<std::string> owned{
      clangPath, "-S", "-x", "ir", "-O0", irPath.string(), "-o", outputPath.string()};
  llvm::SmallVector<llvm::StringRef, 8> arguments;
  for (const std::string& item : owned) {
    arguments.push_back(item);
  }
  const int code = llvm::sys::ExecuteAndWait(clangPath, arguments);
  if (code != 0) {
    llvm::errs() << "error: clang -S failed with exit code " << code << '\n';
  }
  return code;
}

[[nodiscard]] int linkExecutable(const std::filesystem::path& irPath,
                                 const std::filesystem::path& outputPath,
                                 const std::vector<std::filesystem::path>& extraLibs,
                                 const std::vector<std::string>& importedModules) {
  const std::optional<std::string> clang = findClang();
  const std::optional<std::filesystem::path> runtime = findRuntimeLibrary();
  if (!clang.has_value()) {
    llvm::errs() << "error: clang not found; set SERE_LLVM_DIR or re-run the Sere installer\n";
    return 1;
  }
  if (!runtime.has_value()) {
#ifdef _WIN32
    llvm::errs() << "error: sere_rt.lib not found next to the compiler\n";
#else
    llvm::errs() << "error: sere_rt.a (or libsere_rt.a) not found next to the compiler\n";
#endif
    return 1;
  }
  prependLlvmToolsToPath();
  applyHostLinkEnvironment();
  const std::string clangPath = *clang;
  const std::string ir = irPath.string();
  const std::string runtimeLib = runtime->string();
  const std::string output = outputPath.string();
  std::vector<std::string> owned{clangPath, ir};
#ifdef _WIN32
  owned.push_back("-fms-runtime-lib=static");
#endif
  for (const std::filesystem::path& lib : extraLibs) {
    owned.push_back(lib.string());
  }
  auto addSystemLib = [&](const char* name) {
    if (const std::optional<std::filesystem::path> found = findSystemLibrary(name)) {
      owned.push_back(found->string());
      return true;
    }
    llvm::errs() << "error: system library '" << name
                 << "' not found; reinstall the complete Sere package\n";
    return false;
  };
  if (importsModule(importedModules, "qt6")) {
    const std::optional<std::filesystem::path> qt6 = findNativeLibrary("sere_qt6");
    if (!qt6.has_value()) {
      llvm::errs() << "error: sere_qt6 library not found next to the compiler; rebuild sere\n";
      return 1;
    }
    owned.push_back(qt6->string());
#ifdef _WIN32
    if (!addSystemLib("msvcprt.lib")) {
      return 1;
    }
#endif
    copyBesideOutput(qt6->parent_path() / "sere_qt6.dll", outputPath);
    copyBesideOutput(qt6->parent_path() / "libsere_qt6.so", outputPath);
    copyBesideOutput(qt6->parent_path() / "libsere_qt6.dylib", outputPath);
    copyBesideOutput(qt6->parent_path() / "Qt6Core.dll", outputPath);
    copyBesideOutput(qt6->parent_path() / "Qt6Gui.dll", outputPath);
    copyBesideOutput(qt6->parent_path() / "Qt6Widgets.dll", outputPath);
    const std::filesystem::path plugin = qt6->parent_path() / "platforms" / "qwindows.dll";
    if (std::filesystem::exists(plugin)) {
      std::filesystem::path destDir = outputPath.parent_path();
      if (destDir.empty()) {
        destDir = ".";
      }
      std::error_code error;
      std::filesystem::create_directories(destDir / "platforms", error);
      std::filesystem::copy_file(plugin,
                                 destDir / "platforms" / "qwindows.dll",
                                 std::filesystem::copy_options::overwrite_existing,
                                 error);
    }
  }
#ifdef _WIN32
  for (const char* name : {"user32.lib",
                           "gdi32.lib",
                           "opengl32.lib",
                           "shell32.lib",
                           "advapi32.lib",
                           "winhttp.lib",
                           "ws2_32.lib"}) {
    if (!addSystemLib(name)) {
      return 1;
    }
  }
  for (const std::filesystem::path& iconRes :
       {compilerDirectory() / "sere_icon.res", runtime->parent_path() / "sere_icon.res"}) {
    if (std::filesystem::exists(iconRes)) {
      owned.push_back(iconRes.string());
      break;
    }
  }
#else
  for (const char* flag : {"-lm", "-ldl", "-lpthread"}) {
    owned.push_back(flag);
  }
  if (importsModule(importedModules, "gl")) {
    owned.push_back("-lGL");
  }
#endif
  owned.push_back(runtimeLib);
  owned.push_back("-fuse-ld=lld");
#ifndef _WIN32
  owned.push_back("-Wl,--as-needed");
#endif
  owned.push_back("-o");
  owned.push_back(output);
  llvm::SmallVector<llvm::StringRef, 24> arguments;
  for (const std::string& item : owned) {
    arguments.push_back(item);
  }
  const int code = llvm::sys::ExecuteAndWait(clangPath, arguments);
  if (code != 0) {
    llvm::errs() << "error: clang failed with exit code " << code << '\n';
  }
  return code;
}

[[nodiscard]] const char* severityName(DiagnosticSeverity severity) {
  if (severity == DiagnosticSeverity::Warning) {
    return "warning";
  }
  if (severity == DiagnosticSeverity::Note) {
    return "note";
  }
  return "error";
}

void printAnalyzeJson(const DiagnosticEngine& diagnostics) {
  llvm::json::Array items;
  for (const Diagnostic& diagnostic : diagnostics.diagnostics()) {
    items.push_back(llvm::json::Object{
        {"line", static_cast<int64_t>(diagnostic.range.start.line)},
        {"column", static_cast<int64_t>(diagnostic.range.start.column)},
        {"endLine", static_cast<int64_t>(diagnostic.range.end.line)},
        {"endColumn", static_cast<int64_t>(diagnostic.range.end.column)},
        {"severity", severityName(diagnostic.severity)},
        {"code", std::string(diagnosticCodeName(diagnostic.code))},
        {"message", diagnostic.message},
        {"help", diagnostic.help},
    });
  }
  llvm::outs() << llvm::json::Value(llvm::json::Object{{"diagnostics", std::move(items)}}) << '\n';
}

} // namespace

int compileInput(const CompilerOptions& options) {
  std::string readError;
  const std::optional<std::string> text = readFile(options.inputPath, readError);
  if (!text.has_value()) {
    DiagnosticEngine diagnostics;
    diagnostics.setColorMode(options.colorMode);
    diagnostics.error(readError);
    diagnostics.printAll();
    return 1;
  }

  if (options.dumpTokens) {
    DiagnosticEngine diagnostics;
    diagnostics.setColorMode(options.colorMode);
    SourceManager source(options.inputPath.string(), *text);
    diagnostics.setSource(&source);
    Lexer lexer(source, diagnostics);
    dumpTokens(lexer.tokenizeAll());
    if (diagnostics.hasErrors()) {
      diagnostics.printAll(source);
      return 1;
    }
  }

  Frontend frontend;
  const LanguageContext language = resolveLanguageContext(options.inputPath);
  const std::filesystem::path stdlibDir =
      language.stdlib.empty() ? findStdlibDirectory(compilerDirectory()) : language.stdlib;
  const bool ok = frontend.analyze(options.inputPath.string(), *text, stdlibDir);
  frontend.diagnostics().setColorMode(options.colorMode);
  if (options.analyze) {
    printAnalyzeJson(frontend.diagnostics());
    return frontend.diagnostics().hasErrors() ? 1 : 0;
  }
  if (!ok || frontend.module() == nullptr || frontend.types() == nullptr ||
      frontend.source() == nullptr) {
    frontend.diagnostics().printAll();
    return 1;
  }

  llvm::LLVMContext context;
  IRGenerator generator(context, frontend.diagnostics(), *frontend.types());
  std::vector<const Module*> imported;
  for (const std::unique_ptr<Module>& extra : frontend.importedModules()) {
    imported.push_back(extra.get());
  }
  std::unique_ptr<llvm::Module> module =
      generator.emit(*frontend.module(), options.inputPath.string(), &imported);
  if (module == nullptr || frontend.diagnostics().hasErrors()) {
    frontend.diagnostics().printAll();
    return 1;
  }
  std::string optError;
  if (!runOptPipeline(*module, options.optLevel, options.passes, optError)) {
    frontend.diagnostics().error("optimization pipeline failed: " + optError);
    frontend.diagnostics().printAll();
    return 1;
  }

  const std::filesystem::path outputPath = defaultOutput(options);
  if (options.emitLlvm) {
    std::string writeError;
    if (!writeIr(*module, outputPath, writeError)) {
      frontend.diagnostics().error(writeError);
      frontend.diagnostics().printAll();
      return 1;
    }
    llvm::outs() << "wrote " << outputPath.string() << '\n';
    return 0;
  }
  if (options.emitAsm) {
    const std::filesystem::path irPath =
        std::filesystem::temp_directory_path() / (options.inputPath.stem().string() + ".sere.ll");
    std::string writeError;
    if (!writeIr(*module, irPath, writeError)) {
      frontend.diagnostics().error(writeError);
      frontend.diagnostics().printAll();
      return 1;
    }
    const int code = emitAssembly(irPath, outputPath);
    if (code == 0) {
      llvm::outs() << "wrote " << outputPath.string() << '\n';
    }
    return code;
  }

  const std::filesystem::path irPath =
      std::filesystem::temp_directory_path() / (options.inputPath.stem().string() + ".sere.ll");
  std::string writeError;
  if (!writeIr(*module, irPath, writeError)) {
    frontend.diagnostics().error(writeError);
    frontend.diagnostics().printAll();
    return 1;
  }
  std::vector<std::filesystem::path> linkLibraries = options.linkLibraries;
  prepareImportedLibraryNative(frontend.importedModulePaths());
  appendExtractedLibraryLinks(frontend.importedModulePaths(), linkLibraries);
  std::vector<std::filesystem::path> runtimeFiles;
  appendExtractedLibraryRuntimes(frontend.importedModulePaths(), runtimeFiles);
  for (const std::filesystem::path& runtime : runtimeFiles) {
    copyBesideOutput(runtime, outputPath);
  }
  const int code =
      linkExecutable(irPath, outputPath, linkLibraries, frontend.importedModuleNames());
  if (code == 0) {
    llvm::outs() << "wrote " << outputPath.string() << '\n';
  }
  return code;
}

void printCompilerEnv() {
  const std::filesystem::path compilerDir = compilerDirectory();
  const std::string executable =
      llvm::sys::fs::getMainExecutable(nullptr, reinterpret_cast<void*>(&printCompilerEnv));
  std::filesystem::path stdlib = compilerDir / "stdlib";
  if (!std::filesystem::exists(stdlib / "prelude.sere")) {
    const std::filesystem::path parentStdlib = compilerDir.parent_path() / "stdlib";
    stdlib = std::filesystem::exists(parentStdlib / "prelude.sere")
                 ? parentStdlib
                 : findStdlibDirectory(compilerDir);
  }
  llvm::json::Object env{
      {"version", std::string(SERE_VERSION_STRING)},
      {"compiler", executable},
      {"compilerDir", compilerDir.string()},
      {"stdlib", stdlib.string()},
  };
  if (const std::optional<std::filesystem::path> llvmDir = llvmToolsDirectory()) {
    env["llvmDir"] = llvmDir->parent_path().string();
    env["llvmBin"] = llvmDir->string();
  }
  if (const std::optional<std::string> clang = findClang()) {
    env["clang"] = *clang;
  }
  if (const std::optional<std::filesystem::path> runtime = findRuntimeLibrary()) {
    env["runtimeLib"] = runtime->string();
  }
#ifdef _WIN32
  env["platform"] = "windows";
#else
  env["platform"] = "posix";
#endif
  llvm::outs() << llvm::json::Value(std::move(env)) << '\n';
}

int Compiler::run(const CompilerOptions& options) {
  if (options.version) {
    std::cout << "sere " << SERE_VERSION_STRING << " (LLVM frontend)\n";
    return 0;
  }
  if (options.printEnv) {
    printCompilerEnv();
    return 0;
  }
  if (options.help) {
    return 0;
  }
  if (options.lsp) {
    return runLanguageServer();
  }
  if (options.projectCommand == ProjectCommand::Init) {
    std::string initError;
    const int code = initSereProject(options.initName, compilerDirectory(), initError);
    if (code != 0) {
      llvm::errs() << "error: " << initError << '\n';
    }
    return code;
  }
  if (options.projectCommand == ProjectCommand::InitLib) {
    std::string initError;
    const int code = initSereLibrary(options.initName, initError);
    if (code != 0) {
      llvm::errs() << "error: " << initError << '\n';
    }
    return code;
  }
  if (options.projectCommand == ProjectCommand::Build) {
    return buildProject(options);
  }
  if (options.projectCommand == ProjectCommand::Pack) {
    return packLibrary(options);
  }
  if (options.projectCommand == ProjectCommand::Run) {
    return runProject(options);
  }
  if (options.projectCommand == ProjectCommand::Clean) {
    return cleanProject(options);
  }
  if (options.projectCommand == ProjectCommand::Shell) {
    return enterProjectShell(options);
  }
  if (options.projectCommand == ProjectCommand::BuildInstaller) {
    return buildInstaller(options);
  }
  if (options.projectCommand == ProjectCommand::RefreshBin) {
    std::string refreshError;
    const int code = refreshCompilerBin({}, refreshError);
    if (code != 0 && !refreshError.empty()) {
      llvm::errs() << "error: " << refreshError << '\n';
    }
    return code;
  }
  if (options.projectCommand == ProjectCommand::Update) {
    std::string updateError;
    const int code = updateSereEnvironment({}, updateError);
    if (code != 0 && !updateError.empty()) {
      llvm::errs() << "error: " << updateError << '\n';
    }
    return code;
  }
  return compileInput(options);
}

} // namespace sere
