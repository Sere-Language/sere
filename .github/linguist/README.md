# Linguist payload

Everything needed to land `Sere` in [github-linguist](https://github.com/github-linguist/linguist),
which is what GitHub uses to decide a repository's languages and to pick a
syntax grammar.

| File | Destination in the Linguist PR |
| --- | --- |
| `languages.yml.fragment` | inserted into `lib/linguist/languages.yml` |
| `grammars.yml.fragment` | written by `script/add-grammar`, kept here for review |
| `samples/Sere/*.sere` | copied to `samples/Sere/` |
| `pr-body.md` | the pull request description (Linguist's template) |

## One command

```powershell
# stage the PR locally, verify the payload, print the remaining steps
.\scripts\linguist-pr.ps1

# check the payload against a live Linguist checkout without touching anything
.\scripts\linguist-pr.ps1 -Validate

# after Ruby is available: stage, run script/add-grammar + script/update-ids + rake test
.\scripts\linguist-pr.ps1 -LinguistDir C:\src\linguist -Test
```

`scripts/linguist-pr.ps1` is idempotent; running it twice changes nothing.

## Why a PR is required

`linguist-language=Sere` in `.gitattributes` is only honoured when `Sere` exists
in Linguist. `Linguist::LazyBlob#language` calls `Language.find_by_alias`:

```ruby
@language = if lang = git_attributes['linguist-language']
  detected_language = Language.find_by_alias(lang)   # nil for an unknown name
  ...
```

`find_by_alias` returns `nil` for a name that is not in `languages.yml`, and a
`nil` language means the file is skipped by the language statistics. So until
the language exists upstream, `.sere` files are simply uncounted — the
`.gitattributes` entry changes nothing either way, and starts working the
moment this PR ships. See [../../docs/linguist.md](../../docs/linguist.md).

## Sample provenance

`samples/Sere/*.sere` are real Sere sources from this repository
(`examples/wsgi.sere`, the `examples/` and `stdlib/` trees), MIT licensed like
the rest of the project, with a header comment saying so.
