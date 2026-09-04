#!/usr/bin/env bash
set -euo pipefail
prefix="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[[ -f "$prefix/bin/sere" && -d "$prefix/toolchains" ]] || { echo 'Not a Sere installation' >&2; exit 1; }
if [[ "$(readlink "${HOME}/.local/bin/sere" || true)" == "$prefix/bin/sere" ]]; then rm "${HOME}/.local/bin/sere"; fi
for name in bin toolchains stdlib include packaging editors docs examples LICENSE MANIFEST.txt README.md install.sh uninstall.sh; do
  rm -rf -- "$prefix/$name"
done
printf 'Removed Sere from %s\n' "$prefix"
