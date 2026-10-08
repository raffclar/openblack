---
name: create-issues
description: >-
  Maintainer (diegoscood-ai) only. Turn planned work into well-scoped GitHub issues on diegoscood-ai/openblack and
  distribute them so several contributor agents can work in parallel: one pull request per issue, evidence-backed,
  ordered by dependencies and priority, with no two open issues fighting over the same files. Also confirms claims and
  keeps the queue full. Use when asked to "create issues", "plan work for the other agents", "fill the queue", or when
  fewer than five unclaimed agent-task issues are open.
---

# Create and distribute issues on diegoscood-ai/openblack

Read `CLAUDE.md` ("Shared rules", "Labels" and "Code") first if you haven't this session.

## 0. Check your role

```sh
gh api repos/diegoscood-ai/openblack --jq .permissions.push   # must be true
```

If it isn't `true`, stop: only the maintainer runs this skill.

## 1. Look at the queue

```sh
gh issue list -R diegoscood-ai/openblack --state open --label agent-task --limit 200 \
  --json number,title,labels,assignees,createdAt,body
```

Sort them into: unclaimed and ready, `claimed`, `blocked`, `needs-info`. Then:

- **Confirm claims.** For each unclaimed issue, read its comments
  (`gh issue view <n> -R diegoscood-ai/openblack --comments`). A claim is a comment by `raffclar` starting with
  `Claiming this.` that names an agent and a branch. The earliest claim wins; add the label and assignee:
  `gh issue edit <n> -R diegoscood-ai/openblack --add-label claimed --add-assignee raffclar`. If a later claim exists
  from a different agent name, reply to it: `Already claimed by <agent>; please pick another issue.`
- **Release stale claims.** A `claimed` issue with no pull request and no comment from its agent for 48 hours: comment
  asking for status. If nothing comes back in another 24 hours, remove `claimed` and the assignee.
- **Unblock.** For each `blocked` issue whose `Depends on` issues are all closed, remove `blocked`.
- **Answer** `needs-info` questions, then remove the label.

Aim for at least five unclaimed, unblocked issues at all times, and more when several contributor agents are
active (about two per agent seen claiming in the last day).

## 2. Find work to hand out

Sources, in order:

1. What the human asked for in this session.
2. `docs/refactor/PROGRESS.md` ("Known gaps") and `docs/bw1-notes/parity.md`.
3. `TODO`s and stubs in the code (`git grep -n "TODO\|not implemented"` in `src`).
4. Gaps you found while reviewing pull requests.

Skip anything that is already an open issue (`gh issue list -R diegoscood-ai/openblack --search "<keywords>" --state
all`) or that you are about to do yourself.

## 3. Shape each issue

- **One pull request.** One deliverable, aim for under ~2,000 changed lines. Split larger work into a chain:
  model/pure logic → component or service → integration → debug UI, each its own issue with `Depends on #N`.
- **Evidence first.** Research the vanilla behaviour before filing: Ghidra addresses, `docs/bw1-notes` pages, game data
  files. Write down what is known exactly and what is still unknown. An issue without evidence makes the contributor
  guess, which the rules forbid.
- **Acceptance criteria** the review will check, as a checklist, including the tests that must exist and any fidelity
  run (`docs/refactor/TESTING.md`).
- **Where in the code**: the area, services, components and files most likely involved.

## 4. Distribute for parallel work

Several contributor agents may take issues at the same time, so make the open set safe to work on in parallel:

- **No overlap.** Two unclaimed issues that would edit the same files (beyond one-line registrations such as
  `Locator.cpp` or `test/CMakeLists.txt`) must not both be ready. Chain them with `Depends on #N` and `blocked`
  instead.
- **Spread across areas.** Label each with one `area:<name>` and keep ready issues spread over several areas, so each
  agent can stay in one area and its context.
- **Priority.** `priority:high` for what unblocks the most other work or what the human asked for; `priority:low` for
  polish. Most issues get none (normal).
- **Size mix.** Keep some small issues (under ~300 lines) ready so an agent waiting on review has something quick.

## 5. File it

Use the form's sections (`.github/ISSUE_TEMPLATE/agent-task.yml`). With the CLI:

```sh
gh issue create -R diegoscood-ai/openblack --title "<short broad phrase>" --body-file <file> \
  --label agent-task --label area:<name> [--label priority:high] [--label blocked]
```

Body sections, in order: `### Goal`, `### Vanilla behaviour and evidence`, `### Acceptance criteria`,
`### Where in the code`, `### Depends on`. Titles say what the game will do, e.g. "Thrown rocks split on hard
landings".

## 6. Report

End with: issues created (number, title, area, priority, depends on), claims confirmed or released, issues
unblocked, and the count of ready issues per area.
