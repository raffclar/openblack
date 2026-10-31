"""Serves the folder of the web page on 127.0.0.1 only, for tests on this computer.

Usage: serve.py <folder> <port> [--connect <origin>] [--tls <cert.pem> <key.pem> --game-proxy <origin>]

With --connect, the page may also connect to that origin (such as wss://127.0.0.1:9999, a test game server on this
computer), and to nothing else but itself.

The web build opens a game socket only to a path of its own origin, from an HTTPS page, as on the site, where the proxy
of the site sends wss://<site>/game/<id> to the game server. --tls serves the page over HTTPS with that certificate, and
--game-proxy does what the proxy of the site does: it sends each request for a path under /game/ to the test game
server at that origin (wss://127.0.0.1:<port>, TLS with no check of its certificate, as it is on this computer), and
then passes the bytes both ways until one side closes. The page then joins the game with the path /game/<id>.

The server sends the headers that the game threads need (COOP and COEP) and the content policy of the page. It
writes its process number first, so that the caller can stop it by that number.
"""
import functools
import http.server
import os
import socket
import ssl
import sys
import threading


# The origin that the page may also connect to: a test game server on this computer
extra_connect = ""
# The test game server that gets the requests for /game/, as (host, port), or None
game_upstream = None

GAME_PATH = "/game/"


def upstream_of(origin):
    """The host and the port of a wss:// origin on 127.0.0.1, or None for another origin"""
    if not origin.startswith("wss://127.0.0.1:"):
        return None
    port = origin[len("wss://127.0.0.1:"):].split("/", 1)[0]
    return ("127.0.0.1", int(port)) if port.isdigit() else None


def request_head(request_line, headers, host):
    """The bytes of the start of a request for the game server: the request line and the headers, with the Host of the
    game server"""
    lines = [request_line]
    for name, value in headers.items():
        lines.append(f"{name}: {host if name.lower() == 'host' else value}")
    return ("\r\n".join(lines) + "\r\n\r\n").encode("latin-1")


def pass_bytes(source, target):
    """Passes the bytes of one socket to the other until the first one closes, then closes the sending side of the
    other"""
    try:
        while True:
            data = source.recv(65536)
            if not data:
                break
            target.sendall(data)
    except OSError:
        pass
    try:
        target.shutdown(socket.SHUT_WR)
    except OSError:
        pass


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
        if game_upstream is not None and self.path.startswith(GAME_PATH):
            self.pass_to_game_server()
            return
        super().do_GET()

    def pass_to_game_server(self):
        context = ssl.create_default_context()
        # The test game server is on this computer, with a certificate of a test authority of another computer
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        try:
            upstream = context.wrap_socket(socket.create_connection(game_upstream, timeout=10))
        except OSError as error:
            self.send_error(502, f"no game server: {error}")
            return
        upstream.settimeout(None)
        self.connection.settimeout(None)
        host = f"{game_upstream[0]}:{game_upstream[1]}"
        upstream.sendall(request_head(self.requestline, self.headers, host))
        back = threading.Thread(target=pass_bytes, args=(upstream, self.connection), daemon=True)
        back.start()
        pass_bytes(self.connection, upstream)
        back.join()
        upstream.close()
        self.close_connection = True

    def log_message(self, format, *args):
        sys.stderr.write("serve: " + (format % args) + "\n")


def main():
    global extra_connect, game_upstream
    Handler.extensions_map[".wasm"] = "application/wasm"
    Handler.extensions_map[".js"] = "text/javascript"
    folder, port = sys.argv[1], int(sys.argv[2])
    options = sys.argv[3:]
    tls = None
    while options:
        name = options.pop(0)
        if name == "--connect" and options:
            origin = options.pop(0)
            if not origin.startswith(("wss://127.0.0.1:", "ws://127.0.0.1:")):
                sys.exit("serve: --connect takes an origin on 127.0.0.1 only")
            extra_connect = " " + origin
        elif name == "--tls" and len(options) >= 2:
            tls = (options.pop(0), options.pop(0))
        elif name == "--game-proxy" and options:
            game_upstream = upstream_of(options.pop(0))
            if game_upstream is None:
                sys.exit("serve: --game-proxy takes a wss:// origin on 127.0.0.1 only")
        else:
            sys.exit(f"serve: unknown option {name}")
    if game_upstream is not None and tls is None:
        sys.exit("serve: --game-proxy needs --tls: the page opens a game socket only from an HTTPS page")
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=folder))
    scheme = "http"
    if tls is not None:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(*tls)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        scheme = "https"
    print(f"serve: pid {os.getpid()} on {scheme}://127.0.0.1:{port} for {folder}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
