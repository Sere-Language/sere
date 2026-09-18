"""Temporary diagnostic: log exactly what `sere publish` sends."""

import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self):
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length)
        with open(sys.argv[2], "w", encoding="utf-8") as log:
            log.write(f"PATH {self.path}\n")
            for name, value in self.headers.items():
                log.write(f"HEADER {name}: {value}\n")
            log.write(f"BODY-BYTES {len(body)}\n")
            log.write(body[:400].decode("utf-8", "replace") + "\n")
        payload = json.dumps(
            {
                "published": {
                    "name": "hello",
                    "version": "0.1.0",
                    "url": "/api/packages/hello",
                    "install": "sere add hello@0.1.0",
                }
            }
        ).encode()
        self.send_response(201)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *args):
        pass


HTTPServer(("127.0.0.1", int(sys.argv[1])), Handler).serve_forever()
