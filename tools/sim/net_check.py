"""Checks that two real clients play one network game through apps/openblack-server on this computer.

The script makes a test key pair for the join tickets and a test certificate authority with a certificate for
127.0.0.1, all in the output folder (never the live keys of the lobby). It starts the game server with --listen on a free
port of 127.0.0.1 (wss), then two openblack clients with --join and a ticket each. The land, the seed and the start
options go to the server only: each client takes them from the server's reply to its join and loads that land. The
first client plays recorded hand input (--input), the second keeps a still hand. The world turns only with the turns of the server (authoritative
lockstep). At the end it compares the checksum of each turn on the server and on both clients.
An input of the hand needs a map with temples (TwoGods, the online maps, --gods-ring-map); Land 1 and the testbed are
checks with a still hand. A story land makes no temple at its start, so its network game is decided at the first turn
and the end box takes the input: a run with an input whose game the server decided at its first turn is not the same.

    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --input tools/sim/hand_inputs/twogods_scoop_throw.json --turns 2000
    python tools/sim/net_check.py --turns 600 --config RelWithDebInfo
    python tools/sim/net_check.py --testbed --turns 400 --stop-second-after 40 --come-back-seconds 5
    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --turns 2000 --hold-creature 2:2942.91,2607.57
    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --turns 2000 --hash-parts
        --input2 tools/sim/hand_inputs/twogods_slot1_tree_press.json --camera-again-after 700
        --camera2 '{"focus":[2928.87,88.44,2620.91],"yaw":0,"pitch":20,"distance":25}'

A client's --cameraN goes into its input: the client puts its camera there as its land starts, in place of the start view
and its flight to the temple, so the pixels of the input fall on the things that the input was made for. An input file
can have its own "camera" and an "expect" (the least count of each deed of the hand, {"scoops": 1, "throws": 1}). The
summary gives the deeds of the hand of each god slot that the server did (hand_deeds: takes, scoops, throws, puts_down,
pours, uses). The run fails when a client counts other deeds than the server, or when the hand of a slot did fewer deeds
than its input expects.
The information box of a land whose script opens it (TwoGods) takes every press of the buttons until the player closes
it, so the input of each client first closes it with the Escape key.

With --hold-creature the hand of that client holds its god's creature through the inspector once the client compared
--hold-after turns: the camera looks at the creature, the pointer goes to a point of its body that the interface picks,
the hand holds and strokes it, lets go, clicks it for the leash and leads it with a tap on the land. The creature walks,
so a recorded input cannot find it. NET_CHECK_DEBUG=1 prints each try of the pointer. The information box of the land
takes every press of the buttons while it shows, so the hand closes it with the Escape key first. --hold-after can be
any turn: the log tells the hundreds, the inspector tells the last turns.

With --slot-fight SECONDS the first client sends the fight cheat once it compared --hold-after turns: at the turn, the
creature of its god fights the nearest creature. Then the fight autopilot of each client fights for its own god with the
presses of its hand, and each client draws the star of the special move. The server takes test commands
(--test-commands). The land must give both gods a creature near each other, such as a copy of TwoGods.txt with a
creature of the first god next to the creature of the second god, in a game folder of its own (--game-path). Give
both clients a camera that looks at the arena (--camera1, --camera2 and --camera-again-after): the fight view starts
when the camera looks at a fight, and the autopilot makes no double click, because a click on the land grips it. Once
the god's creature duels, the script frames the camera on it from within the arena. Until the turns read the pose of
the turn for wounds, give the clients the frame time of the server (--frame-ms 100).
The summary gives the moves that each client queued and the lean that each creature learnt from them at each turn:
learnt_same compares the leans of the clients, and both_learnt tells if the lean of each creature changed.

With --trace-draws the server and each client write a draw trace (each synced random draw with its turn and the place
of its call), and the summary shows the first draw where each client is not the same as the server (draw_trace.py).

With --chat TURNS:CLIENT:TEXT a client says a line in the chat once it compared that many turns (a multiple of 100,
as the client writes the count each 100 turns): the inspector
presses the speech button of its chat, types the line and presses Return. At the end every client must have the line
in its log, from the god of that client, and the checksums must still agree: a line of the chat goes with the turns
but changes nothing that the checksum has.
With --ban-account the script also starts a fake lobby on 127.0.0.1 (HTTPS with mutual TLS, a client certificate for
the server from the test authority). The fake lobby takes the status reports of the server and answers its bans with
that account. Before the other clients, a client with a god's ticket of that account tries to join; the server must
refuse it with "kicked" while the other clients play in step.

With --restart-server-at N the script stops the game server by its own process id once the server did turn N, as a
crash does, and starts it again with --resume-snapshot on the same port (deviation 100). The new server loads its newest
snapshot and does again the turns of its journal; the clients come back, and the checksums of the turns that both
servers did must be the same. The deeds of the hands of the server are the deeds of both server logs: a deed of a turn
that both servers did counts one time. The clients send again the commands that no turn held at the stop, and the
summary gives these lines ("resent"). Compare the server checksums with a run with no restart to show an unbroken game:

    python tools/sim/net_check.py --land "Online Maps/mpm_2p_1.map" --turns 1500 --restart-server-at 1000

With --stop-second-after the script stops the second client (by its own process id) that many seconds after it
started. The server then defeats its god after --come-back-seconds, and the first client must stay in step.

The live server keeps its profile from game to game and waits in its room. These options do the same before the game:
--server-alignment gives the server's profile an alignment from an earlier game (--server-profile names a profile
folder), --room-wait keeps the server alone in its room, and --leave-room-first lets one more client join the room and
leave it with no start. In each case every client must stay in step with the server from the first turn:

    python tools/sim/net_check.py --land "Online Maps/mpm_2p_1.map" --turns 300 --hash-fields
        --server-alignment 0.0025 --leave-room-first

With --box-back CLIENT:TURN the client (from 0) clicks Back in the information box of the land at the turn, while the
box shows: the player leaves the network game, as with Leave Game. The server must see the leave of one slot, and the
other clients must stay in step with the server. The summary has a "box_back" block:

    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --clients 2 --turns 600 --box-back 1:200

--alignment gives the profile of a client an alignment from earlier games. Each god of the game starts at the alignment
of its own player's profile, on the server and on every client:

    python tools/sim/net_check.py --land "Online Maps/mpm_2p_1.map" --turns 300 --hash-fields
        --alignment 1:-0.5 --alignment 2:0.7

The AI soak (developer builds only): with --all-computer-gods the room has no human god. The server gives every god with a
citadel a computer god, starts at once, and turns the world when the --observers clients joined; every client is an
observer, and the clients and the server must stay in step. --ai-seed gives the computer gods the personality of that
seed. --observers N adds N desktop observers to the gods (up to 4), --web-client adds one more observer in headless
Chrome (through web/scripts/cdp-run.mjs, with the web build of this tree), --until-end plays until the game is decided
(at most --turns, up to 100000). The summary has an "ai" block: the computer gods of the game and, for each god, the
needs that it chose, the deeds of its hand and the miracles that it cast, as the server's log tells. tools/sim/ai_soak.py
runs many such games.

    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --all-computer-gods --observers 2 --turns 2000 --ai-seed 1

Cross play: with --web-god the web client plays the god of the slot after the desktop gods, and --web-chat TURNS:TEXT
makes it say a line in the chat (cdp-run steps: it waits for the line of the turns in its log, clicks the speech button,
types the line and presses Return). The chat check then also needs the lines of the desktop gods in the web log, and the
line of the web god in each desktop log. With --web-remote the script starts no Chrome: it writes web/join.json and waits
for web/done. tools/sim/web_remote.py on another computer reads that file over SSH, opens a tunnel to the server, plays
the web client there and copies its checksums and its log back into web/. A game on mpm_3p_1 with two human gods has one
computer god in the third slot:

    python tools/sim/net_check.py --land "Online Maps/mpm_3p_1.map" --clients 1 --web-client --web-god --web-remote
        --chat 300:1:hello --web-chat 500:hi --turns 2000

With --scenario (developer builds) the server plays a testbed scenario: it lays the scenario out before the first turn,
and each client gets the scenario with the start and lays it out before the same turn. In a network game a scenario
gives its commands before the turns, not in the frames, so every computer gives them before the same turn. The state of
a scenario is not in the world, so the observers join the room before the start, and the server refuses a join to the
game as it turns:

    python tools/sim/net_check.py --scenario mind.computer_god_rivals --observers 1 --turns 2000

The game reads its vanilla switches anywhere in its command line, as the original game did, so a path with "-multiplayer"
or "/multiplayer" in it opens the multiplayer front end (the -MULTIPLAYER switch). The script copies the programs into
its output folder and refuses an output folder with such a name.

Every program starts with no console window of its own, the clients with --background. The script stops only the
processes that it started, by their own process ids.
"""

import argparse
import base64
import datetime
import hashlib
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time

import draw_trace

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.x509.oid import NameOID
import http.server
import ipaddress
import ssl
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_report  # noqa: E402
import row_dump  # noqa: E402
import save_diff  # noqa: E402
import sim_paths  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BUILDS = sim_paths.DEFAULT_BUILDS
DEFAULT_GAME_PATH = sim_paths.DEFAULT_GAME_PATH
CHECKSUM_LINE = re.compile(r"turn (\d+) checksum ([0-9a-f]{16})")
LISTEN_LINE = re.compile(r"The game server listens on [0-9.]+:(\d+)")
SERVER_ID = "net-check-1"
# The input of the first client starts after this many frames (50 s at 20 ms), when both clients loaded and the game
# started
INPUT_OFFSET_FRAMES = 2500
# The input closes the information box of the land this many frames before its first event, and holds the Escape key for
# BOX_KEY_FRAMES. The box closes as the key goes down, so the hand is free well before the input starts.
BOX_CLOSE_LEAD_FRAMES = 100
BOX_KEY_FRAMES = 5
RUN_LIMIT_SECONDS = 900
# The most turns of a game. The game has no time limit; a soak of 100000 turns takes about 3 hours at 10 turns a
# second.
MOST_TURNS = 100000
# The run of a long game: a turn takes 100 ms on the server and the clients follow, with the time to load
SECONDS_PER_TURN = 0.1
LOAD_SECONDS = 600
DEFAULT_CHROME = os.environ.get("OB_CHROME", "C:/Program Files/Google/Chrome/Application/chrome.exe")
DEFAULT_NODE = os.environ.get("OB_NODE", "node")
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def worktree_root():
    return os.path.normpath(os.path.join(HERE, "..", ".."))


def default_bin(config):
    return sim_paths.default_bin(config)


def b64(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def pae(*pieces):
    out = [len(pieces).to_bytes(8, "little")]
    for piece in pieces:
        out.append(len(piece).to_bytes(8, "little"))
        out.append(piece)
    return b"".join(out)


SWITCH_IN_PATH = re.compile(r"[-/\\]multiplayer", re.IGNORECASE)


def copy_programs(folder, out):
    """The server, the game and their libraries, in a folder whose path has no switch of the game"""
    target = os.path.join(out, "bin")
    os.makedirs(target, exist_ok=True)
    for name in os.listdir(folder):
        if name.lower().endswith(".dll") or name in ("openblack.exe", "openblack-server.exe", "openblack",
                                                       "openblack-server"):
            shutil.copy2(os.path.join(folder, name), os.path.join(target, name))
    return target


def make_ticket_key(folder):
    """A test Ed25519 key pair, and the keys file in the shape of the lobby's /api/v1/ticket-keys"""
    key = Ed25519PrivateKey.generate()
    public = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    paserk = "k4.public." + b64(public)
    kid = "k4.pid." + b64(hashlib.blake2b(b"k4.pid." + paserk.encode("ascii"), digest_size=33).digest())
    path = os.path.join(folder, "ticket-keys.json")
    with open(path, "w", encoding="utf-8") as file:
        json.dump({"keys": [{"kid": kid, "public_key": paserk, "use": "current"}]}, file)
    return key, kid, path


def make_ticket(key, kid, account, name, role, lifetime):
    """A PASETO v4.public join ticket as the lobby makes one, for this test server"""
    now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
    claims = {
        "iss": "bwgame-service", "sub": str(account), "aud": SERVER_ID,
        "iat": now.isoformat(), "nbf": now.isoformat(),
        "exp": (now + datetime.timedelta(seconds=lifetime)).isoformat(),
        "jti": b64(os.urandom(16)), "name": name, "clans": [], "admin": False, "role": role,
    }
    message = json.dumps(claims, separators=(",", ":")).encode("utf-8")
    footer = json.dumps({"kid": kid}, separators=(",", ":")).encode("utf-8")
    header = b"v4.public."
    signature = key.sign(pae(header, message, footer, b""))
    return header.decode() + b64(message + signature) + "." + b64(footer)


def make_certificates(folder):
    """A test certificate authority and a server certificate for 127.0.0.1"""
    now = datetime.datetime.now(datetime.timezone.utc)
    ca_key = ec.generate_private_key(ec.SECP256R1())
    ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "openblack net_check test CA")])
    ca = (x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name).public_key(ca_key.public_key())
          .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(minutes=5))
          .not_valid_after(now + datetime.timedelta(days=1))
          .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
          .add_extension(x509.KeyUsage(digital_signature=False, content_commitment=False, key_encipherment=False,
                                       data_encipherment=False, key_agreement=False, key_cert_sign=True, crl_sign=True,
                                       encipher_only=False, decipher_only=False), critical=True)
          .add_extension(x509.SubjectKeyIdentifier.from_public_key(ca_key.public_key()), critical=False)
          .sign(ca_key, hashes.SHA256()))
    key = ec.generate_private_key(ec.SECP256R1())
    cert = (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "127.0.0.1")]))
            .issuer_name(ca_name).public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now - datetime.timedelta(minutes=5)).not_valid_after(now + datetime.timedelta(days=1))
            .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address("127.0.0.1"))]),
                           critical=False)
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .add_extension(x509.ExtendedKeyUsage([x509.oid.ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
            .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), critical=False)
            .sign(ca_key, hashes.SHA256()))
    # The client certificate of the game server for the status reports to a fake lobby (mutual TLS)
    client_key = ec.generate_private_key(ec.SECP256R1())
    client = (x509.CertificateBuilder()
              .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, SERVER_ID)]))
              .issuer_name(ca_name).public_key(client_key.public_key()).serial_number(x509.random_serial_number())
              .not_valid_before(now - datetime.timedelta(minutes=5)).not_valid_after(now + datetime.timedelta(days=1))
              .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
              .add_extension(x509.ExtendedKeyUsage([x509.oid.ExtendedKeyUsageOID.CLIENT_AUTH]), critical=False)
              .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), critical=False)
              .sign(ca_key, hashes.SHA256()))
    paths = {name: os.path.join(folder, name)
             for name in ("ca.pem", "server.pem", "server.key", "client.pem", "client.key")}
    with open(paths["ca.pem"], "wb") as file:
        file.write(ca.public_bytes(serialization.Encoding.PEM))
    for (cert_name, key_name), (certificate, private) in {("server.pem", "server.key"): (cert, key),
                                                          ("client.pem", "client.key"): (client, client_key)}.items():
        with open(paths[cert_name], "wb") as file:
            file.write(certificate.public_bytes(serialization.Encoding.PEM))
        with open(paths[key_name], "wb") as file:
            file.write(private.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                             serialization.NoEncryption()))
    return paths


class FakeLobby:
    """The game servers' API of a lobby on 127.0.0.1, for the test only: status reports and the bans of one account.
    It asks the game server for its client certificate of the test authority, as the lobby does (mutual TLS)."""

    def __init__(self, certs, banned):
        self.counts = {"status": 0, "bans": 0, "other": 0}
        lobby = self
        bans = {"bans": [{"account": {"id": banned, "name": f"Player{banned}"}, "reason": "net_check", "by": "test",
                          "created": "2026-10-26T00:00:00Z", "expires": None}]}

        class Handler(http.server.BaseHTTPRequestHandler):
            def answer(self, status, body):
                data = json.dumps(body).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def do_PUT(self):
                length = int(self.headers.get("Content-Length", 0))
                self.rfile.read(length)
                if self.path == f"/api/v1/servers/{SERVER_ID}/status":
                    lobby.counts["status"] += 1
                    self.answer(200, {"next_status_in": 5, "listed": True, "clans": []})
                else:
                    lobby.counts["other"] += 1
                    self.answer(404, {"code": "not_found"})

            def do_GET(self):
                if self.path == f"/api/v1/servers/{SERVER_ID}/bans":
                    lobby.counts["bans"] += 1
                    self.answer(200, bans)
                else:
                    lobby.counts["other"] += 1
                    self.answer(404, {"code": "not_found"})

            def log_message(self, *args):
                pass

        # The handshake refuses a game server with no client certificate of the test authority
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(certs["server.pem"], certs["server.key"])
        context.load_verify_locations(certs["ca.pem"])
        context.verify_mode = ssl.CERT_REQUIRED
        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.socket = context.wrap_socket(self.server.socket, server_side=True)
        self.address = f"https://127.0.0.1:{self.server.server_address[1]}/api/v1"
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()


def start(command, cwd):
    """Starts a program with no console window of its own"""
    os.makedirs(cwd, exist_ok=True)
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    return subprocess.Popen(command, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)


def stop(process):
    """Stops a program that the script started, by its own process id"""
    if process.poll() is None:
        process.kill()
        process.wait()


def wait_for_port(log, process, limit=180):
    end = time.monotonic() + limit
    while time.monotonic() < end:
        if process.poll() is not None:
            return None
        if os.path.exists(log):
            with open(log, encoding="utf-8", errors="replace") as file:
                match = LISTEN_LINE.search(file.read())
            if match:
                return int(match.group(1))
        time.sleep(0.5)
    return None


def wait_for_line(log, text, process, limit=300):
    end = time.monotonic() + limit
    while time.monotonic() < end:
        if process.poll() is not None:
            return False
        if os.path.exists(log):
            with open(log, encoding="utf-8", errors="replace") as file:
                if text in file.read():
                    return True
        time.sleep(0.5)
    return False


def server_checksums(log):
    turns = {}
    with open(log, encoding="utf-8", errors="replace") as file:
        for line in file:
            match = CHECKSUM_LINE.search(line)
            if match:
                turns[int(match.group(1))] = match.group(2)
    return turns


def deeds_by_turn(lines, until_turn=None):
    """The count of each deed of the hand of each player at each turn, as {(player, deed, turn): count}. With
    until_turn, a deed after that turn is not counted."""
    deeds = {}
    for line in lines:
        match = sim_report.HAND_DEED.search(line)
        if match is None:
            continue
        turn = int(match.group(4)) if match.group(4) is not None else None
        if until_turn is not None and turn is not None and turn > until_turn:
            continue
        key = (int(match.group(1)), sim_report.HAND_DEED_NAMES.get(match.group(2), match.group(2)), turn)
        deeds[key] = deeds.get(key, 0) + 1
    return deeds


def restarted_server_deeds(first_lines, restarted_lines, until_turn=None):
    """The deeds of the hands of a server that stopped and of the server that continued its game from a snapshot, as
    {player: {deed: count}}. The new server does again the turns from its snapshot, so a turn that both logs cover has
    its deeds in both: they count one time. The turns before the snapshot are only in the first log, and the turns after
    the stop only in the second."""
    first = deeds_by_turn(first_lines, until_turn)
    restarted = deeds_by_turn(restarted_lines, until_turn)
    deeds = {}
    for key in sorted(set(first) | set(restarted), key=lambda item: (item[0], item[1], item[2] is None, item[2] or 0)):
        player, name, _ = key
        counts = deeds.setdefault(player, {})
        counts[name] = counts.get(name, 0) + max(first.get(key, 0), restarted.get(key, 0))
    return deeds


def server_deeds_in_logs(server_log, restarted_log, until_turn=None):
    """The deeds of the hands that the server counts, up to a turn: of both logs when the server restarted"""
    if restarted_log is None or not os.path.exists(restarted_log):
        return sim_report.hand_deeds_in_file(server_log, until_turn)
    with open(server_log, encoding="utf-8", errors="replace") as first, \
            open(restarted_log, encoding="utf-8", errors="replace") as restarted:
        return restarted_server_deeds(first.readlines(), restarted.readlines(), until_turn)


def server_against_run(server_turns, other_turns, last_turn):
    """Compares the server checksums of this run with the server checksums of another run, from turn 1 to the last
    turn of the clients. The turns after it are left out: each client stops at its own time, so the turn that its god
    leaves at is not the same in two runs."""
    compared = [turn for turn in sorted(server_turns) if 1 <= turn <= last_turn and turn in other_turns]
    different = [turn for turn in compared if server_turns[turn] != other_turns[turn]]
    return {"compared": len(compared), "same_turns": len(compared) - len(different),
            "first_different": different[0] if different else None, "last_different": different[-1] if different else None,
            "same": bool(compared) and compared[0] == 1 and compared[-1] == last_turn and not different}


# The folder in the folder of the server and of each client where it writes its world states (--world-states)
WORLD_STATES_FOLDER = "world-states"


def world_state_turns(text):
    """The turns of a list such as "250" or "100,250", as the game reads it; None when the text is not such a list"""
    items = text.split(",")
    if not all(item.isdigit() and item.isascii() and int(item) < 2 ** 32 for item in items):
        return None
    return [int(item) for item in items]


def world_states_against_server(server_folder, client_folder, turns):
    """Compares the synced parts of the world state of a client with the world state of the server at each turn
    (save_diff.compare_synced). A turn whose world state one of them did not write is not the same."""
    results = []
    for turn in turns:
        name = f"world-{turn}.obsav"
        paths = [os.path.join(folder, WORLD_STATES_FOLDER, name) for folder in (server_folder, client_folder)]
        missing = [who for who, path in zip(("server", "client"), paths) if not os.path.exists(path)
                   or not os.path.exists(os.path.splitext(path)[0] + ".json")]
        if missing:
            results.append({"turn": turn, "same": False, "missing": missing})
            continue
        maps = [save_diff.read_state(path)[1] for path in paths]
        try:
            difference = save_diff.compare_synced_files(*paths)
        except (ValueError, struct.error) as error:
            difference = {"unread": str(error)}
        results.append({"turn": turn, "same": not difference, "difference": difference or None,
                        "waiting_sounds": [mapping.get("waitingSounds") for mapping in maps]})
    return {"same": all(result["same"] for result in results), "turns": results}


def record_checksums(record):
    turns = {}
    if not os.path.exists(record):
        return turns
    with open(record, encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            value = json.loads(line)
            synced = value.get("synced")
            if isinstance(synced, dict) and "checksum" in synced and "turn" in value:
                turns[int(value["turn"])] = synced["checksum"]
    return turns


def parted_parts(first, other, turn):
    """The parts whose hash differs between two clients' records at a turn"""
    def synced(path):
        if not os.path.exists(path):
            return {}
        with open(path, encoding="utf-8") as file:
            for line in file:
                line = line.strip()
                if line:
                    value = json.loads(line)
                    if value.get("turn") == turn:
                        return value.get("synced", {})
        return {}
    ours, theirs = synced(first), synced(other)
    names = set(ours.get("parts", {})) | set(theirs.get("parts", {}))
    return sorted(name for name in names if ours.get("parts", {}).get(name) != theirs.get("parts", {}).get(name))


def lines_with(log, words):
    if not os.path.exists(log):
        return []
    with open(log, encoding="utf-8", errors="replace") as file:
        return [line.strip() for line in file if any(word in line for word in words)]


# The counts of the session: each 100 compares, and once more with the exact counts when the network game ends. A client
# that skipped turns in a resume can stop up to 100 compares after its last count of the 100s.
SESSION_COUNTS = re.compile(r"(?:Network game:|The network game ended \([^)]*\):) (\d+) turns compared with the server, "
                            r"(\d+) the same")
RESUMES = re.compile(r"Back in the game at turn")
PARTS_AT = re.compile(r"parts from the server's world at turn (\d+)")


# The compares with the server that one part on purpose (--part-second-at) makes different
PURPOSEFUL_PART_COMPARES = 1
RECOVERED_AT = re.compile(r"of turn (\d+)")


def recovery_turns(lines):
    """The turns of the server's worlds that a client went on from, in the order of the log"""
    return [int(match.group(1)) for line in lines if (match := RECOVERED_AT.search(line))]


def purposeful_part_left_out(different, recoveries, part_at):
    """The different turns of a client without those of its part on purpose: from the turn of the part up to the first
    world that it went on from after it. A part after that recovery is a real part, also when a later recovery hides it."""
    back_at = next((turn for turn in recoveries if turn >= part_at), None)
    if back_at is None:
        return different
    return [turn for turn in different if not part_at <= turn <= back_at]


def session_counts(log):
    """The last counts of the compares that the client's session wrote: compared, the same, and the resumes"""
    if not os.path.exists(log):
        return None
    found = None
    resumes = 0
    with open(log, encoding="utf-8", errors="replace") as file:
        for line in file:
            match = SESSION_COUNTS.search(line)
            if match:
                found = (int(match.group(1)), int(match.group(2)))
            if RESUMES.search(line):
                resumes += 1
    return None if found is None else (found[0], found[1], resumes)


INSPECTOR_ADAPTER = os.path.join(HERE, "..", "inspector", "openblack_inspector_mcp.py")


def set_camera(pid, camera):
    """Puts the camera of a client where its input plays, through the inspector; false when the inspector did not do
    it. A camera that stays at the start makes the hand input miss the things that it must take."""
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    done = subprocess.run([sys.executable, INSPECTOR_ADAPTER, "--pid", str(pid), "--call", "camera_set", camera],
                          timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
    return done.returncode == 0


def inspector_call(pid, query, request):
    """One request to the inspector of a client, its answer (none when it fails)"""
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    done = subprocess.run([sys.executable, INSPECTOR_ADAPTER, "--pid", str(pid), "--call", query, json.dumps(request)],
                          timeout=120, capture_output=True, text=True, creationflags=flags)
    try:
        return json.loads(done.stdout).get("result")
    except ValueError:
        return None


def say_in_chat(pid, text):
    """A client says a line in the chat as the player does: a click on the speech button at the top right corner, the
    line typed and Return. The inspector takes only the input of the player while a network game is live."""
    frame = inspector_call(pid, "engine.frame", {}) or {}
    width = (frame.get("window") or [1280, 1024])[0]
    inspector_call(pid, "input.pointer", {"params": {"screen": [width - 8, 8]}})
    time.sleep(0.5)
    clicked = inspector_call(pid, "input.button", {"params": {"button": "left", "action": "click"}})
    time.sleep(1.0)
    inspector_call(pid, "input.text", {"params": {"text": text}})
    time.sleep(0.5)
    inspector_call(pid, "input.key", {"params": {"key": "Return"}})
    print(f"chat: client with pid {pid} says {text!r} (click: {clicked})")


# The points of the menu's buttons on a window of 1280 by 1024: Options on the first page, the Players tab, and Edit
# Tattoo on the Players page
TATTOO_MENU_CLICKS = (("Options", (640, 704)), ("Players", (640, 80)), ("Edit Tattoo", (773, 772)))


def menu_page(pid):
    """The page that the game's menu of a client shows, or none when the menu is shut"""
    windows = inspector_call(pid, "gui.windows", {}) or {}
    for item in windows.get("items", []):
        if isinstance(item, dict) and item.get("name") == "menu":
            if not item.get("open"):
                return None
            # The Edit Tattoo button shows only while the profile keeps its creature
            tattoo = " with Edit Tattoo" if "Edit Tattoo" in item.get("buttons", []) else ""
            return f"{item.get('page')}{tattoo}"
    return None


def open_tattoo_menu(pid):
    """A client opens the tattoo editor from the game's menu as the player does, with the input of the player only:
    Escape opens the menu, then clicks on Options, the Players tab and Edit Tattoo. The editor shows a creature of its
    own over the menu. True when the editor is open."""
    frame = inspector_call(pid, "engine.frame", {}) or {}
    width, height = frame.get("window") or [1280, 1024]
    inspector_call(pid, "input.key", {"params": {"key": "Escape"}})
    time.sleep(1.5)
    pages = [menu_page(pid)]
    for _, (x, y) in TATTOO_MENU_CLICKS:
        inspector_call(pid, "input.pointer", {"params": {"screen": [round(x * width / 1280), round(y * height / 1024)]}})
        time.sleep(0.5)
        inspector_call(pid, "input.button", {"params": {"button": "left", "action": "click"}})
        time.sleep(1.5)
        pages.append(menu_page(pid))
    windows = inspector_call(pid, "gui.windows", {}) or {}
    shown = any(item.get("name") == "tattoo_editor" and item.get("open") for item in windows.get("items", [])
                if isinstance(item, dict))
    print(f"tattoo menu: client with pid {pid} opened the tattoo editor: {shown} (menu pages {pages})")
    return shown


def chat_lines(log_path):
    """The lines of the chat that came to a client, as (turn, god, text)"""
    found = []
    pattern = re.compile(r'Chat: turn (\d+) god (\d+) says "(.*)" to ')
    try:
        with open(log_path, encoding="utf-8", errors="replace") as log:
            for line in log:
                match = pattern.search(line)
                if match:
                    found.append((int(match.group(1)), int(match.group(2)), match.group(3)))
    except OSError:
        pass
    return found


def chat_came_to(received, god, text):
    """For the lines that each client received, as chat_lines gives them, whether the line came from the god"""
    return [any(sender == god and line == text for _, sender, line in lines) for lines in received]


def client_turn(pid):
    """The game turn that a client has reached, through the inspector; None when the inspector does not tell it"""
    state = inspector_call(pid, "game.state", {})
    turn = state.get("turn") if isinstance(state, dict) else None
    return turn if isinstance(turn, int) else None


def reached_turn(log, pid, turns, turn_of=None):
    """Whether a client has reached a game turn, whatever the turn is. The log of the client tells the compared turns
    each 100 turns, so it answers for the whole hundreds at once and the inspector tells the last turns. The turns of
    the network game and of the client are the same count."""
    turn_of = turn_of or client_turn
    hundreds = turns - turns % 100
    if hundreds > 0 and not lines_with(log, [f"Network game: {hundreds} turns compared"]):
        return False
    if turns == hundreds:
        return True
    turn = turn_of(pid)
    return turn is not None and turn >= turns


def close_land_info_box(pid, call=None, wait=time.sleep):
    """Closes the information box of the land when it shows, with the Escape key as Go! does. The box takes every press
    of the buttons while it shows, so the hand cannot hold anything before it is closed. Whether the box was open."""
    call = call or inspector_call
    for _ in range(3):
        windows = items_of(call(pid, "gui.windows", {}))
        if not any(window.get("name") == "land_info_box" and window.get("open") for window in windows):
            return _ > 0
        call(pid, "input.key", {"params": {"key": "Escape", "how": "press"}})
        wait(0.5)
        call(pid, "input.key", {"params": {"key": "Escape", "how": "release"}})
        wait(1.0)
    return True


def hand_holds(pid):
    """The creature that the hand of a client holds to stroke or slap it, through the inspector; None for none"""
    systems = inspector_call(pid, "creatures.systems", {})
    hand = systems.get("hand") if isinstance(systems, dict) else None
    return hand.get("creature") if isinstance(hand, dict) else None


def hold_creature(pid, near, slaps=0):
    """The hand of a client holds the creature nearest a point, strokes it and lets go; then a click puts the leash on
    and a tap on the land leads the creature there. The camera looks at the creature first, and the pointer goes to a
    pixel where the interface picks the creature."""
    x, z = (float(value) for value in near.split(","))
    if close_land_info_box(pid):
        print("hold: closed the information box of the land")
    found = inspector_call(pid, "objects.find", {"params": {"kind": "Creature"}, "near": [x, z], "radius": 600,
                                                "limit": 1})
    items = found.get("items", []) if isinstance(found, dict) else []
    if not items:
        print("hold: no creature near", near)
        return
    creature = items[0]["id"]
    def pixel_on_creature(action=None):
        """Looks at the creature from each side in turn, and gives a pixel where the interface picks it: the pointer goes
        to points of the creature's body, from its feet up. The creature walks, so the button action (press or click)
        follows the pick at once: a later press would find the creature a step away from the pointer."""
        for yaw in (0, 90, 180, 270):
            inspector_call(pid, "camera.frame", {"params": {"id": creature, "yaw": yaw, "pitch": 40, "distance": 18}})
            time.sleep(1.0)
            for height in (1.0, 2.0, 0.5, 3.0, 1.5, 4.0, 2.5):
                # The creature walks: its place is asked again for each try
                found = inspector_call(pid, "ecs.entity", {"params": {"id": creature}})
                at = found.get("position") if isinstance(found, dict) else None
                if not isinstance(at, list):
                    return None
                moved = inspector_call(pid, "input.pointer", {"params": {"world": [at[0], at[1] + height, at[2]],
                                                                         "timing": "immediate"}})
                # The hand takes the pick of the last frame that was drawn, so a frame must draw with the pointer there
                time.sleep(0.15)
                pick = inspector_call(pid, "players.pick", {})
                pointer = moved.get("pointer") if isinstance(moved, dict) else None
                if os.environ.get("NET_CHECK_DEBUG"):
                    print("hold debug", yaw, height, json.dumps(moved)[:160], pick.get("object") if isinstance(pick, dict) else pick)
                if isinstance(pick, dict) and pick.get("object") == creature and isinstance(pointer, list):
                    if action:
                        inspector_call(pid, "input.button", {"params": {"button": "right", "action": action}})
                    return [int(value) for value in pointer]
        return None

    # The creature walks among the houses and the people: the search tries again a while later
    # The hand takes the pick of the last frame that was drawn, and the creature walks: a press can come when the pick
    # has left the creature. The hand holds the creature when the creature systems say so, else the press is let go and
    # tried again.
    pixel = None
    for _ in range(8):
        pixel = pixel_on_creature("press")
        if pixel is not None:
            time.sleep(1.0)
            if hand_holds(pid) == creature:
                break
            inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
            time.sleep(1.0)
            pixel = None
            continue
        time.sleep(4.0)
    if pixel is None:
        print("hold: the hand did not hold creature", creature)
        return
    # The hand goes round a little on the body while it holds the creature
    for dx, dy in ((10, 0), (0, 10), (-10, 0), (0, -10), (0, 0)):
        inspector_call(pid, "input.pointer", {"params": {"screen": [pixel[0] + dx, pixel[1] + dy]}})
        time.sleep(0.3)
    # With slaps, the hand strokes the body for some seconds, then each slap is a fast sweep across the middle of the
    # screen. Two slaps come close together, so one slap comes while the reaction to the other waits for its turn.
    if slaps > 0:
        for step in range(12):
            inspector_call(pid, "input.pointer", {"params": {"screen": [pixel[0], pixel[1] + (20 if step % 2 else -20)]}})
            time.sleep(0.3)
        for index in range(slaps):
            side = 400 if index % 2 == 0 else -400
            inspector_call(pid, "input.pointer", {"params": {"screen": [640 + side, pixel[1]], "timing": "immediate"}})
            time.sleep(0.05 if index % 2 == 0 else 1.2)
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
    # The camera flies back to where it was before the hold
    time.sleep(8.0)
    # A click on the creature puts the leash on
    pixel = pixel_on_creature("click")
    if pixel is None:
        print("hold: the pointer found no pixel on creature", creature, "for the leash")
        return
    time.sleep(2.0)
    # A tap on the land below leads the creature there
    inspector_call(pid, "input.pointer", {"params": {"screen": [pixel[0], min(pixel[1] + 300, 1000)], "timing": "immediate"}})
    time.sleep(0.5)
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "click"}})
    print(f"hold: held creature {creature} at pixel {pixel}, clicked it and tapped the land")


def items_of(answer):
    return answer.get("items", []) if isinstance(answer, dict) else answer if isinstance(answer, list) else []


def wait_for(test, seconds):
    """The first true value of the test within the seconds, or None"""
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        value = test()
        if value:
            return value
        time.sleep(0.5)
    return None


def pixel_on(pid, entity, heights=(0.5, 1.0, 2.0, 3.0, 5.0)):
    """Looks at a thing from each side in turn and gives a pixel where the interface picks it, or None"""
    for yaw in (0, 90, 180, 270):
        inspector_call(pid, "camera.frame", {"params": {"id": entity, "yaw": yaw, "pitch": 40, "distance": 30}})
        time.sleep(1.0)
        found = inspector_call(pid, "ecs.entity", {"params": {"id": entity}})
        at = found.get("position") if isinstance(found, dict) else None
        if not isinstance(at, list):
            return None
        for height in heights:
            moved = inspector_call(pid, "input.pointer", {"params": {"world": [at[0], at[1] + height, at[2]],
                                                                     "timing": "immediate"}})
            time.sleep(0.3)
            pick = inspector_call(pid, "players.pick", {})
            pointer = moved.get("pointer") if isinstance(moved, dict) else None
            if isinstance(pick, dict) and pick.get("object") == entity and isinstance(pointer, list):
                return [int(value) for value in pointer]
    return None


def give_to_creature(pid, log):
    """The hand of a client takes a villager near its god's creature and gives it to the creature, as a player does: a
    hold of the Action button on the villager takes it, and a press on the creature gives it. The log of the client
    must tell the take of the villager by the hand of its god, else the hand tries the next villager. The camera looks at each
    thing first, and the pointer goes to a pixel where the interface picks it. Returns what it did."""
    done = {"creature": None, "object": None, "took": False, "gave": False}
    own = [item for item in items_of(inspector_call(pid, "players.list", {})) if item.get("local")]
    creature = own[0].get("primary_creature") if own else None
    if creature is None:
        done["why"] = "the god of the client has no creature"
        return done
    done["creature"] = creature
    player = own[0].get("player")
    found = inspector_call(pid, "ecs.entity", {"params": {"id": creature}})
    at = found.get("position") if isinstance(found, dict) else None
    if not isinstance(at, list):
        done["why"] = "the creature has no place"
        return done
    # The hand takes only within the influence of its god: the people of the god's own towns come first, then the
    # people near the creature
    places = [town["position"] for town in items_of(inspector_call(pid, "town.list", {}))
              if town.get("owner") == player and isinstance(town.get("position"), list)]
    villagers = []
    for place, radius in [(town, 80) for town in places] + [(at, 400)]:
        for villager in items_of(inspector_call(pid, "objects.find", {"params": {"kind": "Villager"},
                                                                      "near": [place[0], place[2]], "radius": radius,
                                                                      "limit": 12})):
            if all(villager["id"] != known["id"] for known in villagers):
                villagers.append(villager)
    done["villagers_near"] = len(villagers)
    def press_on_walker(entity):
        """The villager walks: the pointer goes to its place now and the press follows at once, when the interface
        picks it there. Whether the press came."""
        for height in (1.0, 0.5, 1.5, 2.0):
            found = inspector_call(pid, "ecs.entity", {"params": {"id": entity}})
            place = found.get("position") if isinstance(found, dict) else None
            if not isinstance(place, list):
                return False
            inspector_call(pid, "input.pointer", {"params": {"world": [place[0], place[1] + height, place[2]],
                                                             "timing": "immediate"}})
            state = inspector_call(pid, "input.state", {})
            pick = state.get("pick") if isinstance(state, dict) else None
            if isinstance(pick, dict) and pick.get("object") == entity:
                grab = pick.get("grab") or {}
                if not grab.get("takes"):
                    # The hand would not take it here: the reason goes to the summary
                    reasons = done.setdefault("why_not", [])
                    if grab.get("why_not") not in reasons:
                        reasons.append(grab.get("why_not"))
                    return False
                inspector_call(pid, "input.button", {"params": {"button": "right", "action": "press"}})
                time.sleep(1.0)
                inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
                return True
        return False

    for villager in villagers:
        inspector_call(pid, "camera.frame", {"params": {"id": villager["id"], "yaw": 0, "pitch": 40, "distance": 30}})
        time.sleep(1.0)
        for _ in range(8):
            if not press_on_walker(villager["id"]):
                continue
            done.setdefault("tries", []).append(villager["id"])
            if wait_for(lambda: lines_with(log, [f"Hand deed: the hand of player {player} takes thing "
                                                 f"{villager['id']} "]), 6):
                done["object"] = villager["id"]
                done["took"] = True
            break
        if done["took"]:
            break
    if not done["took"]:
        done["why"] = "the pointer found no villager near the creature"
        return done
    pixel = pixel_on(pid, creature, heights=(1.0, 2.0, 3.0, 4.0, 5.0))
    if pixel is None:
        done["why"] = "the pointer found no pixel on the creature"
        return done
    inspector_call(pid, "input.pointer", {"params": {"screen": pixel}})
    time.sleep(0.3)
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "press"}})
    time.sleep(0.5)
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
    done["gave"] = True
    print(f"give: client with pid {pid} gives thing {done['object']} to creature {creature}")
    return done


def slot_totem(pid, player):
    """The hand of the client's god takes hold of a totem of its own towns that it can take hold of, slides it to the top
    and lets go, so that the town's people go to worship and the worship site gathers prayer power. Returns what it
    did."""
    done = {}
    # The totems of the god's towns, the town with the most homes first, as each totem stands by its town's centre
    towns = [town for town in items_of(inspector_call(pid, "town.list", {})) if town.get("owner") == player]

    def town_of(totem):
        at = totem.get("position") or [0.0, 0.0, 0.0]
        return min(towns, key=lambda town: (town["position"][0] - at[0]) ** 2 + (town["position"][2] - at[2]) ** 2,
                   default=None)

    totems = [totem for totem in items_of(inspector_call(pid, "living.totems", {})) if town_of(totem) is not None]
    totems.sort(key=lambda totem: -town_of(totem).get("homes", 0))
    for totem in totems:
        for part in (totem.get("id"), totem.get("icon")):
            if part is None:
                continue
            pixel = pixel_on(pid, part)
            if pixel is None:
                continue
            state = inspector_call(pid, "input.state", {}) or {}
            grip = (state.get("pick") or {}).get("totem") if isinstance(state, dict) else None
            if not isinstance(grip, dict) or not grip.get("grips"):
                done.setdefault("refused", []).append({"totem": totem.get("id"), "why": (grip or {}).get("why_not")})
                break
            done["totem"] = totem.get("id")
            done["pixel"] = pixel
            inspector_call(pid, "input.button", {"params": {"button": "right", "action": "press"}})
            time.sleep(1.0)
            # Up the screen from the hand in steps: the game puts the pointer back on the hand each frame, so each step
            # starts where the hand now is
            for _ in range(20):
                state = inspector_call(pid, "input.state", {}) or {}
                at = state.get("pointer") if isinstance(state, dict) else None
                at = at if isinstance(at, list) else pixel
                inspector_call(pid, "input.pointer", {"params": {"screen": [at[0], max(at[1] - 100, 0)],
                                                                 "timing": "immediate"}})
                time.sleep(0.3)
            inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
            time.sleep(3.0)
            after = [found for found in items_of(inspector_call(pid, "living.totems", {}))
                     if found.get("id") == totem.get("id")]
            done["after"] = after[0] if after else None
            return done
    return done


def inspector_answer(pid, query, request):
    """As inspector_call, but a failed request gives its error, so that a summary shows why"""
    adapter = os.path.join(HERE, "..", "inspector", "openblack_inspector_mcp.py")
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    done = subprocess.run([sys.executable, adapter, "--pid", str(pid), "--call", query, json.dumps(request)],
                          timeout=120, capture_output=True, text=True, creationflags=flags)
    try:
        answer = json.loads(done.stdout)
    except ValueError:
        return {"error": (done.stdout + done.stderr)[-300:]}
    return answer.get("result") if answer.get("ok") else {"error": answer.get("error")}


def slot_fight(pids, seconds):
    """The first client sends the fight cheat, so that the creature of its god fights the nearest creature: the creature
    of the other god. The server must take test commands. The fight autopilot of each client then fights for the god
    of that client with the presses of its hand, and each client draws the star of the special move once its hand
    directs the fight. Every input goes through the inspector's input, so each client sends its commands as a player
    would. Returns what each client did."""
    done = {"cheat": inspector_answer(pids[0], "creatures.fight_cheat", {}), "clients": []}
    for index, pid in enumerate(pids):
        # The fight view starts as the camera looks at the arena: a double click would grip the land
        started = inspector_answer(pid, "autopilot.start",
                                   {"params": {"policy": "random", "seed": index + 1, "enter_view": False}})
        done["clients"].append({"autopilot": started, "fighter": None, "star": None, "state": None})
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        for index, pid in enumerate(pids):
            client = done["clients"][index]
            systems = inspector_call(pid, "creatures.systems", {})
            fighter = systems.get("players_fighter") if isinstance(systems, dict) else None
            state = inspector_call(pid, "autopilot.state", {})
            if fighter is not None and client["fighter"] is None:
                # The camera looks at its god's fighter from within the arena, so that the fight view starts by the look
                client["framed"] = inspector_answer(
                    pid, "camera.frame", {"params": {"id": fighter, "yaw": 90 * index, "pitch": 60, "distance": 8}})
            if fighter is not None:
                client["fighter"] = fighter
            if isinstance(state, dict):
                client["state"] = state
            # The star waits for presses that the fight took, so that the hand directs the fight
            if (client["star"] is None and fighter is not None and isinstance(state, dict)
                    and state.get("accepted", 0) >= 3):
                client["star"] = inspector_answer(pid, "input.gesture", {"params": {"name": "Star"}})
        time.sleep(1.0)
    for pid in pids:
        inspector_call(pid, "autopilot.stop", {})
    # Each client keeps the transcript of the fight: the moves that the turns put in each creature's queue, and from
    # where (the hand or a gesture). Every computer must have the same moves at the same turns.
    for index, pid in enumerate(pids):
        answer = inspector_answer(pid, "fights.transcript",
                                  {"params": {"records": 3000, "kinds": "move_queued"}, "max_bytes": 4000000})
        entries = answer.get("entries", []) if isinstance(answer, dict) else []
        moves = [[entry["turn"], entry["fields"].get("creature"), entry["fields"].get("move"),
                  entry["fields"].get("source")] for entry in entries]
        counts = {}
        for _, creature, _, source in moves:
            counts[f"{creature}:{source}"] = counts.get(f"{creature}:{source}", 0) + 1
        done["clients"][index]["queued"] = counts
        done["clients"][index]["moves"] = moves
    done["moves_same"] = len(pids) == 2 and done["clients"][0]["moves"] == done["clients"][1]["moves"]
    # Each creature learns its lean from the moves of its god on every computer: the same lean at the same turns
    for index, pid in enumerate(pids):
        answer = inspector_answer(pid, "fights.transcript",
                                  {"params": {"records": 3000, "kinds": "learning"}, "max_bytes": 4000000})
        entries = answer.get("entries", []) if isinstance(answer, dict) else []
        learnt = [[entry["turn"], entry["fields"].get("creature"), entry["fields"].get("move"),
                   entry["fields"].get("before"), entry["fields"].get("after")] for entry in entries
                  if entry["fields"].get("what") == "tendency"]
        leans = {}
        for _, creature, _, before, after in learnt:
            lean = leans.setdefault(str(creature), {"first": before, "last": after, "count": 0})
            lean["last"] = after
            lean["count"] += 1
        done["clients"][index]["leans"] = leans
        done["clients"][index]["learnt"] = learnt
    done["learnt_same"] = len(pids) == 2 and done["clients"][0]["learnt"] == done["clients"][1]["learnt"]
    leans = done["clients"][0].get("leans", {})
    done["both_learnt"] = len(leans) == 2 and all(lean["first"] != lean["last"] for lean in leans.values())
    return done


def slot_miracle(pid, player):
    """The hand of a client in any god slot raises the totem of one of its towns, so that its worship site gathers
    prayer power, then asks an icon of its own worship site for a miracle and casts it on the land, then draws the leash
    gesture and a scribble that shakes the leash off. Every step goes through the inspector's input, so the client sends
    its commands as a player would. Returns what each step did."""
    done = {"totem": slot_totem(pid, player)}

    def site_prayer():
        """The prayer power stored at the god's worship sites, with their dancers, once some is stored"""
        sites = [site for site in items_of(inspector_call(pid, "worship.sites", {})) if site.get("player") == player]
        found = {site["id"]: {"dancers": site.get("dancers"), "battery": site.get("battery")} for site in sites}
        return found if any((site.get("battery") or 0.0) > 0.0 for site in sites) else None

    # The worshippers walk to the site and dance; the site stores prayer power while they dance
    done["prayer"] = wait_for(site_prayer, 300)
    done["miracle"] = slot_cast(pid, player)
    near = done["miracle"].get("near")

    def leashes():
        """Whether each creature near the god's worship site wears a leash, and which"""
        if near is None:
            return None
        found = items_of(inspector_call(pid, "objects.find", {"params": {"kind": "Creature"}, "near": near,
                                                            "radius": 2000, "limit": 4}))
        result = {}
        for creature in found:
            leash = inspector_call(pid, "creatures.leash", {"params": {"id": creature["id"]}})
            if isinstance(leash, dict):
                result[creature["id"]] = {key: leash.get(key) for key in ("leashed", "worn", "picked", "tied")}
        return result

    done["leash_before"] = leashes()
    # The leash gesture (the square spiral) puts the leash on the creature, and a scribble of the empty hand shakes it off
    for name in ("square spiral", "scribble"):
        drawn = inspector_call(pid, "input.gesture", {"params": {"name": name}})
        time.sleep(3.0)
        gestures = inspector_call(pid, "players.gestures", {})
        done[f"gesture_{name}"] = {"drawn": drawn, "last": (gestures or {}).get("last_recognised")
                                   if isinstance(gestures, dict) else gestures, "leash": leashes()}
    print("slot miracle:", json.dumps(done)[:3000])
    return done


def slot_icons(pid, player):
    """The icons of the worship sites of the towns of the player"""
    icons = []
    for town in items_of(inspector_call(pid, "town.list", {})):
        if town.get("owner") != player:
            continue
        spells = inspector_call(pid, "town.spells", {"params": {"id": town.get("id")}})
        icons += (spells or {}).get("site_icons", []) if isinstance(spells, dict) else []
    return icons


def slot_cast(pid, player):
    """The tap on the icon with the most prayer power stored, and the cast of the miracle that comes into the hand"""
    done = {}
    icons = slot_icons(pid, player)
    # The icon with the most prayer power stored; with none stored, a miracle of low cost first (water, heal, food,
    # wood), so that the site's prayer power charges it soon
    cheap = [9, 7, 3, 8]
    icon = max(icons, key=lambda found: (found.get("store", 0.0),
                                         -cheap.index(found.get("seed")) if found.get("seed") in cheap else -len(cheap))
               ) if icons else None
    done["icons"] = icons
    if icon is None:
        return done
    place = inspector_call(pid, "ecs.entity", {"params": {"id": icon["id"]}})
    at = place.get("position") if isinstance(place, dict) else None
    done["near"] = [at[0], at[2]] if isinstance(at, list) else None
    pixel = pixel_on(pid, icon["id"])
    done["icon_pixel"] = pixel
    if pixel is None:
        return done
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "click"}})
    time.sleep(3.0)
    done["tapped"] = [found for found in slot_icons(pid, player) if found.get("id") == icon["id"]]
    done["held_seed"] = wait_for(lambda: (inspector_call(pid, "magic.state", {}) or {}).get("held_seed"), 240)
    if not done["held_seed"]:
        return done
    # The seed waits until it is ready, then the hand casts it on the land below the icon
    time.sleep(6.0)
    spells_before = (inspector_call(pid, "magic.state", {}) or {}).get("spells")
    inspector_call(pid, "input.pointer", {"params": {"screen": [pixel[0], min(pixel[1] + 200, 1000)], "timing": "immediate"}})
    time.sleep(0.5)
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "press"}})
    time.sleep(1.0)
    # A miracle cast from the hand while the button is down (as water pours) shows among the spells then
    spells_held = (inspector_call(pid, "magic.state", {}) or {}).get("spells")
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
    time.sleep(3.0)
    state = inspector_call(pid, "magic.state", {}) or {}
    done["cast"] = {"spells_before": spells_before, "spells_while_held": spells_held, "spells_after": state.get("spells"),
                    "held_after": state.get("held_seed"), "last_hand_result": state.get("last_hand_result")}
    return done


def leash_state(pid):
    """The leash of the creature of the god of a desktop client, as the inspector tells it"""
    leash = inspector_call(pid, "creatures.leash", {})
    if not isinstance(leash, dict):
        return leash
    return {key: leash.get(key) for key in ("id", "leashed", "type", "tied_to")}


def press_key(pid, key, tap):
    """Presses a key of a desktop client through the inspector. A tap holds the key down for one frame; else the key
    stays down for half a second, as a player presses it. Gives the answer of the inspector to the key down."""
    if tap:
        pressed = inspector_call(pid, "input.key", {"params": {"key": key, "how": "tap"}})
    else:
        pressed = inspector_call(pid, "input.key", {"params": {"key": key, "how": "press"}})
        time.sleep(0.5)
        inspector_call(pid, "input.key", {"params": {"key": key, "how": "release"}})
    time.sleep(1.0)
    return pressed


def box_keys_check(pid, log, tap=False):
    """Presses the leash key L and the temple key F4 of a desktop client while the information box of the land shows.
    Tells whether the box still shows after the keys, whether the player went into the temple, the leash after the
    keys, and the turns that the client compared when the check ended."""
    for key in ("L", "F4"):
        press_key(pid, key, tap)
    time.sleep(1.0)
    windows = items_of(inspector_call(pid, "gui.windows", {}))
    interior = inspector_call(pid, "temple.interior", {})
    counts = session_counts(log)
    return {"open_after_keys": any(window.get("name") == "land_info_box" and window.get("open") for window in windows),
            "in_temple": interior.get("active") if isinstance(interior, dict) else None,
            "leash": leash_state(pid), "checked_after_turns": counts[0] if counts else None}


def box_kept_keys(box, server_commands):
    """Whether the information box of the land kept the keys L and F4 from the controls: the box still showed after the
    keys, the player stayed out of the temple, the creature wore no leash, and the server got no leash command of the
    slot up to the turn of the check. None when the box did not show, so there was nothing to check."""
    if box is None:
        return None
    leash = box.get("leash")
    checked = box.get("checked_after_turns")
    return (box.get("open_after_keys") is True and box.get("in_temple") is False and isinstance(leash, dict)
            and not leash.get("leashed") and checked is not None
            and not any(command[0] <= checked for command in server_commands))


def desktop_leash_step(leash, done, pid, log):
    """Presses the next keys of a leash check on a desktop client once its log tells the turns of the key. Gives True
    when the check pressed every key."""
    presses = leash_key_presses(leash)
    while len(done) < len(presses):
        turns, key, before = presses[len(done)]
        if not lines_with(log, [f"Network game: {turns} turns compared"]):
            return False
        time.sleep(before)
        if not done:
            # The information box of the land keeps the keys from the controls while it shows. The Escape key closes
            # it, as Go! does: the inspector presses no button of a window in a network game.
            windows = items_of(inspector_call(pid, "gui.windows", {}))
            box = any(window.get("name") == "land_info_box" and window.get("open") for window in windows)
            print(f"leash: the information box of the land shows on the client with pid {pid}: {box}")
            if box:
                # While the box shows, the leash key and the temple key must do nothing
                leash["box"] = box_keys_check(pid, log, leash.get("tap", False))
                print(f"leash: L and F4 with the information box open on the client with pid {pid}: "
                      f"{json.dumps(leash['box'])[:400]}")
                press_key(pid, "Escape", leash.get("tap", False))
                time.sleep(1.0)
        pressed = press_key(pid, key, leash.get("tap", False))
        done.append({"turns": turns, "key": key, "pressed": pressed, "leash": leash_state(pid)})
        print(f"leash: client with pid {pid} pressed {key} after {turns} turns: {json.dumps(done[-1])[:400]}")
    return True


def leash_result(leash, server_log, logs):
    """What the server and each client did with the leash of the god's creature. The server must put the leash on and
    take it off. Each client must tell the same leash commands of the slot, with the same leash after each, as the
    server. logs gives (name, path, first turn, last turn) for each client; a turn of None sets no limit."""
    slot = leash["slot"]
    server = [command for command in sim_report.leash_commands_in_file(server_log) if command[1] == slot]
    on = [command for command in server if command[0] >= leash["turn"] and command[2] == "presses the leash key"
          and " wears leash " in command[3]]
    off_words = "goes into the temple" if leash["off"] == "temple" else "presses the leash key"
    off = [command for command in server if command[0] >= leash["turn"] + leash["hold"] and command[2] == off_words
           and command[3].endswith("wears no leash")]
    result = {"client": "web" if leash["client"] == "web" else leash["client"] + 1, "slot": slot,
              "server": [list(command) for command in server], "on": bool(on), "off": bool(off),
              "held_turns": off[0][0] - on[0][0] if on and off else None, "clients": {},
              "box": leash.get("box"), "box_kept_keys": box_kept_keys(leash.get("box"), server)}
    same = bool(on) and bool(off) and result["box_kept_keys"] is not False
    for name, path, first, last in logs:
        mine = [command for command in sim_report.leash_commands_in_file(path, last) if command[1] == slot]
        theirs = [command for command in server if last is None or command[0] <= last]
        mine = sim_report.leash_commands_from_turn(mine, first)
        theirs = sim_report.leash_commands_from_turn(theirs, first)
        result["clients"][name] = mine == theirs
        same = same and mine == theirs
        if mine != theirs:
            print(f"client {name} tells other leash commands of slot {slot} than the server: {mine} / {theirs}")
    result["same"] = same
    return result


SLOT_LEAVES = re.compile(r"The player of slot (\d+) leaves the game")


def box_back_leave(client_lines, server_lines, slot):
    """Whether a click of Back in the information box of the land left the network game as Leave Game does. The client
    clicked Back while the box showed, Back left the network game, and the first slot that the server saw leave is the
    slot of the client. A god that stays can win a game of two gods: its client then stops and leaves after."""
    pressed = any("Tests: the player presses Back in the land information box" in line for line in client_lines)
    back_left = any("The land information box: Back leaves the network game" in line for line in client_lines)
    left = any("The player leaves the network game" in line for line in client_lines)
    slots = [int(match.group(1)) for line in server_lines if (match := SLOT_LEAVES.search(line))]
    return {"pressed": pressed, "back_left": back_left, "left": left, "server_slots": slots,
            "same": pressed and back_left and left and slots[:1] == [slot]}


def box_back_result(client_log, server_log, client):
    """What the client that clicked Back and the server wrote of the leave. The desktop gods join in the order of their
    numbers, so the client from 0 plays the slot of the same number."""
    result = box_back_leave(lines_with(client_log, ["Tests: the player presses Back", "Back leaves the network game",
                                                    "The player leaves the network game"]),
                            lines_with(server_log, ["leaves the game"]), client)
    result["client"] = client + 1
    return result


def write_kept_alignment(folder, alignment):
    """Writes the list of profiles of a server or a client whose profile kept an alignment from an earlier game. The
    world of each game must start the same on every computer with any value."""
    os.makedirs(folder, exist_ok=True)
    lines = ["current\tPlayer", "profile\tPlayer\tC4fcabbcd.erc\t0\tPlayer", f"alignment\tPlayer\t{alignment!r}"]
    with open(os.path.join(folder, "Profiles.txt"), "w", encoding="utf-8", newline="\n") as file:
        file.write("\n".join(lines) + "\n")


def leave_room(game_exe, options, url, key, kid, certs, out, server_log):
    """A client joins the room of the server as a god and leaves it before the start, as a player who closes the page
    in the room does. The server must then start the next game from the same world as a server that had no visitor."""
    leaver_dir = os.path.join(out, "leaver")
    os.makedirs(leaver_dir, exist_ok=True)
    log = os.path.join(leaver_dir, "client.log")
    if os.path.exists(log):
        os.remove(log)
    ticket = make_ticket(key, kid, 99, "Leaver", "god", 600)
    command = [game_exe, "-g", options.game_path, "--skip-logos", "--sim-still-hand", "-n", "4000000000",
               "--background", "-l", log, "--join", url, "--join-ticket", ticket, "--join-test-ca", certs["ca.pem"]]
    leaver = start(command, leaver_dir)
    try:
        if not wait_for_line(log, "Joined the game server", leaver):
            print(f"the client that leaves the room did not join; see {leaver_dir}")
            return False
        # The client loads the land of the server in the room, as the page does
        wait_for_line(log, "the player is ready for the start", leaver, limit=120)
    finally:
        stop(leaver)
    end = time.monotonic() + 60
    while not lines_with(server_log, ["left the room"]):
        if time.monotonic() > end:
            print(f"the server did not see the leave of the room; see {server_log}")
            return False
        time.sleep(0.5)
    time.sleep(options.leave_room_wait)
    return True


def box_closing_events(frame):
    """The input that closes the information box of the land at a frame: the Escape key, as Go! does. The box shows at
    the start of a network game on a land whose script opens it, and it takes every press of the buttons until the player
    closes it."""
    return [{"frame": frame, "event": {"kind": "key_down", "timing": "mouse", "key": "Escape"}},
            {"frame": frame + BOX_KEY_FRAMES, "event": {"kind": "key_up", "timing": "mouse", "key": "Escape"}}]


def shifted_input(path, folder, extra=0, camera=None, close_box=False):
    """The recorded input with its frames moved after the start of the network game, and the extra frames. A camera (JSON
    as camera_set takes it) goes into the input, in place of a camera of the file: the client puts its camera there as
    its land starts, in place of the start view and its flight to the temple. With close_box, the input first closes the
    information box of the land, before the first event of the file."""
    with open(path, encoding="utf-8") as file:
        value = json.load(file)
    events = value.get("events", []) if isinstance(value, dict) else value
    for event in events:
        event["frame"] = event["frame"] + INPUT_OFFSET_FRAMES + extra
    if close_box:
        first = min((event["frame"] for event in events), default=INPUT_OFFSET_FRAMES + extra)
        events = box_closing_events(first - BOX_CLOSE_LEAD_FRAMES) + events
    if isinstance(value, dict):
        value["events"] = events
    else:
        value = {"events": events}
    if camera:
        value["camera"] = json.loads(camera)
    out = os.path.join(folder, "input.json")
    with open(out, "w", encoding="utf-8") as file:
        json.dump(value, file)
    return out


def max_turn_ms(record):
    """The longest turn of a client in milliseconds, from the turn times in its run record (none without a time). The
    turn time does not include the time of the step that records the run (max_record_ms gives it)."""
    return longest_of(record, ("turn_ms_max", "turn_ms"))


def max_record_ms(record):
    """The longest time of the step that records the run of a client, in milliseconds (none without a time)"""
    return longest_of(record, ("record_ms_max", "record_ms"))


def longest_of(record, keys):
    """The largest value of the keys in the lines of a run record, none when no line has one"""
    longest = None
    if not os.path.exists(record):
        return longest
    with open(record, encoding="utf-8") as file:
        for line in file:
            try:
                value = json.loads(line)
            except ValueError:
                continue
            if not isinstance(value, dict):
                continue
            for key in keys:
                if isinstance(value.get(key), (int, float)):
                    longest = value[key] if longest is None else max(longest, value[key])
    return longest


def run_limit_seconds(turns):
    """The time that a run may take: the turns at the pace of the server, the load of the land, and the clients' catch up"""
    return max(RUN_LIMIT_SECONDS, int(turns * SECONDS_PER_TURN * 1.5) + LOAD_SECONDS)


def web_page_of(options):
    """The folder of the web page of this tree"""
    return options.web_page or os.path.join(DEFAULT_BUILDS, os.path.basename(worktree_root()) + "-web", "bin", "Release")


def spki_hash(pem_path):
    """The base64 SHA-256 of the public key of a certificate, as Chrome's --ignore-certificate-errors-spki-list takes it"""
    with open(pem_path, "rb") as file:
        cert = x509.load_pem_x509_certificate(file.read())
    der = cert.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    return base64.b64encode(hashlib.sha256(der).digest()).decode("ascii")


def free_port():
    """A free TCP port of 127.0.0.1"""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


WEB_WIDTH = 1280
# The height of the strip above the game in the web page (--badge-bar in web/page/page.css): the build, Join Online Game
# and Log have their own strip, so they lie over no control of the game
WEB_BADGE_BAR = 22
# The click on the speech button of the web client: the centre of the button, from the top right corner of the game
# canvas, which starts below the strip
WEB_SPEECH_FROM_CORNER = (16, 12)


# The keys of the leash check in the web page, as the page reads them
WEB_KEYS = {"L": {"key": "l", "code": "KeyL", "keyCode": 76}, "F4": {"key": "F4", "code": "F4", "keyCode": 115},
            "Escape": {"key": "Escape", "code": "Escape", "keyCode": 27}}
# The seconds in the temple before the Escape key takes the player out again
LEASH_TEMPLE_SECONDS = 8.0


def parse_leash(values, clients):
    """The leash checks of --leash, as dicts: the client (an index from 0, or "web"), the turn that the leash goes on,
    the turns that the hand holds it and how it comes off ("temple" or "key")"""
    leashes = []
    for value in values:
        parts = value.split(":")
        if len(parts) not in (3, 4):
            raise ValueError(f"--leash {value}: give CLIENT:TURN:HOLD[:OFF]")
        client = "web" if parts[0] == "web" else int(parts[0]) - 1
        turn, hold = int(parts[1]), int(parts[2])
        off = parts[3] if len(parts) == 4 else "temple"
        if off not in ("temple", "key"):
            raise ValueError(f"--leash {value}: the leash comes off by 'temple' or 'key'")
        if turn % 100 or hold % 100 or turn <= 0 or hold <= 0:
            raise ValueError(f"--leash {value}: the turn and the hold are multiples of 100 (the log lines of the turns)")
        if client != "web" and not 0 <= client < clients:
            raise ValueError(f"--leash {value}: no desktop god of that number")
        # The web god plays the slot after the desktop gods
        slot = clients if client == "web" else client
        leashes.append({"client": client, "slot": slot, "turn": turn, "hold": hold, "off": off})
    return leashes


def leash_key_presses(leash):
    """The keys of a leash check, as (turns, key, seconds before) tuples: L puts the leash on; after the hold, F4 goes
    into the temple and Escape comes out of it, or L takes the leash off"""
    off_turn = leash["turn"] + leash["hold"]
    if leash["off"] == "temple":
        return [(leash["turn"], "L", 0.0), (off_turn, "F4", 0.0), (off_turn, "Escape", LEASH_TEMPLE_SECONDS)]
    return [(leash["turn"], "L", 0.0), (off_turn, "L", 0.0)]


def decided_after_the_run(decided_at_turn, turns):
    """Whether the game was decided only after the last turn that the clients play. Then the clients stop at their own
    last turn, before the stop of the turns of the game."""
    return decided_at_turn is not None and decided_at_turn > turns


def land_shows_info_box(game_path, land):
    """Whether the land script opens the information box of the land at the start (its game message). In a network
    game the box stays open until the player closes it, and it keeps the keys from the controls meanwhile."""
    if not land.lower().endswith(".txt"):
        return False
    try:
        with open(os.path.join(game_path, "Scripts", land), encoding="latin-1") as file:
            return any(line.strip().upper().startswith("START_GAME_MESSAGE") for line in file)
    except OSError:
        return False


def web_leash_presses(leash, close_box):
    """The keys of a leash check of the web god: an Escape first closes the information box of the land when it shows"""
    presses = leash_key_presses(leash)
    if close_box:
        presses = [(leash["turn"], "Escape", 0.0)] + presses
    return presses


def web_chat_steps(chats, seconds, width=WEB_WIDTH, leashes=(), close_box=False):
    """The cdp-run steps of the lines that the web client says, from (turns, text) pairs: for each line in the order of
    its turns, the page waits for the line of those turns in its log, then the mouse clicks the speech button at the top
    right corner, the keys type the line and Return sends it. A wait moves the next steps later by its time. The keys of
    the leash checks (leash_key_presses) go in the same order of turns."""
    steps = []
    at = 0.0
    events = sorted([(turns, 0, number, text) for number, (turns, text) in enumerate(sorted(chats))] +
                    [(turns, 1, order, (key, before)) for leash in leashes
                     for order, (turns, key, before) in enumerate(web_leash_presses(leash, close_box))],
                    key=lambda event: (event[0], event[1], event[2]))
    waited = None
    for turns, kind, number, value in events:
        if waited != turns:
            steps.append({"at": at, "do": "wait-log", "text": f"Network game: {turns} turns compared", "seconds": seconds})
            at += 0.5
            waited = turns
        if kind == 1:
            key, before = value
            at += before
            steps.append(dict({"at": at, "do": "key", "hold": 300}, **WEB_KEYS[key]))
            at += 1.0
            steps.append({"at": at, "do": "shot", "name": f"leash-{turns}-{key.lower()}"})
            at += 0.5
            continue
        text = value
        for kind in ("mouseMoved", "mousePressed", "mouseReleased"):
            steps.append({"at": at, "do": "mouse", "type": kind, "x": width - WEB_SPEECH_FROM_CORNER[0],
                          "y": WEB_BADGE_BAR + WEB_SPEECH_FROM_CORNER[1],
                          "button": "none" if kind == "mouseMoved" else "left"})
            at += 0.1
        steps.append({"at": at, "do": "shot", "name": f"chat{number}-open"})
        at += 1.0
        for letter in text:
            steps.append({"at": at, "do": "key", "key": letter})
            at += 0.05
        steps.append({"at": at, "do": "shot", "name": f"chat{number}-typed"})
        at += 0.5
        steps.append({"at": at, "do": "key", "key": "Enter", "code": "Enter", "keyCode": 13})
        at += 0.5
    return steps


def web_join_path(join_url):
    """The path of the game socket that the page opens on its own origin: the web build opens no socket to another
    origin, so the server of the page passes this path to the game server, as the proxy of the site does"""
    return "/" + join_url.split("://", 1)[1].split("/", 1)[1]


def web_join_plan(options, join_url, ticket, turns):
    """What the web client needs to play: the address of the game server, the arguments of the game in the page, the
    steps of its input, the turns and the time limit of the run. A remote web client gets it as web/join.json."""
    arguments = ["--skip-logos", "--sim-frame-ms", str(options.frame_ms or 20.0), "--sim-until-turn", str(turns),
                 "--sim-still-hand", "--sim-checksums", "-", "--join", web_join_path(join_url), "--join-ticket", ticket]
    if getattr(options, "web_part_at", None) is not None:
        arguments += ["--join-part-at", str(options.web_part_at)]
    if getattr(options, "web_creature", None):
        # The page reads the game folder at its own path
        arguments += ["--join-creature", "/game/" + options.web_creature.replace("\\", "/")]
    seconds = run_limit_seconds(turns)
    chats = [(int(value.split(":", 1)[0]), value.split(":", 1)[1]) for value in options.web_chat]
    leashes = [leash for leash in parse_leash(getattr(options, "leash", []), getattr(options, "clients", 0))
               if leash["client"] == "web"]
    return {"join_url": join_url, "arguments": arguments,
            "actions": web_chat_steps(chats, seconds, leashes=leashes,
                                      close_box=bool(leashes) and getattr(options, "web_close_box", False)),
            "turns": turns,
            "seconds": seconds}


def run_web_plan(plan, folder, page, chrome, game_path, processes):
    """One web client in headless Chrome: a server for the web page on 127.0.0.1, and cdp-run.mjs, which starts the Chrome
    with its own profile folder in the folder of the run. The server of the page has a test certificate of its own and
    passes the game socket of the page to the game server of the plan. The page writes the checksum of each turn
    (--sim-checksums -) to checksums.jsonl. Returns the cdp-run process."""
    os.makedirs(folder, exist_ok=True)
    checksums = os.path.join(folder, "checksums.jsonl")
    if os.path.exists(checksums):
        os.remove(checksums)
    port = free_port()
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    # The page is an HTTPS page, and its game socket goes to its own origin, which passes it to the game server
    os.makedirs(os.path.join(folder, "page-keys"), exist_ok=True)
    certs = make_certificates(os.path.join(folder, "page-keys"))
    server = subprocess.Popen([sys.executable, os.path.join(ROOT, "web", "scripts", "serve.py"), page, str(port),
                               "--tls", certs["server.pem"], certs["server.key"],
                               "--game-proxy", plan["join_url"].split("/game/")[0]],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
    processes.append(server)
    time.sleep(1.0)
    query = "autoplay&args=" + "%20".join(plan["arguments"])
    url = f"https://127.0.0.1:{port}/index.html?{query}"
    command = [DEFAULT_NODE, os.path.join(ROOT, "web", "scripts", "cdp-run.mjs"), chrome, url, str(plan["seconds"]),
               folder, "--game-folder", game_path, "--checksums", checksums, "--checksum-turns", str(plan["turns"]),
               "--trust-spki", spki_hash(certs["server.pem"]), "--size", f"{WEB_WIDTH}x1024"]
    if plan["actions"]:
        actions = os.path.join(folder, "web_actions.json")
        with open(actions, "w", encoding="utf-8") as file:
            json.dump(plan["actions"], file, indent=1)
        command += ["--actions", actions]
    log = open(os.path.join(folder, "cdp-run.log"), "w", encoding="utf-8")
    process = subprocess.Popen(command, cwd=folder, stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
    processes.append(process)
    return process


def start_web_client(options, out, join_url, ticket, certs, turns, processes):
    """The web client of the run: in headless Chrome on this computer, or with --web-remote the plan in web/join.json
    for tools/sim/web_remote.py. Returns the folder of the run and the cdp-run process (none for a remote client)."""
    folder = os.path.join(out, "web")
    os.makedirs(folder, exist_ok=True)
    plan = web_join_plan(options, join_url, ticket, turns)
    if not options.web_remote:
        return folder, run_web_plan(plan, folder, web_page_of(options), options.chrome, options.game_path, processes)
    for name in ("done", "checksums.jsonl", "console.log", "page-log.txt"):
        if os.path.exists(os.path.join(folder, name)):
            os.remove(os.path.join(folder, name))
    # The file appears whole: the remote side never reads half of it
    with open(os.path.join(folder, "join.json.new"), "w", encoding="utf-8") as file:
        json.dump(plan, file)
    os.replace(os.path.join(folder, "join.json.new"), os.path.join(folder, "join.json"))
    print(f"web: the plan of the remote web client is in {os.path.join(folder, 'join.json')}")
    return folder, None


def web_log_of(folder):
    """The game log of the web client: cdp-run writes the log that the page keeps to page-log.txt at the end"""
    return os.path.join(folder, "page-log.txt")


def web_back_at(log):
    """The turn of the last world state of the server that a client loaded after a part, or None"""
    recovered = lines_with(log, ["The world goes on from the server's world"])
    match = re.search(r"of turn (\d+)", recovered[-1]) if recovered else None
    return int(match.group(1)) if match else None


def web_remote_done(folder):
    """Whether the remote web client copied its results back"""
    return os.path.exists(os.path.join(folder, "done"))


def web_checksums(folder):
    """The checksum of each turn that the web client wrote, by the turn"""
    turns = {}
    path = os.path.join(folder, "checksums.jsonl")
    if not os.path.exists(path):
        return turns
    with open(path, encoding="utf-8") as file:
        for line in file:
            try:
                value = json.loads(line)
            except ValueError:
                continue
            if isinstance(value, dict) and "turn" in value and "checksum" in value:
                turns[int(value["turn"])] = str(value["checksum"])
    return turns


def join_order(gods, clients, scenario):
    """The order in which the clients join: by their number, or the observers first for a scenario. The state of a
    scenario is not in the world, so its observers must be in the room before the start."""
    order = list(range(clients))
    return order[gods:] + order[:gods] if scenario else order


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--turns", type=int, default=600, help="the turns that each client plays")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--land", default="Land1.txt")
    parser.add_argument("--gods-ring-map", action="store_true", help="the network map for 32 gods")
    parser.add_argument("--testbed", action="store_true")
    parser.add_argument("--scenario", help="a testbed scenario (developer builds): the server lays it out before the first "
                                           "turn and gives it to each client with the start; every computer gives its "
                                           "commands before the same turns. The observers join the room before the start.")
    parser.add_argument("--opening", default="cow")
    parser.add_argument("--frame-ms", type=float, default=20.0,
                        help="the frame time of the clients (1 to 1000); 0 for their real frame time")
    parser.add_argument("--server-frame-ms", type=float, help="the frame time of the server (its own default with none)")
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--bin")
    parser.add_argument("--game-path", default=DEFAULT_GAME_PATH)
    parser.add_argument("--out", default=os.path.join(os.getcwd(), "net-check"))
    parser.add_argument("--input", help="recorded input for the first client's hand")
    parser.add_argument("--throw-lead-latency-ms", type=int, help="a fixed latency in ms for the reports of the hand, so that the lead of each throw is the same in each run")
    parser.add_argument("--input2", help="recorded input for the second client's hand (its god's own hand)")
    parser.add_argument("--input2-delay-frames", type=int, default=1500,
                        help="the second client's input starts this many frames later than the first's")
    parser.add_argument("--camera1", help="the camera of the first client before its input, as the inspector's "
                                           "camera_set takes it (JSON)")
    parser.add_argument("--camera2", help="the camera of the second client before its input (JSON)")
    parser.add_argument("--camera-again-after", type=int,
                        help="put the camera of each client (--camera1 to --camera4) there again once the client "
                        "compared this many turns (a multiple of 100), because a land's scripts can move the camera "
                        "after the start")
    parser.add_argument("--row-dump", action="store_true",
                        help="the server and each client keep the rows of the synced world of the last turns and write "
                        "the rows of a turn at which a client parts (server/rows.txt, clientN/rows.txt); the summary "
                        "names the first different entity, component and field of each client (row_dump.py)")
    parser.add_argument("--world-states", metavar="TURNS",
                        help="the server and each desktop client write their world state at the end of these turns "
                             "(such as 250 or 100,250), and the synced parts of each client must be the same as the "
                             "server's (save_diff.py --synced)")
    parser.add_argument("--row-dump-turn", type=int,
                        help="with --row-dump, the server and the clients also write the rows of this turn")
    parser.add_argument("--input3", help="recorded input for the third client's hand")
    parser.add_argument("--input4", help="recorded input for the fourth client's hand")
    parser.add_argument("--camera3", help="the camera of the third client before its input (JSON)")
    parser.add_argument("--camera4", help="the camera of the fourth client before its input (JSON)")
    parser.add_argument("--hold-creature", action="append", default=[],
                        help="client:x,z (client 1 or 2): once the client compared --hold-after turns, its hand holds "
                        "the creature nearest this point of the land through the inspector, strokes it, lets go, clicks "
                        "it for the leash and leads it with a tap on the land. Give it once for each client.")
    parser.add_argument("--creature", action="append", default=[],
                        help="CLIENT:FILE, the creature file that the client sends before the start, in place of the "
                             "creature of its profile (clients from 1; repeat for each client)")
    parser.add_argument("--hold-after", type=int, default=300, help="the turns before --hold-creature")
    parser.add_argument("--hold-slaps", type=int, default=0,
                        help="with --hold-creature, the hand also strokes the creature for some seconds and slaps it "
                             "this many times before it lets go")
    parser.add_argument("--chat", action="append", default=[],
                        help="TURNS:CLIENT:TEXT, the client (from 1) says the line in the chat after that many turns")
    parser.add_argument("--tattoo-menu", action="append", default=[], metavar="TURNS:CLIENT",
                        help="the client (from 1) opens the tattoo editor from the game's menu after that many turns")
    parser.add_argument("--profile-creature", action="append", default=[], metavar="CLIENT:FOLDER",
                        help="the profile of the client (from 1) keeps the creature files of the folder (Player.erc and "
                             "PhysiquePlayer.erc, as a game alone keeps them), so its menu shows Edit Tattoo")
    parser.add_argument("--slot-miracle", type=int, choices=(1, 2),
                        help="this client asks an icon of its god for a miracle, casts it and uses the leash gestures, "
                             "through the inspector, once it compared --hold-after turns")
    parser.add_argument("--leash", action="append", default=[], metavar="CLIENT:TURN:HOLD[:OFF]",
                        help="the client (from 1, or 'web' for the web god) presses L after TURN turns, so that the leash "
                             "goes on its god's creature, holds it HOLD more turns, then goes into the temple (F4, then "
                             "Escape; OFF 'temple', the default) or presses L again (OFF 'key'). TURN and HOLD are "
                             "multiples of 100. The server and every client must tell the same leash commands")
    parser.add_argument("--key-tap", action="store_true",
                        help="the leash checks of the desktop clients tap each key (down for one frame) in place of a "
                             "press of half a second")
    parser.add_argument("--give", type=int, metavar="CLIENT",
                        help="once the client (from 1) compared --hold-after turns, its hand takes a villager near its "
                             "god's creature and gives it to the creature through the inspector; the server and every "
                             "client must then tell the same gift (the deed 'gives')")
    parser.add_argument("--slot-fight", type=int, metavar="SECONDS",
                        help="once the first client compared --hold-after turns, it sends the fight cheat (its god's "
                             "creature fights the nearest creature); then both clients fight with the fight autopilot "
                             "and draw the star of the special move for this many seconds. The server takes test "
                             "commands. Needs a land where both gods have a creature near each other.")
    parser.add_argument("--part-second-at", type=int,
                        help="the second client's world parts from the server's at this turn (--join-part-at), and must "
                             "come back in step by the server's world state")
    parser.add_argument("--part-second-again", action="store_true",
                        help="with --part-second-at, the world parts again after each load of the server's world, so that "
                             "the part stays (tests of the AI soak: the runner must find it)")
    parser.add_argument("--part-client", type=int, default=2,
                        help="CLIENT (from 1): the client whose world parts with --part-second-at, 2 by default. A "
                             "number after the clients that play a god is an observer")
    parser.add_argument("--frame-guard", action="store_true",
                        help="each client logs the frame steps that change the synced world (--sim-frame-guard)")
    parser.add_argument("--client-arg", action="append", default=[],
                        help="CLIENT:ARGUMENT: one more argument for the client (from 1), such as 2:--frame-stats=600; "
                             "give the option more times for more arguments")
    parser.add_argument("--stop-second-after", type=float, help="stop the second client after this many seconds")
    parser.add_argument("--come-back-seconds", type=int, default=10)
    parser.add_argument("--restart-server-at", type=int,
                        help="stop the server by its process id after this turn, and start it again from its snapshots")
    parser.add_argument("--ban-account", type=int,
                        help="a fake lobby bans this account id; a client of that account must be refused before the "
                             "start (the other clients use the ids 100 and up)")
    parser.add_argument("--clients", type=int, choices=(0, 1, 2, 3, 4), default=2,
                        help="the number of clients that play a god (a god slot each)")
    parser.add_argument("--min-gods", type=int,
                        help="the fewest gods that start the game (the server's --min-gods); the clients with none")
    parser.add_argument("--observers", type=int, choices=(0, 1, 2, 3, 4), default=0,
                        help="the number of desktop clients that join as observers (they watch and compare the "
                             "checksum of each turn)")
    parser.add_argument("--web-client", action="store_true",
                        help="one more observer: the web build in headless Chrome (cdp-run.mjs). Needs the web build of "
                             "this tree (--web-page, tools/build/ob-web build)")
    parser.add_argument("--web-page", help="the folder of the web page (the web build's bin/Release)")
    parser.add_argument("--web-god", action="store_true",
                        help="with --web-client, the web client plays the god of the slot after the desktop gods")
    parser.add_argument("--web-chat", action="append", default=[],
                        help="TURNS:TEXT, with --web-god: the web client says the line in the chat after that many turns "
                             "(a multiple of 100)")
    parser.add_argument("--web-creature", metavar="FILE",
                        help="with --web-god, the creature file that the web god sends before the start, as a path in "
                             "the game folder of the page, such as Scripts/CreatureMind/C643e7b7f.erc")
    parser.add_argument("--web-part-at", type=int,
                        help="the world of the web client parts from the server's at this turn (--join-part-at), and "
                             "must come back in step by the server's world state")
    parser.add_argument("--web-remote", action="store_true",
                        help="with --web-client, start no Chrome: write web/join.json and wait for web/done, which "
                             "tools/sim/web_remote.py writes from another computer")
    parser.add_argument("--chrome", default=os.environ.get("OB_CHROME", "C:/Program Files/Google/Chrome/Application/chrome.exe"))
    parser.add_argument("--all-computer-gods", action="store_true",
                        help="the AI soak: the room has no human god, every god with a citadel is a computer god; all the "
                             "clients are observers (--observers, --web-client)")
    parser.add_argument("--ai-seed", type=int, help="the seed of the personalities of the computer gods")
    parser.add_argument("--until-end", action="store_true",
                        help="play until the game is decided, at most --turns turns (up to 100000)")
    parser.add_argument("--expect-ai-action", action="store_true",
                        help="the run fails when no computer god chose a need, did a deed of the hand or cast a miracle")
    parser.add_argument("--trace-draws", action="store_true",
                        help="the server and the clients write a draw trace, and the summary shows the first draw where "
                             "each client is not the same as the server")
    parser.add_argument("--all-clients-arg", action="append", default=[],
                        help="one more switch for each client, such as --all-clients-arg=--show-ping (repeat for more)")
    parser.add_argument("--hash-fields", action="store_true", help="as --hash-parts, with the hash of each field")
    parser.add_argument("--time-limit", type=int,
                        help="the time limit of the network game in minutes (the server's --time-limit); 30 with none")
    parser.add_argument("--conditions", help="the winning conditions of the network game (the server's --conditions), "
                        "[LAND:]NAME=VALUE,...; the clients take them from the start")
    parser.add_argument("--temple-loss", help="PLAYER:TURN: at the turn the temple of the player (from 0) starts its "
                        "destruction on the server and on every client, so that the game is decided and its turns stop")
    parser.add_argument("--leave", help="CLIENT:TURN: at the turn the client (from 0) leaves the game as a player does: "
                        "Escape, Exit Online Game and Leave Game")
    parser.add_argument("--box-back", help="CLIENT:TURN: at the turn the client (from 0) clicks Back in the information "
                        "box of the land, and the player leaves the network game")
    parser.add_argument("--no-hand-reports", action="store_true",
                        help="the desktop clients send no report of their hands and views (--sim-no-hand-reports). The "
                             "reports carry each client's camera, which moves at each frame, so without this two runs "
                             "of the same seed give other server checksums until the cameras stop")
    parser.add_argument("--same-server-as", action="append", default=[], metavar="FOLDER",
                        help="the output folder of another net_check run of the same land and seed: the server "
                             "checksums of this run must be the same from turn 1 to the last turn (repeat for more runs)")
    parser.add_argument("--hash-parts", action="store_true",
                        help="the clients' records keep the hash of each component, to find what parted")
    parser.add_argument("--client-args", default="",
                        help="more arguments of each client game, divided by spaces, such as --turn-beside-draw")
    parser.add_argument("--server-profile",
                        help="the profile folder of the server (--profile-folder), as the live server keeps its profile "
                             "from game to game; with none, the server keeps no profile")
    parser.add_argument("--server-alignment", type=float,
                        help="the server's profile keeps this alignment from an earlier game, as the profile of the live "
                             "server does (a new profile folder in the output folder, unless --server-profile names one)")
    parser.add_argument("--alignment", action="append", default=[],
                        help="CLIENT:VALUE, the profile of the client (clients from 1) keeps this alignment from earlier "
                             "games (a new profile folder in the client's folder); repeat for each client")
    parser.add_argument("--room-wait", type=float, default=0.0,
                        help="the seconds that the server waits in its room with nobody, before any client joins, as "
                             "a live server waits for its players")
    parser.add_argument("--leave-room-first", action="store_true",
                        help="before the clients of the game join, another client joins the room and leaves it with no "
                             "start; the game that follows must stay in step with the server from its first turn")
    parser.add_argument("--leave-room-wait", type=float, default=5.0,
                        help="the seconds that the server waits in its room after the leave of --leave-room-first")
    options = parser.parse_args()
    if options.frame_ms != 0 and not 1.0 <= options.frame_ms <= 1000.0:
        # The game does not start with another frame time, and the run would only say that the first client did not join
        parser.error("--frame-ms takes 0 (the real frame time of the clients) or 1 to 1000 milliseconds, as the game's --sim-frame-ms")
    options.hash_parts = options.hash_parts or options.hash_fields
    state_turns = world_state_turns(options.world_states) if options.world_states else []
    if state_turns is None:
        parser.error("--world-states takes a list of turns such as 250 or 100,250")
    if options.all_computer_gods:
        options.clients = 0
    if not 1 <= options.turns <= MOST_TURNS:
        parser.error(f"--turns takes 1 to {MOST_TURNS}")
    try:
        leashes = parse_leash(options.leash, options.clients)
    except ValueError as error:
        parser.error(str(error))
    if any(leash["client"] == "web" for leash in leashes) and not (options.web_client and options.web_god):
        parser.error("--leash web needs --web-client and --web-god")
    # The information box of the land takes the presses until the player closes it. The web god closes it with the Escape
    # key before its first leash key, and the recorded input of a desktop client closes it before its first event.
    options.web_close_box = not options.testbed and land_shows_info_box(options.game_path, options.land)
    waited_observers = options.observers + (1 if options.web_client else 0)
    if options.all_computer_gods and waited_observers == 0:
        parser.error("--all-computer-gods needs --observers or --web-client: the clients watch the game")
    if not options.all_computer_gods and options.clients == 0:
        parser.error("a room with no human god needs --all-computer-gods")
    if options.scenario and (options.all_computer_gods or (options.web_client and not options.web_god)
                             or options.testbed or options.gods_ring_map):
        # The state of a scenario is not in the world, so nobody can join its game as it turns: the room of computer gods
        # starts at once and the web observer joins late. A web god joins before the start. The scenario chooses its own
        # land.
        parser.error("--scenario does not go with --all-computer-gods, a web observer, --testbed or --gods-ring-map")
    if options.web_part_at is not None and not options.web_client:
        parser.error("--web-part-at needs --web-client")
    if (options.web_god or options.web_chat or options.web_remote) and not options.web_client:
        parser.error("--web-god, --web-chat and --web-remote need --web-client")
    if options.web_chat and not options.web_god:
        parser.error("--web-chat needs --web-god: an observer says nothing in the chat")
    if options.web_god and (options.observers or options.all_computer_gods):
        # The observers wait for the start of the game, and the start waits for the web god, which joins last
        parser.error("--web-god does not go with --observers or --all-computer-gods")
    if options.web_client and not options.web_remote and not os.path.isfile(
            os.path.join(web_page_of(options), "openblack.wasm")):
        parser.error(f"--web-client needs the web build: no openblack.wasm in {web_page_of(options)}")
    desktop_clients = options.clients + options.observers
    if options.part_second_at is not None and not 1 <= options.part_client <= desktop_clients:
        parser.error(f"--part-client takes a client from 1 to {desktop_clients}")
    client_inputs = ([options.input, options.input2, options.input3, options.input4] + [None] * 8)[:max(desktop_clients, 4)]
    client_cameras = ([options.camera1, options.camera2, options.camera3, options.camera4] + [None] * 8)[:max(desktop_clients, 4)]
    if (any(client_cameras) or options.hold_creature) and not os.path.isfile(INSPECTOR_ADAPTER):
        # A copy of tools/sim alone cannot move the cameras: the inputs would play at the start camera
        parser.error(f"--cameraN and --hold-creature need the inspector adapter {os.path.normpath(INSPECTOR_ADAPTER)}")

    out = os.path.abspath(options.out)
    if SWITCH_IN_PATH.search(out):
        print(f"the output folder {out} has a switch of the game in its path; choose another folder")
        return 1
    folder = copy_programs(options.bin or default_bin(options.config), out)
    exe = ".exe" if os.name == "nt" else ""
    server_exe = os.path.join(folder, "openblack-server" + exe)
    game_exe = os.path.join(folder, "openblack" + exe)
    keys_dir = os.path.join(out, "keys")
    os.makedirs(keys_dir, exist_ok=True)
    key, kid, keys_file = make_ticket_key(keys_dir)
    certs = make_certificates(keys_dir)
    where = (["--scenario", options.scenario] if options.scenario else ["--gods-ring-map"] if options.gods_ring_map
             else ["--testbed"] if options.testbed else ["-s", options.land, "--skip-opening", options.opening])

    server_dir = os.path.join(out, "server")
    server_log = os.path.join(server_dir, "server.log")
    client_dirs = [os.path.join(out, f"client{index + 1}") for index in range(desktop_clients)]
    web_dir = os.path.join(out, "web")
    leaver, leave_turn = (int(part) for part in options.leave.split(":")) if options.leave else (None, None)
    if options.box_back:
        if options.leave:
            parser.error("--box-back and --leave each make a client leave: give only one")
        # The client that clicks Back leaves the game as the client of --leave does
        leaver, leave_turn = (int(part) for part in options.box_back.split(":"))
    server_trace = os.path.join(server_dir, "draws.tsv")
    for path in [server_log, server_trace, os.path.join(server_dir, "rows.txt")] + [os.path.join(d, n) for d in client_dirs
                                              for n in ("client.log", "record.jsonl", "draws.tsv", "rows.txt")]:
        if os.path.exists(path):
            os.remove(path)

    # The server ends when every god left; its last turn is only a limit for a run that goes wrong
    server_command = [server_exe, "-g", options.game_path] + where + [
        "--seed", str(options.seed), "-n", str(options.turns + 3000), "-l", server_log, "--log-checksums",
        "--listen", f"127.0.0.1:{free_port() if options.restart_server_at is not None else 0}", "--tls-cert", certs["server.pem"], "--tls-key", certs["server.key"],
        "--server-id", SERVER_ID, "--ticket-keys", keys_file, "--come-back-seconds", str(options.come_back_seconds)]
    # The fewest gods that start the game: each client by default, and in a room of computer gods the observers to wait
    # for. The old name --gods also works on an older server.
    if options.min_gods is not None:
        server_command += ["--min-gods", str(options.min_gods)]
    else:
        server_command += ["--gods", str(waited_observers if options.all_computer_gods
                                         else options.clients + (1 if options.web_god else 0))]
    if options.all_computer_gods:
        server_command.append("--all-computer-gods")
    if options.slot_fight:
        server_command.append("--test-commands")
    if options.ai_seed is not None:
        server_command += ["--ai-seed", str(options.ai_seed)]
    if options.server_alignment is not None:
        options.server_profile = options.server_profile or os.path.join(out, "server_profile")
        write_kept_alignment(options.server_profile, options.server_alignment)
    if options.server_profile:
        server_command.append(f"--profile-folder={os.path.abspath(options.server_profile)}")
    if options.server_frame_ms is not None:
        server_command += ["--frame-ms", str(options.server_frame_ms)]
    # A land with no temple at the start (a story land, the testbed) would end the game at its first turn
    land_kind = {"land": options.land, "testbed": options.testbed, "gods_ring_map": options.gods_ring_map}
    server_command += sim_report.gods_need_no_temple_args(True, **land_kind)
    lobby = None
    if options.ban_account is not None:
        if 100 <= options.ban_account < 100 + options.clients:
            parser.error("--ban-account must not be the account of a client that plays")
        lobby = FakeLobby(certs, options.ban_account)
        server_command += ["--lobby-api", lobby.address, "--client-cert", certs["client.pem"],
                           "--client-key", certs["client.key"], "--lobby-test-ca", certs["ca.pem"]]
    banned_result = None
    if options.trace_draws:
        server_command += ["--trace-draws", server_trace]
    if options.temple_loss:
        server_command += ["--temple-loss", options.temple_loss]
    if options.time_limit is not None:
        server_command += ["--time-limit", str(options.time_limit)]
    if options.conditions:
        server_command += ["--conditions", options.conditions]
    if options.hash_parts:
        server_command += ["--record", os.path.join(server_dir, "record.jsonl")]
        if options.hash_fields:
            server_command.append("--record-fields")
    if state_turns:
        server_command += ["--world-states", os.path.join(server_dir, WORLD_STATES_FOLDER),
                           "--world-state-turns", options.world_states]
    if options.row_dump:
        server_command += ["--row-dump", os.path.join(server_dir, "rows.txt")]
        if options.row_dump_turn is not None:
            server_command += ["--row-dump-turn", str(options.row_dump_turn)]
    processes = []
    miracle_done = None
    fight_done = None
    give_done = None
    # The keys that each leash check of a desktop client pressed, by the place of the check in the list
    leash_done = {number: [] for number, leash in enumerate(leashes) if leash["client"] != "web"}
    for leash in leashes:
        leash["tap"] = options.key_tap
    try:
        server = start(server_command, server_dir)
        processes.append(server)
        port = wait_for_port(server_log, server)
        if port is None:
            print(f"the server did not listen; see {server_log}")
            return 1
        url = f"wss://127.0.0.1:{port}/game/{SERVER_ID}"
        if options.room_wait > 0:
            time.sleep(options.room_wait)
        if options.leave_room_first and not leave_room(game_exe, options, url, key, kid, certs, out, server_log):
            return 1
        if lobby is not None:
            banned_result = try_banned_join(options, game_exe, url, (key, kid), certs, server_log, server,
                                            os.path.join(out, "banned"), processes)
            if banned_result is None:
                print(f"the server did not read the bans of the fake lobby; see {server_log}")
                return 1
        clients = [None] * desktop_clients
        gods_ring_file = os.path.join(out, "obm_32p_1.map")
        if options.gods_ring_map:
            # The lobby gives the map for 32 gods to each player before the start. The run has no lobby, so each client
            # gets the server's file in its profile folder before it starts.
            if subprocess.run([server_exe, "-g", options.game_path, "--write-gods-ring-map", gods_ring_file],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0:
                print(f"the server did not write the map for 32 gods to {gods_ring_file}")
                return 1
        for index in join_order(options.clients, desktop_clients, bool(options.scenario)):
            client_dir = client_dirs[index]
            os.makedirs(client_dir, exist_ok=True)
            if options.gods_ring_map:
                kept_alignment = any(int(value.split(":", 1)[0]) - 1 == index for value in options.alignment)
                # A run of a tool keeps its profile in the developer folder of the profile folder, unless the run gives one
                profile = os.path.join(client_dir, "profile") if kept_alignment else os.path.join(client_dir, "Profiles",
                                                                                                    "developer")
                maps_dir = os.path.join(profile, "Online Maps")
                os.makedirs(maps_dir, exist_ok=True)
                shutil.copyfile(gods_ring_file, os.path.join(maps_dir, "obm_32p_1.map"))
            for value in options.profile_creature:
                client, folder = value.split(":", 1)
                if int(client) == index + 1:
                    kept = os.path.join(client_dir, "Profiles", "developer", "Scripts", "CreatureMind")
                    shutil.copytree(folder, kept, dirs_exist_ok=True)
            is_observer = index >= options.clients
            # The observers of a scenario are in the room before the start; other observers join the game as it turns
            if (is_observer and not options.all_computer_gods and not options.scenario
                    and not wait_for_line(server_log, "The network game starts", server)):
                print(f"the game did not start, so observer {index + 1} cannot join it; see {server_dir}")
                return 1
            # The test tickets live longer than the lobby's 60 seconds, because the clients load the land first
            ticket = make_ticket(key, kid, 100 + index, f"{'Observer' if is_observer else 'Player'}{index + 1}",
                                 "observer" if is_observer else "god", 600)
            # No land, seed or start option: the client takes them from the server. A frame time of 0 lets
            # the client run at its real frame rate, as a player's game does
            frame_time = ["--sim-frame-ms", str(options.frame_ms)] if options.frame_ms > 0 else []
            command = [game_exe, "-g", options.game_path, "--skip-logos"] + frame_time + [
                       "--sim-until-turn", str(options.turns),
                       "--sim-record", os.path.join(client_dir, "record.jsonl"), "--sim-record-every", "1",
                       "--inspect-lock-input", "-n", "4000000000", "--background",
                       "-l", os.path.join(client_dir, "client.log"),
                       "--join", url, "--join-ticket", ticket, "--join-test-ca", certs["ca.pem"]]
            command += options.all_clients_arg
            if options.trace_draws:
                command += ["--sim-trace-draws", os.path.join(client_dir, "draws.tsv")]
            if state_turns:
                command += ["--sim-world-states", os.path.join(client_dir, WORLD_STATES_FOLDER),
                            "--sim-world-state-turns", options.world_states]
            if options.row_dump:
                command += ["--sim-row-dump", os.path.join(client_dir, "rows.txt")]
                if options.row_dump_turn is not None:
                    command += ["--sim-row-dump-turn", str(options.row_dump_turn)]
            if options.temple_loss:
                command += ["--sim-temple-loss", options.temple_loss]
            command += options.client_args.split()
            if options.throw_lead_latency_ms is not None:
                command += ["--throw-lead-latency-ms", str(options.throw_lead_latency_ms)]
            command += sim_report.gods_need_no_temple_args(False, **land_kind)
            if leaver == index:
                command += ["--sim-info-box-back-at" if options.box_back else "--sim-leave-at", str(leave_turn)]
            if options.no_hand_reports:
                command.append("--sim-no-hand-reports")
            if options.hash_fields:
                command.append("--sim-hash-fields")
            elif options.hash_parts:
                command.append("--sim-hash-parts")
            client_input = client_inputs[index]
            if client_input:
                command += ["--sim-input", shifted_input(client_input, client_dir, options.input2_delay_frames * index,
                                                         client_cameras[index], options.web_close_box)]
            else:
                command.append("--sim-still-hand")
            camera = client_cameras[index]
            holds = {int(value.split(":")[0]) - 1: value.split(":")[1] for value in options.hold_creature}
            chatting = any(value.split(":", 2)[1] == str(index + 1) for value in options.chat)
            chatting = chatting or any(value.split(":", 1)[1] == str(index + 1) for value in options.tattoo_menu)
            if (camera or index in holds or options.slot_miracle == index + 1 or chatting or options.slot_fight
                    or options.give == index + 1 or any(leash["client"] == index for leash in leashes)):
                command += ["--inspect-port", "0"]
            creatures = {int(value.split(":", 1)[0]) - 1: value.split(":", 1)[1] for value in options.creature}
            if index in creatures:
                command += ["--join-creature", os.path.abspath(creatures[index])]
            alignments = {int(value.split(":", 1)[0]) - 1: float(value.split(":", 1)[1]) for value in options.alignment}
            if index in alignments:
                profile = os.path.join(client_dir, "profile")
                write_kept_alignment(profile, alignments[index])
                command.append(f"--profile-folder={os.path.abspath(profile)}")
            if index == options.part_client - 1 and options.part_second_at is not None:
                command += ["--join-part-at", str(options.part_second_at)]
                if options.part_second_again:
                    command.append("--join-part-again")
            if options.frame_guard:
                command.append("--sim-frame-guard")
            command += [value.split(":", 1)[1] for value in options.client_arg if int(value.split(":", 1)[0]) == index + 1]
            clients[index] = start(command, client_dir)
            processes.append(clients[index])
            # The server gives the god slots in join order: client N must join before client N + 1 starts,
            # so that client N gets slot N - 1 and its input and camera play at that god
            if not wait_for_line(os.path.join(client_dir, "client.log"), "Joined the game server", clients[index]):
                print(f"client {index + 1} did not join; see {client_dir}")
                return 1
            joins = lines_with(os.path.join(client_dir, "client.log"), ["Joined the game server"])
            if (not is_observer and any("in slot" in line for line in joins)
                    and not any(line.endswith(f"in slot {index}") for line in joins)):
                print(f"client {index + 1} did not get god slot {index}; see {client_dir}")
                return 1
        web_process = None
        web_folder = None
        if options.web_client:
            ticket = (make_ticket(key, kid, 200, "WebPlayer", "god", 600) if options.web_god
                      else make_ticket(key, kid, 200, "WebObserver", "observer", 600))
            web_folder, web_process = start_web_client(options, out, url, ticket, certs, options.turns, processes)
        # Each client with a camera and no input looks there once its network game started. A client with an input put
        # its camera as its land started, from the input.
        for index, client in enumerate(clients):
            camera = client_cameras[index]
            if camera and not client_inputs[index]:
                log = os.path.join(client_dirs[index], "client.log")
                if wait_for_line(log, "Joined the game server", client) and not set_camera(client.pid, camera):
                    print(f"the camera of client {index + 1} was not set; see {client_dirs[index]}")
                    return 1
        started = time.monotonic()
        restarted_log = None
        killed_at = None
        stopped_second = False
        holds = {int(value.split(":")[0]) - 1: value.split(":")[1] for value in options.hold_creature}
        held = {index for index in holds if index >= len(clients)}
        chats = [(int(turns), int(client) - 1, text) for turns, client, text in
                 (value.split(":", 2) for value in options.chat)]
        said = set()
        tattoo_menus = [(int(turns), int(client) - 1) for turns, client in
                        (value.split(":", 1) for value in options.tattoo_menu)]
        tattooed = set()
        cameras_again = set()
        while time.monotonic() - started < run_limit_seconds(options.turns):
            if (options.restart_server_at is not None and restarted_log is None
                    and max(server_checksums(server_log), default=0) >= options.restart_server_at):
                # The server stops with no warning, as in a crash, and a new server continues from its snapshots
                stop(server)
                killed_at = max(server_checksums(server_log), default=0)
                restarted_log = os.path.join(server_dir, "server-restarted.log")
                if os.path.exists(restarted_log):
                    os.remove(restarted_log)
                command = list(server_command)
                command[command.index("-l") + 1] = restarted_log
                server = start(command + ["--resume-snapshot"], server_dir)
                processes.append(server)
                if wait_for_port(restarted_log, server) != port:
                    print(f"the restarted server did not listen on port {port}; see {restarted_log}")
                    return 1
            if (all(client.poll() is not None for client in clients if not (stopped_second and client is clients[-1]))
                    and (web_process is None or web_process.poll() is not None)
                    and (web_folder is None or not options.web_remote or web_remote_done(web_folder))):
                break
            for number, (turns, index, text) in enumerate(chats):
                if (number not in said and 0 <= index < len(clients) and clients[index].poll() is None and lines_with(
                        os.path.join(client_dirs[index], "client.log"), [f"Network game: {turns} turns compared"])):
                    say_in_chat(clients[index].pid, text)
                    said.add(number)
            for number, (turns, index) in enumerate(tattoo_menus):
                if (number not in tattooed and 0 <= index < len(clients) and clients[index].poll() is None and lines_with(
                        os.path.join(client_dirs[index], "client.log"), [f"Network game: {turns} turns compared"])):
                    open_tattoo_menu(clients[index].pid)
                    tattooed.add(number)
            for index, near in holds.items():
                if index not in held and clients[index].poll() is None and reached_turn(
                        os.path.join(client_dirs[index], "client.log"), clients[index].pid, options.hold_after):
                    hold_creature(clients[index].pid, near, options.hold_slaps)
                    held.add(index)
            slot = (options.slot_miracle or 0) - 1
            if (miracle_done is None and 0 <= slot < len(clients) and clients[slot].poll() is None and reached_turn(
                    os.path.join(client_dirs[slot], "client.log"), clients[slot].pid, options.hold_after)):
                miracle_done = slot_miracle(clients[slot].pid, slot)
            if (options.slot_fight and fight_done is None and len(clients) == 2
                    and all(client.poll() is None for client in clients) and reached_turn(
                        os.path.join(client_dirs[0], "client.log"), clients[0].pid, options.hold_after)):
                fight_done = slot_fight([client.pid for client in clients], options.slot_fight)
            for index, client in enumerate(clients):
                camera = client_cameras[index]
                if (camera and options.camera_again_after is not None and index not in cameras_again
                        and client.poll() is None and lines_with(
                            os.path.join(client_dirs[index], "client.log"),
                            [f"Network game: {options.camera_again_after} turns compared"])):
                    if not set_camera(client.pid, camera):
                        print(f"the camera of client {index + 1} was not set again; see {client_dirs[index]}")
                        return 1
                    cameras_again.add(index)
            for number, done in leash_done.items():
                index = leashes[number]["client"]
                if index < len(clients) and clients[index].poll() is None:
                    desktop_leash_step(leashes[number], done, clients[index].pid,
                                       os.path.join(client_dirs[index], "client.log"))
            giver = (options.give or 0) - 1
            if (give_done is None and 0 <= giver < len(clients) and clients[giver].poll() is None and reached_turn(
                    os.path.join(client_dirs[giver], "client.log"), clients[giver].pid, options.hold_after)):
                give_done = give_to_creature(clients[giver].pid, os.path.join(client_dirs[giver], "client.log"))
            if (options.stop_second_after is not None and not stopped_second and len(clients) > 1
                    and time.monotonic() - started > options.stop_second_after):
                stop(clients[1])
                stopped_second = True
            time.sleep(0.5)
        # The server stops at its last turn; give it the time to write its summary
        end = time.monotonic() + 60
        while server.poll() is None and time.monotonic() < end:
            time.sleep(0.5)
    finally:
        for process in processes:
            stop(process)
        if lobby is not None:
            lobby.close()

    server_turns = server_checksums(server_log)
    restart = None
    if options.restart_server_at is not None:
        # The turns that both servers did must have the same checksums. The new server goes on after them.
        again = server_checksums(restarted_log) if restarted_log is not None and os.path.exists(restarted_log) else {}
        both = sorted(set(server_turns) & set(again))
        resumed = lines_with(restarted_log, ["continues its game from the snapshot"]) if again else []
        restart = {"killed_at": killed_at, "resumed": resumed[-1:], "restarted_turns": len(again),
                   "overlap": len(both), "overlap_same": all(server_turns[turn] == again[turn] for turn in both),
                   "snapshots": lines_with(server_log, ["The snapshot of turn"])[-2:],
                   "last_turn": max(again, default=None),
                   # The commands that the clients sent again after the stop, and the blocks that the new server had
                   "resent": [line for client_dir in client_dirs for line in lines_with(
                       os.path.join(client_dir, "client.log"), ["sends again"])],
                   "left_out": lines_with(restarted_log, ["it leaves out the block"]) if again else []}
        server_turns.update(again)
    summary = {"turns": options.turns, "server_turns": len(server_turns), "clients": []}
    # A decided game: the server stops its turns after one turn, and every computer must stop at that turn. The stop
    # lines name the last turn by the number of the world, as the records and the line of the decision do.
    stop_lines = lines_with(server_log, ["the turns of the game stop after turn"])
    stopped_after = int(re.search(r"after turn (\d+)", stop_lines[-1]).group(1)) if stop_lines else None
    summary["stopped_after"] = stopped_after
    all_same = True
    if (options.temple_loss or options.time_limit is not None) and stopped_after is None:
        all_same = False
    # An input of the hand on a game that was decided at its first turn compares nothing of the hand
    summary["decided_at_turn"] = sim_report.decided_at_turn_in_file(server_log)
    decided_reason = sim_report.hand_input_on_decided_game(any(client_inputs), summary["decided_at_turn"])
    summary["hand_input_decided"] = decided_reason is not None
    if decided_reason is not None:
        print(decided_reason)
        all_same = False
    # A god whose client stopped at its last turn leaves the game, and the god that stays can win a game of two gods: a
    # game decided after the last turn of the run tells nothing of the stop of the turns on the clients
    decided_after_run = decided_after_the_run(summary["decided_at_turn"], options.turns)
    summary["decided_after_run"] = decided_after_run
    # The winner and the rank of each god, as the server tells them. Each client that plays to the end must tell the same.
    server_end = sim_report.game_end_in_file(server_log)
    summary["game_end"] = server_end
    for index, client_dir in enumerate(client_dirs):
        client_turns = record_checksums(os.path.join(client_dir, "record.jsonl"))
        compared = [turn for turn in sorted(client_turns) if turn in server_turns]
        different = [turn for turn in compared if client_turns[turn] != server_turns[turn]]
        # The record keeps few turns of a client that catches up or resynchronises: the session of the client names the
        # turn at which its world parted too
        session_parts = [int(match.group(1)) for line in lines_with(os.path.join(client_dir, "client.log"),
                                                                    ["parts from the server's world at turn"])
                         if (match := PARTS_AT.search(line))]
        different = sorted(set(different) | set(session_parts))
        parts = parted_parts(os.path.join(server_dir, "record.jsonl"), os.path.join(client_dir, "record.jsonl"),
                             different[0]) if different and options.hash_parts else None
        # A client that catches up does several turns in a frame, and its record keeps only the last of them; the
        # session compares every turn and writes its counts to the log
        counts = session_counts(os.path.join(client_dir, "client.log"))
        parted_on_purpose = index == options.part_client - 1 and options.part_second_at is not None and not options.part_second_again
        recovered = lines_with(os.path.join(client_dir, "client.log"), ["The world goes on from the server's world"])
        if parted_on_purpose and recovered:
            different = purposeful_part_left_out(different, recovery_turns(recovered), options.part_second_at)
        expected = options.turns - 1 if not (index == 1 and options.stop_second_after) else 1
        is_observer = index >= options.clients
        if is_observer and client_turns:
            # An observer that joined a running game compares the turns from the turn of its world
            expected -= min(client_turns)
        client_stop = lines_with(os.path.join(client_dir, "client.log"), ["The turns of the network game stopped after turn"])
        client_stopped_after = (int(re.search(r"after turn (\d+)", client_stop[-1]).group(1)) if client_stop else None)
        left = index == leaver
        if left:
            # The client that left did the turns up to the turn that it left at, the same as the server, and stopped
            expected = leave_turn
            if not lines_with(os.path.join(client_dir, "client.log"), ["The player leaves the network game"]):
                different = different + [-2]
        elif stopped_after is not None and not decided_after_run:
            # The client did every turn up to the last turn of the game, and none after it
            expected = stopped_after
            if client_stopped_after != stopped_after or max(client_turns, default=0) > stopped_after:
                different = different + [-1]
        # An observer that loaded the world of a running game catches up in frames of several turns, and the record keeps
        # the last turn of each frame: its session counts every turn
        enough = counts is not None and counts[0] >= expected // 2 if is_observer else len(compared) >= expected // 2
        client_end = sim_report.game_end_in_file(os.path.join(client_dir, "client.log"))
        if server_end and not left and not decided_after_run and client_end != server_end:
            print(f"client {index + 1} tells another end of the game than the server: {client_end} against {server_end}")
            different = different + [-3]
        same = (not different and enough and counts is not None
                and (counts[0] - counts[1] <= (PURPOSEFUL_PART_COMPARES if parted_on_purpose else 0))
                and (left or counts[0] >= expected - 100) and bool(recovered or not parted_on_purpose))
        all_same = all_same and same
        draws = None
        if options.trace_draws and os.path.exists(server_trace) and os.path.exists(os.path.join(client_dir, "draws.tsv")):
            draws = draw_trace.compare(server_trace, os.path.join(client_dir, "draws.tsv"))
            for line in draw_trace.summary_lines(draws, "server", f"client {index + 1}"):
                print(line)
        summary["clients"].append({
            "client": index + 1, "turns": len(client_turns), "compared": len(compared),
            "observer": is_observer, "max_turn_ms": max_turn_ms(os.path.join(client_dir, "record.jsonl")),
            "max_record_ms": max_record_ms(os.path.join(client_dir, "record.jsonl")),
            "last_compared": compared[-1] if compared else None,
            "stopped_after": client_stopped_after, "game_end": client_end,
            "end_box": lines_with(os.path.join(client_dir, "client.log"),
                                  ["The end box opens", "The end box asks", "leaves the network game"])[-3:],
            "session_compared": counts[0] if counts else None, "session_same": counts[1] if counts else None,
            "recovered": recovered[-1:] if recovered else [],
            "different": different[:20], "same": same, "parts_against_server": parts, "draws_against_server": draws,
            "rows_against_server": rows_against_server(server_dir, client_dir) if options.row_dump else None,
            "session": lines_with(os.path.join(client_dir, "client.log"),
                                  ["The network game ended", "Network game:", "parts from", "Resynchronising"])[-3:],
        })
    # What the turns did with the hand of each god slot, as the server counts it; each client must count the same. A
    # client's input that expects deeds of its hand (its "expect") fails the run when the hand of its slot did fewer:
    # the checksums are then the same only because the hand did nothing.
    # After a restart, the deeds of the server are in two logs (deviation 100)
    restarted_deeds_log = restarted_log if options.restart_server_at is not None else None
    server_deeds = server_deeds_in_logs(server_log, restarted_deeds_log)
    summary["hand_deeds"] = {str(slot): counts for slot, counts in sorted(server_deeds.items())}
    summary["until_end"] = options.until_end
    summary["ended"] = stopped_after is not None
    summary["all_computer_gods"] = options.all_computer_gods
    summary["ai_seed"] = options.ai_seed
    summary["observers"] = options.observers
    if options.scenario:
        summary["scenario"] = options.scenario
    # What the computer gods did, as the server's log tells. Each client's log must tell the same gods.
    ai = sim_report.ai_actions_in_file(server_log)
    summary["ai"] = ai
    if options.all_computer_gods and not ai["gods"]:
        print("the room has no human god, but the server made no computer god")
        all_same = False
    if options.expect_ai_action and not ai["acted"]:
        print("no computer god chose a need, did a deed of the hand or cast a miracle")
        all_same = False
    if web_folder is not None:
        server_checks = {turn: text for turn, text in server_turns.items()}
        web = web_checksums(web_folder)
        compared = [turn for turn in sorted(web) if turn in server_checks]
        different = [turn for turn in compared if web[turn].lower() != server_checks[turn].lower()]
        # A part on purpose (--web-part-at): the turns up to the world state of the server that the page loaded differ
        back_at = web_back_at(web_log_of(web_folder)) if options.web_part_at is not None else None
        if back_at is not None:
            different = [turn for turn in different if turn > back_at]
        elif options.web_part_at is not None:
            print("the web client parted on purpose, but did not load the world state of the server")
            different = different or [options.web_part_at]
        web_same = not different and len(compared) >= max(1, (options.turns - 1 - min(web, default=0)) // 2)
        all_same = all_same and web_same
        summary["clients"].append({"client": "web", "turns": len(web), "compared": len(compared),
                                   "different": different[:20], "same": web_same})
        if options.web_god:
            joins = lines_with(web_log_of(web_folder), ["Joined the game server"])
            in_slot = any(f"in slot {options.clients}" in line for line in joins)
            summary["clients"][-1]["god_slot"] = options.clients if in_slot else None
            if not in_slot:
                print(f"the web client did not get god slot {options.clients}: {joins[:2]}")
                all_same = False
    summary["missing_deeds"] = {}
    for index, client_dir in enumerate(client_dirs):
        missing = sim_report.missing_deeds(server_deeds.get(index, {}), sim_report.expected_deeds(client_inputs[index]))
        if missing:
            summary["missing_deeds"][str(index)] = missing
            print(f"the hand of slot {index} did not do what its input expects: {missing}")
            all_same = False
        # Only the deeds up to the last turn that the server and the client both reported are compared: a deed on the
        # last turn of one of them is not a difference
        last_common = summary["clients"][index].get("last_compared")
        client_deeds = sim_report.hand_deeds_in_file(os.path.join(client_dir, "client.log"), last_common)
        server_deeds_common = server_deeds_in_logs(server_log, restarted_deeds_log, last_common)
        summary["clients"][index]["hand_deeds_same"] = client_deeds == server_deeds_common
        if index >= options.clients and min(record_checksums(os.path.join(client_dir, "record.jsonl")), default=0) > 1:
            # An observer that joined a running game did not see the deeds of the turns before its world
            continue
        steady = (stopped_after is None and options.stop_second_after is None and leaver is None
                  and options.part_second_at is None)
        if client_deeds != server_deeds_common and steady:
            print(f"client {index + 1} counts other deeds of the hands than the server: {client_deeds}")
            all_same = False
    if options.slot_miracle:
        summary["slot_miracle"] = miracle_done
    if options.slot_fight:
        summary["slot_fight"] = fight_done
    if options.give:
        # The server did the gift of the hand of the giver's god at a turn
        gifts = sim_report.hand_deeds_in_file(server_log).get(options.give - 1, {}).get("gives", 0)
        summary["give"] = dict(give_done or {}, server_gifts=gifts)
        if gifts == 0:
            print(f"the server did no gift of the hand of client {options.give}: {give_done}")
            all_same = False
    if leashes:
        # The leash of each god's creature: the server put it on and took it off, and each client tells the same
        # commands with the same leash after each. An observer that joined a running game compares from its world.
        logs = []
        for index, client_dir in enumerate(client_dirs):
            first = min(record_checksums(os.path.join(client_dir, "record.jsonl")), default=0)
            logs.append((str(index + 1), os.path.join(client_dir, "client.log"), first if index >= options.clients else 0,
                         summary["clients"][index].get("last_compared")))
        if web_folder is not None:
            web = web_checksums(web_folder)
            compared = [turn for turn in sorted(web) if turn in server_turns]
            logs.append(("web", web_log_of(web_folder), min(web, default=0) if not options.web_god else 0,
                         compared[-1] if compared else None))
        summary["leash"] = []
        for number, leash in enumerate(leashes):
            result = leash_result(leash, server_log, logs)
            result["keys"] = leash_done.get(number)
            summary["leash"].append(result)
            if not result["same"]:
                print(f"the leash check of client {result['client']} failed: on {result['on']}, off {result['off']}, "
                      f"clients {result['clients']}")
                all_same = False
    if options.box_back:
        summary["box_back"] = box_back_result(os.path.join(client_dirs[leaver], "client.log"), server_log, leaver)
        print(f"box back: {json.dumps(summary['box_back'])[:600]}")
        if not summary["box_back"]["same"]:
            print("the client that clicked Back in the information box of the land did not leave as Leave Game does")
            all_same = False
    # The synced parts of the world state of each desktop client at each turn of --world-states are the server's
    if state_turns:
        summary["world_states"] = {}
        for index, client_dir in enumerate(client_dirs):
            states = world_states_against_server(server_dir, client_dir, state_turns)
            summary["world_states"][str(index + 1)] = states
            for result in states["turns"]:
                print(f"world state of turn {result['turn']}, client {index + 1}: "
                      f"{'same synced parts' if result['same'] else 'DIFFERENT'}"
                      f"{'' if result['same'] else ' ' + json.dumps(result.get('difference') or result)[:600]}"
                      f", sounds that wait (server, client): {result.get('waiting_sounds')}")
            if not states["same"]:
                all_same = False
    summary["server"] = lines_with(server_log, ["Network game:", "parts from", "defeated", "starts with", "lost the",
                                                "turns of the game stop", "leaves the game"])
    # A component with no registration is left out of the checksum: a change in it would never show here
    unregistered = sim_report.unregistered_in_files(
        server_log, os.path.join(server_dir, "record.jsonl"),
        *[os.path.join(d, n) for d in client_dirs for n in ("client.log", "record.jsonl")])
    summary["unregistered"] = sorted(unregistered)
    if unregistered:
        all_same = False
        print(sim_report.unregistered_line(unregistered))
    # Each line of the chat came to every client, from the god of the client that said it
    if options.chat or options.web_chat:
        summary["chat"] = []
        logs = [os.path.join(client_dir, "client.log") for client_dir in client_dirs]
        if options.web_god and web_folder is not None:
            logs.append(web_log_of(web_folder))
        received = [chat_lines(log) for log in logs]
        # The web god plays the slot after the desktop gods; its client number is the next one
        said = [(int(client), text) for _, client, text in (value.split(":", 2) for value in options.chat)]
        said += [(options.clients + 1, value.split(":", 1)[1]) for value in options.web_chat]
        for client, text in said:
            got = chat_came_to(received, client - 1, text)
            summary["chat"].append({"client": "web" if client > options.clients else client, "text": text,
                                    "came_to": got})
            if not all(got):
                print(f"the chat line {text!r} of client {client} did not come to every client: {got}")
                all_same = False
    if lobby is not None:
        banned_result["lobby_requests"] = dict(lobby.counts)
        # The server reads the bans with each status report
        banned_result["bans_with_each_report"] = lobby.counts["status"] > 0 and lobby.counts["bans"] >= lobby.counts["status"] - 1
        summary["banned"] = banned_result
        all_same = all_same and banned_result["refused"] and banned_result["bans_with_each_report"]
    if options.same_server_as:
        summary["server_against_runs"] = []
        for other in options.same_server_as:
            other_log = os.path.join(other, "server", "server.log")
            result = server_against_run(server_turns, server_checksums(other_log) if os.path.exists(other_log) else {},
                                        options.turns)
            result["run"] = other
            summary["server_against_runs"].append(result)
            if not result["same"]:
                print(f"the server checksums are not the same as the run in {other}: {json.dumps(result)}")
                all_same = False
    if restart is not None:
        summary["restart"] = restart
        summary["server"] += lines_with(restarted_log, ["Network game:", "is back", "comes back", "continues its game",
                                                        "did again"]) if restart["restarted_turns"] else []
        if not restart["restarted_turns"] or not restart["overlap_same"] or not restart["resumed"]:
            print(f"the restarted server did not continue the game in step: {json.dumps(restart)}")
            all_same = False
    summary["same"] = all_same
    with open(os.path.join(out, "net_check.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=2)
    print(json.dumps(summary, indent=2))
    return 0 if all_same else 1


def try_banned_join(options, game_exe, url, signer, certs, server_log, server, banned_dir, processes):
    """Waits until the server has the bans of the fake lobby, then lets a client of the banned account try to join.
    Gives the result, or None when the bans did not come."""
    if not wait_for_line(server_log, "The lobby bans 1 accounts from the server", server, limit=60):
        return None
    os.makedirs(banned_dir, exist_ok=True)
    log = os.path.join(banned_dir, "client.log")
    if os.path.exists(log):
        os.remove(log)
    ticket = make_ticket(signer[0], signer[1], options.ban_account, f"Player{options.ban_account}", "god", 600)
    command = [game_exe, "-g", options.game_path, "--skip-logos", "--sim-frame-ms", "20", "--sim-until-turn", "10",
               "--sim-still-hand", "-n", "4000000000", "--background", "-l", log,
               "--join", url, "--join-ticket", ticket, "--join-test-ca", certs["ca.pem"]]
    client = start(command, banned_dir)
    processes.append(client)
    refused = wait_for_line(log, "The game server refused the client: kicked", client, limit=180) or bool(
        lines_with(log, ["The game server refused the client: kicked"]))
    # The client stops by itself after the refusal
    end = time.monotonic() + 60
    while client.poll() is None and time.monotonic() < end:
        time.sleep(0.5)
    stopped_by_itself = client.poll() is not None
    stop(client)
    return {"account": options.ban_account, "refused": bool(refused), "stopped_by_itself": stopped_by_itself,
            "client": lines_with(log, ["refused the client", "Joined the game server", "the game stops"]),
            "server": lines_with(server_log, ["The lobby bans", "refuses connection"])}


def rows_against_server(server_dir, client_dir):
    """The first entity, component and field that differ in the row dumps of the server and a client, at the first
    turn that both wrote, with each different entity"""
    server_rows = os.path.join(server_dir, "rows.txt")
    client_rows = os.path.join(client_dir, "rows.txt")
    if not (os.path.exists(server_rows) and os.path.exists(client_rows)):
        return None
    result = row_dump.compare(row_dump.read_dump(server_rows), row_dump.read_dump(client_rows))
    if "entities" in result:
        result["entities"] = result["entities"][:10]
    return result


if __name__ == "__main__":
    sys.exit(main())
