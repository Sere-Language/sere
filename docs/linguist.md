# GitHub Linguist and `.sere` files

This repository uses `.gitattributes` to declare that `*.sere` files are Sere:

```gitattributes
*.sere linguist-language=Sere linguist-detectable
```

Repository-side attributes can only select languages that already exist in
[github/linguist](https://github.com/github-linguist/linguist). If `Sere` is not
yet present in Linguist's `languages.yml`, GitHub cannot fully recognize it as a
global language name/color just from this repository.

To add first-class global support, submit the upstream Linguist change. This
repository keeps that payload in `.github/linguist/`
(`languages.yml.fragment`, `grammars.yml.fragment`, and `samples/Sere/`).
