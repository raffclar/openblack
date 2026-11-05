"""The map download check: every player gets the land of the game from the lobby, keeps it, and plays it in step.

The script runs on this computer only, with test accounts and test certificates in the output folder:
1. A local bwgame-service (its own SQLite data, uvicorn and the push process) behind lobby_front.py, a TLS front on
   127.0.0.1 as Caddy is in front of the real service.
2. The land of the map download test (openblack-server --write-download-test-map): a land of a network game that no
   game folder has.
3. openblack-server with --custom-map: it plays the land and sends it to the lobby (PUT /api/v1/servers/{id}/map).
4. Desktop clients, each in a new working folder, so with an empty "Online Maps" folder in the developer profile.
   The script drives the multiplayer front end of each through the inspector: the Internet, the log-in, the first
   game, Join, Next, Ready.
   Each client gets the land when its ticket comes, checks its start checksum and its SHA-256, and keeps it.
5. Optionally a web client in headless Chrome (--web-dir), through web/scripts/cdp-run.mjs.
6. The checks: each client keeps a file with the SHA-256 of the lobby's list, the game starts, and each client
   compares its turns with the server's. Then a second game: the server starts again, the desktop clients join with
   their kept files, and the lobby gives the file to nobody (no new GET of the map in the log of the front).

The passwords of the test accounts are random, and the script never prints them. cdp-run.mjs writes each typed key in
its logs, so the script deletes console.log and the actions file of a web run.

    python tools/sim/map_download_check.py --service E:/openblack/worktrees/<bwgame clone> \\
        --service-python <venv>/Scripts/python.exe --bin E:/openblack/builds/<lane>/bin/RelWithDebInfo --out <folder>
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import secrets
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
INSPECTOR = os.path.join(ROOT, "tools", "inspector", "openblack_inspector_mcp.py")
CDP = os.path.join(ROOT, "web", "scripts", "cdp-run.mjs")
CHROME = "C:/Program Files/Google/Chrome/Application/chrome.exe"
OPENSSL = "C:/Program Files/Git/mingw64/bin/openssl.exe" if os.path.exists(
    "C:/Program Files/Git/mingw64/bin/openssl.exe") else "openssl"
MAP_FILE = "obt_dl_3p_1"
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)
SESSION_COUNTS = re.compile(r"(\d+) turns compared with the server, (\d+) the same")
# The places of the buttons of the front end on a 1280x1024 window, for the pages where the inspector refuses gui.press
PIXELS = {"Next": (1200, 912), "Ready": (122, 786)}


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def run(args, env=None, cwd=None):
    result = subprocess.run(args, env=env, cwd=cwd, capture_output=True, text=True, creationflags=NO_WINDOW)
    if result.returncode != 0:
        raise SystemExit(f"failed: {os.path.basename(args[0])} {args[1] if len(args) > 1 else ''}\n{result.stderr[-2000:]}")
    return result.stdout


class Processes:
    """The processes that the script started. Each one stops by its own PID, never by its name."""

    def __init__(self, out):
        self.items = []
        self.file = os.path.join(out, "pids.txt")

    def start(self, name, args, **kwargs):
        process = subprocess.Popen(args, creationflags=NO_WINDOW, **kwargs)
        self.items.append((name, process))
        with open(self.file, "a", encoding="utf-8") as f:
            f.write(f"{name} {process.pid}\n")
        return process

    def stop(self, name=None):
        for item_name, process in list(self.items):
            if name is not None and item_name != name:
                continue
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(15)
                except subprocess.TimeoutExpired:
                    process.kill()
            self.items.remove((item_name, process))


def make_certificates(certs):
    """A CA for the site on 127.0.0.1, a CA of the game servers and the client certificate of the test server"""
    def c(name):
        return os.path.join(certs, name)

    def write(name, text):
        with open(c(name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)

    if os.path.exists(c("site.pem")):
        return
    os.makedirs(certs, exist_ok=True)
    run([OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", c("ca.key")])
    run([OPENSSL, "req", "-x509", "-new", "-key", c("ca.key"), "-sha256", "-days", "30", "-subj", "/CN=lobby test CA",
         "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign",
         "-out", c("ca.pem")])
    run([OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", c("site.key")])
    run([OPENSSL, "req", "-new", "-key", c("site.key"), "-subj", "/CN=127.0.0.1", "-out", c("site.csr")])
    write("site.ext", "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n"
          "extendedKeyUsage=serverAuth\nsubjectAltName=IP:127.0.0.1,DNS:localhost\n")
    run([OPENSSL, "x509", "-req", "-in", c("site.csr"), "-CA", c("ca.pem"), "-CAkey", c("ca.key"), "-CAcreateserial",
         "-days", "30", "-sha256", "-extfile", c("site.ext"), "-out", c("site.pem")])
    run([OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", c("gs-ca.key")])
    run([OPENSSL, "req", "-x509", "-new", "-key", c("gs-ca.key"), "-sha256", "-days", "30", "-subj",
         "/CN=game servers test CA", "-addext", "basicConstraints=critical,CA:TRUE,pathlen:0",
         "-addext", "keyUsage=critical,keyCertSign,cRLSign", "-out", c("gs-ca.pem")])
    run([OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", c("client.key")])
    run([OPENSSL, "req", "-new", "-key", c("client.key"), "-subj", "/CN=127.0.0.1", "-out", c("client.csr")])
    write("client.ext", "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n"
          "extendedKeyUsage=clientAuth\nsubjectAltName=IP:127.0.0.1\n")
    run([OPENSSL, "x509", "-req", "-in", c("client.csr"), "-CA", c("gs-ca.pem"), "-CAkey", c("gs-ca.key"),
         "-CAcreateserial", "-days", "30", "-sha256", "-extfile", c("client.ext"), "-out", c("client.pem")])
    run([OPENSSL, "genpkey", "-algorithm", "ed25519", "-out", c("signing.key")])
    run([OPENSSL, "pkey", "-in", c("signing.key"), "-pubout", "-out", c("signing.pub")])


def site_spki(certs):
    """The base64 SHA-256 of the key of the site, which Chrome trusts for the test (--trust-spki)"""
    command = (f'"{OPENSSL}" x509 -in "{os.path.join(certs, "site.pem")}" -pubkey -noout | "{OPENSSL}" pkey -pubin '
               f'-outform der | "{OPENSSL}" dgst -sha256 -binary | "{OPENSSL}" enc -base64')
    return subprocess.run(command, shell=True, capture_output=True, text=True).stdout.strip()


def setup_service(options, out, front_port, names):
    """The data of the local service: migrations, the ticket key, the accounts and the game server"""
    certs = os.path.join(out, "certs")
    make_certificates(certs)
    state_path = os.path.join(out, "service.json")
    state = json.load(open(state_path, encoding="utf-8")) if os.path.exists(state_path) else {
        "proxy_secret": secrets.token_hex(24), "push_secret": secrets.token_hex(24)}
    data = os.path.join(out, "svc")
    os.makedirs(data, exist_ok=True)
    env = dict(os.environ)
    env.update({"DJANGO_DEBUG": "1", "BW_DATA_DIR": data, "BW_EMAIL_DNS_CHECK": "0", "BW_RATE_LIMITS_ENABLED": "0",
                "BW_PROXY_SECRET": state["proxy_secret"], "BW_PUSH_SECRET": state["push_secret"],
                "BW_EMAIL_BACKEND": "locmem", "DJANGO_ALLOWED_HOSTS": "127.0.0.1,localhost", "PYTHONIOENCODING": "utf-8",
                "BW_PLAY_BETA_ONLY": "0", "BW_SERVER_HOST": "", "BW_SITE_DOMAIN": ""})
    py = options.service_python
    run([py, "manage.py", "migrate", "--noinput"], env=env, cwd=options.service)
    if "ticket_key" not in state:
        for line in run([py, "manage.py", "make_ticket_key"], env=env, cwd=options.service).splitlines():
            if line.startswith("BW_TICKET_KEY="):
                state["ticket_key"] = line.split("=", 1)[1].strip()
    env["BW_TICKET_KEY"] = state["ticket_key"]
    accounts_path = os.path.join(out, "accounts.json")
    accounts = json.load(open(accounts_path, encoding="utf-8")) if os.path.exists(accounts_path) else {}
    for name in names:
        accounts.setdefault(name, {"password": secrets.token_urlsafe(12)[:16], "email": f"{name.lower()}@example.org"})
    with open(accounts_path, "w", encoding="utf-8") as f:
        json.dump(accounts, f)
    script = os.path.join(out, "make_accounts.py")
    with open(script, "w", encoding="utf-8") as f:
        f.write("from accounts.models import Player\nimport json,sys\n"
                "a=json.load(open(sys.argv[2],encoding='utf-8'))\n"
                "for n,v in a.items():\n"
                "    p=Player.objects.filter(username=n).first() or Player(username=n)\n"
                "    p.email=v['email']; p.set_password(v['password']); p.save()\n")
    run([py, "-c", "import django,os,sys;os.environ.setdefault('DJANGO_SETTINGS_MODULE','bwsite.settings');"
         "django.setup();exec(open(sys.argv[1]).read())", script, accounts_path], env=env, cwd=options.service)
    if "server_id" not in state:
        added = run([py, "manage.py", "game_server", "add", "--name", "Map download test", "--host",
                     f"127.0.0.1:{front_port}", "--cert", os.path.join(certs, "client.pem"), "--signing-key",
                     os.path.join(certs, "signing.pub")], env=env, cwd=options.service)
        for line in added.splitlines():
            if line.startswith("added "):
                state["server_id"] = line.split()[1]
    state["spki"] = site_spki(certs)
    with open(state_path, "w", encoding="utf-8") as f:
        json.dump(state, f)
    return env, state, accounts


def start_stack(options, out, env, state, ports, processes):
    py = options.service_python
    stack_env = dict(env)
    stack_env.update({"BW_PUSH_INTERNAL": f"127.0.0.1:{ports['internal']}",
                      "BW_PUSH_INTERNAL_BIND": f"127.0.0.1:{ports['internal']}",
                      "BW_PUSH_EXTRA_ORIGINS": f"https://127.0.0.1:{ports['front']}"})
    processes.start("django", [py, "-m", "uvicorn", "bwsite.asgi:application", "--host", "127.0.0.1", "--port",
                               str(ports["django"]), "--workers", "1"], cwd=options.service, env=stack_env,
                    stdout=open(os.path.join(out, "django.log"), "w"), stderr=subprocess.STDOUT)
    processes.start("push", [py, "manage.py", "run_push", "--bind", f"127.0.0.1:{ports['push']}", "--internal",
                             f"127.0.0.1:{ports['internal']}"], cwd=options.service, env=stack_env,
                    stdout=open(os.path.join(out, "push.log"), "w"), stderr=subprocess.STDOUT)
    certs = os.path.join(out, "certs")
    processes.start("front", [sys.executable, "-I", os.path.join(HERE, "lobby_front.py"), "--port", str(ports["front"]),
                              "--cert", os.path.join(certs, "site.pem"), "--key", os.path.join(certs, "site.key"),
                              "--client-ca", os.path.join(certs, "gs-ca.pem"), "--secret", state["proxy_secret"],
                              "--django", str(ports["django"]), "--push", str(ports["push"]), "--game-port-file",
                              os.path.join(out, "game_port.txt"), "--play-dir", options.web_dir or "", "--log",
                              os.path.join(out, "front.log")],
                    stdout=subprocess.DEVNULL, stderr=open(os.path.join(out, "front.err"), "w"))
    time.sleep(8)


def start_server(options, out, state, ports, map_path, gods, processes, name):
    certs = os.path.join(out, "certs")
    log = os.path.join(out, f"{name}.log")
    port_file = os.path.join(out, "game_port.txt")
    if os.path.exists(port_file):
        os.remove(port_file)
    work = os.path.join(out, "server-work")
    os.makedirs(work, exist_ok=True)
    lobby = f"https://127.0.0.1:{ports['front']}"
    server = processes.start("server", [
        os.path.join(options.bin, "openblack-server.exe"), f"--game-path={options.game_path}", "--listen=127.0.0.1:0",
        f"--server-id={state['server_id']}", f"--ticket-keys={lobby}/api/v1/ticket-keys", f"--lobby-api={lobby}/api/v1",
        f"--client-cert={os.path.join(certs, 'client.pem')}", f"--client-key={os.path.join(certs, 'client.key')}",
        "--room-name=Map download test", f"--gods={gods}", f"--custom-map={map_path}",
        f"--profile-folder={os.path.join(work, 'profile')}", "--log-file=stdout",
        f"--lobby-test-ca={os.path.join(certs, 'ca.pem')}"], cwd=work, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    deadline = time.time() + 240
    while time.time() < deadline:
        if server.poll() is not None:
            raise SystemExit(f"the server stopped with {server.returncode}; see {log}")
        found = re.search(r"listens on 127\.0\.0\.1:(\d+)", open(log, encoding="utf-8", errors="replace").read())
        if found:
            with open(port_file, "w", encoding="utf-8") as f:
                f.write(found.group(1))
            return log
        time.sleep(1)
    raise SystemExit("the server did not listen in time")


def start_desktop(options, out, name, ports, processes, frames):
    work = os.path.join(out, name)
    os.makedirs(work, exist_ok=True)
    log = os.path.join(work, "game.log")
    if os.path.exists(log):
        os.remove(log)
    env = dict(os.environ)
    env["ALSOFT_CONF"] = options.alsoft_conf
    process = processes.start(name, [
        os.path.join(options.bin, "openblack.exe"), "-g", options.game_path, "--background", "--inspect-port", "0",
        "--skip-logos", "-n", str(frames), "-MULTIPLAYER",
        "--lobby", f"https://127.0.0.1:{ports['front']}", "--lobby-test-ca", os.path.join(out, "certs", "ca.pem"),
        "-l", log], cwd=work, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return process, log


class Driver:
    """Drives the multiplayer front end of one desktop game through its inspector"""

    def __init__(self, pid, account, password):
        self.pid, self.account, self.password = pid, account, password
        self.pages = []

    def call(self, query, params=None):
        args = [sys.executable, INSPECTOR, "--pid", str(self.pid), "--wait-ms", "180000", "--timeout", "30", "--call",
                query]
        if params is not None:
            args.append(json.dumps(params))
        out = subprocess.run(args, capture_output=True, text=True, creationflags=NO_WINDOW).stdout
        try:
            return json.loads(out)
        except json.JSONDecodeError:
            return {"raw": out[-500:]}

    def window(self):
        result = self.call("gui.windows").get("result", {})
        items = result.get("items", []) if isinstance(result, dict) else result
        for w in items or []:
            if w.get("name") == "multiplayer":
                page = w.get("page")
                if page and (not self.pages or self.pages[-1] != page):
                    self.pages.append(page)
                return w
        return {}

    def wait(self, test, seconds, what):
        end = time.time() + seconds
        w = {}
        while time.time() < end:
            w = self.window()
            if test(w):
                return w
            time.sleep(0.5)
        raise RuntimeError(f"{self.account}: waited for {what}; the box is {w.get('page')} {w.get('lines')}")

    def press(self, button):
        answer = self.call("gui.press", {"window": "multiplayer", "button": button})
        if not answer.get("ok") and button in PIXELS:
            self.click(*PIXELS[button])
            return
        if not answer.get("ok"):
            raise RuntimeError(f"{self.account}: press {button}: {answer.get('error')}")
        time.sleep(1.0)

    def click(self, x, y):
        self.call("input.pointer", {"screen": [x, y]})
        time.sleep(0.5)
        self.call("input.button", {"button": "left"})
        time.sleep(1.0)

    def to_room(self):
        self.wait(lambda w: w.get("page") == "choose_connection", 300, "the first page")
        self.press("Next")
        self.wait(lambda w: w.get("page") == "find_games", 60, "find games")
        self.press("On the Internet")
        self.press("Next")
        self.wait(lambda w: w.get("page") == "login", 60, "the log-in")
        self.call("input.text", {"text": self.account})
        time.sleep(0.5)
        self.call("input.key", {"key": "Tab"})
        time.sleep(0.5)
        self.call("input.text", {"text": self.password})
        time.sleep(0.5)
        self.press("Next")
        self.wait(lambda w: w.get("page") == "game_list" and any("Map download test" in line for line in
                                                                 (w.get("lines") or [])), 120, "the game list")
        self.click(320, 336)
        self.press("Join game")
        self.wait(lambda w: w.get("page") == "room", 60, "the room")

    def ready(self):
        self.press("Next")
        self.wait(lambda w: w.get("page") == "conditions", 30, "the conditions")
        self.press("Ready")

    def started(self, seconds):
        self.wait(lambda w: not w.get("open"), seconds, "the start of the game")


def profile_of(work):
    """A developer's run (-n) plays as the developer profile in the Profiles folder of its working folder"""
    return os.path.join(work, "Profiles", "developer")


def kept_map(profile):
    path = os.path.join(profile, "Online Maps", MAP_FILE + ".map")
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def map_gets(front_log, sha):
    if not os.path.exists(front_log):
        return 0
    with open(front_log, encoding="utf-8", errors="replace") as f:
        return sum(1 for line in f if f"GET /api/v1/maps/{sha}" in line)


def session_counts(log):
    found = None
    if os.path.exists(log):
        with open(log, encoding="utf-8", errors="replace") as f:
            for line in f:
                match = SESSION_COUNTS.search(line)
                if match:
                    found = (int(match.group(1)), int(match.group(2)))
    return found


def wait_for_turns(logs, turns, seconds):
    end = time.time() + seconds
    while time.time() < end:
        counts = [session_counts(log) for log in logs]
        if all(c is not None and c[0] >= turns for c in counts):
            return counts
        time.sleep(5)
    return [session_counts(log) for log in logs]


def web_profile(options, out, state, ports):
    """A first web run makes the profile of the player in the private storage of the site (Chrome keeps it)"""
    web_out = os.path.join(out, "web")
    os.makedirs(web_out, exist_ok=True)
    actions = os.path.join(web_out, "actions.json")
    with open(actions, "w", encoding="utf-8") as f:
        json.dump([{"at": 0.5, "do": "wait-log", "text": "Load startup:", "seconds": 600},
                   {"at": 15.5, "do": "shot", "name": "profile"}], f)
    url = f"https://127.0.0.1:{ports['front']}/play/index.html?renderer=webgl2&args=--skip-logos"
    subprocess.run(["node", CDP, CHROME, url, "25", web_out, "--game-folder", options.game_path, "--click", "#play",
                    "--keep-profile", "--trust-spki", state["spki"], "--actions", actions],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=NO_WINDOW, timeout=1200)


def web_client(options, out, state, ports, account, seconds, processes):
    """The web client: the front end in headless Chrome through cdp-run.mjs, as a player clicks it"""
    web_out = os.path.join(out, "web")
    os.makedirs(web_out, exist_ok=True)
    steps, at, pointer = [], 0.0, [640, 180]
    scale = 0.8

    def step(delay, action):
        nonlocal at
        at += delay
        steps.append({"at": at, **action})

    def click(x, y):
        nonlocal at
        count = 10
        step(0.3, {"do": "synth-move", "dx": (x - pointer[0]) / scale / count, "dy": (y - pointer[1]) / scale / count,
                   "count": count, "ms": 400})
        pointer[:] = [x, y]
        at += 0.5
        step(0.3, {"do": "synth-button", "type": "mousedown", "button": 0})
        step(0.2, {"do": "synth-button", "type": "mouseup", "button": 0})
        at += 1

    def type_text(text):
        for c in text:
            upper = c.upper()
            code = f"Digit{c}" if c.isdigit() else f"Key{upper}"
            step(0.25, {"do": "key", "key": c, "code": code, "keyCode": ord(upper), "hold": 60})

    step(0.5, {"do": "wait-log", "text": "The -MULTIPLAYER switch: the multiplayer front end opens", "seconds": 600})
    step(4, {"do": "mouse", "type": "mouseMoved", "x": 640, "y": 180})
    step(0.3, {"do": "mouse", "type": "mousePressed", "x": 640, "y": 180, "button": "left"})
    step(0.2, {"do": "mouse", "type": "mouseReleased", "x": 640, "y": 180, "button": "left"})
    at += 1
    click(1200, 912)
    click(448, 656)
    click(1200, 912)
    click(704, 464)
    type_text(account["name"])
    click(704, 656)
    type_text(account["password"])
    click(1200, 912)
    step(6, {"do": "shot", "name": "list"})
    click(320, 336)
    click(1200, 912)
    step(0.5, {"do": "wait-log", "text": "Joined the game server as god", "seconds": 120})
    step(2, {"do": "shot", "name": "room"})
    click(1200, 912)
    click(122, 786)
    step(0.5, {"do": "wait-log", "text": "The land of the network game loaded", "seconds": 900})
    step(5, {"do": "shot", "name": "playing"})
    step(0.5, {"do": "wait-log", "text": "100 turns compared", "seconds": 900})
    actions = os.path.join(web_out, "actions.json")
    with open(actions, "w", encoding="utf-8") as f:
        json.dump(steps, f)
    url = f"https://127.0.0.1:{ports['front']}/play/index.html?renderer=webgl2&args=" + "--skip-logos%20-MULTIPLAYER"
    return processes.start("web", ["node", CDP, CHROME, url, str(int(at + seconds)), web_out, "--game-folder",
                                   options.game_path, "--click", "#play", "--keep-profile", "--trust-spki",
                                   state["spki"], "--actions", actions],
                           stdout=open(os.path.join(web_out, "cdp.out"), "w"), stderr=subprocess.STDOUT)


def web_lines(out, game):
    """The lines of the game log of the web run that the check needs. The next run of cdp-run.mjs writes over the log."""
    path = os.path.join(out, "web", "page-log.txt")
    keep = ("has the land of the game", "turns compared with the server", "starts the game on",
            "Could not get", "Joined the game server")
    lines = []
    if os.path.exists(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            lines = [line.rstrip() for line in f if any(k in line for k in keep)]
    with open(os.path.join(out, f"web-{game}.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    counts = None
    for line in lines:
        match = SESSION_COUNTS.search(line)
        if match:
            counts = (int(match.group(1)), int(match.group(2)))
    return {"has_land": any("has the land of the game" in line for line in lines), "counts": counts}


def stop_chrome(out):
    """Stops the Chrome of the web run of this script, found by its own profile folder, when cdp-run.mjs did not"""
    profile = os.path.join(out, "web", "chrome-profile")
    if os.name != "nt" or not os.path.isdir(profile):
        return
    query = ("Get-CimInstance Win32_Process -Filter \"Name='chrome.exe'\" | Where-Object { $_.CommandLine -like '*"
             + profile.replace("'", "''") + "*' -and $_.CommandLine -notlike '*--type=*' } | "
             "ForEach-Object { $_.ProcessId }")
    found = subprocess.run(["powershell", "-NoProfile", "-Command", query], capture_output=True, text=True,
                           creationflags=NO_WINDOW).stdout.split()
    for pid in found:
        subprocess.run(["taskkill", "/PID", pid, "/T", "/F"], capture_output=True, creationflags=NO_WINDOW)


def scrub_web(out):
    """cdp-run.mjs writes each typed key in its logs: the password of the test account goes"""
    web_out = os.path.join(out, "web")
    for name in ("actions.json", "console.log", "cdp.out"):
        path = os.path.join(web_out, name)
        if os.path.exists(path):
            os.remove(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--service", required=True, help="a clone of bwgame-service")
    parser.add_argument("--service-python", required=True, help="the python of the service's environment")
    parser.add_argument("--bin", required=True, help="the folder of openblack.exe and openblack-server.exe")
    parser.add_argument("--game-path", default="C:/projects/black")
    parser.add_argument("--out", required=True)
    parser.add_argument("--clients", type=int, default=2, help="the desktop clients")
    parser.add_argument("--web-dir", help="the folder of a web build: one more client in headless Chrome")
    parser.add_argument("--turns", type=int, default=300, help="the turns that each client compares with the server")
    parser.add_argument("--alsoft-conf", default="E:/openblack/tools/audio/alsoft_silent.ini")
    parser.add_argument("--no-second-game", action="store_true", help="leave out the second game with the kept files")
    options = parser.parse_args()
    out = os.path.abspath(options.out)
    os.makedirs(out, exist_ok=True)
    names = [f"Desk{n + 1}" for n in range(options.clients)] + (["WebGod"] if options.web_dir else [])
    ports = {"front": free_port(), "django": free_port(), "push": free_port(), "internal": free_port()}
    env, state, accounts = setup_service(options, out, ports["front"], names)
    processes = Processes(out)
    report = {"ok": False, "clients": {}}
    try:
        start_stack(options, out, env, state, ports, processes)
        maps = os.path.join(out, "maps")
        os.makedirs(maps, exist_ok=True)
        map_path = os.path.join(maps, MAP_FILE + ".map")
        run([os.path.join(options.bin, "openblack-server.exe"), f"--game-path={options.game_path}",
             f"--write-download-test-map={map_path}"])
        with open(map_path, "rb") as f:
            sha = hashlib.sha256(f.read()).hexdigest()
        report["map"] = {"file": MAP_FILE, "sha256": sha, "size": os.path.getsize(map_path)}
        frames = 20000000
        for game in ("first", "second"):
            if game == "second" and options.no_second_game:
                break
            gets_before = map_gets(os.path.join(out, "front.log"), sha)
            gods = len(names)
            start_server(options, out, state, ports, map_path, gods, processes, f"server-{game}")
            desktops = []
            for name in names:
                if name == "WebGod":
                    continue
                process, log = start_desktop(options, out, name, ports, processes, frames)
                desktops.append((name, process, log))
            web = None
            if options.web_dir:
                if game == "first":
                    web_profile(options, out, state, ports)
                web = web_client(options, out, state, ports, {"name": "WebGod", **accounts["WebGod"]}, 30,
                                 processes)
            drivers = [Driver(p.pid, n, accounts[n]["password"]) for n, p, _ in desktops]
            for driver in drivers:
                driver.to_room()
            for driver in drivers:
                driver.ready()
            for driver in drivers:
                driver.started(900)
            logs = [log for _, _, log in desktops]
            counts = wait_for_turns(logs, options.turns, 1800)
            result = {"map_gets": map_gets(os.path.join(out, "front.log"), sha) - gets_before, "clients": {}}
            for (name, _, log), count, driver in zip(desktops, counts, drivers):
                result["clients"][name] = {"kept": kept_map(profile_of(os.path.join(out, name))) == sha,
                                           "counts": count, "pages": driver.pages}
            if web is not None:
                try:
                    web.wait(2400)
                except subprocess.TimeoutExpired:
                    pass
                processes.stop("web")
                stop_chrome(out)
                result["web"] = {"exit": web.returncode, **web_lines(out, game)}
                scrub_web(out)
            report[game] = result
            processes.stop("server")
            for name, _, _ in desktops:
                processes.stop(name)
        first = report.get("first", {})
        second = report.get("second")
        web_ok = all(g is None or "web" not in g or (g["web"]["has_land"] and g["web"]["counts"] and
                                                     g["web"]["counts"][0] == g["web"]["counts"][1])
                     for g in (first, second))
        ok_first = all(c["kept"] and c["counts"] and c["counts"][0] >= options.turns and c["counts"][0] == c["counts"][1]
                       for c in first.get("clients", {}).values()) and first.get("map_gets", 0) >= len(names) and web_ok
        ok_second = second is None or (second["map_gets"] == 0 and all(
            c["counts"] and c["counts"][0] == c["counts"][1] for c in second["clients"].values()))
        report["ok"] = bool(ok_first and ok_second)
    finally:
        processes.stop()
        if options.web_dir:
            stop_chrome(out)
            web_lines(out, "last")
            scrub_web(out)
        with open(os.path.join(out, "report.json"), "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1)
        print(json.dumps(report, indent=1))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
