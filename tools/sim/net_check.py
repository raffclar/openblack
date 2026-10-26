"""Checks that two real clients play one network game through apps/openblack-server on this computer.

The script makes a test key pair for the join tickets and a test certificate authority with a certificate for
127.0.0.1, all in the output folder (never the live keys of the lobby). It starts the game server with --listen on a free
port of 127.0.0.1 (wss), then two openblack clients with --join and a ticket each. The land, the seed and the start
options go to the server only: each client takes them from the server's reply to its join and loads that land. The
first client plays recorded hand input (--input), the second keeps a still hand. The world turns only with the turns of the server (authoritative
lockstep). At the end it compares the checksum of each turn on the server and on both clients.

    python tools/sim/net_check.py --testbed --input tools/sim/hand_inputs/testbed.json --turns 600
    python tools/sim/net_check.py --turns 600 --config RelWithDebInfo
    python tools/sim/net_check.py --testbed --turns 400 --stop-second-after 40 --come-back-seconds 5
    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --turns 2000 --hold-creature 2:2942.91,2607.57
    python tools/sim/net_check.py --land Playgrounds/TwoGods.txt --turns 2000 --hash-parts
        --input2 tools/sim/hand_inputs/twogods_slot1_tree_press.json --camera-again-after 700
        --camera2 '{"focus":[2928.87,88.44,2620.91],"yaw":0,"pitch":20,"distance":25}'

With --hold-creature the hand of that client holds its god's creature through the inspector once the client compared
--hold-after turns: the camera looks at the creature, the pointer goes to a point of its body that the interface picks,
the hand holds and strokes it, lets go, clicks it for the leash and leads it with a tap on the land. The creature walks,
so a recorded input cannot find it. NET_CHECK_DEBUG=1 prints each try of the pointer.

With --trace-draws the server and each client write a draw trace (each synced random draw with its turn and the place
of its call), and the summary shows the first draw where each client is not the same as the server (draw_trace.py).

With --stop-second-after the script stops the second client (by its own process id) that many seconds after it
started. The server then defeats its god after --come-back-seconds, and the first client must stay in step.

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
import subprocess
import sys
import time

import draw_trace

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.x509.oid import NameOID
import ipaddress

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_report  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BUILDS = os.environ.get("OB_BUILDS", "E:/openblack/builds")
DEFAULT_GAME_PATH = os.environ.get("OB_GAME_PATH", "C:/projects/black")
CHECKSUM_LINE = re.compile(r"turn (\d+) checksum ([0-9a-f]{16})")
LISTEN_LINE = re.compile(r"The game server listens on [0-9.]+:(\d+)")
SERVER_ID = "net-check-1"
# The input of the first client starts after this many frames (50 s at 20 ms), when both clients loaded and the game
# started
INPUT_OFFSET_FRAMES = 2500
RUN_LIMIT_SECONDS = 900


def worktree_root():
    return os.path.normpath(os.path.join(HERE, "..", ".."))


def default_bin(config):
    return os.path.join(DEFAULT_BUILDS, os.path.basename(worktree_root()), "bin", config)


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
    paths = {name: os.path.join(folder, name) for name in ("ca.pem", "server.pem", "server.key")}
    with open(paths["ca.pem"], "wb") as file:
        file.write(ca.public_bytes(serialization.Encoding.PEM))
    with open(paths["server.pem"], "wb") as file:
        file.write(cert.public_bytes(serialization.Encoding.PEM))
    with open(paths["server.key"], "wb") as file:
        file.write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                     serialization.NoEncryption()))
    return paths


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


SESSION_COUNTS = re.compile(r"Network game: (\d+) turns compared with the server, (\d+) the same")
RESUMES = re.compile(r"Back in the game at turn")


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


def set_camera(pid, camera):
    """Puts the camera of a client where its input plays, through the inspector"""
    adapter = os.path.join(HERE, "..", "inspector", "openblack_inspector_mcp.py")
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    subprocess.run([sys.executable, adapter, "--pid", str(pid), "--call", "camera_set", camera], timeout=120,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)


def inspector_call(pid, query, request):
    """One request to the inspector of a client, its answer (none when it fails)"""
    adapter = os.path.join(HERE, "..", "inspector", "openblack_inspector_mcp.py")
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    done = subprocess.run([sys.executable, adapter, "--pid", str(pid), "--call", query, json.dumps(request)],
                          timeout=120, capture_output=True, text=True, creationflags=flags)
    try:
        return json.loads(done.stdout).get("result")
    except ValueError:
        return None


def hold_creature(pid, near):
    """The hand of a client holds the creature nearest a point, strokes it and lets go; then a click puts the leash on
    and a tap on the land leads the creature there. The camera looks at the creature first, and the pointer goes to a
    pixel where the interface picks the creature."""
    x, z = (float(value) for value in near.split(","))
    found = inspector_call(pid, "objects.find", {"params": {"kind": "Creature"}, "near": [x, z], "radius": 600,
                                                "limit": 1})
    items = found.get("items", []) if isinstance(found, dict) else []
    if not items:
        print("hold: no creature near", near)
        return
    creature = items[0]["id"]
    def pixel_on_creature():
        """Looks at the creature from each side in turn, and gives a pixel where the interface picks it: the pointer goes
        to points of the creature's body, from its feet up"""
        for yaw in (0, 90, 180, 270):
            inspector_call(pid, "camera.frame", {"params": {"id": creature, "yaw": yaw, "pitch": 40, "distance": 18}})
            time.sleep(1.0)
            for height in (1.0, 2.0, 0.5, 3.0, 1.5, 4.0, 2.5):
                found = inspector_call(pid, "objects.find", {"params": {"kind": "Creature"}, "near": [x, z],
                                                             "radius": 600, "limit": 1})
                items = found.get("items", []) if isinstance(found, dict) else []
                if not items:
                    return None
                at = items[0]["position"]
                moved = inspector_call(pid, "input.pointer", {"params": {"world": [at[0], at[1] + height, at[2]],
                                                                         "timing": "immediate"}})
                time.sleep(0.2)
                pick = inspector_call(pid, "players.pick", {})
                pointer = moved.get("pointer") if isinstance(moved, dict) else None
                if os.environ.get("NET_CHECK_DEBUG"):
                    print("hold debug", yaw, height, json.dumps(moved)[:160], pick.get("object") if isinstance(pick, dict) else pick)
                if isinstance(pick, dict) and pick.get("object") == creature and isinstance(pointer, list):
                    return [int(value) for value in pointer]
        return None

    # The creature walks among the houses and the people: the search tries again a while later
    pixel = None
    for _ in range(8):
        pixel = pixel_on_creature()
        if pixel is not None:
            break
        time.sleep(4.0)
    if pixel is None:
        print("hold: the pointer found no pixel on creature", creature)
        return
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "press"}})
    time.sleep(1.0)
    # The hand goes round a little on the body while it holds the creature
    for dx, dy in ((10, 0), (0, 10), (-10, 0), (0, -10), (0, 0)):
        inspector_call(pid, "input.pointer", {"params": {"screen": [pixel[0] + dx, pixel[1] + dy]}})
        time.sleep(0.3)
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "release"}})
    # The camera flies back to where it was before the hold
    time.sleep(8.0)
    # A click on the creature puts the leash on
    pixel = pixel_on_creature()
    if pixel is None:
        print("hold: the pointer found no pixel on creature", creature, "for the leash")
        return
    inspector_call(pid, "input.button", {"params": {"button": "right", "action": "click"}})
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


def shifted_input(path, folder, extra=0):
    """The recorded input with its frames moved after the start of the network game, and the extra frames"""
    with open(path, encoding="utf-8") as file:
        value = json.load(file)
    events = value.get("events", []) if isinstance(value, dict) else value
    for event in events:
        event["frame"] = event["frame"] + INPUT_OFFSET_FRAMES + extra
    if isinstance(value, dict):
        value["events"] = events
    else:
        value = events
    out = os.path.join(folder, "input.json")
    with open(out, "w", encoding="utf-8") as file:
        json.dump(value, file)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--turns", type=int, default=600, help="the turns that each client plays")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--land", default="Land1.txt")
    parser.add_argument("--gods-ring-map", action="store_true", help="the network map for 32 gods")
    parser.add_argument("--testbed", action="store_true")
    parser.add_argument("--opening", default="cow")
    parser.add_argument("--frame-ms", type=float, default=20.0, help="the frame time of the clients")
    parser.add_argument("--server-frame-ms", type=float, help="the frame time of the server (its own default with none)")
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--bin")
    parser.add_argument("--game-path", default=DEFAULT_GAME_PATH)
    parser.add_argument("--out", default=os.path.join(os.getcwd(), "net-check"))
    parser.add_argument("--input", help="recorded input for the first client's hand")
    parser.add_argument("--input2", help="recorded input for the second client's hand (its god's own hand)")
    parser.add_argument("--input2-delay-frames", type=int, default=1500,
                        help="the second client's input starts this many frames later than the first's")
    parser.add_argument("--camera1", help="the camera of the first client before its input, as the inspector's "
                                           "camera_set takes it (JSON)")
    parser.add_argument("--camera2", help="the camera of the second client before its input (JSON)")
    parser.add_argument("--camera-again-after", type=int,
                        help="put the cameras of --camera1 and --camera2 there again once the client compared this many "
                        "turns (a multiple of 100), because a land's scripts can move the camera after the start")
    parser.add_argument("--hold-creature", action="append", default=[],
                        help="client:x,z (client 1 or 2): once the client compared --hold-after turns, its hand holds "
                        "the creature nearest this point of the land through the inspector, strokes it, lets go, clicks "
                        "it for the leash and leads it with a tap on the land. Give it once for each client.")
    parser.add_argument("--creature", action="append", default=[],
                        help="CLIENT:FILE, the creature file that the client sends before the start, in place of the "
                             "creature of its profile (clients from 1; repeat for each client)")
    parser.add_argument("--hold-after", type=int, default=300, help="the turns before --hold-creature")
    parser.add_argument("--slot-miracle", type=int, choices=(1, 2),
                        help="this client asks an icon of its god for a miracle, casts it and uses the leash gestures, "
                             "through the inspector, once it compared --hold-after turns")
    parser.add_argument("--part-second-at", type=int,
                        help="the second client's world parts from the server's at this turn (--join-part-at), and must "
                             "come back in step by the server's world state")
    parser.add_argument("--stop-second-after", type=float, help="stop the second client after this many seconds")
    parser.add_argument("--come-back-seconds", type=int, default=10)
    parser.add_argument("--clients", type=int, choices=(1, 2), default=2, help="the number of clients")
    parser.add_argument("--trace-draws", action="store_true",
                        help="the server and the clients write a draw trace, and the summary shows the first draw where "
                             "each client is not the same as the server")
    parser.add_argument("--hash-fields", action="store_true", help="as --hash-parts, with the hash of each field")
    parser.add_argument("--temple-loss", help="PLAYER:TURN: at the turn the temple of the player (from 0) starts its "
                        "destruction on the server and on every client, so that the game is decided and its turns stop")
    parser.add_argument("--leave", help="CLIENT:TURN: at the turn the client (from 0) leaves the game as a player does: "
                        "Escape, Exit Online Game and Leave Game")
    parser.add_argument("--hash-parts", action="store_true",
                        help="the clients' records keep the hash of each component, to find what parted")
    options = parser.parse_args()
    options.hash_parts = options.hash_parts or options.hash_fields

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
    where = (["--gods-ring-map"] if options.gods_ring_map else ["--testbed"] if options.testbed
             else ["-s", options.land, "--skip-opening", options.opening])

    server_dir = os.path.join(out, "server")
    server_log = os.path.join(server_dir, "server.log")
    client_dirs = [os.path.join(out, f"client{index + 1}") for index in range(options.clients)]
    leaver, leave_turn = (int(part) for part in options.leave.split(":")) if options.leave else (None, None)
    server_trace = os.path.join(server_dir, "draws.tsv")
    for path in [server_log, server_trace] + [os.path.join(d, n) for d in client_dirs
                                              for n in ("client.log", "record.jsonl", "draws.tsv")]:
        if os.path.exists(path):
            os.remove(path)

    # The server ends when every god left; its last turn is only a limit for a run that goes wrong
    server_command = [server_exe, "-g", options.game_path] + where + [
        "--seed", str(options.seed), "-n", str(options.turns + 3000), "-l", server_log,
        "--listen", "127.0.0.1:0", "--tls-cert", certs["server.pem"], "--tls-key", certs["server.key"],
        "--server-id", SERVER_ID, "--ticket-keys", keys_file, "--gods", str(options.clients),
        "--come-back-seconds", str(options.come_back_seconds)]
    if options.server_frame_ms is not None:
        server_command += ["--frame-ms", str(options.server_frame_ms)]
    if options.temple_loss:
        server_command += ["--temple-loss", options.temple_loss]
    if options.trace_draws:
        server_command += ["--trace-draws", server_trace]
    if options.hash_parts:
        server_command += ["--record", os.path.join(server_dir, "record.jsonl")]
        if options.hash_fields:
            server_command.append("--record-fields")
    processes = []
    miracle_done = None
    try:
        server = start(server_command, server_dir)
        processes.append(server)
        port = wait_for_port(server_log, server)
        if port is None:
            print(f"the server did not listen; see {server_log}")
            return 1
        url = f"wss://127.0.0.1:{port}/game/{SERVER_ID}"
        clients = []
        for index, client_dir in enumerate(client_dirs):
            os.makedirs(client_dir, exist_ok=True)
            # The test tickets live longer than the lobby's 60 seconds, because the clients load the land first
            ticket = make_ticket(key, kid, 100 + index, f"Player{index + 1}", "god", 600)
            # No land, seed or start option: the client takes them from the server
            command = [game_exe, "-g", options.game_path, "--skip-logos",
                       "--sim-frame-ms", str(options.frame_ms), "--sim-until-turn", str(options.turns),
                       "--sim-record", os.path.join(client_dir, "record.jsonl"), "--sim-record-every", "1",
                       "--inspect-lock-input", "-n", "4000000000", "--background",
                       "-l", os.path.join(client_dir, "client.log"),
                       "--join", url, "--join-ticket", ticket, "--join-test-ca", certs["ca.pem"]]
            if options.temple_loss:
                command += ["--sim-temple-loss", options.temple_loss]
            if leaver == index:
                command += ["--sim-leave-at", str(leave_turn)]
            if options.trace_draws:
                command += ["--sim-trace-draws", os.path.join(client_dir, "draws.tsv")]
            if options.hash_fields:
                command.append("--sim-hash-fields")
            elif options.hash_parts:
                command.append("--sim-hash-parts")
            client_input = options.input if index == 0 else options.input2
            if client_input:
                command += ["--sim-input",
                            shifted_input(client_input, client_dir, options.input2_delay_frames if index else 0)]
            else:
                command.append("--sim-still-hand")
            camera = options.camera1 if index == 0 else options.camera2
            holds = {int(value.split(":")[0]) - 1: value.split(":")[1] for value in options.hold_creature}
            if camera or index in holds or options.slot_miracle == index + 1:
                command += ["--inspect-port", "0"]
            creatures = {int(value.split(":", 1)[0]) - 1: value.split(":", 1)[1] for value in options.creature}
            if index in creatures:
                command += ["--join-creature", os.path.abspath(creatures[index])]
            if index == 1 and options.part_second_at is not None:
                command += ["--join-part-at", str(options.part_second_at)]
            clients.append(start(command, client_dir))
            processes.append(clients[-1])
            if index == 0:
                # The world has one human hand for now (the first god slot), so the client with the input joins first
                if not wait_for_line(os.path.join(client_dir, "client.log"), "Joined the game server", clients[0]):
                    print(f"the first client did not join; see {client_dir}")
                    return 1
        # Each client looks where its input plays, once its network game started
        for index, client in enumerate(clients):
            camera = options.camera1 if index == 0 else options.camera2
            if camera:
                log = os.path.join(client_dirs[index], "client.log")
                if wait_for_line(log, "Joined the game server", client):
                    set_camera(client.pid, camera)
        started = time.monotonic()
        stopped_second = False
        holds = {int(value.split(":")[0]) - 1: value.split(":")[1] for value in options.hold_creature}
        held = {index for index in holds if index >= len(clients)}
        cameras_again = set()
        while time.monotonic() - started < RUN_LIMIT_SECONDS:
            if all(client.poll() is not None for client in clients if not (stopped_second and client is clients[-1])):
                break
            for index, near in holds.items():
                if index not in held and clients[index].poll() is None and lines_with(
                        os.path.join(client_dirs[index], "client.log"),
                        [f"Network game: {options.hold_after} turns compared"]):
                    hold_creature(clients[index].pid, near)
                    held.add(index)
            for index, client in enumerate(clients):
                camera = options.camera1 if index == 0 else options.camera2
                if (camera and options.camera_again_after is not None and index not in cameras_again
                        and client.poll() is None and lines_with(
                            os.path.join(client_dirs[index], "client.log"),
                            [f"Network game: {options.camera_again_after} turns compared"])):
                    set_camera(client.pid, camera)
                    cameras_again.add(index)
            slot = (options.slot_miracle or 0) - 1
            if (miracle_done is None and 0 <= slot < len(clients) and clients[slot].poll() is None and lines_with(
                    os.path.join(client_dirs[slot], "client.log"), [f"Network game: {options.hold_after} turns compared"])):
                miracle_done = slot_miracle(clients[slot].pid, slot)
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

    server_turns = server_checksums(server_log)
    summary = {"turns": options.turns, "server_turns": len(server_turns), "clients": []}
    # A decided game: the server stops its turns after one turn, and every computer must stop at that turn
    stop_lines = lines_with(server_log, ["the turns of the game stop after turn"])
    stopped_after = int(re.search(r"after turn (\d+)", stop_lines[-1]).group(1)) if stop_lines else None
    summary["stopped_after"] = stopped_after
    all_same = True
    if options.temple_loss and stopped_after is None:
        all_same = False
    for index, client_dir in enumerate(client_dirs):
        client_turns = record_checksums(os.path.join(client_dir, "record.jsonl"))
        compared = [turn for turn in sorted(client_turns) if turn in server_turns]
        different = [turn for turn in compared if client_turns[turn] != server_turns[turn]]
        parts = parted_parts(os.path.join(server_dir, "record.jsonl"), os.path.join(client_dir, "record.jsonl"),
                             different[0]) if different and options.hash_parts else None
        # A client that catches up does several turns in a frame, and its record keeps only the last of them; the
        # session compares every turn and writes its counts to the log
        counts = session_counts(os.path.join(client_dir, "client.log"))
        parted_on_purpose = index == 1 and options.part_second_at is not None
        recovered = lines_with(os.path.join(client_dir, "client.log"), ["The world goes on from the server's world"])
        if parted_on_purpose and recovered:
            back_at = int(re.search(r"of turn (\d+)", recovered[-1]).group(1))
            different = [turn for turn in different if turn > back_at]
        expected = options.turns - 1 if not (index == 1 and options.stop_second_after) else 1
        client_stop = lines_with(os.path.join(client_dir, "client.log"), ["The turns of the network game stopped after turn"])
        client_stopped_after = (int(re.search(r"after turn (\d+)", client_stop[-1]).group(1)) if client_stop else None)
        left = index == leaver
        if left:
            # The client that left did the turns up to the turn that it left at, the same as the server, and stopped
            expected = leave_turn
            if not lines_with(os.path.join(client_dir, "client.log"), ["The player leaves the network game"]):
                different = different + [-2]
        elif stopped_after is not None:
            # The client did every turn up to the last turn of the game, and none after it
            expected = stopped_after
            if client_stopped_after != stopped_after or max(client_turns, default=0) > stopped_after + 1:
                different = different + [-1]
        same = (not different and len(compared) >= expected // 2 and counts is not None
                and (counts[0] - counts[1] <= (counts[2] if parted_on_purpose else 0))
                and (left or counts[0] >= expected - 100) and bool(recovered or not parted_on_purpose))
        all_same = all_same and same
        draws = None
        if options.trace_draws and os.path.exists(server_trace) and os.path.exists(os.path.join(client_dir, "draws.tsv")):
            draws = draw_trace.compare(server_trace, os.path.join(client_dir, "draws.tsv"))
            for line in draw_trace.summary_lines(draws, "server", f"client {index + 1}"):
                print(line)
        summary["clients"].append({
            "client": index + 1, "turns": len(client_turns), "compared": len(compared),
            "stopped_after": client_stopped_after,
            "end_box": lines_with(os.path.join(client_dir, "client.log"),
                                  ["The end box opens", "The end box asks", "leaves the network game"])[-3:],
            "session_compared": counts[0] if counts else None, "session_same": counts[1] if counts else None,
            "recovered": recovered[-1:] if recovered else [],
            "different": different[:20], "same": same, "parts_against_server": parts, "draws_against_server": draws,
            "session": lines_with(os.path.join(client_dir, "client.log"),
                                  ["The network game ended", "Network game:", "parts from", "Resynchronising"])[-3:],
        })
    if options.slot_miracle:
        summary["slot_miracle"] = miracle_done
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
    summary["same"] = all_same
    with open(os.path.join(out, "net_check.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=2)
    print(json.dumps(summary, indent=2))
    return 0 if all_same else 1


if __name__ == "__main__":
    sys.exit(main())
