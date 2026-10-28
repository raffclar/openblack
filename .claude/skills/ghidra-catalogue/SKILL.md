---
name: ghidra-catalogue
description: Use before ANY Ghidra lookup of a Black & White function (decompile, disassemble, xrefs) by any openblack or bw1-decomp agent, and after you have understood one. A shared on-disk catalogue keeps each function's original disassembly, Ghidra's decompile, its callers and the analysis and comments agents add, so no function is decompiled and worked out twice. Covers the ghcat.py tool, the research loop, and how claude-mem (when the user pairs it) recalls past analysis across sessions and agents.
---

# Ghidra catalogue

Every agent that reads Ghidra pays for the same functions again and again: the same decompile, the same
disassembly, the same hour of working out what `fn_004C5390` does. The catalogue stops that. It is one
Markdown file per function, keyed by address, shared by every agent and session:

`<catalogue>/BW1W120/fn/<ADDR>.md` (8 hex digits, upper case, e.g. `004E4B90.md`)

`<catalogue>` is `paths.ghidraCatalogue` in `.claude/openblack.local.json` (or `GHCAT_ROOT`). The catalogue is each
developer's own data. It is not in the openblack repository.

Each file holds:

| Part | What | Who writes it |
|---|---|---|
| front matter | address, binary, full name, short name, signature, size, fetch date, status (`raw`/`analysed`), one-line summary, tags | the tool; summary and tags by agents |
| `## Analysis` | what the function does, in plain words: inputs, outputs, rules, constants with their meaning, quirks | agents, dated, with lane name |
| `## Comments` | short facts: who calls it and why, the vtable slot, the info.dat field, links to notes, open doubts | agents, dated |
| `## Disassembly` | the original instructions, exactly as Ghidra lists them | the tool |
| `## Decompiled` | Ghidra's C, as it was on the fetch date | the tool |
| `## Callers` | Ghidra's xrefs to the entry | the tool |
| `## Log` | when it was fetched, refreshed and annotated, and by whom | the tool |

The disassembly is the ground truth. The decompile is a reading aid and can be wrong (wrong types, merged
locals, missed `this`). Analysis must agree with the disassembly; say so where the decompile misleads.

## The tool

`python .claude/skills/ghidra-catalogue/ghcat.py <command>` (from the repository root). It needs Ghidra open with
the HTTP bridge (`tools.ghidraBridge`, default 127.0.0.1:8080) only to fetch; reading the catalogue needs nothing.

| Command | Use |
|---|---|
| `get <addr\|name>` | Print the entry. Fetches from Ghidra only if there is no entry yet. First line is `CACHED`, `FETCHED` or `REFRESHED`; second is the `GHCAT` line. Default sections: meta, analysis, comments, decompile, notes that mention the address. |
| `get <addr> --sections asm` | The original disassembly (add `,c,callers` as needed). `--max-lines N` (default 300). |
| `get <addr> --refresh` | Fetch Ghidra's parts again (after a rename or retype in Ghidra). Keeps summary, tags, analysis and comments. |
| `find <regex>` | Search names, summaries, tags, analysis and comments. `--status raw` lists entries nobody has explained yet. |
| `note <addr> --summary S --tags a,b --analysis-file F --comment S --by <lane>` | Add what you learnt. Analysis and comments append with the date and lane; they never overwrite. Adding analysis sets status `analysed`. |
| `xrefs <data addr>` | Which functions read or write a data address (bridge `xrefs_to`): instruction address, access (`READ`/`WRITE`/`DATA`), function. Sorted, prints at once. Needs Ghidra open. |
| `xrefs <data addr> --write --by <lane> [--name N] [--summary S] [--comment S]` | Also keep the list in a data entry `<catalogue>/BW1W120/data/<ADDR>.md` (same front matter, `kind: "data"`; sections Analysis, Comments, Callers, Log) and commit it. Run again to refresh the list; comments append. |
| `stats` | Count of entries, analysed and raw. |

Data addresses (globals, tables, struct fields in the data segment) are not functions, so `get` refuses them. Use
`xrefs` and read the readers with `get`. `xrefs` sees only references Ghidra has found. A struct field reached as
`[reg + offset]` has no address; to find its users scan the image for the offset (a displacement search with
capstone over `.text`) and state in the comment that you did.

Names work for `get` when they are unique (`get CanCreatureEatMe`); prefer addresses.

## The research loop (every agent, every time)

1. **Recall first.** Before you touch Ghidra for a feature:
   - `ghcat.py find "<feature words>"` (e.g. `find "eat|food"`), and
   - if claude-mem is paired (see below), the `claude-mem-cowork:mem-search` skill with the feature words
     plus `GHCAT`.
   - Also read the feature's notes under bw1-decomp's `docs/openblack/` (`paths.bw1DecompDocs`); `get` lists the
     notes that cite an address.
2. **Look up through the catalogue, never around it.** For each function you need, run `ghcat.py get <addr>`
   instead of calling `decompile_function` / `disassemble_function` / the bridge directly. A `CACHED` entry with
   analysis may answer your question with no Ghidra call at all.
3. **Read the disassembly when it matters.** Constants, float compares, `this` offsets, vtable slots and
   argument order come from `--sections asm`, not from the decompile.
4. **Write back before you move on.** As soon as you understand a function, record it:
   `ghcat.py note <addr> --summary "<one line>" --tags <area>,<feature> --analysis-file <tmp.md> --by <lane>`.
   Even a partial answer helps; say what is still unknown. A one-line `--comment` is fine for a function you
   only glanced at (for example "only sets flag 0x1000; no movement effect").
5. **Keep notes and catalogue linked.** Research notes in `docs/openblack/...` cite addresses as before; the
   catalogue holds the detail, the notes hold the feature story. Do not paste whole decompiles into notes.

### What a good analysis says

- What it is for, in one sentence, and where it is called from (the callers that matter).
- Inputs (this, arguments, globals read) and outputs (return value, fields and globals written).
- Every constant with its meaning and its address when it is a global (e.g. `0x8CF3FC = 0.98, up-axis limit`).
- Branches and rules in order, as a list.
- Virtual calls with their slot and the class that implements them.
- Quirks and bugs (link the vanilla-bugs row if there is one).
- What is still unknown, so the next agent knows where to start.

Plain English is fine here (the STE rule covers commits, code comments and look texts).

## claude-mem: recall across sessions and agents

The `claude-mem-cowork` plugin is installed but **not paired**: its config has no API key, so every hook is a
silent no-op and nothing is captured or injected yet. Pairing is the user's decision (the `mem-setup` skill,
values from cmem.ai → Connect). Do not pair it on your own.

When the user pairs it, the catalogue and claude-mem work in two layers:

| Layer | Role | Strength |
|---|---|---|
| Catalogue (this skill) | the exact record: disassembly, decompile, analysis per address | complete, local, never loses detail |
| claude-mem | the recall index: which functions were studied, for what, by whom, and where the entry is | finds past work from loose words; injects it into new agents |

How they meet:

- Every `get` and `note` prints one `GHCAT <binary> 0x<addr> <name> [status] <summary> -> <path>` line. The
  plugin's PostToolUse hook sends tool output to cmem.ai, where its observer turns it into an observation, so
  the GHCAT lines become searchable memory with no extra step.
- The plugin's PreToolUse hook on `Agent` adds observations relevant to a new agent's prompt. A lane briefed
  on "creature eating" then starts with the GHCAT line for `0x4E4B90` and its catalogue path.
- `mem-search` with `GHCAT <words>` finds entries by meaning across sessions; the catalogue gives the detail.
- Summaries must be specific (what rule, which feature), because the summary is what memory search matches.

Data leaves the machine once paired: the hook sends tool inputs and outputs (up to 16 KB a field) to cmem.ai,
which includes decompiles and file contents. That is part of what the user agrees to when pairing.

## Version control

The catalogue (`<catalogue>`) is a git repository, branch `main`. Every change is committed.

To start a new catalogue, make the folder, run `git init -b main` in it, and install the commit-msg hook. The file
`<catalogue>/.git/hooks/commit-msg` holds:

```sh
#!/bin/sh
exec python "<openblack checkout>/.claude/skills/ghidra-catalogue/check_commit_msg.py" "$1"
```

- `get` (first fetch or `--refresh`) and `note` commit the changed file at once, with an STE subject such as
  `Add analysis of GameThingWithPos::CanCreatureEatMe at 0x004E4B90`, a `Lane: <by>.` body and the Claude
  trailer. Always pass `--by <lane>` so the history says who did what.
- If you must change a file outside the tool, commit it yourself before you continue, one file or one topic a
  commit, with your own git identity (no `--author`) and the Claude trailer (`commit.trailer`).
- Commit messages follow ASD-STE100 Simplified Technical English: an instruction verb first (Add, Change,
  Remove, Correct), at most 72 characters in the subject, no full stop, an empty line before the body, at most
  25 words a sentence, approved words only. The repository's commit-msg hook
  (`check_commit_msg.py`) refuses messages that break these rules; fix the message, never bypass the hook
  (`--no-verify` is forbidden).
- Never leave changes uncommitted at a handover. `git -C <catalogue> status` must be clean.
- Never rewrite history (no amend of another agent's commit, no rebase, no force). There is no remote.
- The tool retries when another agent holds the git index lock.

## Rules

- One binary per tree: `BW1W120` is the default. Set `GHCAT_BINARY` for another binary, and point the bridge
  at the matching Ghidra program first.
- Never edit the Disassembly, Decompiled or Callers sections by hand; use `--refresh`.
- After you rename or retype a function in Ghidra, run `get <addr> --refresh` so the catalogue follows.
- Two agents annotating the same entry at the same moment can lose one write; `note` again if your line is
  missing.
- Settings: `paths.ghidraCatalogue`, `tools.ghidraBinary`, `tools.ghidraBridge`, `paths.bw1DecompDocs` plus
  `paths.handovers` (the notes `get` searches), and `commit.trailer`. Environment variables win over the settings:
  `GHCAT_ROOT`, `GHCAT_BINARY`, `GHCAT_BRIDGE`, `GHCAT_DOCS` (`;`-separated), `GHCAT_TRAILER` (a lane on another
  model sets its own trailer, for example `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`).
