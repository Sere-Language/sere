# Language detection on GitHub

GitHub decides a repository's languages with
[github-linguist](https://github.com/github-linguist/linguist), not with the
editor extension. A file only counts once its language exists in Linguist's
`languages.yml`, and only then does GitHub pick a grammar for highlighting.

## Status

`Sere` is **not** in Linguist yet, so `.sere` files are currently uncounted:
GitHub treats the extension as unknown and skips those files in the language
bar. That is why `.github/linguist/` exists — it holds the ready-to-submit
payload and `scripts/linguist-pr.ps1` stages the pull request.

Until the upstream entry ships, `linguist-language` cannot invent a language.
`Linguist::LazyBlob#language` resolves the override through
`Language.find_by_alias`, which returns `nil` for an unknown name:

```ruby
@language = if lang = git_attributes['linguist-language']
  detected_language = Language.find_by_alias(lang)   # nil for an unknown name
```

A `nil` language means the file is skipped by the statistics, so the attribute
is a no-op today and becomes correct the moment Linguist ships `Sere`. If you
want `.sere` files counted *right now* in a specific repository, point the
attribute at a language Linguist already knows (Sere is a Python superset):

```gitattributes
*.sere linguist-language=Python
```

That shows "Python" in the language bar, so it is a stopgap — the default in
this repository and in generated projects is `linguist-language=Sere`.

## What ships in every project

`sere init` and `sere init-lib` write a `.gitattributes` next to `.gitignore`:

```gitattributes
# GitHub language detection for Sere sources (see docs/linguist.md).
*.sere linguist-language=Sere linguist-detectable text eol=lf
*.slib binary
*.lib binary
*.dll binary
*.exe binary
```

For repositories created before this, backfill them in place:

```powershell
.\scripts\linguist-attributes.ps1 -Path C:\Users\me\sere-projects -Recurse -DryRun
.\scripts\linguist-attributes.ps1 -Path C:\Users\me\sere-projects -Recurse
```

The block is fenced by `# >>> sere linguist >>>` / `# <<< sere linguist <<<`
markers, so re-running rewrites only that block.

## Landing the language upstream

```powershell
.\scripts\linguist-pr.ps1 -Validate      # check the payload, change nothing
.\scripts\linguist-pr.ps1 -Test          # stage the PR in a Linguist checkout
.\scripts\linguist-pr.ps1 -Fork <you> -Push
```

The payload is:

| Piece | Value |
| --- | --- |
| Extension | `.sere` |
| Type | `programming` |
| Color | `#6f42c1` (violet, Sere's brand accent) |
| Grammar | `editors/vscode/syntaxes/sere.tmLanguage.json` (scope `source.sere`, MIT) |
| Samples | `.github/linguist/samples/Sere/` (three sources from this repository, MIT) |

Two Linguist helper commands complete the branch and need Ruby:

```bash
script/add-grammar https://github.com/Sere-Language/sere
script/update-ids
```

`script/update-ids` assigns the `language_id`, which is why the fragment in
`.github/linguist/languages.yml.fragment` deliberately omits it.

After the pull request merges, GitHub ships the change in a Linguist release.
Repositories then pick it up on their next statistics run; nothing else has to
change in this repository or in user projects, because detection is by
extension.

## Why the grammar matters

GitHub highlights with TextMate grammars compiled through PCRE, and the scope
must match the language entry (`tm_scope: source.sere`). The extension's
grammar is the single source of truth for both VS Code and GitHub, so fixes
made for the editor show up on GitHub after the next Linguist release.
`scripts/linguist-pr.ps1 -Validate` warns about Oniguruma-only constructs —
chiefly `\G` and variable-width lookbehind — that PCRE rejects.
