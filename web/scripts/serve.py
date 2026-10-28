"""Serves the folder of the web page on 127.0.0.1 only, for tests on this computer.

Usage: serve.py <folder> <port> [--connect <origin>]

With --connect, the page may also connect to that origin (such as wss://127.0.0.1:9999, a test game server on this
computer), and to nothing else but itself. The page has its own content policy in a meta element of index.html, and a
browser obeys both policies. So the server adds the origin to the connect-src of that meta element too, in the copy of
index.html that it sends. The file on the disk stays as it is.

The server sends the headers that the game threads need (COOP and COEP) and the content policy of the page. It
writes its process number first, so that the caller can stop it by that number.
"""
import functools
import http.server
import os
import re
import sys


# The origin that the page may also connect to: a test game server on this computer
extra_connect = ""

# The connect-src of the content policy in the meta element of the page
META_CONNECT = re.compile(r"(<meta\s+http-equiv=\"Content-Security-Policy\"[^>]*?connect-src 'self')", re.IGNORECASE)


def page_with_connect(html, origin):
    """The page with the origin added to the connect-src of its meta content policy (the page as it is with none)"""
    if not origin:
        return html
    return META_CONNECT.sub(lambda match: match.group(1) + " " + origin, html, count=1)


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

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if not extra_connect or path not in ("/", "/index.html"):
            super().do_GET()
            return
        with open(os.path.join(self.directory, "index.html"), encoding="utf-8") as file:
            body = page_with_connect(file.read(), extra_connect.strip()).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        sys.stderr.write("serve: " + (format % args) + "\n")


def main():
    global extra_connect
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


if __name__ == "__main__":
    main()
