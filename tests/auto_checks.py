"""Auto parameter diagnostics and editor protocol regressions."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = str(Path(sys.argv[1]).resolve())
invalid = {
    "function": ("def f(auto x: i32) -> void:\n    pass\n", "only valid"),
    "free_init": ("def __init__(self, auto x: i32) -> void:\n    pass\n", "only valid"),
    "method": ("class C:\n    x: i32\n    def f(self, auto x: i32) -> void:\n        pass\n", "only valid"),
    "missing": ("class C:\n    def __init__(self, auto x: i32) -> void:\n        pass\n", "instance field"),
    "static": ("class C:\n    static x: i32\n    def __init__(self, auto x: i32) -> void:\n        pass\n", "instance field"),
    "mismatch": ("class C:\n    x: i64\n    def __init__(self, auto x: i32) -> void:\n        pass\n", "same type"),
    "self": ("class C:\n    def __init__(auto self) -> void:\n        pass\n", "other than self"),
    "untyped": ("class C:\n    x: i32\n    def __init__(self, auto x) -> void:\n        pass\n", "typed parameter"),
    "vararg": ("class C:\n    x: list[i32]\n    def __init__(self, auto *x: i32) -> void:\n        pass\n", "typed parameter"),
    "lambda": ("f = lambda(auto x: i32): x\n", "only valid"),
    "variable": ("auto x: i32 = 1\n", "error"),
    "type": ("x: auto i32 = 1\n", "error"),
    "private": ("class C:\n    @private x: i32\n    def __init__(self, auto x: i32) -> void:\n        pass\ndef f() -> void:\n    c = C(1)\n    c.x = 2\n", "private"),
}
with tempfile.TemporaryDirectory(prefix="sere-auto-") as directory:
    for name, (source, expected) in invalid.items():
        path = Path(directory) / (name + ".sere")
        path.write_text(source + "\ndef main() -> i32:\n    return 0\n")
        result = subprocess.run([compiler, "--emit-llvm", str(path)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode != 0, (name, result.stdout)
        assert expected in result.stderr, (name, result.stderr)
        assert "compiler bug" not in result.stderr, (name, result.stderr)
print("auto diagnostics passed")

source = "class C:\n    x: late i32\n    def __init__(self, auto x: i32) -> void:\n        pass\ndef main() -> i32:\n    c = C(4)\n    return c.x\n"
uri = "file:///auto_lsp.sere"
messages = [
    {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
    {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
        "textDocument": {"uri": uri, "languageId": "sere", "version": 1, "text": source}}},
]
for request_id, method, position in [
    (2, "completion", {"line": 6, "character": 13}),
    (3, "hover", {"line": 2, "character": 10}),
    (4, "semanticTokens/full", None),
    (5, "completion", {"line": 2, "character": 25}),
    (7, "signatureHelp", {"line": 5, "character": 10}),
    (8, "definition", {"line": 6, "character": 13}),
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
assert "auto x: i32" in json.dumps(responses[3]), responses[3]
assert any(item["label"] == "auto" for item in responses[5]), responses[5]
assert "x: i32" in responses[7]["signatures"][0]["label"], responses[7]
assert responses[8][0]["range"]["start"]["line"] == 1, responses[8]
legend = responses[1]["capabilities"]["semanticTokensProvider"]["legend"]["tokenTypes"]
line = column = 0
found = False
parameter_found = False
tokens = responses[4]["data"]
for index in range(0, len(tokens), 5):
    dl, dc, length, token, _ = tokens[index:index + 5]
    line += dl
    column = dc if dl else column + dc
    if line == 2 and column == 23:
        assert length == 4 and legend[token] == "keyword", (length, legend[token])
        found = True
    if line == 2 and column == 28:
        assert length == 1 and legend[token] == "parameter", (length, legend[token])
        parameter_found = True
assert found and parameter_found, tokens
grammar = json.loads((Path(__file__).parents[1] / "editors/vscode/syntaxes/sere.tmLanguage.json").read_text())
assert "auto|" in json.dumps(grammar)
snippets = json.loads((Path(__file__).parents[1] / "editors/vscode/snippets/modifiers.json").read_text())
assert snippets["Auto constructor parameter"]["prefix"] == "auto"
print("auto LSP and extension checks passed")
