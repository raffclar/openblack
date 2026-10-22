"""Serves the folder of the web page on 127.0.0.1 only, for tests on this computer.

Usage: serve.py <folder> <port>

The server sends the headers that the game threads need (COOP and COEP) and the content policy of the page. It
writes its process number first, so that the caller can stop it by that number.
"""
import functools
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; connect-src 'self'; "
            "img-src 'self' data: blob:; style-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'",
        )
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, format, *args):
        sys.stderr.write("serve: " + (format % args) + "\n")


Handler.extensions_map[".wasm"] = "application/wasm"
Handler.extensions_map[".js"] = "text/javascript"
folder, port = sys.argv[1], int(sys.argv[2])
server = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=folder))
print(f"serve: pid {os.getpid()} on http://127.0.0.1:{port} for {folder}", flush=True)
server.serve_forever()
