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
        [--tars E:/openblack/scratch/live-deploy-4/out] [--site https://bw.zanidrak.com] [--skip-browser]

It prints one line PASS, FAIL or STOP for each item, then the rollback note. The exit code is 0 only when every item
passes. STOP means that the item cannot run: the line says what the user must give.

Items (each one request, or one browser visit):
1. The deploy log: the installed web and server builds and the game build id are the builds of the tars.
2. The service answers: GET / gives the redirect to the sign-in page (the site has no public health address).
3. The play gate answers: GET /play/openblack.wasm with no sign-in gives 401.
4. With the test account (the site's test-accounts.json; the script reads it, never prints it, and never writes it):
   - the API signs in, and /api/v1/servers lists at least one game server (the servers that reported in 15 s);
   - /play/version.txt (the version that the loading screen shows) is the commit of the tars;
   - one headless Chrome visit of /play/: single player starts, the land drew, the console is clean, then the visit
     ends. The cookies go to Chrome in a file that the script deletes after the visit, and the console log and the
     actions file of the visit are deleted too (cdp-run.mjs logs typed keys; the visit types none).
5. Sentry (sentry.exe, org zanidrak, project openblack): no issue that Sentry saw first after the deploy start.
6. The rollback note: the previous tars in out-prev-* and the run.sh line that puts them back.
"""
import argparse
import datetime
import http.cookiejar
import json
import os
import re
import shutil
import ssl
import subprocess
import sys
import tarfile
import tempfile
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate_checks  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
SITE = "https://bw.zanidrak.com"
TARS = "E:/openblack/scratch/live-deploy-4/out"
ACCOUNTS = "E:/openblack/deploy/bwgame/test-accounts.json"
QUESTIONS = "E:/openblack/handovers/questions_for_user.md"
CHROME = "C:/Program Files/Google/Chrome/Application/chrome.exe"
GAME_FOLDER = "C:/projects/black"
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
    labels = sorted(set(re.findall(r"\b(\d{8}-\d{6}-[0-9a-f]{9})\b", text)))
    game_ids = sorted(set(re.findall(r"\b(\d+\.\d+\.\d+\+[0-9a-f]{12}/codec\d+)\b", text)))
    installed = [label for label in labels if sha and label.endswith(sha[:9])]
    ids_ok = bool(game_ids) and all(game_id.split("+")[1].startswith(sha[:12]) for game_id in game_ids[-1:])
    passed = "every check passed" in text
    item("deploy log: installed builds are the tars' commit", bool(installed) and ids_ok and passed,
         f"labels {', '.join(installed) or 'none for ' + str(sha)[:9]}; game build id {game_ids[-1] if game_ids else 'none'}; "
         f"workflow says every check passed: {'yes' if passed else 'no'}")


def account():
    if not os.path.exists(ACCOUNTS):
        return None
    with open(ACCOUNTS, encoding="utf-8") as file:
        data = json.load(file)
    rows = data.get("accounts", data) if isinstance(data, dict) else data
    return rows[0] if rows else None


def ask_user(text):
    os.makedirs(os.path.dirname(QUESTIONS), exist_ok=True)
    existing = open(QUESTIONS, encoding="utf-8").read() if os.path.exists(QUESTIONS) else ""
    if text not in existing:
        with open(QUESTIONS, "a", encoding="utf-8") as file:
            file.write(f"\n- {datetime.date.today()} openblack-deploy-check: {text}\n")


def signed_in_items(site, sha, skip_browser, work):
    user = account()
    if user is None:
        message = ("The post-deploy check needs a test account with the beta tester and stream marks on the live "
                   f"site, in {ACCOUNTS} (scripts/test-accounts.sh create of bwgame-service, then the two marks). "
                   "No such file exists. Make one, or say that this item stays off.")
        for name in ("API sign-in and game server list", "play version (loading screen)", "live web client"):
            item(name, None, "no test account: question in questions_for_user.md")
        ask_user(message)
        return
    name, password = user.get("name") or user.get("username"), user.get("password")
    # The API session gives the server list
    status, _, body = one_request(f"{site}/api/v1/sessions", data=json.dumps({"name": name, "password": password}).encode(),
                                  headers={"Content-Type": "application/json"}, method="POST")
    token = json.loads(body).get("access_token") if status in (200, 201) else None
    servers = []
    if token:
        status, _, body = one_request(f"{site}/api/v1/servers", headers={"Authorization": f"Bearer {token}"})
        servers = json.loads(body).get("servers", []) if status == 200 else []
    item("API sign-in and game server list", bool(token) and bool(servers),
         f"sign-in {'ok' if token else 'refused'}, {len(servers)} game servers reported in the last 15 s")
    # The site session (form sign-in) gives the play files
    jar = http.cookiejar.CookieJar()
    opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar),
                                         urllib.request.HTTPSHandler(context=tls13()))
    status, _, body = one_request(f"{site}/account/login/", opener=opener)
    csrf = re.search(rb'name="csrfmiddlewaretoken" value="([^"]+)"', body)
    form = urllib.parse.urlencode({"csrfmiddlewaretoken": csrf.group(1).decode() if csrf else "", "username": name,
                                   "password": password, "next": "/play/"}).encode()
    one_request(f"{site}/account/login/", opener=opener, data=form,
                headers={"Referer": f"{site}/account/login/", "Content-Type": "application/x-www-form-urlencoded"})
    status, _, body = one_request(f"{site}/play/version.txt", opener=opener)
    live = body.decode("ascii", "replace").strip() if status == 200 else f"status {status}"
    item("play version (loading screen) is the tars' commit", status == 200 and live == sha, f"live {live}, tars {sha}")
    if skip_browser:
        item("live web client", None, "--skip-browser")
        return
    cookies = [{"name": cookie.name, "value": cookie.value, "url": site + "/"} for cookie in jar]
    cookie_file = os.path.join(work, "cookies.json")
    with open(cookie_file, "w", encoding="utf-8") as file:
        json.dump(cookies, file)
    folder = os.path.join(work, "live-web")
    steps = [{"at": 1, "do": "wait-log", "text": "asking the new player for their name", "seconds": 400},
             *[{"at": 3 + i * 0.5, "do": "key", "key": key, "code": f"Key{key.upper()}", "keyCode": ord(key.upper())}
               for i, key in enumerate("gate")],
             {"at": 6, "do": "key", "key": "Enter", "code": "Enter", "keyCode": 13},
             {"at": 7, "do": "wait-log", "text": "The logo effect ends", "seconds": 300},
             {"at": 8, "do": "wait-frames", "count": 3, "seconds": 60},
             {"at": 9, "do": "shot", "name": "question"}]
    actions = os.path.join(work, "live-actions.json")
    with open(actions, "w", encoding="utf-8") as file:
        json.dump(steps, file)
    try:
        run = subprocess.run(["node", os.path.join(REPO, "web", "scripts", "cdp-run.mjs"), CHROME, f"{site}/play/", "15",
                              folder, "--frame-url", "/play/index.html", "--cookie-file", cookie_file,
                              "--game-folder", GAME_FOLDER, "--click", "#play", "--gpu", "--run-all-steps",
                              "--actions", actions], capture_output=True, text=True, timeout=1500)
    finally:
        os.remove(cookie_file)
    console = open(os.path.join(folder, "console.log"), encoding="utf-8", errors="replace").read() \
        if os.path.exists(os.path.join(folder, "console.log")) else ""
    faults = gate_checks.console_faults(console)
    for name in ("console.log", "actions.json"):
        if os.path.exists(os.path.join(folder, name)):
            os.remove(os.path.join(folder, name))
    os.remove(actions)
    shutil.rmtree(os.path.join(folder, "chrome-profile"), ignore_errors=True)
    shot = os.path.join(folder, "question.png")
    if os.path.exists(shot):
        from PIL import Image
        ok, detail = gate_checks.land_check(gate_checks.picture_measures(Image.open(shot)))
    else:
        ok, detail = False, f"no screenshot (cdp-run exit {run.returncode})"
    item("live web client: single player land drew", ok, detail)
    item("live web client: console clean", not faults,
         f"{len(faults)} faulty lines; first: {faults[0][1][:200]}" if faults else "no faulty line")


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
    parser.add_argument("--skip-browser", action="store_true")
    options = parser.parse_args()
    sha = tar_version(os.path.join(options.tars, "web-build.tar"))
    print(f"post-deploy check of {options.site}: tars {options.tars} at {sha}", flush=True)
    deploy_log_item(options.deploy_log, sha)
    status, headers, _ = one_request(options.site + "/")
    item("service answers (GET /)", status == 302 and "/account/login/" in (headers.get("Location") or ""),
         f"{status} to {headers.get('Location')}")
    status, _, _ = one_request(options.site + "/play/openblack.wasm")
    item("play gate answers (GET /play/openblack.wasm signed out)", status == 401, f"{status}")
    work = tempfile.mkdtemp(prefix="post-deploy-", dir=os.path.join(os.path.dirname(os.path.abspath(options.tars))))
    signed_in_items(options.site, sha, options.skip_browser, work)
    sentry_item(options.since)
    rollback_note(options.tars, sha)
    bad = [row for row in results if row["state"] != "PASS"]
    print(f"post-deploy check: {'PASS' if not bad else 'FAIL'}: {len(results) - len(bad)} of {len(results)} items pass"
          + (f"; work folder {work}" if os.listdir(work) else ""), flush=True)
    if not os.listdir(work):
        os.rmdir(work)
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
