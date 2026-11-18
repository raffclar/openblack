# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Serves an unpacked web-build.tar on 127.0.0.1 only, with the headers that the live site sends for /play/<file>.

Usage: python -I gate_serve.py <folder> <port>

The headers are the headers of the /play/ files in docker/Caddyfile of bwgame-service: COOP, COEP, CORP, the content
policy of the game page, nosniff, the referrer policy and "private, no-cache". The types of .wasm and .js are the types
of the site. /assets/index.json answers 204, as the site does for an account that may not stream the game files: the
page then reads the game files from the folder that the test gives it. Each other path that is not a file answers 404,
as on the site.

The server writes its process number first, so that the caller can stop it by that number. It writes one line for each
request to the standard error, with the status, so that the gate can find each answer that was not a success.
"""
import functools
import http.server
import os
import sys

# The headers of the /play/ files on the live site (bwgame-service docker/Caddyfile). The connect-src of the site also
# has the origins of BW_PLAY_CONNECT_SRC, for the game sockets. The page of the gate opens no game socket.
LIVE_HEADERS = (
    ("Cross-Origin-Opener-Policy", "same-origin"),
    ("Cross-Origin-Embedder-Policy", "require-corp"),
    ("Cross-Origin-Resource-Policy", "same-origin"),
    ("Content-Security-Policy",
     "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; connect-src 'self'; img-src 'self' data: blob:; "
     "style-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'self'"),
    ("X-Content-Type-Options", "nosniff"),
    ("Referrer-Policy", "same-origin"),
    ("Cache-Control", "private, no-cache"),
)
LIVE_TYPES = {".wasm": "application/wasm", ".js": "text/javascript; charset=utf-8", ".json": "application/json"}
ASSET_INDEX_PATH = "/assets/index.json"


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        for name, value in LIVE_HEADERS:
            self.send_header(name, value)
        super().end_headers()

    def do_GET(self):
        if self.path.split("?", 1)[0] == ASSET_INDEX_PATH:
            self.send_response(204)
            self.end_headers()
            return
        super().do_GET()

    def log_message(self, format, *args):
        sys.stderr.write("serve: " + (format % args) + "\n")
        sys.stderr.flush()


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: gate_serve.py <folder> <port>")
    folder, port = sys.argv[1], int(sys.argv[2])
    Handler.extensions_map.update(LIVE_TYPES)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=folder))
    print(f"serve: pid {os.getpid()} on http://127.0.0.1:{port} for {folder}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
