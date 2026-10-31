---
name: openblack-visual-review
description: Show the user openblack visual changes for sign-off as one local HTML review page ("Looks waiting on you") with each lane's before/after/vanilla GIFs or videos, what changed, what to check and any deviation to sign off. Use whenever an openblack lane changes something visible or animated (rendering, effects, animation, UI, camera, motion), when an agent's report includes GIFs/screenshots for the user's look, or when the user asks to see the looks waiting on them. Agents write a look file; the lead builds and opens the page.
---

# openblack visual review page

The user signs off every visual change by watching it move (and every deviation) before it merges. Show visuals
this way: one page, one card per lane, the GIFs side by side, not a list of paths in chat.

The folders come from `.claude/openblack.local.json` (`python .claude/obconfig.py get paths.looks`): `<looks>` is
`paths.looks` and `<screenshots>` is `paths.screenshots`.

## Lane agents: write a look file

When your change is visible, finish your motion capture (GIFs or short videos, catalogued in
`<screenshots>/catalogue.jsonl`) and write **`<looks>/<branch>.json`**:

```json
{
  "branch": "creature-dispenser-vials",
  "title": "Creature dispenser vials",
  "meta": "d6d64aa3c · vanilla-bugs row 26 (Kept) · no deviations",
  "summary": "What vanilla does, what openblack did before, what it does now. Strict ASD-STE100 STE.",
  "bullets": ["optional extra points"],
  "check": "Exactly what the user should look for in the media.",
  "deviations": ["row 47 PENDING: <the row's text>"],
  "media": [
    {"path": "<screenshots>/miracles/dispensers/..._before-after-vanilla.gif", "caption": "Overview: before | after | vanilla"}
  ],
  "grid": false,
  "status": "waiting"
}
```

Rules for good looks:
- **Write all text in strict ASD-STE100 Simplified Technical English (STE)**: `title`, `summary`,
  `bullets`, `check`, `deviations` and media `caption`s. Use the same spec as the commit messages: the core rules in
  the `openblack-commit-check` skill, section 2, and the words in `DICTIONARY.md` at the root of the openblack
  repository. In short: descriptive sentences of 25 words or fewer, instructions of 20 or fewer; one topic per
  sentence; active voice; articles; no phrasal verbs, -ing nouns, contractions or idioms; approved words with one
  meaning. Describe the change in behaviour and in looks, not the code.
- **Show the comparison, not just the result.** Best: one GIF with before | after | vanilla side by side (same
  camera, same moment). Otherwise separate before and after media, with `"grid": true` so they sit next to each other.
  Include vanilla whenever a reference exists (a recorded video of vanilla, a construct-map capture).
- **Motion, not stills**, for anything animated (rule: check motion). Real-time speed unless the caption says otherwise.
- Keep each file under ~25 MB; trim to the moment that matters. `.mp4`/`.webm` (looping, muted) are fine and much
  smaller than GIFs.
- `check` names the thing to judge. If something is unsettled ("the bolt ends 0.1 s early; vanilla?"), say so there.
- Any deviation goes in `deviations` with the row's text; the page shows it in red for sign-off.
- Mention the look file in your report and handover. Never set `status` yourself beyond "waiting".

## Lead: build and show the page

```
python .claude/skills/openblack-visual-review/looks.py            # every waiting look, opens the browser
python .claude/skills/openblack-visual-review/looks.py BRANCH ...  # just these
python .claude/skills/openblack-visual-review/looks.py --all --no-open
```

It writes `<looks>/looks.html` (local file:// media, so nothing is uploaded: the GIFs are far over the
artifact size limits, and these are private) and opens it. Exit 2 means a media path is missing: fix the look file.

When the lead writes a look itself (from a handover), read the lane's handover and use its words for summary and check, changed to STE. The lead also writes in STE when it tells the user about the looks in chat.

Then tell the user in chat, briefly: the page path, one line per lane, and anything they must decide (deviations,
unsettled points). Ask for approve/reject per lane.

## After the user answers

Set each look's `status` to `approved` or `rejected` and `answer` to the user's words (quote them), then:
- approved: the lane goes into the next batch car (merge train) with "Approved by the user after the motion look" in
  its PR body;
- rejected: send the user's words to the lane (or queue a fix lane) and keep the look `rejected` until a new capture
  replaces it (then back to `waiting`).

## Scenarios that check themselves

A testbed scenario can state its own outcome as `checks` (see `src/Debug/TestbedScenarioChecks.h`). Use them when a
scenario shows a thing that you can also measure, such as a creature that gets to a point. They do not replace the look.

- Command line: `openblack --scenario ID --scenario-check --background -n 4000` writes one line for each check and the
  verdict, then ends. The exit code is 0 when all checks pass, 1 when one fails, and 2 when the scenario has no checks.
- Inspector: `game.scenario_checks` (tool `game_scenario_checks`) gives the verdict of the last scenario while the game
  runs: `passed`, `failed` or `waiting`.
- The merge train and the lanes can run the scenarios that have checks, and fail the car on exit code 1.
- A scenario can load a story land (`Land::Story` with the land's script), and `tools/sim/recorded_scenario.py` records
  a play-through and checks it again later by its hashes.

Related: openblack-deviations skill (deviation sign-off), test-triage (motion check tier).
