"""Serves the folder of the web page on 127.0.0.1 only, for tests on this computer.

Usage: serve.py <folder> <port> [--connect <origin>]

With --connect, the page may also connect to that origin (such as wss://127.0.0.1:9999, a test game server on this
computer), and to nothing else but itself.

The server sends the headers that the game threads need (COOP and COEP) and the content policy of the page. It
writes its process number first, so that the caller can stop it by that number.
"""
import functools
import http.server
import os
import sys


# The origin that the page may also connect to: a test game server on this computer
extra_connect = ""


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; connect-src 'self'" + extra_connect + "; "
            "img-src 'self' data: blob:; style-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'",
        )
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, format, *args):
        sys.stderr.write("serve: " + (format % args) + "\n")


Handler.extensions_map[".wasm"] = "application/wasm"
Handler.extensions_map[".js"] = "text/javascript"
folder, port = sys.argv[1], int(sys.argv[2])
if len(sys.argv) > 4 and sys.argv[3] == "--connect":
    origin = sys.argv[4]
    if not origin.startswith(("wss://127.0.0.1:", "ws://127.0.0.1:")):
        sys.exit("serve: --connect takes an origin on 127.0.0.1 only")
    extra_connect = " " + origin
server = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=folder))
print(f"serve: pid {os.getpid()} on http://127.0.0.1:{port} for {folder}", flush=True)
server.serve_forever()
