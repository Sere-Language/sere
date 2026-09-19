This PR adds the Sere programming language.

- Name: Sere
- Extension: `.sere`
- Type: `programming`
- Color: `#6f42c1`
- Grammar will be hosted at: https://github.com/Sere-Language/sere
  (`editors/vscode/syntaxes/sere.tmLanguage.json`, scope `source.sere`, MIT licensed).
  Vendor it with `script/add-grammar https://github.com/Sere-Language/sere`.
- Samples added (real sources from the language repository, MIT licensed):
  - `samples/Sere/wsgi_server.sere` — https://github.com/Sere-Language/sere/blob/main/examples/wsgi.sere
  - `samples/Sere/language_tour.sere` — https://github.com/Sere-Language/sere/tree/main/examples
  - `samples/Sere/gc_and_native.sere` — https://github.com/Sere-Language/sere/tree/main/examples

In-the-wild usage:

- https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.sere+%22def+main%22
- https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.sere+%22-%3E+i32%22

`language_id` is omitted on purpose, per CONTRIBUTING (`script/update-ids` assigns it).

Sere is a statically typed Python superset compiled to native code with an LLVM
backend: https://sere-lang.com/
