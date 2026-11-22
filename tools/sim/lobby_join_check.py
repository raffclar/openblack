#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The lobby join check: a web player joins a lobby game through the site's /play/ page, as on the live site.

The live web client gets its lobby session from the sign-in of the site, can have the editor right, and joins the game
server of a pool slot from the lobby room. net_check.py starts the web client with --join and a ticket, so it does not
go through that path. This script runs the whole path on this computer, with test data only:

1. A local bwgame-service (its own SQLite data) with the processes of the live site: uvicorn (Django), the push
   process and the jobs process (the timed steps of the lobbies), behind lobby_front.py, a TLS front on 127.0.0.1 as
   Caddy is. The lobby uses the pool of slots (lobbies.pool.SlotPool) with one slot.
2. The slot agent of the service clone (slot_agent_local.py): it starts openblack-server for the lobby game with the
   options of the lobby, as the slot container of the live site does. With --trace-draws the server writes its draw
   trace.
3. A test account, with the editor right unless --no-editor. The script signs it in through the site's form and gives
   the session cookie to headless Chrome (cdp-run.mjs --cookie-file).
4. The account makes the lobby through the API: the land (default mpm_4p_1), two open god slots and computer gods in
   the other slots. The list of the game shows a lobby with no free slot as full and refuses it, so the second open
   slot stays until the page joined. Then the account gives that slot to a computer god.
5. Headless Chrome opens /play/?multiplayer with the game in a frame. The script clicks through the front end of the
   game (connections, the Internet, the lobby list) and joins the lobby, which the list gives as the lobby to join
   again. When the page asked for the lobby, the account starts the game through the API.
6. In the game, the web player takes a thing near the temple at about turn 99 and throws it at about turn 125, as the
   player of the live desync did. The run ends after --turns turns of the web client.
7. The checks: the server's log has no part of the world of the slot, and with --trace-draws the draw trace of the web
   client (--sim-trace-draws -, a developer build only) is the same as the server's (draw_trace.py).

Optional --from-land (a developer build): the web player pushes Play, plays a land script first (for example
Land4.txt) and then pushes the page's Join Online Game button over the land, as the live player did after the game
loaded the save of Land 4.

A shipping web build takes no developer arguments, so only the checksums of the server compare it. Optional
--save-first: a first web run with the same Chrome profile plays a land (-s <script>) and the game keeps its save on
exit, so the network game starts in a program that loaded a saved land first, as the live client did.

The password of the test account is new and random in each run. The script never prints it or writes it to a file.
cdp-run.mjs writes each typed key in its logs. This script types no password (the site session signs the game in), and
it deletes the cookie file after each run.

    python tools/sim/lobby_join_check.py --service E:/openblack/worktrees/<bwgame clone> \\
        --service-python <venv>/Scripts/python.exe --bin E:/openblack/builds/<lane>/bin/RelWithDebInfo \\
        --web-dir <web page folder> --out E:/openblack/sim-runs/<lane>/<run> --trace-draws
"""

from __future__ import annotations

import argparse
import http.cookiejar
import json
import os
import re
import secrets
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import draw_trace  # noqa: E402
from map_download_check import CDP, CHROME, NO_WINDOW, Processes, free_port, make_certificates, run, site_spki  # noqa: E402

ACCOUNT = "LobbyJoinWeb"
# The window of headless Chrome in CSS pixels, and the height of the site's header bar over the game's frame
WINDOW = (1280, 1121)
FRAME_TOP = 97
# The game draws its front end at 1280x1024; the page moves the game's pointer 1.25 pixels for each pixel of a page
# movement under the lock
POINTER_SCALE = 0.8
LOCK_AT = (640, 512)
# Trees near the temple of slot 0 on mpm_4p_1 in the view of the start (game pixels of a 1280x1024 frame)
TREE_POINTS = [(215, 372), (240, 360), (190, 380), (690, 178), (735, 192), (600, 172)]
# The first line of the lobby list in the front end
LOBBY_LINE = (320, 336)
SERVER_PARTS = re.compile(r"parts from the server's world at turn (\d+)")
SERVER_COUNTS = re.compile(r"Slot (\d+): (\d+) checksums compared, (\d+) the same")


class Steps:
    """The input of the web player for cdp-run.mjs --actions, in game pixels of the frame"""

    def __init__(self):
        self.items = []
        self.at = 0.0
        self.pointer = list(LOCK_AT)

    def step(self, delay, action):
        self.at += delay
        self.items.append({"at": round(self.at, 2), **action})

    def wait(self, seconds):
        self.at += seconds

    def wait_log(self, text, seconds=600):
        self.step(0.5, {"do": "wait-log", "text": text, "seconds": seconds})

    def shot(self, name):
        self.step(0.5, {"do": "shot", "name": name})

    def lock(self):
        x, y = LOCK_AT
        self.step(0.5, {"do": "mouse", "type": "mouseMoved", "x": x, "y": y + FRAME_TOP})
        self.step(0.3, {"do": "mouse", "type": "mousePressed", "x": x, "y": y + FRAME_TOP, "button": "left"})
        self.step(0.2, {"do": "mouse", "type": "mouseReleased", "x": x, "y": y + FRAME_TOP, "button": "left"})
        self.pointer = list(LOCK_AT)
        self.wait(1.0)

    def move(self, x, y, count=10, ms=400, buttons=0):
        dx = (x - self.pointer[0]) / POINTER_SCALE / count
        dy = (y - self.pointer[1]) / POINTER_SCALE / count
        self.step(0.3, {"do": "synth-move", "dx": dx, "dy": dy, "count": count, "ms": ms, "buttons": buttons})
        self.pointer = [x, y]
        self.wait(ms / 1000.0 + 0.1)

    def button(self, kind, button):
        self.step(0.2, {"do": "synth-button", "type": kind, "button": button})

    def click(self, x, y):
        self.move(x, y)
        self.button("mousedown", 0)
        self.button("mouseup", 0)
        self.wait(1.0)

    def type_text(self, text):
        """Keys of lower-case letters and digits, as a player types them"""
        for c in text:
            upper = c.upper()
            code = f"Digit{c}" if c.isdigit() else f"Key{upper}"
            self.step(0.25, {"do": "key", "key": c, "code": code, "keyCode": ord(upper), "hold": 60})

    def key(self, key, code, key_code):
        self.step(0.4, {"do": "key", "key": key, "code": code, "keyCode": key_code, "hold": 60})


def front_end_steps(steps, new_profile, land_seconds=None):
    """From the start of the page to the lobby room. A new Chrome profile has no player yet: the game asks for the name
    of the new player first. Then the pointer lock, the connections, the Internet, the lobby list (the site session signs
    the game in), the lobby of the account and the join.

    With land_seconds, the game plays a land first (the Play button). After these seconds, the page's Join Online Game
    button opens the multiplayer front end over the land."""
    if new_profile and land_seconds is None:
        steps.wait_log("asking the new player for their name", 900)
        steps.wait(4)
        steps.type_text("lobbyjoin")
        steps.lock()
        steps.wait_log("The page locked the pointer", 240)
        # Return answers the box, as its OK button does. It needs no place on the screen, so it holds at any layout of
        # the box (a fixed pointer place for the OK button missed it), and the log line is the one the game writes
        steps.key("Enter", "Enter", 13)
        steps.wait_log("Made a player profile", 120)
    if land_seconds is None:
        steps.wait_log("The -MULTIPLAYER switch: the multiplayer front end opens", 900)
        steps.wait(4)
        if not new_profile:
            steps.lock()
    else:
        # A land script asks for no player name. The land asks the question of the start: OK starts it normally
        steps.wait(40)
        steps.lock()
        steps.shot("l0-question")
        steps.click(415, 731)
        steps.wait(land_seconds)
        steps.shot("l1-land")
        steps.step(0.5, {"do": "eval", "expression": "document.getElementById('join-online').click(), true"})
        steps.wait_log("Join Online Game of the page: the multiplayer front end opens", 300)
        steps.wait(4)
    steps.shot("f0-connections")
    steps.click(1200, 912)
    steps.click(448, 656)
    steps.click(1200, 912)
    steps.wait_log("The lobby session comes from the sign-in of the site", 120)
    steps.wait(6)
    steps.shot("f1-list")
    # The list has one lobby, the lobby of the account: a click on its line, then the button of the join
    steps.click(LOBBY_LINE[0], LOBBY_LINE[1])
    steps.wait(1)
    steps.shot("f2-picked")
    steps.click(1200, 912)
    steps.wait(4)
    steps.shot("f3-room")


def throw_steps(steps, take_turn, throw_turn, turns_per_second):
    """The deed of the live desync: the hand takes a thing, holds it and throws it. The right button takes: the hand
    holds the thing while the button is down, and a quick movement before the release throws it. The camera of the
    start looks at the temple of the player on mpm_4p_1; the points are trees of the groups on the hill at the left and
    behind the temple. The first throw comes at about the throw turn. If the hand took nothing there, the next points
    give more throws later."""
    steps.wait_log("Each player loaded the world: the turns start", 900)
    steps.shot("g0-start")
    first, *others = TREE_POINTS
    steps.move(*first, count=12, ms=500)
    steps.wait(max(0.0, take_turn / turns_per_second - 2.5))
    steps.button("mousedown", 2)
    # A screenshot takes some seconds in the page, so no screenshot comes between the take and the throw
    steps.wait(max(0.5, (throw_turn - take_turn) / turns_per_second - 0.5))
    flick(steps)
    steps.shot("g2-throw")
    for point in others:
        steps.move(*point, count=12, ms=500)
        steps.button("mousedown", 2)
        steps.wait(2.0)
        flick(steps)
    steps.shot("g2b-throws")


def flick(steps):
    """A slow movement with the thing in the hand, then a quick one up the screen and the release: a throw"""
    x, y = steps.pointer
    steps.move(x, y - 60, count=10, ms=300, buttons=2)
    steps.move(x, max(20, y - 300), count=4, ms=80, buttons=2)
    steps.button("mouseup", 2)
    steps.wait(1.0)


def http_json(method, url, context, body=None, token=None, opener=None, headers=None):
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(url, data=data, method=method)
    request.add_header("Accept", "application/json")
    if data is not None:
        request.add_header("Content-Type", "application/json")
    if token:
        request.add_header("Authorization", "Bearer " + token)
    for name, value in (headers or {}).items():
        request.add_header(name, value)
    try:
        if opener is not None:
            answer = opener.open(request, timeout=30)
        else:
            answer = urllib.request.urlopen(request, timeout=30, context=context)
        raw = answer.read()
        return answer.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as error:
        raw = error.read()
        try:
            return error.code, json.loads(raw)
        except ValueError:
            return error.code, {"raw": raw[:300].decode("utf-8", "replace")}


def service_env(options, out, state):
    data = os.path.join(out, "svc")
    os.makedirs(data, exist_ok=True)
    env = dict(os.environ)
    env.update({"DJANGO_DEBUG": "1", "BW_DATA_DIR": data, "BW_EMAIL_DNS_CHECK": "0", "BW_RATE_LIMITS_ENABLED": "0",
                "BW_PROXY_SECRET": state["proxy_secret"], "BW_PUSH_SECRET": state["push_secret"],
                "BW_EMAIL_BACKEND": "locmem", "DJANGO_ALLOWED_HOSTS": "127.0.0.1,localhost", "PYTHONIOENCODING": "utf-8",
                "BW_PLAY_BETA_ONLY": "0", "BW_SERVER_HOST": "", "BW_SITE_DOMAIN": "",
                "BW_LOBBY_POOL": "lobbies.pool.SlotPool"})
    if "ticket_key" in state:
        env["BW_TICKET_KEY"] = state["ticket_key"]
    if "server_id" in state:
        env["BW_POOL_SLOT_SERVERS"] = state["server_id"]
    return env


def django_shell(options, env, code, *args):
    """Runs Python code with Django set up in the service clone"""
    return run([options.service_python, "-c", "import django,os,sys;os.environ.setdefault('DJANGO_SETTINGS_MODULE',"
                "'bwsite.settings');django.setup();exec(sys.argv[1])", code, *args], env=env, cwd=options.service)


def setup_service(options, out, front_port):
    """The data of the local service: migrations, the ticket key, the test account and the game server of the slot"""
    certs = os.path.join(out, "certs")
    make_certificates(certs)
    state_path = os.path.join(out, "service.json")
    state = json.load(open(state_path, encoding="utf-8")) if os.path.exists(state_path) else {
        "proxy_secret": secrets.token_hex(24), "push_secret": secrets.token_hex(24)}
    env = service_env(options, out, state)
    py = options.service_python
    if not os.path.isfile(os.path.join(options.service, "web", "static_build", "css", "app.css")):
        # The image of the service builds the style sheet of the site; a clone has none, and the game frame of /play/
        # is then a small box
        raise SystemExit("the clone has no web/static_build/css/app.css: copy the style sheet of a built service there")
    run([py, "manage.py", "migrate", "--noinput"], env=env, cwd=options.service)
    if "ticket_key" not in state:
        for line in run([py, "manage.py", "make_ticket_key"], env=env, cwd=options.service).splitlines():
            if line.startswith("BW_TICKET_KEY="):
                state["ticket_key"] = line.split("=", 1)[1].strip()
    # A new random password in each run: it stays in the memory of this script and goes to Django on the standard input
    password = secrets.token_urlsafe(12)[:16]
    env = service_env(options, out, state)
    subprocess.run([options.service_python, "-c", "import django,os,sys;os.environ.setdefault('DJANGO_SETTINGS_MODULE',"
                    "'bwsite.settings');django.setup();exec(sys.argv[1])",
                    "from accounts.models import Player\n"
                    "n=sys.argv[2]; w=sys.stdin.readline().strip()\n"
                    "p=Player.objects.filter(username=n).first() or Player(username=n)\n"
                    "p.email='lobby-join@example.org'; p.set_password(w); p.email_verified=True\n"
                    "p.game_session_editor=sys.argv[3]=='1'\n"
                    # The account chose no error reports, so the consent banner of the site does not cover the game
                    "if hasattr(p, 'consent_error_reports'):\n"
                    "    from web import consent; from django.utils import timezone\n"
                    "    p.consent_error_reports=consent.OFF; p.consent_version=consent.NOTICE_VERSION\n"
                    "    p.consent_at=timezone.now()\n"
                    "p.save()\n",
                    ACCOUNT, "0" if options.no_editor else "1"], input=password + "\n", env=env, cwd=options.service,
                   capture_output=True, text=True, check=True, creationflags=NO_WINDOW)
    if "server_id" not in state:
        added = run([py, "manage.py", "game_server", "add", "--name", "Lobby join slot", "--host",
                     f"127.0.0.1:{front_port}", "--cert", os.path.join(certs, "client.pem"), "--signing-key",
                     os.path.join(certs, "signing.pub")], env=env, cwd=options.service)
        for line in added.splitlines():
            if line.startswith("added "):
                state["server_id"] = line.split()[1]
    # The front has a new port in each run: the game address of the slot names it
    django_shell(options, env, "from servers.models import GameServer\n"
                 "GameServer.objects.filter(pk=sys.argv[2]).update(host=sys.argv[3])\n",
                 state["server_id"], f"127.0.0.1:{front_port}")
    state["spki"] = site_spki(certs)
    with open(state_path, "w", encoding="utf-8") as f:
        json.dump(state, f)
    return service_env(options, out, state), state, password


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
    processes.start("jobs", [py, "manage.py", "run_jobs"], cwd=options.service, env=stack_env,
                    stdout=open(os.path.join(out, "jobs.log"), "w"), stderr=subprocess.STDOUT)
    certs = os.path.join(out, "certs")
    with open(os.path.join(out, "game_port.txt"), "w", encoding="utf-8") as f:
        f.write(str(ports["slot"]))
    processes.start("front", [sys.executable, "-I", os.path.join(HERE, "lobby_front.py"), "--port", str(ports["front"]),
                              "--cert", os.path.join(certs, "site.pem"), "--key", os.path.join(certs, "site.key"),
                              "--client-ca", os.path.join(certs, "gs-ca.pem"), "--secret", state["proxy_secret"],
                              "--django", str(ports["django"]), "--push", str(ports["push"]), "--game-port-file",
                              os.path.join(out, "game_port.txt"), "--play-dir", options.web_dir, "--log",
                              os.path.join(out, "front.log")],
                    stdout=subprocess.DEVNULL, stderr=open(os.path.join(out, "front.err"), "w"))
    time.sleep(8)


def start_slot(options, out, state, ports, processes):
    """The slot agent: it reports the slot idle, and starts the game server when the lobby gives the slot a game"""
    certs = os.path.join(out, "certs")
    slot = os.path.join(out, "slot")
    builds = os.path.join(slot, "builds")
    os.makedirs(builds, exist_ok=True)
    # The agent takes the server program of a build folder: a junction to the folder of the programs
    link = os.path.join(builds, "local")
    if not os.path.exists(link):
        run(["cmd", "/c", "mklink", "/J", os.path.normpath(link), os.path.normpath(os.path.abspath(options.bin))])
    lobby = f"https://127.0.0.1:{ports['front']}"
    server_options = [f"--game-path={options.game_path}", f"--ticket-keys={lobby}/api/v1/ticket-keys",
                      f"--lobby-api={lobby}/api/v1", f"--client-cert={os.path.join(certs, 'client.pem')}",
                      f"--client-key={os.path.join(certs, 'client.key')}",
                      f"--lobby-test-ca={os.path.join(certs, 'ca.pem')}",
                      f"--profile-folder={os.path.join(slot, 'profile')}"]
    if options.trace_draws:
        server_options.append(f"--trace-draws={os.path.join(out, 'server-draws.tsv')}")
    processes.start("slot", [
        sys.executable, "-I", os.path.join(HERE, "slot_agent_local.py"), "--service", options.service,
        "--heartbeat", os.path.join(slot, "alive"), "--", "--slot=1", f"--port={ports['slot']}",
        f"--api={lobby}/api/v1", f"--client-cert={os.path.join(certs, 'client.pem')}",
        f"--client-key={os.path.join(certs, 'client.key')}", f"--ca-file={os.path.join(certs, 'ca.pem')}",
        f"--builds={builds}", f"--games-folder={os.path.join(slot, 'games')}",
        f"--logs-folder={os.path.join(slot, 'logs')}", "--restarts=0", "--poll-seconds=2", "--", *server_options],
        stdout=open(os.path.join(out, "slot.log"), "w"), stderr=subprocess.STDOUT)


def stop_tree(pid):
    """Stops a process that this script started, with the processes that it started (taskkill by the process id)"""
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, creationflags=NO_WINDOW)


def stop_all(processes):
    """Stops every process of the run by its own process id: the slot agent's game server and the Chrome of cdp-run go
    with their parents"""
    for _, process in list(processes.items):
        if process.poll() is None:
            stop_tree(process.pid)
    processes.stop()


def ssl_context(out):
    context = ssl.create_default_context(cafile=os.path.join(out, "certs", "ca.pem"))
    return context


def sign_in_cookie(out, base, account, password):
    """Signs in through the site's form, as a browser does, and writes the session cookie for cdp-run's --cookie-file"""
    jar = http.cookiejar.CookieJar()
    opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar),
                                         urllib.request.HTTPSHandler(context=ssl_context(out)))
    page = opener.open(base + "/account/login/?next=/play/", timeout=30).read().decode()
    token = re.search(r'name="csrfmiddlewaretoken" value="([^"]+)"', page).group(1)
    form = urllib.parse.urlencode({"csrfmiddlewaretoken": token, "username": account, "password": password,
                                   "next": "/play/"}).encode()
    opener.open(urllib.request.Request(base + "/account/login/?next=/play/", data=form,
                                       headers={"Referer": base + "/account/login/", "Origin": base}), timeout=30)
    session = [c for c in jar if c.name.endswith("bw-session")]
    if not session:
        raise SystemExit("no site session: the sign-in failed")
    path = os.path.join(out, "cookies.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump([{"name": c.name, "value": c.value, "url": base + "/"} for c in session], f)
    return path


def make_lobby(out, base, account, password, build_id, land, gods):
    context = ssl_context(out)
    status, body = http_json("POST", base + "/api/v1/sessions", context, {"name": account, "password": password})
    if status not in (200, 201):
        raise SystemExit(f"the sign-in of the API failed: {status} {body}")
    token = body["access_token"]
    status, listed = http_json("GET", base + "/api/v1/lobbies?in_game=1", context, token=token)
    for old in (listed or {}).get("lobbies", []):
        http_json("DELETE", f"{base}/api/v1/lobbies/{old['id']}/members/me", context, token=token)
    # The list of the game shows a lobby with no free god slot as full, and the game then refuses the join, also for
    # the host. So the lobby starts with a second open slot, which the host gives to a computer god after the join.
    kinds = ["open", "open"] + ["computer"] * (gods - 2)
    status, lobby = http_json("POST", base + "/api/v1/lobbies", context,
                              {"name": "Lobby join check", "map": {"file": land}, "god_slots": gods,
                               "slot_kinds": kinds, "visibility": "public", "build_id": build_id}, token=token)
    if status not in (200, 201):
        raise SystemExit(f"the lobby was not made: {status} {body}")
    http_json("PUT", f"{base}/api/v1/lobbies/{lobby['id']}/members/me", context, {"have_map": True}, token=token)
    return token, lobby


def server_build_id(options):
    text = run([os.path.join(options.bin, "openblack-server.exe"), "--version"])
    found = re.search(r"build id ([^)\s]+)", text)
    if not found:
        raise SystemExit(f"no build id in: {text}")
    return found.group(1)


def page_url(options, base, arguments):
    # The Play button plays a land first: that page has no "multiplayer" parameter, which shows only Join Online Game
    query = "" if options.from_land else "multiplayer"
    if arguments:
        query += ("&" if query else "") + "args=" + urllib.parse.quote(" ".join(arguments))
    return f"{base}/play/?{query}"


def chrome_command(options, out, url, seconds, steps, cookie_file, state, folder, click="#play-online"):
    actions = os.path.join(folder, "actions.json")
    with open(actions, "w", encoding="utf-8") as f:
        json.dump(steps.items, f, indent=1)
    command = ["node", CDP, options.chrome, url, str(int(seconds)), folder, "--game-folder", options.game_path,
               "--click", click, "--keep-profile", "--trust-spki", state["spki"], "--actions", actions, "--run-all-steps",
               "--frame-url", "/play/index.html", "--cookie-file", cookie_file,
               "--size", f"{WINDOW[0]}x{WINDOW[1]}"]
    if options.checksums:
        command += ["--checksums", os.path.join(folder, "checksums.jsonl")]
    if options.gpu:
        command.append("--gpu")
    return command


def save_first_run(options, out, base, state, cookie_file, processes):
    """A first run with the same Chrome profile: the game plays a land and keeps its save on exit"""
    folder = os.path.join(out, "web")
    steps = Steps()
    steps.wait_log("Load startup:", 900)
    steps.wait(options.save_seconds)
    steps.shot("s0-land")
    url = f"{base}/play/?args=" + urllib.parse.quote(f"-s {options.save_first} --skip-logos")
    command = chrome_command(options, out, url, steps.at + 20, steps, cookie_file, state, folder, click="#play")
    command = [part for part in command if part not in ("--checksums",)]
    process = processes.start("web-save", command, stdout=open(os.path.join(out, "web-save.out"), "w"),
                              stderr=subprocess.STDOUT)
    process.wait(3600)


def wait_member_join(front_log, lobby_id, since_line, seconds, chrome):
    """Waits until the page asked the lobby for the room (the front's log names the lobby after the given line)"""
    end = time.time() + seconds
    while time.time() < end and chrome.poll() is None:
        if os.path.exists(front_log):
            with open(front_log, encoding="utf-8", errors="replace") as f:
                lines = f.readlines()[since_line:]
            if any(f"/api/v1/lobbies/{lobby_id}/members" in line for line in lines):
                return True
        time.sleep(1)
    return False


def line_count(path):
    if not os.path.exists(path):
        return 0
    with open(path, encoding="utf-8", errors="replace") as f:
        return sum(1 for _ in f)


def game_logs(out):
    logs = os.path.join(out, "slot", "logs")
    if not os.path.isdir(logs):
        return []
    return sorted((os.path.join(logs, name) for name in os.listdir(logs) if name.endswith(".log")),
                  key=os.path.getmtime)


def server_result(log):
    parts, counts = [], None
    if log and os.path.exists(log):
        with open(log, encoding="utf-8", errors="replace") as f:
            for line in f:
                found = SERVER_PARTS.search(line)
                if found:
                    parts.append(line.strip()[-400:])
                found = SERVER_COUNTS.search(line)
                if found:
                    counts = (int(found.group(2)), int(found.group(3)))
    return parts, counts


def web_draw_trace(folder):
    """The draw lines that the page wrote with its checksum lines (--sim-trace-draws -)"""
    path = os.path.join(folder, "checksums.jsonl")
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as f:
        lines = [line for line in f if not line.startswith("{")]
    if not lines:
        return None
    trace = os.path.join(folder, "draws.tsv")
    with open(trace, "w", encoding="utf-8") as f:
        f.writelines(lines)
    return trace


def page_lines(folder, keep):
    path = os.path.join(folder, "page-log.txt")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8", errors="replace") as f:
        return [line.rstrip() for line in f if any(k in line for k in keep)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--service", required=True, help="a clone of bwgame-service")
    parser.add_argument("--service-python", required=True, help="the python of the service's environment")
    parser.add_argument("--bin", required=True, help="the folder of openblack-server.exe")
    parser.add_argument("--web-dir", required=True, help="the folder of the web page (index.html, openblack.wasm)")
    parser.add_argument("--game-path", default="C:/projects/black")
    parser.add_argument("--out", required=True)
    parser.add_argument("--land", default="mpm_4p_1", help="the land of the lobby")
    parser.add_argument("--gods", type=int, default=4, choices=range(2, 5), help="the god slots: the web player and computer gods")
    parser.add_argument("--turns", type=int, default=600, help="the turns of the web client before the run ends")
    parser.add_argument("--take-turn", type=int, default=99)
    parser.add_argument("--throw-turn", type=int, default=125)
    parser.add_argument("--no-editor", action="store_true", help="the account has no editor right")
    parser.add_argument("--trace-draws", action="store_true",
                        help="the server and the web client write a draw trace (developer builds only)")
    parser.add_argument("--developer-args", default="",
                        help="more game arguments for a developer web build, separated by spaces")
    parser.add_argument("--save-first", metavar="SCRIPT",
                        help="a first web run plays this land script (for example Land4.txt) and keeps the save on exit")
    parser.add_argument("--from-land", metavar="SCRIPT",
                        help="the web player plays this land script first (for example Land4.txt, a developer build), "
                             "then joins with the page's Join Online Game button over the land")
    parser.add_argument("--land-seconds", type=float, default=90.0, help="the seconds of play of --from-land")
    parser.add_argument("--save-seconds", type=float, default=60.0, help="the seconds of play of --save-first")
    parser.add_argument("--gpu", action="store_true", help="Chrome draws with the graphics card")
    parser.add_argument("--chrome", default=os.environ.get("OB_CHROME", CHROME))
    parser.add_argument("--build-id", help="the build id of the lobby (default: the build id of the server program)")
    parser.add_argument("--front-only", action="store_true",
                        help="end the run in the lobby room, 20 seconds after the start (a check of the clicks)")
    parser.add_argument("--keep-stack", action="store_true", help="leave the service running at the end (for a look)")
    options = parser.parse_args()
    options.checksums = options.trace_draws or bool(options.developer_args)
    out = os.path.abspath(options.out)
    os.makedirs(out, exist_ok=True)
    ports = {"front": free_port(), "django": free_port(), "push": free_port(), "internal": free_port(),
             "slot": free_port()}
    env, state, password = setup_service(options, out, ports["front"])
    base = f"https://127.0.0.1:{ports['front']}"
    processes = Processes(out)
    report = {"ok": False, "land": options.land, "editor": not options.no_editor}
    cookie_file = None
    web = os.path.join(out, "web")
    os.makedirs(web, exist_ok=True)
    try:
        start_stack(options, out, env, state, ports, processes)
        start_slot(options, out, state, ports, processes)
        build_id = options.build_id or server_build_id(options)
        report["build_id"] = build_id
        cookie_file = sign_in_cookie(out, base, ACCOUNT, password)
        if options.save_first:
            save_first_run(options, out, base, state, cookie_file, processes)
            report["save_first"] = page_lines(web, ("Saved", "save on exit", "Load saved land", "Loaded the whole"))
            cookie_file = sign_in_cookie(out, base, ACCOUNT, password)
        token, lobby = make_lobby(out, base, ACCOUNT, password, build_id, options.land, options.gods)
        report["lobby"] = lobby["id"]
        arguments = []
        if options.checksums:
            # Only a simulation run writes the checksums: the turn at the end makes the run one
            arguments += ["--sim-checksums", "-", "--sim-until-turn", str(options.turns)]
        if options.trace_draws:
            arguments += ["--sim-trace-draws", "-"]
        arguments += [a for a in options.developer_args.split(" ") if a]
        steps = Steps()
        new_profile = not os.path.isdir(os.path.join(web, "chrome-profile"))
        if options.from_land:
            arguments = ["-s", options.from_land, "--skip-logos"] + arguments
            front_end_steps(steps, new_profile, options.land_seconds)
        else:
            front_end_steps(steps, new_profile)
        turns_per_second = 10.0
        if options.front_only:
            steps.wait(20)
            steps.shot("f4-later")
        else:
            throw_steps(steps, options.take_turn, options.throw_turn, turns_per_second)
            steps.wait(max(5.0, (options.turns - options.throw_turn) / turns_per_second))
            steps.shot("g3-end")
        front_log = os.path.join(out, "front.log")
        since = line_count(front_log)
        command = chrome_command(options, out, page_url(options, base, arguments), steps.at + 60, steps, cookie_file,
                                 state, web, click="#play" if options.from_land else "#play-online")
        chrome = processes.start("web", command, stdout=open(os.path.join(out, "web.out"), "w"),
                                 stderr=subprocess.STDOUT)
        joined = wait_member_join(front_log, lobby["id"], since, 1500, chrome)
        report["page_joined_lobby"] = joined
        if joined:
            time.sleep(3)
            kinds = ["open"] + ["computer"] * (options.gods - 1)
            status, body = http_json("PATCH", f"{base}/api/v1/lobbies/{lobby['id']}", ssl_context(out),
                                     {"slot_kinds": kinds}, token=token)
            report["slot_kinds"] = {"status": status, "body": body if status >= 300 else None}
            time.sleep(3)
            status, body = http_json("POST", f"{base}/api/v1/lobbies/{lobby['id']}/start", ssl_context(out),
                                     token=token)
            report["start"] = {"status": status, "body": body if status >= 300 else None}
        try:
            chrome.wait(int(steps.at) + 1800)
        except subprocess.TimeoutExpired:
            pass
        for name, process in list(processes.items):
            if name == "web" and process.poll() is None:
                stop_tree(process.pid)
        processes.stop("web")
        logs = game_logs(out)
        log = logs[-1] if logs else None
        parts, counts = server_result(log)
        report["server_log"] = log
        report["server_parts"] = parts
        report["server_counts"] = counts
        report["page"] = page_lines(web, ("Joined the game server", "editor right", "parts from the server",
                                          "turns compared with the server", "Hand deed: the hand of player 0",
                                          "Desync report", "Loaded the whole world"))
        if options.trace_draws:
            server_trace = os.path.join(out, "server-draws.tsv")
            web_trace = web_draw_trace(web)
            if web_trace and os.path.exists(server_trace):
                draws = draw_trace.compare(server_trace, web_trace)
                report["draws"] = draws
                for line in draw_trace.summary_lines(draws, "server", "the web client"):
                    print(line)
        report["ok"] = bool(joined and counts and counts[0] > 0 and not parts)
    finally:
        if cookie_file and os.path.exists(cookie_file):
            os.remove(cookie_file)
        if not options.keep_stack:
            stop_all(processes)
        with open(os.path.join(out, "report.json"), "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1, default=str)
        print(json.dumps({k: v for k, v in report.items() if k != "draws"}, indent=1, default=str))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
