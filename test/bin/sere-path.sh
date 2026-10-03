#!/usr/bin/env bash
#   . ./bin/sere-path.sh
#   . ./bin/sere-path.sh --persist
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "$here/sere" || -x "$here/sere.exe" ]]; then
  bin="$here"
elif [[ -x "$here/../venv/bin/sere" || -x "$here/../venv/bin/sere.exe" ]]; then
  bin="$(cd "$here/../venv/bin" && pwd)"
elif [[ -f "$here/../venv/sere.cfg" ]]; then
  bin="$(sed -n 's/^[[:space:]]*home[[:space:]]*=[[:space:]]*//p' "$here/../venv/sere.cfg" | head -n 1 | tr -d '"')"
fi
if [[ -z "${bin:-}" || ! ( -x "$bin/sere" || -x "$bin/sere.exe" ) ]]; then
  echo "sere not found. Run this from a Sere project or compiler bin folder." >&2
  return 1 2>/dev/null || exit 1
fi
export PATH="$bin:${PATH}"
echo "This session PATH starts with:"
echo "  $bin"
if [[ "${1:-}" == "--persist" ]]; then
  touch "${HOME}/.profile"
  grep -Fq "$bin" "${HOME}/.profile" || printf '\nexport PATH="%s:$PATH"\n' "$bin" >> "${HOME}/.profile"
  echo "Saved in ${HOME}/.profile"
fi
