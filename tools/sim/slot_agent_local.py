#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Runs the slot agent of a bwgame-service clone (docker/slot_agent.py) on this computer, for tests only.

The service runs the agent in a Linux container. This script loads the same file from the clone and changes the few
values that only Linux has: the name of the server program (openblack-server.exe on Windows), the file of the
heartbeat (in the folder of the run), and the hard stop signal (Windows has no SIGKILL: a stop ends the process at
once). Every other part of the agent stays as the service runs it, so a pool slot of the lobby starts the game server
with the same options as the live site does.

    python tools/sim/slot_agent_local.py --service <bwgame clone> --heartbeat <file> -- <the options of slot_agent.py>
"""

import argparse
import importlib.util
import os
import signal
import sys


def load_agent(service):
    path = os.path.join(service, "docker", "slot_agent.py")
    spec = importlib.util.spec_from_file_location("slot_agent", path)
    module = importlib.util.module_from_spec(spec)
    # The dataclasses of the agent look up their module by name
    sys.modules["slot_agent"] = module
    spec.loader.exec_module(module)
    return module


def main():
    if not os.environ.get("SENTRY_DSN"):
        os.environ["SENTRY_DSN"] = "http://test@127.0.0.1:9/0"  # no report of a test run goes to the live project
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--service", required=True, help="a clone of bwgame-service")
    parser.add_argument("--heartbeat", required=True, help="the heartbeat file of the agent")
    parser.add_argument("rest", nargs=argparse.REMAINDER, help="-- then the options of slot_agent.py")
    options = parser.parse_args()
    rest = options.rest[1:] if options.rest[:1] == ["--"] else options.rest
    agent = load_agent(options.service)
    if os.name == "nt":
        agent.SERVER_PROGRAM = "openblack-server.exe"
        if not hasattr(signal, "SIGKILL"):
            signal.SIGKILL = signal.SIGTERM
    agent.HEARTBEAT_FILE = options.heartbeat
    return agent.main(rest)


if __name__ == "__main__":
    sys.exit(main())
