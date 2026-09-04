#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prefix="${SERE_INSTALL_PREFIX:-${HOME}/.local/share/sere}"
editor=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix) prefix="$2"; shift 2 ;;
    --editor) editor=1; shift ;;
    *) echo "Usage: $0 [--prefix DIR] [--editor]" >&2; exit 2 ;;
  esac
done
mkdir -p "$prefix" "${HOME}/.local/bin"
prefix="$(cd "$prefix" && pwd)"
if [[ "$prefix" != "$source_dir" ]]; then cp -a "$source_dir/." "$prefix/"; fi
ln -sfn "$prefix/bin/sere" "${HOME}/.local/bin/sere"
case ":$PATH:" in
  *":${HOME}/.local/bin:"*) ;;
  *)
    line='export PATH="$HOME/.local/bin:$PATH"'
    for rc in "$HOME/.profile" "$HOME/.bashrc"; do
      if ! grep -Fqx "$line" "$rc" 2>/dev/null; then printf '\n%s\n' "$line" >> "$rc"; fi
    done ;;
esac
if [[ "$editor" == 1 ]]; then
  for cli in code cursor; do
    if command -v "$cli" >/dev/null 2>&1; then "$cli" --install-extension "$prefix/editors/sere.vsix" --force; fi
  done
fi
printf 'Installed Sere in %s. Open a new terminal to refresh PATH.\n' "$prefix"
