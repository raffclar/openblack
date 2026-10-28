---
name: openblack-deviations
description: Spot, record and get the user's sign-off for every deviation from vanilla Black & White in openblack. Use whenever openblack work (code, shaders, research, reviews, briefs, merges) does or proposes anything that differs from vanilla's logic, visuals, timing or data, including fixes of vanilla bugs, scaling for other screens, debug-only behaviour that leaks into play, or approximations. Keeps docs/deviations.md in the openblack repo current and never lets an unsigned deviation ship.
---

# openblack deviations

openblack replicates vanilla Black & White exactly. A **deviation** is anything openblack does differently on
purpose. Every deviation needs the user's sign-off, and every one is listed in **`docs/deviations.md` in the openblack
repo** (version controlled; bw1-decomp's `docs/openblack/deviations.md` only points there).
Anything that differs from vanilla and is not listed there is a bug.

## 1. Monitor: is this a deviation?

Ask this whenever you write, review or brief openblack work. It is a deviation if openblack, on purpose:

- **fixes a vanilla bug** or quirk that players saw (e.g. #30 trader fetch, #34 the crèche's fit after a load), even
  a cosmetic one;
- **scales or adapts** for screens vanilla never had: aspect ratio, resolution, pixel sizes, input sensitivity, field
  of view (rule #4 cases go under row 4/21 or their own row, e.g. #33 Hor+);
- **changes timing** from vanilla's (frame-stepped effects capped at 60 steps a second, #14/#29);
- **changes a visual** (texture depth, filtering, shading, fog, LOD distances) or **a sound**;
- **adds an option or behaviour** vanilla didn't have that a player can reach (a menu entry, a radio, a key). Debug and
  automation-only switches that a normal player never sees are not deviations, but say so in the report;
- **approximates** where vanilla's exact rule couldn't be found. Don't: leave it partial with evidence instead. An
  approximation that would ship needs a row like any other.

It is **not** a deviation when openblack matches vanilla (a vanilla quirk kept as vanilla, like keeping a vanilla bug
the user chose to keep), when a vanilla feature is missing (that's a gap: progress tracker), or for code structure
(modern C++ is not a behaviour change).

Watch for it in: your own diffs, agents' reports ("I chose to...", "differs from vanilla", "improved", "cleaner"),
reviews before a train car, user requests that change vanilla (confirm the user knows it's a deviation), and old
rows that a change makes stale.

## 2. Record it

Edit `docs/deviations.md` **in the same branch as the code**, so the row merges with the code.

- **New row:** the next number (rows are never renumbered; numbers are kept for good). Columns:
  `| # | Area | What vanilla does | What openblack does instead | Why | Decided | Where |`
  - *What vanilla does*: the rule with its evidence (Ghidra address, data, or a vanilla capture).
  - *Decided*: `PENDING (proposed by <branch>, <date>)` until the user signs off, then
    `User signed off, <date> ("<their words>")`.
  - **Standing rules** (agreed by the maintainer): a row that is a clear case of rule #4 (wide-screen
    layout) or the 60 Hz rule for frame-stepped effects may be added without asking first, with *Decided*
    `Case of the user's rule #4 / 60 Hz rule (<date>)`. The lane's report must name the row so the lead tells the
    user. Anything else, or anything doubtful, is PENDING until the user signs off.
  - **Default is vanilla**: keep vanilla game logic and visuals unless the maintainer decides otherwise. When unsure
    whether something is a deviation, treat it as one and ask.
  - *Where*: the code (files, functions) and the research note in bw1-decomp `docs/openblack/...`.
- **Rejected or withdrawn:** never delete the row. Prefix the vanilla column with
  `**WITHDRAWN <date>: <why>.**` or `**REJECTED <date>: <the user's ruling>.**`, and revert the code to vanilla.
- **Changed scope:** edit the row in place and say what changed and when.

## 3. Ask for sign-off

- **Agents:** never ship a deviation on your own judgement. Implement vanilla, add the row as PENDING, and send the
  lead one message: the case, vanilla vs the proposal, the evidence, and what you'll do meanwhile (keep
  vanilla). Keep working on everything else.
- **The lead:** put the question to the user with the evidence and a recommendation, offering vanilla as an option
  (AskUserQuestion works well: vanilla / the proposal / other). Record the answer in the row at once, relay it to the
  agent, and log it in the queue log.
- **The user's direct request** that changes vanilla is a sign-off; still record the row, quoting them.

## 4. Before a train car merges

- Every PENDING row in the branch is either signed off or the code is back to vanilla. Never merge a PENDING
  deviation.
- If two branches took the same number, renumber the later branch's row (and its mentions) in the car.
- The PR body names every deviation row the car adds or changes.
