---
name: openblack-deploy-check
description: Use in every openblack deploy lane (web client and game server of bw.zanidrak.com) and in every rollback. Before `run.sh game`, run the pre-deploy gate on the tars that will ship; it starts the web build in headless Chrome on WebGPU and WebGL 2, tests that the land drew and the console is clean, and plays the Linux server against a desktop client. Right after the deploy, run the post-deploy checklist once (health answers, play version, one live web visit, Sentry, rollback note). Also use when a brief asks "did the deploy work", "check the live build" or for a deploy brief template.
---

# openblack deploy check

A deploy can pass every workflow check and still ship a broken game. Web 551d052b4 did: the land textures could not
bind on WebGPU, every frame was black, and nothing crashed, so Sentry saw nothing (web-sp-black-1). This skill has two
parts. Both print one line for each item and exit non-zero when an item fails.

Scripts (in `scripts/` beside this file; run them from your own worktree of openblack, which gives
`web/scripts/cdp-run.mjs` and `tools/sim/net_check.py`):

| Script | Use |
|---|---|
| `pre_deploy_gate.py` | Part 1, on this computer only, before `run.sh game`. |
| `post_deploy_check.py` | Part 2, one pass right after the deploy. |
| `gate_serve.py` | Serves the unpacked web tar with the headers of the live site (the gate starts it). |
| `gate_checks.py` | The pure checks (console, pictures, lockstep lines); `test_gate_checks.py` tests them. |

## Part 1: the pre-deploy gate

Run it on the tars that `build-v2.sh` made, after the tar checks of the deploy brief and before `run.sh game`:

```
python -I .claude/skills/openblack-deploy-check/scripts/pre_deploy_gate.py \
    --tars E:/openblack/scratch/live-deploy-4/out \
    --reference E:/openblack/deploy-gate/reference --work E:/openblack/scratch/deploy-gate/<lane>
```

- Memory must be below 95 %; the gate waits before each Chrome run and the network game. Run one gate at a time; it
  starts one Chrome and one desktop game at a time and stops them itself.
- It takes about 15 minutes: two Chrome runs (WebGPU and WebGL 2) and one network game of 300 turns.
- No test report reaches the live Sentry project. The shipping server and web builds have the live DSN in them. The
  gate gives the server and the desktop client a closed local DSN (`SENTRY_DSN`), and the content policy of the
  local page blocks the browser reports. Never run a shipping server tar outside the gate without such a DSN: a
  local test on 2026-11-18 made the Sentry issue OPENBLACK-P ("Desync: GameStanding") that way.
- The merge train runs the web items of the gate for each car on WebGPU, on the page folder of a web build in place of
  the tar (`--web-page <web build>/bin/Release --renderers webgpu`, through `web_check.sh` of the skill
  openblack-merge-train).
- If the gate prints FAIL, STOP. Do not deploy, do not edit or skip an item, and report the gate output. The
  screenshots, `console.log` and `page-log.txt` of each run are in the work folder.

Items:

| Item | PASS when |
|---|---|
| web: tar unpacks with version.txt | `openblack.wasm` and `version.txt` are in the tar. |
| web: served with the live headers | The local server gives COOP, COEP, CORP, the content policy, nosniff and the MIME types of the site's Caddyfile. |
| web `<renderer>` land1: game started | The page as a player gets it: the name box, the first question of Land 1 (shot `question`), then a click on OK and the start of the story 20 seconds later (shot `past`). |
| `... <shot>`: land drew | At least 50 % of the pixels are lit and 5 % have land colours. With `--reference`, the colours are like the same shot of the live build (likeness 0.35 or more). |
| `...`: console clean | No console error (the 404 of `developer-tools.txt` is normal), no WebGPU or WebGL validation message, no uncaught exception, no worker load error. |
| `...`: every page file answered | The local server answered each request of the page (no 404 other than `developer-tools.txt`). |
| `...`: frame time | The page counts the frames of the game; the gate keeps the time of each frame for 10 seconds (more than 50 frames). The numbers are headless Chrome: compare them only with the reference run. |

Land 1 is the only land: a shipping page takes no developer arguments, the game ignores `-SKIRMISH` on the web, and
a shipping game has no --landgen (a developer option). A skirmish or a generated land in the gate needs a page parameter
of its own (progress row "The deploy gate also starts a skirmish or a generated land on the web").
| server: tar unpacks | `openblack-server`, `crashpad_handler` and `version.txt` are in the tar. |
| server: starts in the Linux image | `openblack-server --version` runs in `catthehacker/ubuntu:act-24.04`, the image of the local act runs (`docker run --rm --pull=never`; the gate never downloads it). WSL Ubuntu 22.04 cannot run the server: its glibc is 2.35, and the server needs 2.38. |
| server: build id and codec | The build id of the Linux server (such as `0.1.0+9f73a40c4184/codec36`) is the build id of the desktop build of the same commit (`ob-build master --config RelWithDebInfo <sha>`). |
| (trap) | Stores made before deploy-tools-2 can keep a merge-train car's build under the merge commit (551d052b4 holds the build of `225585f9f`: the same files, another build id). The server refuses such a client, so the gate stops the server part with FAIL. Now `ob-build store` refuses a build with another commit's id, the train builds again at the merge commit before it stores, and `ob-build master` builds again a stored build with another id. `ob-build store-check --config RelWithDebInfo <sha>` tells if a stored build has the id of its commit. If you cannot wait for a build of the master store, build a client of exactly the commit (a worktree at the commit, `ob-build build --config RelWithDebInfo openblack openblack-server`) and give `--client-bin <build>/bin/RelWithDebInfo`. |
| server: ready | The server log has its listen line (TLS on a port of 127.0.0.1). |
| server: one desktop client plays 300 turns in step | `net_check.py --server-linux` on `Online Maps/mpm_2p_1.map`: the server compared 300 or more checksums, all the same, no resume, and the client agrees. |
| pair | The web and the server tars are the same commit. |

The reference: after a deploy whose post-deploy check passed, save the gate pictures of that build with
`--save-reference E:/openblack/deploy-gate/reference` (run the gate again on the same tars, or copy `measures.json` and
the PNG files of its work folder). Never save a reference from a build that failed.

## Part 2: the post-deploy checklist

Run it once, right after `run.sh game` (or `run.sh service`) ends with exit 0:

```
python -I .claude/skills/openblack-deploy-check/scripts/post_deploy_check.py \
    --deploy-log E:/openblack/scratch/bwgame-live-deploy/act-game-live<TAG>.log --since <deploy start, UTC ISO>
```

One pass only: each item is one request or one browser visit. Never repeat it in a loop, and never use it as a load
test. A STOP line is an item that cannot run; it says what the user must give. A STOP item is not a fault: when no item
fails, the summary line says PASS with the number of STOP items (such as "PASS: 4 of 7 items pass, 3 STOP") and the
exit code is 0. When an item fails, the summary says FAIL and the exit code is 1.

| Item | What it does |
|---|---|
| deploy log | The installed web and server labels and the game build id in the act log are the commit of the tars, and the workflow says "every check passed". No SSH. |
| service answers | One GET of `/`: 302 to `/account/login/`. The site has no public health address: the panel health (`/admin/panel/api/health`) needs a staff account with 2FA, and the push `/healthz` is inside the container only. |
| play gate answers | One GET of `/play/openblack.wasm` signed out: 401. |
| API sign-in and game server list | With the test account: `POST /api/v1/sessions`, then `GET /api/v1/servers` lists at least one game server. |
| play version | `/play/version.txt` (the version that the loading screen shows) is the commit of the tars. |
| live web client | One headless Chrome visit of `/play/` as the test account: single player starts, the land drew, the console is clean. Then the visit ends. The script deletes the cookie file, `console.log`, `actions.json` and the Chrome profile. |
| Sentry | `sentry.exe issue list zanidrak/openblack --json`: no issue first seen after `--since`. Older issues with new events are listed for the lead. |
| rollback note | The `out-prev-*` folders with a matched web and server pair, and the `run.sh` line to put the newest one back. A rollback needs the user's word. |

The test account: the script reads `E:/openblack/deploy/bwgame/test-accounts.json` at run time (made by
`scripts/test-accounts.sh create` of bwgame-service, plus the beta tester and stream marks). It never prints, logs or
copies the name or the password. With no file, the three account items print STOP and the script adds the question to
`E:/openblack/handovers/questions_for_user.md`.

## How a deploy lane uses this skill

1. Build the tars (`build-v2.sh`), and do the tar checks of the brief.
2. Run part 1 on `out/`. FAIL: stop and report. PASS: put the gate summary line in the handover.
3. Run `run.sh game` as the brief says.
4. Run part 2 once. FAIL: report at once with the rollback note; roll back only on the user's word.
5. When part 2 passes, save the gate pictures as the new reference.

The deploy brief template (`E:/openblack/handovers/live-deploy-10_brief.md` style) has the gate as a step between the
build and `run.sh game`, and part 2 as the check step.
