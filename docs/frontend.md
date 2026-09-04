# Frontend: lex, parse, AST, macros, types

## Lexer

`lib/lex/Lexer.cpp` scans a `SourceManager` into a token stream, including
Python-style `Indent` / `Dedent` / `Newline`.

Keywords are the table `kKeywords` in `lib/lex/Token.cpp`.
`keywordKind(spelling)` returns a `TokenKind` or `Identifier`.

`TokenKind` lives in `include/sere/lex/TokenKind.h`. The keyword enumerators
**must stay a contiguous range** from `KeywordFalse` through `KeywordWith`.
Semantic highlighting uses that range (`kind >= KeywordFalse && kind <= KeywordWith`).
Insert new keywords **before** `Unknown`, inside that block.

Integer literals accept decimal, `0x`/`0X` hex, `0b`/`0B` binary, and `0o`/`0O`
octal, plus `_` digit separators (`0xFF_FF`, `1_000`). Floats keep decimal
form (`1.0`, `3e2`, `1.0f`) and also allow `_`.

Multi-character operators (`++`, `**`, `//=`, `&=`, …) are recognized in the
lexer. Unary `*` / `&` reuse `Star` / `Amp`; the parser decides prefix vs binary.

## Parser

`Parser` is recursive descent (`include/sere/parse/Parser.h`).

Expression precedence (high to low, roughly):

1. Primary / postfix (call, member, index, `++`/`--` suffix)
2. Unary (`not`, `+`, `-`, `~`, `++`, `--`, `*`, `&`)
3. Cast (`as`)
4. Range (`a..b` may desugar to `range`)
5. Mul / add / shift / bitand / bitxor / bitor
6. Comparisons, `and` / `or`, ternary, assignment-like `:=`

Statements cover `def`, `class`, `struct`, `enum`, `type`, `macro`,
`import` / `from`, control flow (`if`/`while`/`for`/`match`/`try`),
`defer`, `del`, `assert`, `raise`, and assignment.

`extern "C" "symbol"` on a `def` records a native symbol name for codegen.

On error the parser synchronizes and keeps going so the LSP can still highlight
the rest of the file.

## AST

`include/sere/ast/Syntax.h` is the typed Python-superset tree. Nodes carry:

- `NodeKind`
- `SourceRange`
- `resolvedType()` (filled by sema)

Important enums: `UnaryOp` (includes `Deref`, `AddrOf`), `BinaryOp`, `AssignOp`.
`Module` is the root. Prelude statements are flagged `fromPrelude()`.

`lib/ast/Query.cpp` is the LSP-facing walk:

- `findNodeAt` — innermost node covering an offset
- `findCallAt`, `collectCalls`, `collectNameRefs`
- `collectMacroUses` / `findMacroNameAt`
- `formatMacro`, `macroSnippet`

When you add a node kind, update `searchExpr` / `searchStmt` in `Query.cpp` or
hover and go-to-definition will miss it.

## Macros

Pipeline: parse `macro` defs → capture invoke tokens → expand **before** sema.

| Piece | Header | Role |
| --- | --- | --- |
| Token trees | `macro/TokenTree.h` | Nest `()`, `[]`, `{}`, indent blocks |
| Pattern parse | `macro/Parse.h` | Re-parse captured tokens as exprs |
| Quote | `macro/Quote.h` | Clone AST, substitute `$x`, hygiene |
| Expander | `macro/Expander.h` | `quote`, `match`, raw bodies, fuel limit |

Call syntax:

- `name!(…)` bang-paren
- indent body (`html:` raw text)
- `match` token-tree arms

Expansion fuel defaults to 128 (`RecursionError` if exceeded).
User macros are navigable in the LSP; prelude macros (`dbg!`, `todo!`, …) are not.

## Type checker

`Any` accepts every value; conversions back to concrete types are checked at
runtime. See [gradual typing](gradual-typing.md) for inference and collection
safety rules. `None` is `void` as a
named type, so `i32 | None = None` and `i32 | None = void` are both valid.

`TypeChecker::check` (`include/sere/sema/TypeChecker.h`):

1. Register intrinsics (`registerBuiltins`).
2. Inject module dunders (`__name__`, `__file__`, …).
3. Collect class / enum / alias / function / macro / method names.
4. Flatten inheritance.
5. Check bodies: statements, inference, casts, dunders, pointers.

Symbols live in stacked scopes. `SemanticSymbol` is the flattened table the
LSP reads (name, kind, type display, range, snippet).

Pointer rules (sema, not codegen):

- `*p` requires `Unique[T]`, `Shared[T]`, or `Ptr[T]` and types as `T`.
- `&x` requires an addressable lvalue and types as `Ptr[T]`.
- `*p = v` is assignment through a pointer, not multiplication.

Assignments reject non-lvalues. `checkDeref` / `checkAddrOf` are the hooks.

## Adding syntax: which layer?

| Change | Lexer | Parser | AST | Macros | Sema | Codegen | LSP / grammar |
| --- | --- | --- | --- | --- | --- | --- | --- |
| New keyword | yes | yes | maybe | if quoted | yes | yes | yes |
| New operator | maybe | yes | op enum | — | yes | yes | tokens + tmLanguage |
| New intrinsic | — | — | — | — | builtin + check | `emitIntrinsic` | completions |
| New node kind | — | yes | yes | clone/subst | check* | emit* | `Query.cpp` |
