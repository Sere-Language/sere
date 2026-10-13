"""Compile the Sere examples in the documentation.

Documentation that does not compile is a bug in the documentation, so every
fenced ```` ```sere ```` block is treated as a standalone program and handed to
the compiler. A block that is deliberately a fragment rather than a program —
an import list, or a single line showing a constructor — is marked with the
`ignore` attribute on the fence:

    ```sere ignore
    window: gl.Window = gl.Window("title", 960, 540)
    ```

A fragment that does not declare `main` is skipped automatically, so a page can
tell a story with short snippets and still be checked for the programs it shows.

Run from the repository root:

    py scripts/check-doc-examples.py                    # every docs/*.md
    py scripts/check-doc-examples.py docs/gl.md         # one page
    py scripts/check-doc-examples.py --jobs 8 docs/**
"""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_COMPILER = "build/windows-clang-cl-relwithdebinfo/bin/sere.exe"

FENCE = re.compile(r"^```(\S*)([^\n]*)\n(.*?)^```\s*$", re.M | re.S)


def examples(path: Path) -> tuple[list[tuple[int, str]], list[int]]:
    """Return the (line number, source) pairs to compile, and the lines skipped.

    A block is compiled when it declares `main`, so a page can mix complete
    programs with illustrative fragments. `ignore` on the fence skips a block
    either way, which is for the rare fragment that does declare `main` but only
    makes sense next to the rest of the page.
    """
    text = path.read_text(encoding="utf-8")
    found: list[tuple[int, str]] = []
    skipped: list[int] = []
    for match in FENCE.finditer(text):
        language = match.group(1)
        attributes = match.group(2).split()
        if language != "sere":
            continue
        line = text.count("\n", 0, match.start()) + 1
        source = match.group(3)
        if "ignore" in attributes or not re.search(r"^def main\b", source, re.M):
            skipped.append(line)
            continue
        found.append((line, source))
    return found, skipped


def compile_one(
    work: Path,
    index: int,
    line: int,
    source: str,
    compiler: Path,
    stdlib: Path,
    keep: bool,
) -> tuple[bool, str]:
    """Compile one example and return whether it worked, plus a report line."""
    file = work / f"example{index:02d}.sere"
    file.write_text(source, encoding="utf-8", newline="\n")
    environment = dict(os.environ)
    environment["SERE_STDLIB"] = str(stdlib)
    # Keep compiler intermediates next to the example so parallel runs cannot
    # collide on a shared temporary directory.
    environment["TMP"] = str(work)
    environment["TEMP"] = str(work)
    environment["TMPDIR"] = str(work)
    result = subprocess.run(
        [str(compiler), str(file), "-o", str(work / f"example{index:02d}.exe")],
        capture_output=True,
        text=True,
        cwd=str(work),
        env=environment,
    )
    first = source.strip().splitlines()[0] if source.strip() else "(empty)"
    label = f"line {line:<5} {first[:64]}"
    if result.returncode == 0:
        if not keep:
            Path(work / f"example{index:02d}.exe").unlink(missing_ok=True)
        return True, f"ok    {label}"
    errors = [
        text
        for text in (result.stdout + result.stderr).splitlines()
        if text.startswith("error") or "error" in text[:12]
    ]
    detail = "\n      ".join(errors[:6]) if errors else (result.stdout + result.stderr).strip()[:400]
    return False, f"FAIL  {label}\n      {detail}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("pages", nargs="*", default=None, help="markdown files or globs")
    parser.add_argument("--compiler", default=DEFAULT_COMPILER, help="path to sere")
    parser.add_argument("--stdlib", default="stdlib", help="path to the standard library")
    parser.add_argument("--jobs", type=int, default=4, help="parallel compiles")
    parser.add_argument("--keep", action="store_true", help="keep the generated sources")
    parser.add_argument("--verbose", action="store_true", help="print every result, not just failures")
    args = parser.parse_args()

    pages: list[Path] = []
    patterns = args.pages if args.pages else ["docs/*.md"]
    for pattern in patterns:
        matches = sorted(Path().glob(pattern)) if any(ch in pattern for ch in "*?[") else [Path(pattern)]
        for match in matches:
            if match not in pages:
                pages.append(match)

    compiler = (ROOT / args.compiler).resolve()
    stdlib = (ROOT / args.stdlib).resolve()
    if not compiler.exists():
        print(f"no compiler at {compiler}: build it first, or pass --compiler")
        return 2

    work = Path(tempfile.mkdtemp(prefix="sere-doc-examples-"))
    total = 0
    failures = 0
    try:
        for page in pages:
            if not page.exists():
                print(f"skipping missing page {page}")
                continue
            blocks, skipped = examples(page)
            print(f"{page}: {len(blocks)} programs to compile, {len(skipped)} fragments skipped")
            jobs = []
            with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
                for index, (line, source) in enumerate(blocks, start=1):
                    total += 1
                    jobs.append(
                        pool.submit(
                            compile_one, work, total, line, source, compiler, stdlib, args.keep
                        )
                    )
                for job in jobs:
                    ok, report = job.result()
                    if not ok:
                        failures += 1
                    if not ok or args.verbose:
                        print(f"  {report}")
    finally:
        if args.keep:
            print(f"sources kept in {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    print(f"compiled {total} examples, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
