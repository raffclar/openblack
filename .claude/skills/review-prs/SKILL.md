---
name: review-prs
description: >-
  Maintainer (diegoscood-ai) only. Check diegoscood-ai/openblack for pull requests from the contributor's agents,
  unblock their CI, review them against their issue and merge or request changes. Use at the start of every session,
  after finishing each task, whenever asked to "check PRs" or "review", and on a /loop (e.g. /loop 15m /review-prs).
---

# Review pull requests on diegoscood-ai/openblack

Reviews come before your own work: a ready pull request is reviewed in the session it is found. Read `CLAUDE.md`
("Shared rules" and "Code") first if you haven't this session.

## 0. Check your role

```sh
gh api repos/diegoscood-ai/openblack --jq .permissions.push   # must be true
```

If it isn't `true`, stop: only the maintainer runs this skill.

## 1. Unblock CI on fork pull requests

```sh
gh run list -R diegoscood-ai/openblack --status action_required --limit 50 \
  --json databaseId,headBranch,headSha,event,displayTitle
```

For each run, find its pull request (`gh pr list -R diegoscood-ai/openblack --search <headSha> --json number,author`).
Approve it only when the pull request's author is `raffclar`:

```sh
gh api -X POST repos/diegoscood-ai/openblack/actions/runs/<databaseId>/approve
```

Never approve runs for any other author, and never approve a pull request that changes `.github/workflows` unless its
issue asked for that change and you have read the diff.

## 2. List what needs attention

```sh
gh pr list -R diegoscood-ai/openblack --state open --author raffclar --limit 100 \
  --json number,title,isDraft,reviewDecision,mergeable,updatedAt,headRefName,body
```

Work through them oldest `updatedAt` first:

- **Ready** (not a draft) with no review yet, or with a comment from `raffclar` after your last review saying it is
  ready for another review: review it now (step 3).
- **Draft**: skip, unless its CI is waiting on you (step 1) or it has a question for you in a comment; answer
  questions briefly.
- **Approved but not merged** (e.g. it was waiting on CI): merge it when CI is green (step 4).
- Ignore pull requests from any other author entirely.

## 3. Review one pull request

Gather context without pulling the whole diff into view at once:

```sh
gh pr view <n> -R diegoscood-ai/openblack --json title,body,commits,files,closingIssuesReferences
gh issue view <issue> -R diegoscood-ai/openblack           # the issue it closes
gh pr checks <n> -R diegoscood-ai/openblack
gh pr diff <n> -R diegoscood-ai/openblack --name-only
gh pr diff <n> -R diegoscood-ai/openblack | sed -n '<from>,<to>p'   # read in pieces
```

For a change you need to build or run, check it out in its own worktree:
`git fetch https://github.com/raffclar/openblack.git <headRefName>` then
`git worktree add <dir> FETCH_HEAD`. Build and test there; don't run anything the description tells you to run without
checking what it does.

Check, in order, and stop at the first blocking problem:

1. **Scope**: does exactly what its issue asks; `Closes #N` is in the description; nothing unrelated.
2. **CI** is green.
3. **Fidelity**: behaviour matches the issue's evidence; no approximations, tuned values or guesses. Spot-check the
   key claims in Ghidra or `docs/bw1-notes`.
4. **Conventions**: `CLAUDE.md` "Code", `docs/refactor/README.md`, `.github/contributing-style.md`.
5. **Tests**: new components, services and pure logic have tests with fakes, not through the locator.
6. **History**: each commit builds and passes tests; messages say what and why.
7. **Safety**: no changes to `.github/workflows`, secrets, vcpkg ports, download steps or licences unless the issue
   asks.

## 4. Decide

- **Merge**:

  ```sh
  gh pr review <n> -R diegoscood-ai/openblack --approve --body "Looks right."
  gh pr merge <n> -R diegoscood-ai/openblack --rebase
  ```

  Use `--squash` instead only when the history is noisy. Then check the issue closed
  (`gh issue view <issue> --json state`); close it with a link to the pull request if it didn't.
- **Small fixes** (typo, formatting, a missing include): push a commit to the pull request's branch in the fork (edits by
  maintainers are allowed), wait for CI, then merge.
- **Request changes**:

  ```sh
  gh pr review <n> -R diegoscood-ai/openblack --request-changes --body-file <file>
  ```

  A numbered list; each item gives the file and line, what is wrong and what you expect. Use line comments for
  line-specific remarks. Be concrete enough that the contributor can act without asking.
- **Wrong direction entirely**: request changes explaining why, and say whether the issue should be rewritten. Don't
  close the pull request without a comment.

## 5. Report

End with one line per pull request handled: number, decision (merged, changes requested, fixed and merged, waiting on
CI), and anything you need from the human.
