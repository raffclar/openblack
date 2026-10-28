---
name: openblack-vanilla-bugs
description: Find, record and decide bugs and quirks in vanilla Black & White for openblack. Use whenever research, a vanilla run, a review or an agent report shows vanilla doing something that looks unintended (an oversight, inconsistency, crash, invalid memory read, dead code that should run, or a visible glitch). Keeps docs/vanilla-bugs.md in the openblack repo current, and routes any fix through the user's sign-off and the deviations list.
---

# openblack vanilla bugs

openblack replicates vanilla, bugs included, unless the user decides otherwise. Every bug or quirk found in vanilla is
a row in **`docs/vanilla-bugs.md` in the openblack repo** (version controlled, next to `docs/deviations.md`).

## 1. Is it a vanilla bug?

Record it when vanilla, in its own code or data:

- does something inconsistent that looks unintended: one class or path skips what all the others do (the crèche's
  missing refit on load), a value computed and never used, a lookup that can never succeed (GET_HAND_STATE 21);
- reads invalid memory, a null pointer, past a table or string, or can crash;
- shows a visible glitch players would have seen (floating parts, a stuck state, a never-ending wait);
- depends on something it shouldn't (the CPU maker, the frame rate, the machine's speed, uninitialised memory).

Not a vanilla bug: behaviour that is just unusual but deliberate (a design choice), openblack's own bugs (fix those),
missing openblack features (gaps), or differences only in code structure.

Evidence is required: the Ghidra address or decomp function, the data, or a vanilla capture. If it is only a
suspicion, say so in the row and keep looking; don't guess.

## 2. Record it

Add a row to `docs/vanilla-bugs.md` **in the same branch as the related code or research**:

`| # | Area | The bug in vanilla | Evidence | Effect in play | openblack | Decided | Where |`

- *openblack* is one of:
  - **Kept**: openblack does what vanilla does;
  - **Fixed**: openblack differs on purpose, which needs a row in `docs/deviations.md`;
  - **Guarded**: vanilla would crash or read invalid memory, and openblack logs vanilla's message and carries on;
  - **Open**: not decided yet.
- *Decided*: who decided and when, quoting the user, or `—` for Guarded rows and for Kept rows that need no decision.
- *Where*: the openblack code and the bw1-decomp research note.
- Rows are never renumbered or deleted; edit a row in place when the decision changes, saying when.

## 3. Decide it

- **The default is to keep vanilla's behaviour** (the faithfulness rule). Nothing else is needed for **Kept**.
- **Guarded** needs no sign-off when nothing visible changes in play (no crash, same visible result). If guarding
  changes what the player sees, it is a deviation: treat it as Fixed.
- **Fixing** a bug the player could see is a deviation. It needs the user's sign-off: the agent messages the lead;
  the lead asks the user with the evidence, offering "keep vanilla's bug" and "fix it". Then use the
  `openblack-deviations` skill: add the deviation row, link the two rows, and only then build the fix.
- A bug that breaks play (a crash, a stuck story, an impossible challenge) goes to the user straight away. All other
  bugs are recorded and asked about in the next batch of questions.

## 4. Before a train car merges

- Every row the branch adds has evidence and an *openblack* value.
- No **Fixed** row merges without its signed-off deviation row.
- The PR body names the vanilla-bug rows the car adds or changes.
