"""Named tuple parsing, typing, immutability, and editor regressions."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = str(Path(sys.argv[1]).resolve())
base = "type Size: (width: u32, height: u32)\n"
invalid = {
    "duplicate": "type T = (x: i32, x: i32)\n",
    "mixed": "type T = (x: i32, i32)\n",
    "mixed_reverse": "type T = (i32, x: i32)\n",
    "reserved": "type T = (x: i32, first: i32)\n",
    "late": "type T = (x: late i32)\n",
    "auto": "type T = (auto x: i32)\n",
    "modifier": "type T = (@private x: i32)\n",
    "direct_ordinary": "type T: (i32, str)\n",
    "void": "type T = (x: void)\n",
    "missing_argument": base + "s = Size(1)\n",
    "no_arguments": base + "s = Size()\n",
    "extra_argument": base + "s = Size(1, 2, 3)\n",
    "keyword": base + "s = Size(width=1, depth=2)\n",
    "wrong_type": base + "s = Size(1, \"bad\")\n",
    "overflow": base + "s = Size(-1, 2)\n",
    "different_names": base + "type Other = (x: u32, y: u32)\ns: Size = Other(1, 2)\n",
    "different_order": base + "type Other = (height: u32, width: u32)\ns: Size = Other(1, 2)\n",
    "different_types": base + "type Other = (width: i32, height: i32)\ns: Size = Other(1, 2)\n",
}
for name, statement in {
    "named_write": "s.width = 3",
    "positional_write": "s.first = 3",
    "indexed_write": "s[0] = 3",
    "augmented_write": "s.width += 3",
    "increment": "s.width++",
    "address": "pointer = &s.width",
    "unknown_member": "x = s.depth",
    "bad_index": "x = s[2]",
}.items():
    invalid[name] = base + "def f() -> void:\n    s = Size(1, 2)\n    " + statement + "\n"
with tempfile.TemporaryDirectory(prefix="sere-named-tuple-") as directory:
    for name, source in invalid.items():
        path = Path(directory) / (name + ".sere")
        path.write_text(source + "\ndef main() -> i32:\n    return 0\n")
        result = subprocess.run([compiler, "--emit-llvm", str(path)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode != 0 and "error" in result.stderr, (name, result.stdout, result.stderr)
        assert "compiler bug" not in result.stderr, (name, result.stderr)
        if name.endswith("write") or name in ("increment", "address"):
            assert "immutable" in result.stderr, (name, result.stderr)
print("named tuple diagnostics passed")
