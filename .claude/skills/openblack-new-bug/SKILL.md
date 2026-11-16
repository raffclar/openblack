---
name: openblack-new-bug
description: Recognise, record and backlog every bug in openblack or bwgame-service. Use at once when the user's message says "New bug" (in any case or form, for example "new bug:", "Another bug", "Bug:"), and also when the user describes something that goes wrong in play ("X still happens", "it shows ... for all players", "the creature starves"), when Sentry shows a new issue, or when an agent report names a fault it did not fix. Makes sure no bug is lost - each one gets the user's words, a check for duplicates, a class, a grade, a progress row and a lane or a queue place, and a short reply to the user.
---

# openblack new bug

The user reports bugs in chat while they play, often several in one session and often in one line ("New bug: when
casting a forest miracle, all players are pulled to the miracle"). A bug that is only answered in chat is lost at the
next compaction. This skill makes every bug a record that survives: a progress row, and a lane brief or a queue place.

Settings come from `.claude/openblack.local.json` (`python .claude/obconfig.py get <key>`): `paths.handovers`,
`paths.workSources` (the entry that ends in `progress` is the progress folder), `paths.looks`, `paths.leadLog`,
`git.remote`, `git.mainBranch`.

## 1. Recognise a bug

Start this skill when one of these occurs:

- The user's message says "New bug" or "bug", or describes behaviour that is wrong.
- The user disputes a result ("still not acted upon", "I'm still seeing the issue"). This is the same bug, reopened.
  Find its row and lane, and reopen them. Do not make a new row.
- Sentry has a new issue (crash, desync, map download, service error).
- An agent report names a fault outside its scope, or a gap it did not close.

A question ("status on X?") is not a new bug. Answer it from the row and the lane.

## 2. Record the user's words first

Before any research, write down:

- the user's exact words, in quotes, with the date (UTC);
- what the user gave as context: single player or multiplayer, desktop or web (browser), live site or a local build,
  the land or map, the host or a client, what they did just before;
- the build that they ran: the live release (`sentry.exe` release tag, the live build id) or the local commit.

If a fact that decides the fix is missing (for example single player or multiplayer), research it first from the
logs and Sentry. Ask the user only when the research cannot find it. Put the question in
`<handovers>/questions_for_user.md`, not in a pop-up, unless the bug blocks live play now.

## 3. Find duplicates and earlier work

Search before you write anything new:

- the progress folder: `ag -i "<key words>" <progress> </dev/null`;
- the lane briefs and handovers in `<handovers>`: a running lane may own it;
- `<mergeTrain>/queue_all.txt`: a car may already hold the fix;
- `git log <remote>/<mainBranch> --grep "<key word>" -i`: the fix may be on master but not deployed;
- `docs/vanilla-bugs.md` and `docs/deviations.md`: it may be vanilla behaviour that is kept on purpose;
- Sentry: `sentry.exe issue list --org <org> --project openblack`, for the same fault on the live site.

Then decide one of these:

| Found | Do |
|---|---|
| A running lane owns it | Send the lane the user's words and the new facts (SendMessage). |
| A car or master has the fix, but the fix is not live | Tell the user which car or commit has it and when it ships. |
| A fixed row, but the user still sees the fault | Reopen: set the row to partial, add the user's words, start a fresh lane with the old handover as input. |
| A vanilla-bugs row says vanilla does this | Re-verify from first principles (the user's observation wins until the research proves it). |
| Nothing | Continue with step 4. |

## 4. Classify it

Give the bug one class. The class decides the skills, the tests and the reviewer.

| Class | Signs | Also use |
|---|---|---|
| Crash or hang | Sentry crash, the game stops, a white screen | P1. Read the Sentry event and the minidump first. |
| Desync | Sentry "Desync: <key>", different checksums | Synced logic rules: repeat sim runs, server check, net check |
| One player's view shows for all | One player's action moves the camera, opens a text, plays a sound or shows a panel on every screen | Keep the synced part synced. Show the screen part only for the local player. Read the handovers of mp-local-ui-1 and mp-miracle-camera-1. |
| Differs from vanilla | Behaviour or looks that the original game does differently | `openblack-vanilla-bugs`, `openblack-deviations`, `ghidra-catalogue` |
| Visual | Looks, animation, effects | `openblack-visual-review` (look file, motion check) |
| Service or site | bw.zanidrak.com, lobby, sign-in, accounts, maps, assets | bwgame-service, in a clone under the worktrees folder. Never edit the main service checkout. |
| Deploy or live operation | The live build is old, mismatched or down | The deploy workflows only. The workflow approval belongs to the user. |
| Performance | A frame takes more than 10 ms | Benchmark tiers of `test-triage` |

A fault in the original game itself is not an openblack bug. Record it with `openblack-vanilla-bugs`.

## 5. Grade it

Use the `test-triage` skill: impact low, mid or high, and the tier T0 to T4. Give it a priority:

- **P1:** it blocks live play now, for example a crash, a game that does not start, a desync in a live game, a failed
  deploy or a failed nightly. Start a lane at once, before other work. If needed, pause other lanes.
- **P2:** live multiplayer is wrong but it can be played. Start a lane in the next free slot.
- **P3:** the rest. Add the progress row. The lead loop takes it from the progress folder.

## 6. Backlog it

Every bug gets a progress row, at once, in the same turn:

1. Find the domain file in the progress folder (for example `multiplayer/network_play.md`, `miracles/forest.md`,
   `creature/needs.md`). Make a new file only when no file fits, and add it to `INDEX.md`.
2. Add one row in the right section, status `todo` (or `partial` for a reopened row). Use this form for the notes:
   `<date> bug (user): "<exact words>". <context>. <class>, <priority>. Lane <lane> | not started.`
3. Update the file's progress line and its `INDEX.md` score.
4. For each extra fault that the research finds later, add a separate row (rule: every missed thing goes to progress).

Rows are plain English. They contain no addresses and no names from the decompiled code.

## 7. Dispatch it

- P1 and P2: write `<handovers>/<lane>_brief.md` and start the lane in the background. Use the model that the lead
  uses for research lanes. The brief holds:
  - the user's words;
  - the context and the class;
  - the research order (bw1-decomp, then Ghidra through the catalogue);
  - the files that you suspect;
  - the overlaps with running lanes;
  - the tier, the area tests, and the look file if the bug is visible;
  - what to deliver: the branch, the PR body, the handover, the progress row and "Testing done".
- Use a lane name `<area>-<short-name>-<n>`. Make sure that no switch name of a launch command comes right after a
  `-` or a `/`.
- Before you start a lane, check the load limits: memory use under 95 %, the disk free space, and the agent slots.
  If a limit stops the lane, the row stays `todo` with "waits for a slot", and the lead loop starts it later.
- Add one line to `<leadLog>`.

## 8. Reply to the user

Reply in a few lines, in plain words:

- what you understood (one sentence, the user's own terms);
- whether it is new, a duplicate or a reopened bug, and what you found already (a car, a lane, master, the live build);
- the lane that works on it now, or the row that holds it;
- any question that decides the fix.

Do not say that a bug is fixed until a test or a check in the game proves it. Say what is still not proved.

## 9. Close the loop

When the lane reports:

1. Queue its car (`openblack-merge-train`) and put a look on the review page if the bug is visible.
2. Set the row to `done` with the car number, or to `partial` with the reason.
3. When the fix is live, tell the user that the bug they reported is fixed, and in which build. Quote their report.

## Rules

- Never skip the progress row because a lane starts at once. The row is the record; the lane can fail.
- Never fix a bug in a way that differs from vanilla without the user's sign-off (`openblack-deviations`).
- Never probe or load-test the live site to reproduce a bug. Use local builds, the logs and Sentry.
- Never print secrets or personal data (names, addresses, emails) from logs or Sentry in a row, a brief or a reply.
