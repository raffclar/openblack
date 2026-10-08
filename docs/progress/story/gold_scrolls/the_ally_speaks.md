# The Ally Speaks

A cut story cut scene, once meant to be offered from a gold scroll at the player's temple, in which the camera flies a
prepared path while the advisors talk about another god's temple across the land. It was never compiled into the game,
its gold scroll was already commented out, and none of its fifteen lines were ever written. It has no in-game title;
this file uses the script's own name.

**Land:** 2 by its markers (never started) · **Giver:** none (the gold scroll at the player's temple is commented out) · **Script:** AllySpeaks · **Reward:** none · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text and its single-quest test launcher, the compiler's challenge and camera lists, the
game's camera data file and text table, and the land map scripts. Every row is n/a: nothing in the shipped game can
start it.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is not among the files compiled into the shipped `challenge.chl` | n/a | never started by the game: only its own (uncompiled) test launcher runs it |
| Its challenge name keeps an entry in the compiler's list of challenges | n/a | never started by the game |
| Its camera position and camera path are in the compiler's camera list, and the shipped camera data file still holds an entry under that name | n/a | never started by the game; the path data is left over in `Data/camera.edt` |
| None of its fifteen lines, nor the advisor's scroll prompt it names, are in the shipped text table or the development text source | n/a | never started by the game; who would speak each line is unknown |
| Its "my temple" marker is 19 from the player's planned temple on the second land; the other temple it looks at is not near any shipped temple (the nearest, Lethys's on the third land, is about 290 away) | n/a | never started by the game; the second land's other two temples are about 475 and 1900 away; which god the "ally" is cannot be told from the script |

## How it would appear

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll at the player's temple, with the evil advisor pointing it out when the camera comes near | n/a | never started by the game: both lines are commented out in the source, so as written it would play at once with no scroll and no challenge log entry |

## The cut scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera's position is saved, then the camera jumps to the start of a prepared camera path and flies along it; two seconds after it arrives the talk starts | n/a | never started by the game |
| One line is spoken; the evil advisor steps out and a second line starts; a second later the good advisor steps out; a third line follows and a two-second pause | n/a | never started by the game |
| Both advisors turn to look at the other god's temple, a second apart; two lines; the evil advisor looks away for one line and back; three more lines | n/a | never started by the game |
| The good advisor looks away for one line (with a two-second pause) and back; one line; both look away for three lines (another two-second pause); both look at the other temple for the last line | n/a | never started by the game |
| When the camera path ends the camera returns to where the player left it and the cut scene ends | n/a | never started by the game; nothing is logged or rewarded |
