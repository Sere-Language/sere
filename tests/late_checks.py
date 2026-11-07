"""Late-field diagnostics, runtime failures and editor protocol regression tests."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = str(Path(sys.argv[1]).resolve())

def run(*args):
    return subprocess.run(args, capture_output=True, text=True, timeout=60)

with tempfile.TemporaryDirectory(prefix="sere-late-") as directory:
    root = Path(directory)
    invalid = {
        "local": "def f() -> void:\n    x: late i32\n",
        "global": "x: late i32\n",
        "parameter": "def f(x: late i32) -> void:\n    pass\n",
        "return": "def f() -> late i32:\n    return 0\n",
        "alias": "type X = late i32\n",
        "nested": "struct C:\n    x: list[late i32]\n",
        "static": "class C:\n    static x: late i32\n",
        "initializer": "class C:\n    x: late i32 = 1\n",
        "type": "class C:\n    x: late i32\ndef f() -> void:\n    c = C()\n    c.x = \"bad\"\n",
    }
    for name, source in invalid.items():
        path = root / (name + ".sere")
        path.write_text(source + "\ndef main() -> i32:\n    return 0\n")
        result = run(compiler, "--emit-llvm", str(path))
        assert result.returncode != 0, (name, result.stdout)
        assert "error" in result.stderr.lower(), (name, result.stderr)
        assert "compiler bug" not in result.stderr, result.stderr
    for name, body in {
        "read": "    c = C()\n    return c.x\n",
        "augmented": "    c = C()\n    c.x += 1\n    return 0\n",
        "independent": "    a = C()\n    a.x = 3\n    b = C()\n    return b.x\n",
        "copy": "    a = C()\n    b = a\n    return b.x\n",
        "temporary": "    return C().x\n",
    }.items():
        path = root / (name + ".sere")
        path.write_text("struct C:\n    x: late i32\ndef main() -> i32:\n" + body)
        executable = root / (name + ".exe")
        for backend in ("llvm", "serem"):
            result = run(compiler, "--backend=" + backend, str(path), "-o", str(executable))
            assert result.returncode == 0, (backend, result.stderr)
            result = run(str(executable))
            assert result.returncode != 0, (backend, name)
            assert "uninitialized late member 'x'" in result.stderr, (backend, name, result.stderr)
print("late diagnostics and runtime checks passed")

source = "struct C:\n    x: late i32\ndef main() -> i32:\n    c = C()\n    c.x = 1\n    return c.x\n"
uri = "file:///late_lsp.sere"
messages = [
    {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
    {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
        "textDocument": {"uri": uri, "languageId": "sere", "version": 1, "text": source}}},
]
for request_id, method, position in [
    (2, "completion", {"line": 5, "character": 13}),
    (3, "hover", {"line": 0, "character": 7}),
    (4, "semanticTokens/full", None),
    (5, "completion", {"line": 1, "character": 9}),
]:
    params = {"textDocument": {"uri": uri}}
    if position is not None:
        params["position"] = position
    messages.append({"jsonrpc": "2.0", "id": request_id,
                     "method": "textDocument/" + method, "params": params})
messages.extend([{"jsonrpc": "2.0", "id": 6, "method": "shutdown"},
                 {"jsonrpc": "2.0", "method": "exit"}])
wire = b""
for message in messages:
    body = json.dumps(message).encode()
    wire += f"Content-Length: {len(body)}\r\n\r\n".encode() + body
result = subprocess.run([compiler, "--lsp"], input=wire, capture_output=True, timeout=60)
assert result.returncode == 0, result.stderr
responses = {}
data = result.stdout
while data:
    header, data = data.split(b"\r\n\r\n", 1)
    length = int(header.split(b":", 1)[1].strip())
    message = json.loads(data[:length])
    data = data[length:]
    if "id" in message:
        responses[message["id"]] = message.get("result")
    if message.get("method") == "textDocument/publishDiagnostics":
        assert not message["params"]["diagnostics"], message
assert any(item["label"] == "x" and "late i32" in item.get("detail", "")
           for item in responses[2]), responses[2]
assert "late i32" in json.dumps(responses[3]), responses[3]
assert any(item["label"] == "late" for item in responses[5]), responses[5]
legend = responses[1]["capabilities"]["semanticTokensProvider"]["legend"]["tokenTypes"]
line = column = 0
found = False
tokens = responses[4]["data"]
for index in range(0, len(tokens), 5):
    dl, dc, length, token, _ = tokens[index:index + 5]
    line += dl
    column = dc if dl else column + dc
    if line == 1 and column == 7:
        assert length == 4 and legend[token] == "keyword", (length, legend[token])
        found = True
assert found, tokens
grammar = json.loads((Path(__file__).parents[1] / "editors/vscode/syntaxes/sere.tmLanguage.json").read_text())
assert "late|" in json.dumps(grammar)
print("late LSP and extension checks passed")
