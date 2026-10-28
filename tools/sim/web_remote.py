#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Plays the web client of a net_check run on another computer (net_check.py --web-client --web-remote).

net_check runs the game server and the desktop clients on one computer, for example a Mac, and writes the plan of the
web client to <out>/web/join.json. This script runs on the computer with Chrome and the web build. It reads the plan over
SSH, opens an SSH tunnel from the same port of 127.0.0.1 here to the port of the game server there (the test certificate
is for 127.0.0.1, so the address of the join stays the same), and plays the plan in headless Chrome through
web/scripts/cdp-run.mjs, as net_check does on one computer. At the end it copies the checksums and the log of the page
into <out>/web/ there and writes <out>/web/done, so that net_check compares them with the turns of the server.

    python tools/sim/web_remote.py --host user@192.168.1.64 --ssh-key E:/keys/id_ed25519 --known-hosts E:/keys/known_hosts
        --remote-out projects/runs/cross1 --out E:/runs/cross1-web

The script reads no password and sends none: the SSH key does the login. It stops only the processes that it started,
by their own process ids.
"""

import argparse
import json
import os
import shlex
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import net_check  # noqa: E402


def ssh_base(options):
    """The ssh command with the key and the known hosts of the options, and no questions"""
    command = ["ssh", "-o", "BatchMode=yes"]
    if options.ssh_key:
        command += ["-i", options.ssh_key]
    if options.known_hosts:
        command += ["-o", f"UserKnownHostsFile={options.known_hosts}"]
    return command


def remote_path(options, name):
    """A path in the web folder of the run there, quoted for the remote shell"""
    return shlex.quote(f"{options.remote_out.rstrip('/')}/web/{name}")


def port_of(join_url):
    """The port of the game server in the address of the join"""
    return int(join_url.split("://", 1)[1].split("/", 1)[0].rsplit(":", 1)[1])


def port_is_free(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        try:
            probe.bind(("127.0.0.1", port))
        except OSError:
            return False
    return True


def read_plan(options):
    """The plan of the web client, once net_check wrote it there; none after the wait"""
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    end = time.monotonic() + options.wait
    while time.monotonic() < end:
        done = subprocess.run(ssh_base(options) + [options.host, f"cat {remote_path(options, 'join.json')}"],
                              capture_output=True, text=True, timeout=60, creationflags=flags)
        if done.returncode == 0 and done.stdout.strip():
            return json.loads(done.stdout)
        time.sleep(2.0)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="USER@HOST of the computer that runs net_check")
    parser.add_argument("--ssh-key", help="the private key of the SSH login")
    parser.add_argument("--known-hosts", help="the known hosts file that has the key of the host")
    parser.add_argument("--remote-out", required=True, help="the --out folder of net_check there")
    parser.add_argument("--out", required=True, help="the folder of the web run here")
    parser.add_argument("--web-page", required=True, help="the folder of the web page here (the web build's bin/Release)")
    parser.add_argument("--game-path", default=net_check.DEFAULT_GAME_PATH)
    parser.add_argument("--chrome", default=net_check.DEFAULT_CHROME)
    parser.add_argument("--wait", type=float, default=900.0, help="the seconds to wait for the plan")
    options = parser.parse_args()
    if not os.path.isfile(os.path.join(options.web_page, "openblack.wasm")):
        parser.error(f"no openblack.wasm in {options.web_page}")

    plan = read_plan(options)
    if plan is None:
        print("web_remote: net_check wrote no plan of the web client")
        return 1
    port = port_of(plan["join_url"])
    if not port_is_free(port):
        print(f"web_remote: the port {port} of 127.0.0.1 is in use here, so the tunnel cannot open")
        return 1
    folder = os.path.abspath(options.out)
    os.makedirs(folder, exist_ok=True)
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    processes = []
    result = 1
    try:
        tunnel = subprocess.Popen(ssh_base(options) + ["-N", "-o", "ExitOnForwardFailure=yes", "-L",
                                                       f"127.0.0.1:{port}:127.0.0.1:{port}", options.host],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
        processes.append(tunnel)
        time.sleep(3.0)
        if tunnel.poll() is not None:
            print("web_remote: the SSH tunnel did not open")
            return 1
        print(f"web_remote: tunnel 127.0.0.1:{port}; the web client plays {plan['turns']} turns")
        web = net_check.run_web_plan(plan, folder, options.web_page, options.chrome, options.game_path, processes)
        try:
            web.wait(timeout=plan["seconds"] + 120)
        except subprocess.TimeoutExpired:
            print("web_remote: the web client did not end in its time")
        copied = [name for name in ("checksums.jsonl", "console.log", "page-log.txt", "cdp-run.log")
                  if os.path.exists(os.path.join(folder, name))]
        if copied:
            target = f"{options.host}:{options.remote_out.rstrip('/')}/web/"
            scp = ["scp", "-q", "-o", "BatchMode=yes"] + (["-i", options.ssh_key] if options.ssh_key else []) + (
                ["-o", f"UserKnownHostsFile={options.known_hosts}"] if options.known_hosts else [])
            # Bare names in the folder: scp would read the drive letter of a Windows path as the name of a host
            subprocess.run(scp + copied + [target], cwd=folder, check=True, timeout=300, creationflags=flags)
        subprocess.run(ssh_base(options) + [options.host, f"touch {remote_path(options, 'done')}"], check=True,
                       timeout=60, creationflags=flags)
        print(f"web_remote: copied {copied} and wrote done")
        result = 0
    finally:
        for process in reversed(processes):
            net_check.stop(process)
    return result


if __name__ == "__main__":
    sys.exit(main())
