#!/usr/bin/env bash
# Puts the Sere compiler on PATH.
#   . ./scripts/sere-path.sh              this session
#   . ./scripts/sere-path.sh --persist    this session + ~/.profile

find_sere_bin() {
  local here
  here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local dirs=(
    "$here"
    "$here/bin"
    "$here/../bin"
    "$here/../venv/bin"
    "$here/../build/windows-clang-cl-relwithdebinfo/bin"
  )
  local cfg home
  for cfg in "$here/sere.cfg" "$here/../venv/sere.cfg" "$here/../sere.cfg"; do
    if [[ -f "$cfg" ]]; then
      home="$(sed -n 's/^[[:space:]]*home[[:space:]]*=[[:space:]]*//p' "$cfg" | head -n 1 | tr -d '"')"
      if [[ -n "$home" ]]; then
        dirs+=("$home")
      fi
    fi
  done
  local dir
  for dir in "${dirs[@]}"; do
    if [[ -x "$dir/sere" || -x "$dir/sere.exe" ]]; then
      (cd "$dir" && pwd)
      return 0
    fi
  done
  if command -v sere >/dev/null 2>&1; then
    dirname "$(command -v sere)"
    return 0
  fi
  return 1
}

bin="$(find_sere_bin)" || {
  echo "sere not found. Build the compiler or run this script from the folder that contains it." >&2
  return 1 2>/dev/null || exit 1
}

strip_bin() {
  local out="" part
  IFS=':' read -ra parts <<< "${PATH}"
  for part in "${parts[@]}"; do
    if [[ "$part" != "$bin" ]]; then
      if [[ -n "$out" ]]; then
        out="$out:$part"
      else
        out="$part"
      fi
    fi
  done
  PATH="$out"
}

if [[ "${1:-}" == "--remove" ]]; then
  strip_bin
  export PATH
  echo "Removed from this session: $bin"
  return 0 2>/dev/null || exit 0
fi

strip_bin
export PATH="$bin:$PATH"
echo "This session PATH starts with:"
echo "  $bin"
if [[ -x "$bin/sere" ]]; then
  echo "sere -> $bin/sere"
else
  echo "sere -> $bin/sere.exe"
fi

if [[ "${1:-}" == "--persist" ]]; then
  profile="${HOME}/.profile"
  touch "$profile"
  if ! grep -Fq "$bin" "$profile"; then
    printf '\nexport PATH="%s:$PATH"\n' "$bin" >> "$profile"
    echo "Saved in $profile (new terminals will see it)."
  else
    echo "Already listed in $profile."
  fi
fi

echo "Try:  sere --help"
