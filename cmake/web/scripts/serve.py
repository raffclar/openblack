"""Serves one folder on 127.0.0.1 only, with the headers a threads build needs."""
import functools
import http.server
import sys

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Content-Security-Policy", "default-src 'self' 'unsafe-inline' 'unsafe-eval' blob:; connect-src 'self'")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

Handler.extensions_map[".wasm"] = "application/wasm"
folder, port = sys.argv[1], int(sys.argv[2])
server = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=folder))
print(f"serving {folder} on http://127.0.0.1:{port}", flush=True)
server.serve_forever()
