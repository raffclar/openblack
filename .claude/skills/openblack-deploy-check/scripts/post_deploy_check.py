# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The post-deploy checklist: one pass of each item right after a game or service deploy. It is not a load test.

    python -I post_deploy_check.py --deploy-log <act-game-live-N.log> --since <deploy start, ISO UTC>
        [--tars E:/openblack/scratch/live-deploy-4/out] [--site https://bw.zanidrak.com]

Every item runs signed out: the check uses no account and no sign-in. It prints one line PASS, FAIL or STOP for each
item, then the rollback note and the summary. STOP means that the item cannot run (for example, no deploy log was
given). A STOP item is not a fault: when no item fails, the summary says PASS, and the exit code is 0. When an item
fails, the summary says FAIL and the exit code is 1.

Items (each one request, or one browser visit):
1. The deploy log: the installed web and server builds and the game build id are the builds of the tars. A deploy of
   the web build only has no game server, so the item does not ask for a game build id.
2. The service answers: GET / gives the redirect to the sign-in page (the site has no public health address).
3. The play gate answers: GET /play/openblack.wasm with no sign-in gives 401.
4. Sentry (sentry.exe, org zanidrak, project openblack): no issue that Sentry saw first after the deploy start.
5. The rollback note: the previous tars in out-prev-* and the run.sh line that puts them back.
"""
import argparse
import datetime
import json
import os
import re
import ssl
import subprocess
import sys
import tarfile
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate_checks  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SITE = "https://bw.zanidrak.com"
TARS = "E:/openblack/scratch/live-deploy-4/out"
SENTRY = os.path.expanduser("~/.local/bin/sentry.exe")

results = []


def item(name, state, detail=""):
    """state: True (PASS), False (FAIL) or None (STOP: the item cannot run)"""
    word = {True: "PASS", False: "FAIL", None: "STOP"}[state]
    results.append({"item": name, "state": word, "detail": detail})
    print(f"{word} {name}{': ' + detail if detail else ''}", flush=True)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def tls13():
    context = ssl.create_default_context()
    context.minimum_version = ssl.TLSVersion.TLSv1_3
    return context


def one_request(url, opener=None, data=None, headers=None, method=None):
    """(status, headers, body) of one request, with no redirect followed"""
    request = urllib.request.Request(url, data=data, headers=headers or {}, method=method)
    opener = opener or urllib.request.build_opener(NoRedirect, urllib.request.HTTPSHandler(context=tls13()))
    try:
        with opener.open(request, timeout=30) as answer:
            return answer.status, answer.headers, answer.read()
    except urllib.error.HTTPError as error:
        return error.code, error.headers, error.read()


def tar_version(path):
    try:
        with tarfile.open(path) as tar:
            # The web tar names its files "./name", the server tar "name"
            member = next(name for name in tar.getnames() if name.lstrip("./") == "version.txt")
            return tar.extractfile(member).read().decode("ascii").strip()
    except (OSError, KeyError, StopIteration, tarfile.TarError):
        return None


def deploy_log_item(log_path, sha):
    if not log_path or not os.path.exists(log_path):
        item("deploy log: installed builds", None, f"give --deploy-log (no file {log_path})")
        return
    text = open(log_path, encoding="utf-8", errors="replace").read()
    item(*deploy_log_result(text, sha))


def deploy_log_result(text, sha):
    """Read the act log of a deploy and give the name, the state and the detail of the deploy log item.

    A deploy of the web build only (TARGET=web) does not install a game server, so its log has no game build id.
    Then the item does not ask for one. The game server is in the deploy when its log has the server-nightly step."""
    labels = sorted(set(re.findall(r"\b(\d{8}-\d{6}-[0-9a-f]{9})\b", text)))
    game_ids = sorted(set(re.findall(r"\b(\d+\.\d+\.\d+\+[0-9a-f]{12}/codec\d+)\b", text)))
    installed = [label for label in labels if sha and label.endswith(sha[:9])]
    with_server = "server-nightly:" in text
    if with_server:
        ids_ok = bool(game_ids) and all(game_id.split("+")[1].startswith(sha[:12]) for game_id in game_ids[-1:])
        server = f"game build id {game_ids[-1] if game_ids else 'none'}"
    else:
        ids_ok = True
        server = "web build only: no game server in this deploy, so no game build id"
    passed = "every check passed" in text
    return ("deploy log: installed builds are the tars' commit", bool(installed) and ids_ok and passed,
            f"labels {', '.join(installed) or 'none for ' + str(sha)[:9]}; {server}; "
            f"workflow says every check passed: {'yes' if passed else 'no'}")


def sentry_item(since):
    if not os.path.exists(SENTRY):
        item("Sentry: no new issue since the deploy", None, f"no {SENTRY}")
        return
    out = subprocess.run([SENTRY, "issue", "list", "zanidrak/openblack", "--json", "--period", "14d", "--limit", "100",
                          "--fresh"], capture_output=True, text=True, timeout=120)
    try:
        data = json.loads(out.stdout)
    except json.JSONDecodeError:
        item("Sentry: no new issue since the deploy", False, f"sentry.exe gave no list: {out.stderr.strip()[:200]}")
        return
    rows = data if isinstance(data, list) else data.get("issues", data.get("data", []))
    start = datetime.datetime.fromisoformat(since.replace("Z", "+00:00"))
    when = lambda row, key: datetime.datetime.fromisoformat(str(row.get(key)).replace("Z", "+00:00"))
    new = [row for row in rows if row.get("firstSeen") and when(row, "firstSeen") >= start]
    active = [row for row in rows if row.get("lastSeen") and when(row, "lastSeen") >= start and row not in new]
    name = lambda row: f"{row.get('shortId')} {str(row.get('title'))[:60]}"
    item("Sentry: no new issue since the deploy", not new,
         f"{len(new)} new: {'; '.join(map(name, new))}" if new else
         f"none new; {len(active)} older issues had events since then"
         + (f" ({', '.join(str(row.get('shortId')) for row in active)})" if active else ""))


def rollback_note(tars, sha):
    base = os.path.dirname(os.path.abspath(tars))
    rows = []
    for name in sorted(os.listdir(base)):
        folder = os.path.join(base, name)
        if name.startswith("out-prev-") and os.path.isdir(folder):
            web = tar_version(os.path.join(folder, "web-build.tar"))
            server = tar_version(os.path.join(folder, "server-build.tar"))
            if web and web == server and web != sha:
                rows.append((os.path.getmtime(folder), name, web))
    print("rollback note:")
    if not rows:
        print("  no out-prev-* folder has a matched web and server pair of another commit")
        return
    for _, name, version in sorted(rows, reverse=True):
        print(f"  {name}: web and server {version}")
    print("  Choose the newest pair whose post-deploy check passed (the handovers of the deploy lanes say which); a pair"
          " that was rolled back is not a choice.")
    print(f"  To roll back (only on the user's word): move the tars of out/ to out-prev-{sha[:9]}/, copy "
          f"out-prev-<sha9>/web-build.tar and server-build.tar into out/, check version.txt, fetch the deploy clone, "
          f"then\n  CLONE=<clone> TAG=<next free tag> TARGET=both REF=<full sha> sh {base}/run.sh game")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--deploy-log", help="the act log of the deploy (E:/openblack/scratch/bwgame-live-deploy/...)")
    parser.add_argument("--since", required=True, help="the deploy start, ISO UTC, such as 2026-11-18T14:45:00Z")
    parser.add_argument("--tars", default=TARS)
    parser.add_argument("--site", default=SITE)
    options = parser.parse_args()
    sha = tar_version(os.path.join(options.tars, "web-build.tar"))
    print(f"post-deploy check of {options.site}: tars {options.tars} at {sha}", flush=True)
    deploy_log_item(options.deploy_log, sha)
    status, headers, _ = one_request(options.site + "/")
    item("service answers (GET /)", status == 302 and "/account/login/" in (headers.get("Location") or ""),
         f"{status} to {headers.get('Location')}")
    status, _, _ = one_request(options.site + "/play/openblack.wasm")
    item("play gate answers (GET /play/openblack.wasm signed out)", status == 401, f"{status}")
    sentry_item(options.since)
    rollback_note(options.tars, sha)
    line, code = gate_checks.checklist_summary([row["state"] for row in results])
    print(f"post-deploy check: {line}", flush=True)
    return code


if __name__ == "__main__":
    sys.exit(main())
