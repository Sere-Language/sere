# Extending Sere

This is the recipe book. Each section is a complete checklist. Skip a row and
the compiler, tests, or editor will disagree with each other.

Work in this order unless a recipe says otherwise: **lex → parse → AST → sema
→ codegen → runtime/stdlib → LSP/grammar → tests → docs**.

---

## 1. Add a keyword

Example: a new statement keyword `unless`.

1. `include/sere/lex/TokenKind.h` — add `KeywordUnless` **inside** the block
   `KeywordFalse` … `KeywordWith` (before `Unknown`). Semantic tokens treat
   that whole range as keywords.
2. `lib/lex/Token.cpp` — `kKeywords[]` entry `{"unless", TokenKind::KeywordUnless}`
   and a `tokenKindName` case.
3. `include/sere/ast/Syntax.h` — new `NodeKind` and class if it is a new
   statement or expression; or reuse `IfStmt` if it is sugar.
4. `lib/ast/Syntax.cpp` — construct / accessors.
5. `lib/parse/Parser.cpp` — `parseStatement` (or expression) branch.
6. `lib/ast/Query.cpp` — `searchStmt` / `searchExpr` so hover hits the node.
7. `lib/sema/TypeChecker.cpp` — `checkStatement` / `checkExpr`.
8. `lib/codegen/IRGenerator.cpp` — emit IR (or desugar entirely in the parser
   and skip this).
9. Editor: `editors/vscode/syntaxes/sere.tmLanguage.json` keyword list;
   `lib/lsp/LanguageServer.cpp` `addKeywordCompletions`.
10. Tests: parse + sema + an `examples/` file.
11. Mention it in `README.md` language table if users should see it.

---

## 2. Add an operator

Example: prefix `*` / `&` (already implemented — follow the same path).

1. Lexer: new `TokenKind` only if the spelling is not already a token.
   `*` and `&` reuse `Star` / `Amp`.
2. Parser: prefix in `parseUnary` (`prefixOp`) or infix in the right
   precedence function (`parseMul`, `parseBitAnd`, …). Binary `*` / `&` must
   stay multiplication / bitwise AND.
3. AST: `UnaryOp` or `BinaryOp` enumerator.
4. Sema: `checkUnary` / `checkBinary`. Pointer ops: `checkDeref`, `checkAddrOf`.
5. Codegen: `emitUnary` / `emitBinary` / `emitAddress`.
6. LSP: `isOperatorToken` in `SemanticTokens.cpp`; TextMate `#operator`.
7. Tests in `parse_lang.cpp` and `sema_types.cpp`.

---

## 3. Add a builtin type constructor

Example: `Unique[T]`, `Ptr[T]`, `list[T]`.

1. `TypeContext` — factory (`uniqueType`, `ptrType`, …) and intern key.
2. `Type` helpers — `isPointerLike`, `pointeeType`, `isList`, …
3. `TypeChecker::resolveNamedType` — recognize the name and argument count.
4. Codegen `lower` — LLVM representation.
5. LSP builtin-type lists (`SemanticTokens.cpp`, completions, tmLanguage `#type`).
6. Docs in `stdlib/memory.sere` or the README language table.

Do not special-case the name only in codegen. Sema must produce the interned
`Type*` first.

---

## 4. Add an intrinsic

Example: `alloc`, `len`, `typeof`.

1. `include/sere/types/Intrinsic.h` — new `IntrinsicKind`.
2. `lib/types/Intrinsic.cpp` — `intrinsicName` and `intrinsicByName`.
3. `TypeChecker::registerBuiltins` — include the kind.
4. `TypeChecker::checkIntrinsicCall` — arity, type args, return type.
5. `IRGenerator::emitIntrinsic` — runtime calls or LLVM.
6. LSP: builtin function list + optional snippet
   (`unique[${1:i32}](${2:value})`).
7. tmLanguage `#function` if it should highlight before semantic tokens.
8. Sema test + example.

If the operation is a thin C call with a stable type, prefer
`extern "C"` in stdlib instead of a new intrinsic.

---

## 5. Add a diagnostic code

1. `include/sere/diag/DiagnosticCode.h` — enumerator.
2. `lib/diag/DiagnosticCode.cpp` — name, description, catalog, inference
   heuristics (`inferDiagnosticCode`).
3. Emit via `DiagnosticEngine::error` as today; codes are inferred from the
   message unless you thread a code through explicitly.
4. README catalog table.
5. `tests/type_ignore.cpp` if `# type[YourError]: ignore` should hide it.

Keep names Python-shaped (`TypeError`, `NameError`) so ignore comments stay
familiar.

---

## 6. Add a pure stdlib module

No compiler change.

1. `stdlib/foo.sere` — file docstring at the top.
2. `import foo` from a program. The last path segment is the module name.
3. `examples/…` that uses it.
4. `add_test(NAME sere.example.… COMMAND sere --emit-llvm …)` in
   `tests/CMakeLists.txt`.

---

## 7. Add a stdlib module that needs C

1. Declare and implement the C function (`runtime/sere_rt.h` + `.c`).
2. `extern "C" "sere_foo_bar"` in `stdlib/foo.sere`.
3. Rebuild `sere` so `sere_rt` updates.
4. Example + emit test.

See `stdlib/io.sere` and `stdlib/gc.sere`.

---

## 8. Add a native extension library

Two options:

**Typed extern (preferred for simple functions)**

```sere
extern "C" "native_add"
def add(left: i32, right: i32) -> i32
```

```text
sere main.sere --link native.lib
```

**Boxed module API** — implement `sere_mod_init` and `Sere_DefineFunction`
(`include/sere/api/sere_mod.h`).

---

## 9. Add a garbage collector

1. Implement `SereGcVTable` (`include/sere/api/sere_gc.h`).
2. `sere_gc_install(&vtable)` from `sere_mod_init`.
3. Link with `--link`.
4. Call `gc.use("your_name")` only if you also registered a builtin name in
   `sere_gc.c`; otherwise installing from `sere_mod_init` is enough.
5. Install before the program allocates.

Builtin names today: `none`, `mark_sweep`, `arena`.

---

## 10. Add an LSP feature

1. Implement on the typed AST / `TypeChecker` symbols when possible so
   `--analyze` and tests can see it.
2. `LanguageServer.cpp` — handler + `initialize` capability bit.
3. `Query.cpp` if you need hit-testing.
4. Client: `editors/vscode/package.json` / `extension.js` only if the client
   must opt in.
5. Unit test with `Frontend`, not a live editor.

---

## 11. Add a project command

1. `ProjectCommand` in `Options.h`.
2. Parse in `Options.cpp` (`parseProjectCommand` + usage text).
3. Branch in `Compiler::run`.
4. Implementation under `lib/driver/Project*.cpp`.
5. `tests/project_cli.cpp`.

Library projects: `sere init-lib` / `--init-lib` / `init --lib` write
`kind = "lib"` and `src/lib.sere`. `sere pack` (also `--pack`, `build-lib`,
and `sere build` on a lib) writes a `.slib` via `Library.h`. Pack keeps only
reachable local modules and compiled native objects. Import resolution
accepts `.slib` and folder libraries (`name/lib.sere`, `name/name.sere`) in
`ImportPath.cpp`. Full command and `sere.toml` runbook:
[projects.md](projects.md).

---

## 12. Add a test / example

See [testing.md](testing.md). Minimum for a language change:

- one unit test that fails without your patch
- one `examples/*.sere` emit test if users should write that syntax

---

## Common pitfalls

- **Keyword not highlighting** — enumerator placed after `KeywordWith`, or
  missing from `kKeywords`.
- **LSP stale** — `sere` rebuilt but language server not restarted; or editor
  still pointing at a locked `bin/sere.exe`.
- **Prelude parse errors after a real error** — do not early-return from
  `parseIf` / `parseWhile` solely because `diagnostics_->hasErrors()` is
  already true (that flagged later `else:` as errors in imported prelude).
- **`p.field` on a `Unique[T]`** — not valid; complete through `(*p).`.
- **New runtime symbol not found at link** — declaration in `.sere` must match
  the C symbol exactly, and `sere_rt` must have been rebuilt.
- **Call keywords ignored or rejected** — only a known `def` (and `print`)
  accept `name=expr`. Indirect calls and other intrinsics error. Wire
  `CallExpr` keywords through parse → sema `boundArguments` →
  `appendBoundCallArgs`; do not re-parse names in codegen.
- **Touching everything for a stdlib-only feature** — if Sere can express it
  with `extern "C"` and existing types, skip parse/sema/codegen.
