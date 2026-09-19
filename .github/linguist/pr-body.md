<!--- Add Sere (.sere) as a language so GitHub counts and highlights it. -->

## Description

Adds **Sere** — a statically typed Python-superset compiled to native code with
an LLVM backend — as a new language, plus its TextMate grammar for syntax
highlighting.

- Language site: https://sere-lang.com/
- Compiler, standard library, and grammar: https://github.com/Sere-Language/sere
- Grammar scope: `source.sere` (`editors/vscode/syntaxes/sere.tmLanguage.json`)

## Checklist:

- [x] **I am adding a new language.**
  - [x] The extension of the new language is used in hundreds of repositories on GitHub.com.
    - Search results for each extension:
      - https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.sere+%22def+main%22
      - https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.sere+%22import%22
      - https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.sere+%22->+i32%22
  - [x] I have included a real-world usage sample for all extensions added in this PR:
    - Sample source(s):
      - https://github.com/Sere-Language/sere/blob/main/examples/wsgi.sere
      - https://github.com/Sere-Language/sere/tree/main/examples
      - https://github.com/Sere-Language/sere/tree/main/stdlib
    - Sample license(s): MIT (`LICENSE`, Copyright (c) 2026 Sere Language).
      The samples in `samples/Sere/` were written for the language's own
      repository and are contributed here under the MIT license that covers
      Linguist.
  - [x] I have included a syntax highlighting grammar: https://github.com/Sere-Language/sere
    - `script/add-grammar https://github.com/Sere-Language/sere`
  - [x] I have added a color
    - Hex value: `#6f42c1`
    - Rationale: violet, Sere's brand accent, matching the sere-lang.com palette.
  - [ ] I have updated the heuristics to distinguish my language from others using the same extension.
    - Not applicable. `.sere` is not claimed by any other language in
      `languages.yml`, so no disambiguation is needed. Sere is a superset of
      Python, and has keyword-level differences (`def f[T](x: T) -> T`,
      `struct`, `enum ... :` with payloads, `extern "C" "symbol"`, `macro` /
      `quote`, `defer`, `!` macro invocation) that a heuristic could use later
      if `.sere` ever collides with another language.
