# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The pre-deploy gate: checks the web-build.tar and the server-build.tar that a deploy will ship, on this computer only.

    python -I pre_deploy_gate.py [--tars E:/openblack/scratch/live-deploy-4/out] [--reference <folder>]
        [--save-reference <folder>] [--only web|server] [--renderers webgpu,webgl2]
    python -I pre_deploy_gate.py --web-page <web build>/bin/Release --renderers webgpu --work <folder>

It prints one line PASS or FAIL for each item and a last line with the verdict. The exit code is 0 only when every item
passes. The gate never contacts the live site and changes nothing of a deploy.

Web: it unpacks web-build.tar into the work folder and serves it on 127.0.0.1 with the headers of the live site
(gate_serve.py). For each renderer (WebGPU, then WebGL 2 forced with the page parameter "renderer"), headless Chrome
(web/scripts/cdp-run.mjs) starts the page as a player gets it: the new player's name box, the first question of Land 1,
then a click on its OK button and the land after it. The page of a shipping build takes no developer arguments and
ignores the -SKIRMISH switch, and a shipping game has no --landgen (a developer option), so Land 1 is the only land that
the gate can start.
The gate tests each screenshot for the land (lit pixels and land colours, and the likeness of its colours to the same
screenshot of a reference build), the console of each run (no error, no WebGPU or WebGL validation message, no
uncaught exception, no worker load error), and the frame time of the game: the page counts the frames of the game in a
shared counter, and the gate keeps the time of each change of the counter for 10 seconds.

With --web-page, the gate runs the web items only, on the page folder of a web build in place of the tar. The merge
train runs it so on WebGPU for each car (scripts/web_check.sh of the skill openblack-merge-train).

Server: it unpacks server-build.tar and starts the Linux openblack-server in a container of the image that the local act
runs use (no download; Docker must already run). It reads the build id, then plays one network game of 300 turns with
one desktop client of the same commit (tools/sim/net_check.py --server-linux) on an online map, and checks that every
turn has the same checksum on both and that the client did not resume.
"""
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.parse
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate_checks  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
CHROME = "C:/Program Files/Google/Chrome/Application/chrome.exe"
NODE = shutil.which("node") or "node"
OB_BUILD = "C:/projects/obtools/ob-build"
GIT_BASH = "C:/Program Files/Git/bin/bash.exe"
DEFAULT_TARS = "E:/openblack/scratch/live-deploy-4/out"
DEFAULT_WORK = "E:/openblack/scratch/deploy-gate"
SERVER_IMAGE = "catthehacker/ubuntu:act-24.04"
# The address of the error reports for the test games: a closed port of the loopback, so that no report of a test
# reaches the live Sentry project (net_check.py uses the same address for the server in the container)
NO_REPORTS_DSN = "http://test@127.0.0.1:9/0"
GAME_FOLDER = "C:/projects/black"
NET_LAND = "Online Maps/mpm_2p_1.map"
NET_TURNS = 300
# The frame sampler: it reads the frame counter of the game each millisecond and keeps the time of each change
SAMPLE_FRAMES = (
    "(() => { const c = globalThis.openblackWeb && globalThis.openblackWeb.gameFrames; if (!c) return 'no counter';"
    " const s = globalThis.gateFrames = { times: [], last: Atomics.load(c, 0) };"
    " const end = performance.now() + 10000;"
    " const id = setInterval(() => { const n = Atomics.load(c, 0); if (n !== s.last) { s.last = n;"
    " s.times.push(performance.now()); } if (performance.now() > end) clearInterval(id); }, 1);"
    " return 'sampling'; })()")
READ_FRAMES = (
    "(() => { const t = (globalThis.gateFrames || {}).times || []; const d = [];"
    " for (let i = 1; i < t.length; ++i) d.push(t[i] - t[i - 1]); d.sort((a, b) => a - b);"
    " if (!d.length) return 'no frames';"
    " const avg = d.reduce((a, b) => a + b, 0) / d.length;"
    " return 'frames ' + d.length + ' avg ' + avg.toFixed(2) + ' p95 ' + d[Math.floor(d.length * 0.95)].toFixed(2)"
    " + ' max ' + d[d.length - 1].toFixed(2); })()")
FRAME_LINE = re.compile(r"frames (\d+) avg ([\d.]+) p95 ([\d.]+) max ([\d.]+)")
# The places of the first question of Land 1 on the page (CSS pixels of a 1280x1024 window): the page clicks the sea
# below the box to lock the pointer, then moves the game pointer onto the OK button and presses it.
LOCK_CLICK = (640, 960)
QUESTION_OK = (405, 707)

results = []


def item(name, ok, detail=""):
    results.append({"item": name, "pass": bool(ok), "detail": detail})
    print(f"{'PASS' if ok else 'FAIL'} {name}{': ' + detail if detail else ''}", flush=True)


def memory_use():
    """The share of the memory of this computer in use, in percent"""
    out = subprocess.run(["powershell.exe", "-NoProfile", "-Command",
                          "$o=Get-CimInstance Win32_OperatingSystem; "
                          "[int](100-100*$o.FreePhysicalMemory/$o.TotalVisibleMemorySize)"],
                         capture_output=True, text=True)
    return int(out.stdout.strip() or 0)


def wait_for_memory():
    while memory_use() >= 95:
        print("memory use is 95% or more: wait", flush=True)
        time.sleep(60)


def unpack(tar_path, folder):
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder)
    with tarfile.open(tar_path) as tar:
        tar.extractall(folder, filter="data")
    return page_version(folder)


def land1_steps():
    return [
        {"at": 1, "do": "wait-log", "text": "asking the new player for their name", "seconds": 400},
        *[{"at": 3 + i * 0.5, "do": "key", "key": key, "code": f"Key{key.upper()}", "keyCode": ord(key.upper()),
           "hold": 120} for i, key in enumerate("gate")],
        {"at": 6, "do": "key", "key": "Enter", "code": "Enter", "keyCode": 13},
        {"at": 7, "do": "wait-log", "text": "The logo effect ends", "seconds": 300},
        {"at": 8, "do": "wait-frames", "count": 3, "seconds": 60},
        {"at": 9, "do": "shot", "name": "question"},
        {"at": 10, "do": "mouse", "type": "mouseMoved", "x": LOCK_CLICK[0], "y": LOCK_CLICK[1]},
        {"at": 10.3, "do": "mouse", "type": "mousePressed", "x": LOCK_CLICK[0], "y": LOCK_CLICK[1], "button": "left"},
        {"at": 10.4, "do": "mouse", "type": "mouseReleased", "x": LOCK_CLICK[0], "y": LOCK_CLICK[1], "button": "left"},
        {"at": 11, "do": "wait-log", "text": "The page locked the pointer", "seconds": 60},
        {"at": 12, "do": "pointer-to", "x": QUESTION_OK[0], "y": QUESTION_OK[1]},
        {"at": 13, "do": "wait-frames", "count": 3, "seconds": 60},
        {"at": 14, "do": "synth-button", "type": "mousedown", "button": 0},
        {"at": 14.1, "do": "wait-frames", "count": 3, "seconds": 60},
        {"at": 14.5, "do": "synth-button", "type": "mouseup", "button": 0},
        {"at": 30, "do": "shot", "name": "past"},
        {"at": 31, "do": "eval", "expression": SAMPLE_FRAMES},
        {"at": 42, "do": "eval", "expression": READ_FRAMES},
    ]


def run_page(port, folder, renderer, steps):
    """One headless Chrome run of the page. Returns the folder of the run, its console and its game log."""
    actions = folder + "-actions.json"
    with open(actions, "w", encoding="utf-8") as file:
        json.dump(steps, file, indent=1)
    query = {"renderer": renderer}
    url = f"http://127.0.0.1:{port}/index.html?{urllib.parse.urlencode(query, quote_via=urllib.parse.quote)}"
    shutil.rmtree(folder, ignore_errors=True)
    wait_for_memory()
    run = subprocess.run([NODE, os.path.join(REPO, "web", "scripts", "cdp-run.mjs"), CHROME, url, "20", folder,
                          "--game-folder", GAME_FOLDER, "--click", "#play", "--gpu", "--run-all-steps",
                          "--actions", actions], capture_output=True, text=True, timeout=1500)
    read = lambda name: (open(os.path.join(folder, name), encoding="utf-8", errors="replace").read()
                         if os.path.exists(os.path.join(folder, name)) else "")
    return {"folder": folder, "console": read("console.log"), "log": read("page-log.txt"), "exit": run.returncode,
            "stdout": run.stdout[-2000:]}


def check_pictures(label, run, names, reference_dir, measures_out):
    from PIL import Image
    for name in names:
        path = os.path.join(run["folder"], f"{name}.png")
        if not os.path.exists(path):
            item(f"{label} {name}: land drew", False, "no screenshot (the game did not reach this step)")
            continue
        measures = gate_checks.picture_measures(Image.open(path))
        measures_out[f"{label} {name}"] = measures
        reference = None
        reference_file = os.path.join(reference_dir, "measures.json") if reference_dir else None
        if reference_file and os.path.exists(reference_file):
            reference = json.load(open(reference_file, encoding="utf-8")).get(f"{label} {name}")
        ok, detail = gate_checks.land_check(measures, reference)
        if reference_dir and reference is None:
            detail += ", no reference picture"
        item(f"{label} {name}: land drew", ok, detail)


def check_console(label, run, serve_log_path, serve_from):
    faults = gate_checks.console_faults(run["console"])
    names = sorted({name for name, _ in faults})
    first = faults[0][1][:200] if faults else ""
    item(f"{label}: console clean", not faults,
         f"{len(faults)} faulty lines ({', '.join(names)}); first: {first}" if faults else "no faulty line")
    with open(serve_log_path, encoding="utf-8", errors="replace") as file:
        file.seek(serve_from)
        failed = gate_checks.failed_requests(file.read())
    item(f"{label}: every page file answered", not failed, ", ".join(failed[:5]) or "no failed request")


def page_version(page):
    """The commit in version.txt of a page folder, or None"""
    version = os.path.join(page, "version.txt")
    return open(version, encoding="ascii").read().strip() if os.path.exists(version) else None


def web_gate(tar, work, renderers, reference_dir, measures, built_page=None):
    """The web items. built_page is the page folder of a web build (the merge train's car); else the tar unpacks."""
    if built_page:
        page = built_page
        version = page_version(page)
        item("web: page folder has openblack.wasm and version.txt",
             bool(version) and os.path.exists(os.path.join(page, "openblack.wasm")), f"{page}, version {version}")
    else:
        page = os.path.join(work, "page")
        version = unpack(tar, page)
        item("web: tar unpacks with version.txt", bool(version) and os.path.exists(os.path.join(page, "openblack.wasm")),
             f"version {version}")
    if not version:
        return None
    port = free_port()
    serve_log = os.path.join(work, "serve.log")
    server = subprocess.Popen([sys.executable, "-I", os.path.join(HERE, "gate_serve.py"), page, str(port)],
                              stdout=open(serve_log, "w"), stderr=subprocess.STDOUT)
    try:
        time.sleep(2)
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/openblack.wasm") as answer:
            headers = answer.headers
            ok = (headers.get("Content-Type") == "application/wasm"
                  and headers.get("Cross-Origin-Embedder-Policy") == "require-corp"
                  and headers.get("Cross-Origin-Opener-Policy") == "same-origin")
        item("web: served with the live headers", ok, f"wasm {headers.get('Content-Type')}, COOP and COEP")
        for renderer in renderers:
            label = f"web {renderer} land1"
            serve_from = os.path.getsize(serve_log)
            run = run_page(port, os.path.join(work, f"web-{renderer}-land1"), renderer, land1_steps())
            started = "Load startup: first frame" in run["log"]
            item(f"{label}: game started", started and run["exit"] == 0,
                 f"cdp-run exit {run['exit']}" + ("" if started else ", no first frame in the game log"))
            check_pictures(label, run, ["question", "past"], reference_dir, measures)
            check_console(label, run, serve_log, serve_from)
            frames = FRAME_LINE.findall(run["console"])
            if frames:
                count, average, p95, most = frames[-1]
                item(f"{label}: frame time", int(count) > 50,
                     f"{count} frames in 10 s, avg {average} ms, p95 {p95} ms, max {most} ms "
                     "(headless Chrome: compare with the reference run only)")
            else:
                item(f"{label}: frame time", False, "the page gave no frame times")
    finally:
        subprocess.run(["powershell.exe", "-NoProfile", "-Command", f"Stop-Process -Id {server.pid}"],
                       capture_output=True)
    return version


def free_port():
    import socket
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def client_bin(sha, given):
    if given:
        return given
    # Windows looks for "bash" in System32 first, where it is the bash of WSL: name the bash of Git
    out = subprocess.run([GIT_BASH, OB_BUILD, "master", "--config", "RelWithDebInfo", sha[:9]], capture_output=True,
                         text=True)
    lines = [line for line in out.stdout.splitlines() if line.strip()]
    return lines[-1].strip() if out.returncode == 0 and lines else None


def server_gate(tar, work, given_bin, image):
    folder = os.path.join(work, "server")
    version = unpack(tar, folder)
    item("server: tar unpacks with version.txt and crashpad_handler",
         bool(version) and os.path.exists(os.path.join(folder, "openblack-server"))
         and os.path.exists(os.path.join(folder, "crashpad_handler")), f"version {version}")
    if not version:
        return None
    # The shipping server has the address of the live error reports: the test address keeps every report here
    probe = subprocess.run(["docker", "run", "--rm", "--pull=never", "-e", f"SENTRY_DSN={NO_REPORTS_DSN}",
                            "-v", f"{folder}:/srv:ro", image,
                            "/srv/openblack-server", "--version"], capture_output=True, text=True, timeout=300)
    server_id = gate_checks.build_id(probe.stdout + probe.stderr)
    item("server: starts in the Linux image and gives its build id",
         probe.returncode == 0 and server_id is not None and server_id.split("+", 1)[-1].startswith(version[:12]),
         f"{server_id} in {image}" if server_id else (probe.stdout + probe.stderr).strip()[:200])
    binaries = client_bin(version, given_bin)
    if not binaries or not os.path.exists(os.path.join(binaries, "openblack.exe")):
        item("server: desktop client of the same commit", False, f"no openblack.exe for {version[:9]} ({binaries})")
        return version
    probe = subprocess.run([os.path.join(binaries, "openblack-server.exe"), "--version"], capture_output=True,
                           text=True, timeout=120)
    client_id = gate_checks.build_id(probe.stdout + probe.stderr)
    detail = f"server {server_id}, client build {client_id} ({binaries})"
    if client_id != server_id:
        # The store can keep the build of a merge-train car under the commit of its merge: the same files, but the
        # build id of the car's own commit. The server refuses a client with another build id.
        detail += ("; build a client of exactly this commit (a worktree at the commit, ob-build build --config "
                   "RelWithDebInfo) and give its bin/RelWithDebInfo folder with --client-bin")
    item("server: build id and codec equal the desktop client's", client_id == server_id, detail)
    if client_id != server_id:
        item(f"server: one desktop client plays {NET_TURNS} turns in step", False,
             "not run: the server refuses a client with another build id")
        return version
    out = os.path.join(work, "net")
    wait_for_memory()
    net = subprocess.run([sys.executable, os.path.join(REPO, "tools", "sim", "net_check.py"), "--bin", binaries,
                          "--server-linux", folder, "--server-image", image, "--clients", "1", "--min-gods", "1",
                          "--land", NET_LAND, "--turns", str(NET_TURNS), "--game-path", GAME_FOLDER, "--out", out],
                         capture_output=True, text=True, timeout=1800, cwd=REPO,
                         env={**os.environ, "SENTRY_DSN": NO_REPORTS_DSN})
    # The output of net_check.py stays in the work folder: it shows why a network game did not start
    with open(os.path.join(out, "net_check.txt"), "w", encoding="utf-8") as file:
        file.write(f"exit {net.returncode}\n{net.stdout}{net.stderr}")
    read = lambda path: (open(path, encoding="utf-8", errors="replace").read() if os.path.exists(path) else "")
    server_log = read(os.path.join(out, "server", "server.log"))
    listen = re.search(r"The game server listens on \S+ \(wss\)", server_log)
    last = ([line for line in (net.stdout + net.stderr).splitlines() if line.strip()] or [""])[-1].strip()[:200]
    item("server: ready (listens with TLS)", bool(listen),
         listen.group(0) if listen else f"no listen line in the log; net_check.py exit {net.returncode}: {last} "
                                        f"(see {os.path.join(out, 'net_check.txt')})")
    ok, detail = gate_checks.lockstep_check(server_log, read(os.path.join(out, "client1", "client.log")), NET_TURNS)
    item(f"server: one desktop client plays {NET_TURNS} turns in step", ok, f"{NET_LAND}; {detail}")
    shutil.rmtree(os.path.join(out, "bin"), ignore_errors=True)
    return version


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tars", default=DEFAULT_TARS, help="the folder of web-build.tar and server-build.tar")
    parser.add_argument("--work", help="the work folder (default: a new folder in E:/openblack/scratch/deploy-gate)")
    parser.add_argument("--only", choices=("web", "server"))
    parser.add_argument("--web-page", help="the page folder of a web build (such as <build>/bin/Release) in place of "
                                           "web-build.tar: the web items only, for the merge train's car")
    parser.add_argument("--renderers", default="webgpu,webgl2")
    parser.add_argument("--reference", help="the work folder of a gate run of the build that is live now")
    parser.add_argument("--save-reference", help="copy the screenshots and their measures to this folder")
    parser.add_argument("--client-bin", help="the desktop build of the same commit (default: ob-build master)")
    parser.add_argument("--server-image", default=SERVER_IMAGE)
    options = parser.parse_args()

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    work = os.path.abspath(options.work or os.path.join(DEFAULT_WORK, stamp))
    os.makedirs(work, exist_ok=True)
    if options.web_page:
        options.only = "web"
    print(f"deploy gate: {'page ' + options.web_page if options.web_page else 'tars ' + options.tars}, work {work}",
          flush=True)
    measures = {}
    web_version = server_version = None
    if options.only in (None, "web"):
        web_version = web_gate(os.path.join(options.tars, "web-build.tar"), work, options.renderers.split(","),
                               options.reference, measures, options.web_page)
        with open(os.path.join(work, "measures.json"), "w", encoding="utf-8") as file:
            json.dump(measures, file)
    if options.only in (None, "server"):
        server_version = server_gate(os.path.join(options.tars, "server-build.tar"), work, options.client_bin,
                                     options.server_image)
    if options.only is None:
        item("pair: the web and the server are the same commit", web_version and web_version == server_version,
             f"web {web_version}, server {server_version}")
    if options.save_reference and measures:
        os.makedirs(options.save_reference, exist_ok=True)
        shutil.copy2(os.path.join(work, "measures.json"), options.save_reference)
        for run in os.listdir(work):
            if os.path.isdir(os.path.join(work, run)) and run.startswith("web"):
                for name in os.listdir(os.path.join(work, run)):
                    if name.endswith(".png"):
                        shutil.copy2(os.path.join(work, run, name), os.path.join(options.save_reference, f"{run}-{name}"))
    failed = [result["item"] for result in results if not result["pass"]]
    with open(os.path.join(work, "gate.json"), "w", encoding="utf-8") as file:
        json.dump({"tars": options.tars, "web": web_version, "server": server_version, "results": results}, file,
                  indent=1)
    print(f"deploy gate: {'PASS' if not failed else 'FAIL'}: {len(results) - len(failed)} of {len(results)} items "
          f"pass (details in {work}/gate.json)", flush=True)
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
