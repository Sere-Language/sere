"""Regression: the language server retires the `name.get` / `name.set` syntax.

The accessor form still compiles, but the editor must report it as deprecated,
strike it out in completion, and highlight it as deprecated.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

source = """class Temperature:
    celsius: i32

    @public fahrenheit.get:
        return self.celsius

    @public fahrenheit.set(value: i32) -> void:
        self.celsius = value

value: Temperature = Temperature(0)
value.
"""

with tempfile.TemporaryDirectory() as directory:
    source_path = Path(directory) / "legacy_property.sere"
    source_path.write_text(source)
    uri = source_path.as_uri()

    messages = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
        {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
            "textDocument": {"uri": uri, "languageId": "sere", "version": 1,
                             "text": source}}},
    ]
    cursor = source.index("value.") + len("value.")
    position = {"line": source[:cursor].count("\n"),
                "character": len(source[:cursor].rsplit("\n", 1)[-1])}
    messages.extend([
        {"jsonrpc": "2.0", "id": 2, "method": "textDocument/completion",
         "params": {"textDocument": {"uri": uri}, "position": position}},
        {"jsonrpc": "2.0", "id": 3, "method": "textDocument/semanticTokens/full",
         "params": {"textDocument": {"uri": uri}}},
        {"jsonrpc": "2.0", "id": 4, "method": "shutdown"},
        {"jsonrpc": "2.0", "method": "exit"},
    ])
    wire = b""
    for message in messages:
        body = json.dumps(message).encode()
        wire += f"Content-Length: {len(body)}\r\n\r\n".encode() + body
    result = subprocess.run([sys.argv[1], "--lsp"], input=wire, capture_output=True,
                            timeout=90)
    assert result.returncode == 0, result.stderr.decode(errors="replace")
    data = result.stdout
    responses = {}
    diagnostics = []
    while data:
        header, data = data.split(b"\r\n\r\n", 1)
        length = int(header.split(b":", 1)[1].strip())
        message = json.loads(data[:length])
        data = data[length:]
        if message.get("method") == "textDocument/publishDiagnostics":
            if message["params"]["uri"] == uri:
                diagnostics = message["params"]["diagnostics"]
        if "id" in message:
            responses[message["id"]] = message.get("result")

# 1. Both accessors are reported as deprecated, tagged for strikethrough.
deprecations = [item for item in diagnostics if item.get("code") == "Deprecation"]
assert len(deprecations) == 2, diagnostics
for item in deprecations:
    assert item["severity"] == 2, item
    assert item["tags"] == [2], item
    assert "custom access modifier" in item["message"], item
lines = sorted(item["range"]["start"]["line"] for item in deprecations)
assert lines == [3, 6], deprecations

# 2. The member name in each declaration is highlighted as deprecated.
MODIFIERS = ["declaration", "definition", "readonly", "static", "abstract",
             "defaultLibrary", "deprecated"]
EXPECTED = 1 << MODIFIERS.index("deprecated")
tokens = responses[3]["data"]
highlighted = []
line = 0
column = 0
for index in range(0, len(tokens), 5):
    delta_line, delta_start, length, _type, modifiers = tokens[index:index + 5]
    line = line + delta_line
    column = delta_start if delta_line else column + delta_start
    if modifiers & EXPECTED:
        highlighted.append((line, column, length))
assert highlighted == [(3, 4, len("fahrenheit")), (6, 4, len("fahrenheit"))], highlighted

# 3. The member completes struck through, and the accessor methods stay hidden.
items = responses[2]
member = next(item for item in items if item["label"] == "fahrenheit")
assert member.get("tags") == [1], member
assert all(item["label"] != "__get_fahrenheit" for item in items), items
assert all(item["label"] != "__set_fahrenheit" for item in items), items
