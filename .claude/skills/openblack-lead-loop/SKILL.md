---
name: openblack-lead-loop
description: Keep every openblack agent slot busy and the merge train moving. Use when a lead session runs background lane agents and a merge train and the developer asks to start, run, change or stop the recurring slot and train check ("keep N agents running", "check the merge train every 30 minutes", "/openblack-lead-loop start"). One check fills free agent slots inside the machine's load limits and makes sure the merge train makes real progress.
---

# openblack lead loop

A lead session coordinates background lane agents (one task each) and a merge train (one merge agent that batches
finished lane branches onto master). Two things go wrong when nobody watches:

- Lanes finish and their slots stay empty, so the machine sits idle.
- The merge train stops (a failed step, a stuck build, a merge agent that ended) and finished work waits for days.

This skill runs one check (a **tick**) on a fixed interval to stop both.

## Commands

| Call | Does |
|---|---|
| `/openblack-lead-loop start` | Reads the config, runs one tick now, then schedules the tick with CronCreate. |
| `/openblack-lead-loop tick` | Runs one tick. The scheduled job calls this. |
| `/openblack-lead-loop stop` | Removes the scheduled job (CronList, then CronDelete). |
| `/openblack-lead-loop status` | Shows the slots, the load and the train state. Starts and changes nothing. |

### start

1. Read the config (see below). If the local file is missing, copy `config.example.json` to the local path, tell the
   developer which values to set, and stop.
2. Run one tick.
3. Schedule the tick with CronCreate, `recurring: true`, at `intervalMinutes`. Do not use minutes 0 and 30 (use, for
   example, `13,43 * * * *` for 30 minutes). The prompt is `/openblack-lead-loop tick`.
4. Tell the developer the job id, the next run time, and that the job lives only in this session and ends after 7 days.
   Before you schedule, look in CronList for a job from this skill. If there is one, do not make a second one.

## Config

The local file is `.claude/openblack-lead-loop.local.json` in the repository root. Git ignores it. Each developer has
their own paths and machines. `config.example.json` beside this file shows every field:

| Field | Meaning |
|---|---|
| `slots.local` | Agents that may run on this machine at the same time. The merge agent counts as one. |
| `slots.remote` | Agents for each remote build machine (`remotes[]`), with the work each may take. 0 or empty if none. |
| `load.cpuMaxPercent`, `load.freeRamMinGB`, `load.freeDiskMinGB`, `load.disk` | Start a new local agent only when all of these are met. |
| `paths.handovers` | Lane briefs (`<lane>_brief.md`) and handovers (`<lane>_handover.md`). |
| `paths.workSources` | Files and folders to choose new lanes from (a backlog folder, an audit report, trackers). |
| `paths.mergeTrain` | The merge-train folder (queue files, `run_queue.sh`, `check_lost.sh`, `carlogs/`). Empty: skip part 2 of the tick. |
| `paths.worktrees`, `paths.builds` | Where lanes put worktrees and builds. |
| `paths.laneRules` | Rules every brief points to (optional). |
| `paths.leadLog` | The lead's decision log. Each tick that changes something appends one line. |
| `git.remote`, `git.mainBranch` | Where lane branches and master live (for example `me` and `master`). |
| `models.lane`, `models.merge` | The model for research lanes and for the merge agent. |
| `intervalMinutes` | The tick interval. |

## The tick

Do the parts in this order. Read only what you need: use offsets and limits, and filter logs for the lines you look for.

### 1. Count the agents

- List the running background agents (task notifications and ListAgents). Sort them into local and each remote, by
  their brief. The merge agent is local.
- An agent that finished since the last tick: read its final report. Then do the lead's follow-up for it: queue its
  branch for the train (a `queue_all.txt` line and a `priority_order.txt` place, with its holds), put a waiting look on
  the review page, and log any question for the developer. Do not count it as running.

### 2. Merge train health

Skip this part when `paths.mergeTrain` is empty. If the `openblack-merge-train` skill is available, follow it for
every train step. Otherwise use the steps here.

1. Run `check_lost.sh`. Every line it prints is an unmerged car that is not in the order: put it in
   `priority_order.txt` (respect its "after N" notes) or in `lost_ignore.txt` with a reason.
2. **No merge agent runs:** if `priority_order.txt` has unmerged cars with no hold (an unapproved look, an unsigned
   deviation, an unmet "after N"), write batch lines (as many cars per line as possible, a dependency before the cars
   that need it) and start one merge agent (`models.merge`). Only one merge agent at a time.
3. **A merge agent runs:** look for progress in the last 60 minutes. Any of these counts:
   - `git.mainBranch` on `git.remote` moved (fetch first);
   - the newest `carlogs/queue-*.log` or step log changed;
   - the batch worktree has new commits;
   - a build or test process runs (check by process name and PID, for example ninja, cmake, ctest).

   **No progress:** read the tail of the newest logs, filtered for errors. Find the cause: a failed step, a conflict, a
   left-out branch, a test failure, a build that waits on nothing. Then send the merge agent a message with the cause
   and the fix. If the agent has ended, start a fresh merge agent that continues from the batch file and the handover.
   Never start a second run of `run_queue.sh` while one runs.
4. **A batch merged since the last tick:** tell the developer:
   - the PR numbers and the new master hash;
   - anything left out, and why;
   - the turn codec version, if it changed (the server, web and desktop builds must change together);
   - that they must reconnect the inspector with `/mcp` if `src/Inspector` changed.

### 3. Fill free slots

For each free local slot:

1. Check the load: CPU under `load.cpuMaxPercent`, free RAM at least `load.freeRamMinGB`, and at least
   `load.freeDiskMinGB` free on `load.disk`. If one fails, leave the slot free and say which limit stopped it.
   If disk is the limit, remove the worktrees and builds of lanes whose branches are merged, then check again.
2. Choose work from `paths.workSources`, highest impact first. Before you write a brief, search `paths.handovers` and
   `git log <remote>/<mainBranch> --grep` for the item. A lane that already ran it, or a branch that already holds it,
   means the item is not free.
3. Write `<lane>_brief.md` in `paths.handovers`: the goal, the base (`<remote>/<mainBranch>`, fetch first), the files
   and tracker rows, the research order (bw1-decomp, then Ghidra), the checks, what to deliver (a pushed branch, a PR
   body, a handover), and a pointer to `paths.laneRules`.
4. Start the agent in the background with `models.lane` and a short prompt that points to the brief and to
   `CLAUDE.md`.

For each free remote slot, do the same with work of the kind the remote may take (`remotes[].work`). A remote does not
use the local load check. If the remote cannot be reached, say so and leave its slots free.

Folder and lane names must not have a launch switch name right after a `-` or `/`.

### 4. Report

Append one line to `paths.leadLog` for each change: lanes started, a stall found and its fix, a batch merged.
Tell the developer only when something changed or needs them. Use plain words and give the lane names. A tick that
found nothing to do says nothing.

## Rules

- Never edit the main checkout by hand. Lanes and merges work in their own worktrees.
- Never force-push and never amend a pushed commit.
- Stop processes only by their own PID.
- A command the developer refused is not tried another way: stop and report it.
- Visual changes and deviations from vanilla wait for the developer's sign-off. A tick never approves them.
- Never print, log or copy passwords, tokens or other secrets. Do not put them in briefs.
- Do not start an agent that duplicates a running or finished lane.
