---
name: openblack-merge-train
description: Use whenever you plan, queue, batch, run, check or clean up openblack merge-train cars, brief a merge agent, decide the next car, add a finished lane's branch to the queue, or answer "where is the merge train". Covers the queue files, batching as many cars as possible into one build, the lost-car check, merge-agent briefs, hand merges and finish cars, post-merge checks once per batch, registry renumbering and clean-up.
---

# openblack merge train

The train puts lane branches onto the main branch, one car at a time. Each car merges its branches in a fresh
worktree, builds Debug and RelWithDebInfo, runs the tests and the network checks, opens one PR and merges it. Visual
changes and deviations need the maintainer's look or sign-off first.

**Compilation is the bottleneck.** Batch as many cars as possible into ONE car: one configure, one build of each
config, one test run, one PR. Never run one car per branch when the branches can go together.

## Settings

The scripts read `.claude/openblack.local.json` through `.claude/obconfig.py` (see the example file next to it). A
worktree finds the file in the main checkout. The values the train uses:

| Value | Use |
|---|---|
| `git.remote`, `git.mainBranch` | Where lane branches and the main branch live (the scripts call them `$R` and `$M`). |
| `git.githubRepo` | The `owner/name` the PRs go to (`gh pr create -R`). |
| `paths.mergeTrain` | The train's working files (below). Not in the repository. |
| `paths.worktrees`, `paths.builds` | Where cars put their worktrees (`prm-wtb<batch>`) and the ship builds. |
| `paths.leadLog` | Each merged car appends one line here. |
| `tools.obBuild` | The build tool. Default: `tools/build/ob-build` in the main checkout. |
| `tools.clangFormat` | clang-format for the automatic conflict fixes. |
| `commit.trainTrailer` | The trailer of the train's commits. |
| `train.extraCheck` | Optional. A script that checks the car on another machine: called with `start <worktree> <batch>` before the build, and `report <worktree> <batch>` after it. A failed report stops the car. |

Run `python .claude/obconfig.py shell` to see the values the scripts get.

## Files

Scripts, in this skill's `scripts/` folder (call them by their full path from any directory):

| Script | Use |
|---|---|
| `run_queue.sh <queue file>` | Runs each line: `batch.sh`, or `car_finish.sh` for a `finish` line. Plain background command, once. |
| `batch.sh <n> <batch> <short:branch>...` | Merges every branch of the line onto the main branch. test/CMakeLists.txt, docs tables, testbed and include-only conflicts are resolved automatically. A branch with other conflicts is LEFT OUT and the train stops. |
| `car_finish.sh <n> <batch> <short:branch>...` | Finishes a hand-merged car in `<worktrees>/prm-wtb<batch>`. |
| `car_checks.sh` | The checks, PR and merge that both of the scripts above use. Not called directly. |
| `check_lost.sh` | Lists unmerged cars missing from both order files. Run before you choose the next work. The output must be empty. |
| `prep_hand.sh`, `post_hand.sh` | Start and end a hand car, with vanilla-bugs row renumbering (`old:new`). |
| `nc3_check.sh <worktree> <build dir>` | The 3-client net check of each car. |
| `uz_shots.py <ref exe> <new exe> <out dir>` | The pixel check of old against new master (needs `paths.gameData`). |
| `resolve_*.py`, `union_conflicts.py`, `includes_only.py`, `fix_kind_names.py`, `hunk.py`, `sort_rows.py` | Conflict helpers. `hunk.py` resolves one conflict block by hand. |

Working files in `paths.mergeTrain` (each developer's own; not in the repository):

| File | Use |
|---|---|
| `queue_all.txt` | Every car ever queued: `<n> <batch> <short:branch> (notes: deps, looks, holds)`. Add a line when a lane finishes. |
| `priority_order.txt` | The order the lead takes cars in. Every unmerged car must be here or in `lost_ignore.txt`. |
| `lost_ignore.txt` | Unmerged cars that are not lost, each with its reason (carried by car N, already in, dropped, held). |
| `batches_<date>.txt` | The batch lines to run: `<n> <batch> <short:branch> <short:branch> ...` |
| `queue_one.txt` | The one line run_queue runs now. Written by the merge agent only. |
| `prs/<short>.md` | Each lane's PR body; the batch PR joins them. |
| `carlogs/` | Step logs per batch (`batch<name>-car-*.log`, `queue-<name>.log`). |

Handovers (`paths.handovers`): `merge-train_handover.md` (one section for each car or batch), `queue_handover.md`
(questions and answers), the lead log, `batch_merge_brief.md` (the merge agent's steps), `refresh_registry.txt`
(registry row numbers that refresh and merge agents claimed).

## The lead's loop

1. Run `check_lost.sh`. Any line means a car fell out of the order: put it in `priority_order.txt` (respect its
   "after N" notes) or in `lost_ignore.txt` with a reason.
2. Take the unmerged cars from the TOP of `priority_order.txt` that have no unmet hold: look approved
   (`<looks>/<branch>.json` status "approved"), no open deviation sign-off, dependencies ("after N", "contains N")
   met or in the same batch.
3. Group them into as few batch lines as possible (8-11 branches is fine). Keep a dependency before its dependents in
   the same line. Use a new train number and the next free batch name for each line. Write the lines to
   `batches_<date>.txt`.
4. Old branches (2+ days behind the main branch, or known conflicts): first run refresh agents in parallel. Each
   merges the main branch into its branches, resolves conflicts, renumbers registry rows, builds, tests and pushes.
   Then the train merges them with no hand merge.
5. Start ONE merge agent (a cheaper model is fine) for the batch file with the brief below. Only one run_queue may
   run at a time.
6. When it reports: tell the maintainer the PR numbers, the new main hash, the checks and anything left out. Say if
   `src/Inspector` changed (then the inspector MCP server must reconnect, `/mcp`). Delete finished lanes' worktrees
   and builds, and run `check_lost.sh` again.

## Merge agent brief

Point it at: `CLAUDE.md`, the lane rules, `batch_merge_brief.md` if you keep one, the last car sections in
`merge-train_handover.md`, the batch file, and each car's queue_all line, PR body and lane handover. Name the current
main hash. Its steps:

1. Check that no run_queue runs (by process and PID). Copy ONE batch line to `queue_one.txt`; start
   `run_queue.sh queue_one.txt` once.
2. LEFT OUT branches: hand merge ALL of them together in `<worktrees>/prm-wtb<batch>` (read the conflicting main
   commits; keep the main branch's later work plus the car's change), commit, write
   `finish <n> <batch> <every left-out short:branch>` to `queue_one.txt`, and run run_queue once more. A finish line on
   the command line does nothing: it must go through the file.
3. nc3 (3-client) failure: run `nc3_check.sh` again by hand once. If it fails again, stop and report. Extra check
   failed: an outage (unreachable, timeout) runs again by hand, then finishes; a real failure stops. A test failure:
   find the branch (build without it), drop it from the line, run the rest again, and report it.
4. Post-merge checks ONCE per batch on the new main branch: `uz_shots.py` pixels old against new (name the car that
   explains each changed scene), server_check Land 1 100/100, net_check mpm_2p_1 with 2 clients for 2000 turns, a
   TwoGods all-computer net_check with 2 observers, plus any car-specific check from its queue line.
5. Write one "Batch <n>" section in `merge-train_handover.md`. Remove the merge worktree, its build and the local
   batch branch (keep the remote branches). No `rm -rf` on globs.

## Registry rows

Two branches often use the same next free row in `docs/deviations.md` or `docs/vanilla-bugs.md`. Keep the main
branch's rows, give the car's rows the next free numbers, record each claim in `refresh_registry.txt`, and fix every
reference (code comments, tests, docs, the PR body in `prs/`). A renumbered deviation keeps its text and its sign-off.
Never take a number another open branch holds (for example a deviation signed on a car that is not yet merged).

## Rules that always apply

- Never edit the main checkout by hand; never force-push or amend a pushed commit.
- Commits: your own git identity, strict STE, the trailer (skill `openblack-commit-check`).
- A refused command is not tried another way: stop and report it.
- Visual changes need an approved look before their car runs; deviations need the maintainer's sign-off.
  Vanilla-faithful choices need no ask (record "ANSWERED by the lead").
- Games run with `--background` and `-n`; stop processes only by their own PID.
- Keep the machine inside its load limits (`load` in the settings).
