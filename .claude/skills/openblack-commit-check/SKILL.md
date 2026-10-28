---
name: openblack-commit-check
description: Use before EVERY git commit in any openblack checkout or worktree. Keeps every commit authored with your own git identity, writes the message in strict ASD-STE100 Simplified Technical English (subject max 72 chars, body only if necessary), checks nothing local is staged, test hooks are gone, formatting and tests pass, and verifies the commit afterwards.
---

# openblack commit check

Run through this before each `git commit` in any openblack checkout, and verify after it. That means the main
checkout and every worktree (lane worktrees, merge-train cars `prm-wt*`, scratch worktrees). Merge commits count too.

## 1. Who is the author

- **Every commit is the developer's.** Commit with **no** `--author`, so git's configured identity is used. This
  includes work that uses or adapts code from other forks.
- Do not name another person as author or co-author in a commit unless that person wrote the change and agreed to it.
- Every message ends with the Claude trailer: `commit.trailer` in `.claude/openblack.local.json`
  (`python .claude/obconfig.py get commit.trailer`), for example
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Use the name of the model that wrote the change.

## 2. The commit message (ASD-STE100 Simplified Technical English)

Every commit message follows the ASD-STE100 Simplified Technical English (STE) specification strictly. This applies to every commit, including merge commits and merge-train cars.

- **Subject line:** at most **72 characters**, counted with `git log -1 --format=%s | awk '{print length}'`. One
  statement, in STE. No full stop at the end.
- **Body:** only if it is absolutely necessary. Most commits have no body. If you must write one, leave one blank line
  after the subject, write it in STE, and wrap every line at 72 characters.
- **The trailer** (`Co-Authored-By: ...`, section 1) is not a body. Every commit has it, after one blank line.

### Core rules (the project checklist)

Every STE text (a comment, a commit message) must obey all of these:

1. Keep procedural sentences to 20 words or fewer; descriptive sentences to 25 words or fewer.
2. One instruction per sentence.
3. Use the active voice.
4. Use the present tense for procedures.
5. Start an instruction with the command verb ("Remove the bolt.").
6. Use articles ("the file", not "file").
7. Do not use a noun as a verb.
8. Use only approved words, with one meaning per word (see `DICTIONARY.md` at the root of the repository: openblack and bw1-decomp have the same file).
9. Write one topic per paragraph; lead with the topic sentence.

The rules below give more detail.

STE rules to obey in the subject and the body:

- Use only STE approved words, with their approved meaning and part of speech. For technical names (for example
  `CreaturePlanner`, `PickingSystem`, `LandChange.h`) and terms of the domain (for example villager, creature,
  temple, miracle), you can use the name as it is.
- Use one meaning for one word. Use the same word for the same thing every time.
- Write short sentences: a procedural sentence (an instruction) has a maximum of 20 words; a descriptive sentence
  has a maximum of 25 words.
- Write one topic in one sentence.
- Use the active voice. Do not use the passive voice.
- Use the imperative for instructions. For the subject, use a short imperative or a short statement, for example
  `Add the temple entrance pick` or `Make the hand use the picked point`.
- Use only these verb tenses: the simple present, the simple past and the simple future. Do not use the -ing form
  of a verb as a noun or as an adjective (for example, write "the code that picks", not "the picking code", unless it
  is a technical name).
- Do not use phrasal verbs (for example "set up", "carry out", "go on"). Use an approved verb ("prepare", "do",
  "continue").
- Use articles ("the", "a") and demonstratives ("this", "these") where it is possible.
- Do not use contractions ("don't", "it's"). Do not use slang or idioms.
- Do not join more than three nouns in a noun cluster. Divide a long noun cluster.

Check the message before you commit: count the subject length, read each sentence against the rules above, and make
the body shorter or remove it if possible.

## 3. What must not be staged

Check `git diff --cached --name-only`. Never stage:

- `vcpkg` (the submodule pointer), unless the change is a vcpkg update
- local notes and files that are only on your machine (untracked changelogs, progress notes, profiles, crash dumps)
- `CLAUDE.md` changes that you did not mean to share
- `.claude/openblack.local.json` and `.claude/settings.local.json` (git ignores them)
- `screenshot_references/` and other untracked PNGs

Stage files by name; never `git add -A` or `git add .`.

## 4. No test hooks left

`git diff --cached | grep -n "OB_TEST"` must print nothing. Restore any hook script's backups before committing.

## 5. Code checks

- Whole-file `clang-format -i` on every touched C++ file (`tools.clangFormat` in `.claude/openblack.local.json`).
- New files carry the header `Copyright (c) 2018-2026 openblack developers`.
- Comments follow strict ASD-STE100 Simplified Technical English and describe behaviour: no addresses, Mac symbols, assembly or vanilla internal names (skill `openblack-code-comments`).
- Builds clean, and `ctest` passes apart from the known `test_sound_decoding` failure. New behaviour has tests in
  `test/`, registered in `test/CMakeLists.txt`.

## 6. After committing

`git log -1 --format="%an <%ae> | %s"`: check the author matches section 1, and the subject is at most 72 characters
and in STE (section 2). If the commit is the latest and not pushed:

- a wrong author: fix it with `git commit --amend --no-edit --reset-author`;
- a wrong message: fix it with `git commit --amend` and a corrected message.

Never amend or rewrite a pushed commit: make a new commit instead.
