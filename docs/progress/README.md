# openblack progress

How much of Black & White (2001) openblack does, domain by domain. Each folder is one domain of the game; each file in
it is one feature, broken down into everything the original game does for that feature, so it can be ticked off.

Measured against `miracles-rework` at `40d5d371` (stack 1 plus the miracles, their fixes, hand navigation, the hand
morph and the editor camera speed). Update a row's status in the same change that does the work.

## Status

| Status      | Meaning                                                                                     |
|-------------|---------------------------------------------------------------------------------------------|
| **done**    | Works as the original game does, checked against it                                          |
| **partial** | Some of it works, or it works but differs from the original (the notes say how)              |
| **todo**    | Not started                                                                                  |
| **n/a**     | Not applicable to openblack (the notes say why)                                              |

A file's score counts done as 1, partial as ½, todo as 0, and leaves n/a out.

## File format

```markdown
# <Feature>

<One or two sentences: what the feature is in the game.>

**Progress: <done>/<total> done, <partial> partial — <score>%**

## <Part of the feature>

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| <one thing the game does, in plain words> | done | `src/...` (system or file); test name if any |
```

- One row per behaviour a player could notice, or a rule the game follows, not one row per class.
- Notes say where openblack does it (paths under `src/`), or what is missing.
- Plain English: no addresses, no decompiled or internal names of the original.
- A file about work done in another codebase says so under its title, e.g. `**Codebase: bwgame-service**
  (C:\projects\bwgame-service)` for the online servers and web site, and its notes start with that codebase's name
  (`bwgame-service: online/clans.py`). Its rows count in the scores like any other.

## Domains

See [INDEX.md](INDEX.md) for every file and its score.

Bugs in the original game that the research has found are logged, unscored, in [vanilla_bugs/](vanilla_bugs/README.md).

## Tools

- `python docs/progress/tools/progress_score.py <file>...` rewrites each file's Progress line from its rows.
- `python docs/progress/tools/progress_index_regen.py docs/progress` regenerates `INDEX.md`, sub-folders included.
- `python docs/progress/tools/vanilla_bugs_count.py docs/progress/vanilla_bugs` rewrites the bug files' count lines.
