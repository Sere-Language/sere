"""Protocol regression for modifier completion, hover, definition and tokens."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
source_path = root / "tests/modifiers/modifier_parameters.sere"
source = source_path.read_text()
uri = source_path.as_uri()


def position(needle, advance):
    offset = source.index(needle) + advance
    return {"line": source[:offset].count("\n"),
            "character": len(source[:offset].rsplit("\n", 1)[-1])}


messages = [
    {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
    {"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {
        "textDocument": {"uri": uri, "languageId": "sere", "version": 1, "text": source}}},
]
queries = [
    (2, "completion", "get = @param", len("get = @pa")),
    (3, "completion", "@property_access[get", len("@prop")),
    (4, "completion", "@access x", len("@acc")),
    (5, "hover", "@property_access[get", 3),
    (6, "definition", "@property_access[get", 3),
    (7, "completion", "@property_access[get", len("@property_access[g")),
]
for request_id, method, needle, advance in queries:
    messages.append({"jsonrpc": "2.0", "id": request_id,
                     "method": "textDocument/" + method,
                     "params": {"textDocument": {"uri": uri},
                                "position": position(needle, advance)}})
messages.extend([
    {"jsonrpc": "2.0", "id": 8, "method": "textDocument/semanticTokens/full",
     "params": {"textDocument": {"uri": uri}}},
    {"jsonrpc": "2.0", "id": 9, "method": "shutdown"},
    {"jsonrpc": "2.0", "method": "exit"},
])
wire = b""
for message in messages:
    body = json.dumps(message).encode()
    wire += f"Content-Length: {len(body)}\r\n\r\n".encode() + body
result = subprocess.run([sys.argv[1], "--lsp"], input=wire, capture_output=True, timeout=60)
assert result.returncode == 0, result.stderr.decode(errors="replace")
data = result.stdout
responses = {}
while data:
    header, data = data.split(b"\r\n\r\n", 1)
    length = int(header.split(b":", 1)[1].strip())
    message = json.loads(data[:length])
    data = data[length:]
    if "id" in message:
        responses[message["id"]] = message.get("result")


def item(request_id, label):
    return next(entry for entry in responses[request_id] if entry["label"] == label)


assert item(2, "@param")["insertText"] == "param"
assert item(3, "@property_access")["insertText"] == "property_access"
assert all(entry["label"] != "@param" for entry in responses[3])
assert item(4, "@access")["insertText"] == "access"
assert "custom access modifier" in json.dumps(responses[5])
assert responses[6][0]["range"]["start"]["line"] == 0
assert item(7, "get")["insertText"] == "get = self."
assert responses[8]["data"]
print("modifier LSP regressions passed")
