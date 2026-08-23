# Sere compiler libraries.
# Each subdirectory is a self-contained compilation unit with a narrow interface.

add_library(sere_diag
  diag/DiagnosticEngine.cpp
  diag/DiagnosticCode.cpp
  diag/IgnoreDirective.cpp
)
target_link_libraries(sere_diag PUBLIC sere_headers)
sere_apply_warnings(sere_diag)

add_library(sere_source
  source/SourceManager.cpp
)
target_link_libraries(sere_source PUBLIC sere_headers sere_diag)
sere_apply_warnings(sere_source)

add_library(sere_lex
  lex/Token.cpp
  lex/Lexer.cpp
)
target_link_libraries(sere_lex PUBLIC sere_source)
sere_apply_warnings(sere_lex)

add_library(sere_types
  types/Type.cpp
  types/TypeContext.cpp
  types/Intrinsic.cpp
)
target_link_libraries(sere_types PUBLIC sere_headers)
sere_apply_warnings(sere_types)

add_library(sere_ast
  ast/Syntax.cpp
  ast/Query.cpp
)
target_link_libraries(sere_ast PUBLIC sere_source sere_types)
sere_apply_warnings(sere_ast)

add_library(sere_parse
  parse/Parser.cpp
)
target_link_libraries(sere_parse PUBLIC sere_lex sere_ast)
sere_apply_warnings(sere_parse)

add_library(sere_macro
  macro/TokenTree.cpp
  macro/Parse.cpp
  macro/Quote.cpp
  macro/Expander.cpp
)
target_link_libraries(sere_macro PUBLIC sere_parse sere_ast)
sere_apply_warnings(sere_macro)

add_library(sere_sema
  sema/TypeChecker.cpp
)
target_link_libraries(sere_sema PUBLIC sere_parse sere_types sere_macro)
sere_apply_warnings(sere_sema)

add_library(sere_codegen
  codegen/IRGenerator.cpp
  codegen/OptPipeline.cpp
)
target_link_libraries(sere_codegen PUBLIC sere_ast sere_types)
sere_configure_llvm_target(sere_codegen)
sere_apply_warnings(sere_codegen)

add_library(sere_driver
  driver/Options.cpp
  driver/Compiler.cpp
  driver/Prelude.cpp
  driver/Project.cpp
  driver/ProjectInit.cpp
  driver/Library.cpp
  driver/ProjectShell.cpp
  driver/Frontend.cpp
  driver/ImportPath.cpp
  driver/Toolchain.cpp
  driver/Installer.cpp
  lsp/ImportCompletion.cpp
  lsp/LanguageServer.cpp
  lsp/SemanticTokens.cpp
)
target_link_libraries(sere_driver PUBLIC sere_sema sere_codegen)
sere_configure_llvm_target(sere_driver)
sere_apply_warnings(sere_driver)
add_dependencies(sere_driver sere_runtime)
