# Vanilla bugs

Every bug in the original Black & White (2001, "vanilla") that the progress research has found: mistakes in its code,
its compiled story scripts or its shipped data. Each row is one defect, with what the player sees, the evidence, the
progress file(s) where the research found it, and what openblack does about it.

This section is a log, not a feature list: its tables have no Status column and are not counted in the scores
([../INDEX.md](../INDEX.md) lists the files with no rows).

What is not here:

- openblack's own bugs (they belong in the feature files' notes or in issues).
- Deliberate cut content: lines, quests and features the developers switched off or never finished are in
  [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md). A cut thing is listed here only when it is a
  mistake, e.g. a line meant to play that can't.
- Design choices the research merely finds odd. A row marked "may be intended" is one where the evidence points to a
  slip but the developers could have meant it.

The sweep covers every progress file as it stood at the end of 2026-10-08, including the finished gold scrolls of all
lands and the cut ones, football, the vortex (portals and portals per land), fireflies and the gathering box, plus the
physics research spec. New research adds its findings to the area files below.

## Summary

| Area | File | Bugs | Soft-locks and lost progress |
|------|------|------|------------------------------|
| Story scripts: gold scrolls, land control, the guide, the Land 5 curse, the vortices | [story_scripts.md](story_scripts.md) | 77 | 5 |
| Silver quests and the shared swap offer | [silver_quests.md](silver_quests.md) | 71 | 5 |
| Creature | [creature.md](creature.md) | 3 | 0 |
| Villagers and towns (tools, sounds, football) | [villagers_and_towns.md](villagers_and_towns.md) | 8 | 0 |
| Physics and objects | [physics_and_objects.md](physics_and_objects.md) | 7 | 0 |
| Miracles and dispensers | [miracles.md](miracles.md) | 2 | 0 |
| Interface, help and text | [interface_and_text.md](interface_and_text.md) | 11 | 0 |
| Multiplayer and skirmish | [multiplayer_and_skirmish.md](multiplayer_and_skirmish.md) | 9 | 1 |
| Engine (picking, land queries, clips, films) | [engine.md](engine.md) | 5 | 0 |
| **All** | | **193** | **11** |

## How bugs are classified

Each area file groups its rows by how much the bug matters to a player, worst first:

1. **Soft-locks and lost progress**: the story or a quest can stall, or the player loses something they earned (a
   reward, growth, a game in progress).
2. **Wrong outcomes**: the wrong reward, alignment, score or result, or a rule that doesn't do what it plainly means
   to.
3. **Missing or wrong feedback**: lines, sounds, camera work or visuals that never play, play twice, or are wrong.
4. **Data and text errors**: typos and slips in the text tables, land scripts and other shipped data.
5. **Harmless**: real mistakes with no visible effect (dead checks, values set and never read, bugs in quests the game
   never starts).

A file leaves out a group it has no rows for.

## Columns

| Column | What goes in it |
|--------|-----------------|
| Bug | The defect, in plain English |
| What the player sees | Its effect in play, or "nothing visible" |
| Evidence | The script, data or engine evidence in plain words: script file names (`CreatureCurse`, `Reward.txt`) and quoted lines are fine; no addresses, no decompiled or internal names of the original, no assembly. "checked in the script source" or "checked in the game data" marks rows re-read while building this list |
| Found in | Links to the progress file(s) that describe it; "nearest:" when only the physics research spec has it so far |
| openblack | What openblack does about it |

## The openblack column

Whether openblack should reproduce a vanilla bug for fidelity or fix it is a policy question for the project, decided
per bug. Until it is decided a row says **undecided**. Where the progress files already record what openblack does,
the column says so:

- **reproduced**: openblack behaves the same way today.
- **differs: …**: openblack behaves differently today (not a decision, just the current state).
- **undecided (…)**: no decision, with a note when the progress file already leans one way (e.g. "the progress file
  says to keep it").

Many story-script bugs come for free once openblack runs the original compiled scripts, since it runs them as shipped;
fixing one would mean patching the script or the command it relies on.

## Adding a bug

1. Check it is a bug in the original game (not openblack's, not cut content) and find its evidence.
2. Add a row to the area file's matching severity group (add the group if the file lacks it), in the five-column format,
   with the second column never one of the status words (done, partial, todo, n/a) so the scorer skips it.
3. Link the progress file that describes it, and make sure that file mentions it too.
4. Update the file's count line (or rerun
   `python docs/progress/tools/vanilla_bugs_count.py docs/progress/vanilla_bugs`, which rewrites
   every file's count line) and the summary table above.
