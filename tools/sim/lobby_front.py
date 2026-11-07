"""A test TLS front for a local bwgame-service, as Caddy is in front of the real one.

It listens with HTTPS on 127.0.0.1, asks for an optional client certificate (the game servers' mutual TLS), and sends
each request on as plain HTTP:
  /game/...          -> the game server's plain WebSocket port (the game socket)
  /api/v1/events     -> the push process
  /play/...          -> files of the web build, with the headers that the game threads need
  everything else    -> the Django process (uvicorn)
It adds the proxy secret and, after a verified client certificate, the certificate's SHA-256, as Caddy does. For tests
on this computer only.
"""
import argparse
import asyncio
import hashlib
import mimetypes
import os
import ssl
import sys

HOP = (b"x-bwgame-proxy", b"x-bwgame-client-cert-sha256", b"x-bwgame-tls", b"x-forwarded-for", b"x-forwarded-proto")


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, required=True)
    p.add_argument("--cert", required=True)
    p.add_argument("--key", required=True)
    p.add_argument("--client-ca", required=True)
    p.add_argument("--secret", required=True)
    p.add_argument("--django", type=int, required=True)
    p.add_argument("--push", type=int, default=0)
    p.add_argument("--game-port-file", required=True, help="a file that holds the game server's port")
    p.add_argument("--play-dir", default="")
    p.add_argument("--log", required=True)
    return p.parse_args()


ARGS = parse_args()
LOG = open(ARGS.log, "a", buffering=1, encoding="utf-8")


def log(text):
    LOG.write(text + "\n")


async def pipe(reader, writer):
    try:
        while True:
            data = await reader.read(65536)
            if not data:
                break
            writer.write(data)
            await writer.drain()
    except (ConnectionError, OSError):
        pass
    finally:
        try:
            writer.close()
        except Exception:
            pass


def game_port():
    try:
        with open(ARGS.game_port_file, encoding="utf-8") as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return 0


async def serve_play(path, writer):
    rel = path.split("?", 1)[0][len("/play/"):] or "index.html"
    full = os.path.normpath(os.path.join(ARGS.play_dir, rel))
    if not full.startswith(os.path.normpath(ARGS.play_dir)) or not os.path.isfile(full):
        writer.write(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
        await writer.drain()
        writer.close()
        return
    body = open(full, "rb").read()
    if full.endswith(".wasm"):
        kind = "application/wasm"
    elif full.endswith(".js"):
        kind = "text/javascript; charset=utf-8"
    else:
        kind = mimetypes.guess_type(full)[0] or "application/octet-stream"
    # The same headers as the live Caddyfile (play) block
    csp = ("default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; connect-src 'self' ; img-src 'self' data: blob:; "
           "style-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")
    head = (
        "HTTP/1.1 200 OK\r\n"
        f"Content-Type: {kind}\r\nContent-Length: {len(body)}\r\n"
        "Cross-Origin-Opener-Policy: same-origin\r\nCross-Origin-Embedder-Policy: require-corp\r\n"
        "Cross-Origin-Resource-Policy: same-origin\r\n"
        + ("" if os.environ.get("BW_NO_CSP") else f"Content-Security-Policy: {csp}\r\n") +
        "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n"
        "Strict-Transport-Security: max-age=63072000; includeSubDomains\r\n"
        "Permissions-Policy: camera=(), microphone=(), geolocation=(), payment=(), usb=(), serial=(), hid=()\r\n"
        "Cache-Control: no-cache\r\nConnection: close\r\n\r\n"
    )
    writer.write(head.encode() + body)
    await writer.drain()
    writer.close()


async def handle(reader, writer):
    peer = writer.get_extra_info("peername")
    cert = writer.get_extra_info("ssl_object").getpeercert(binary_form=True)
    try:
        head = await reader.readuntil(b"\r\n\r\n")
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError, OSError):
        writer.close()
        return
    lines = head.split(b"\r\n")
    request = lines[0]
    try:
        method, path, version = request.decode("latin-1").split(" ", 2)
    except ValueError:
        writer.close()
        return
    headers = []
    upgrade = any(line.partition(b":")[0].strip().lower() == b"upgrade" and b"websocket" in line.lower()
                  for line in lines[1:])
    for line in lines[1:]:
        if not line:
            continue
        name, _, value = line.partition(b":")
        lower = name.strip().lower()
        if lower in HOP:
            continue
        if lower == b"connection" and not upgrade:
            continue
        headers.append(line)
    if path.startswith("/play/") and ARGS.play_dir:
        log(f"{peer} {method} {path.split('?', 1)[0]} (play)")
        await serve_play(path, writer)
        return
    if path.startswith("/game/"):
        port = game_port()
        target = "game"
    elif path.startswith("/api/v1/events") and ARGS.push:
        port = ARGS.push
        target = "push"
    else:
        port = ARGS.django
        target = "django"
    # The query can hold no token of the game, but cut it from the log anyway
    log(f"{peer} {method} {path.split('?', 1)[0]} -> {target} cert={'yes' if cert else 'no'} upgrade={upgrade}")
    if port == 0:
        writer.write(b"HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
        await writer.drain()
        writer.close()
        return
    if target != "game":
        headers.append(b"X-Bwgame-Proxy: " + ARGS.secret.encode())
        headers.append(b"X-Forwarded-For: " + peer[0].encode())
        headers.append(b"X-Forwarded-Proto: https")
        if cert:
            headers.append(b"X-Bwgame-Client-Cert-Sha256: " + hashlib.sha256(cert).hexdigest().encode())
    if not upgrade:
        headers.append(b"Connection: close")
    try:
        up_reader, up_writer = await asyncio.open_connection("127.0.0.1", port)
    except OSError:
        writer.write(b"HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
        await writer.drain()
        writer.close()
        return
    up_writer.write(request + b"\r\n" + b"\r\n".join(headers) + b"\r\n\r\n")
    await up_writer.drain()
    await asyncio.gather(pipe(reader, up_writer), pipe(up_reader, writer))


async def main():
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_3
    context.load_cert_chain(ARGS.cert, ARGS.key)
    context.verify_mode = ssl.CERT_OPTIONAL
    context.load_verify_locations(ARGS.client_ca)
    server = await asyncio.start_server(handle, "127.0.0.1", ARGS.port, ssl=context)
    log(f"listening on 127.0.0.1:{ARGS.port}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit(0)
