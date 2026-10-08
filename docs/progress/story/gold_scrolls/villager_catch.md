# Villager Catch

A cut gold-scroll mini-game in which a hostile god flings ten villagers out of a town, one at a time, and the player
must catch each in the hand before it lands. It was never compiled into the game and its lines were never written into
the text table; it has no in-game title, so this file uses the script's own name.

**Land:** none (never started; its markers sit beside the player's home town on the second land) · **Giver:** a gold scroll over the town · **Script:** VillagerCatch · **Reward:** none (only advisors' comments on the score) · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text (whose comments give the intended English of most lines) and its single-quest test
launcher, the compiler's challenge list, the game's text table and the land map scripts. Every row is n/a: nothing in
the shipped game can start it.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is not among the files compiled into the shipped `challenge.chl` | n/a | never started by the game: only its own (uncompiled) test launcher runs it |
| Its challenge name keeps an entry in the compiler's list of challenges | n/a | never started by the game |
| None of its 35 lines are in the shipped text table; the source's comments give the intended words of the main ones, and four "Nemesis comment required" notes show his lines were never written | n/a | never started by the game |
| It sets a "villager catch completed" flag "that the control script uses" (a comment credits another designer), but no land's control script, shipped or cut, reads it | n/a | never started by the game |
| Its town marker is 67 from the player's home town on the second land (the Norse town); the source calls it Nemesis's town; no other land has a town within 230 | n/a | never started by the game; which land it was meant for is not certain |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll is put over a point just north of the town as soon as the script starts | n/a | never started by the game |
| The evil advisor's scroll prompt ("Whew! As if you ain't busy enough, here's something for you to do.") is commented out, so nothing points the scroll out | n/a | never started by the game |
| It starts when, checked once a second, the camera is within 100 of the town, the town is on screen and the hand is empty; the scroll does not need clicking | n/a | never started by the game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene: a line meant as "You want the souls in this town? Catch!" (by its wording the hostile god); the good advisor steps out: "Quick! Save them!" and goes home | n/a | never started by the game; words from the source's comments |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ten male villagers are thrown one at a time from a point in the town towards the scroll point, aimed 200 up, with a random spread of up to 10 either side and a speed between 100 and 150 | n/a | never started by the game |
| After the first, each throw waits a random 1 to 8 seconds | n/a | never started by the game |
| For each throw a short cut scene puts the camera at a fixed spot and follows the flying villager for 3 seconds, while he shouts one of five random cries ("Help!") | n/a | never started by the game |
| The script waits while he is flying; if the player catches him, it waits until he is put down, then a second, then follows him with the camera if he was thrown again until he lands | n/a | never started by the game |
| A caught villager who is still alive says one of five random thank-you lines and counts as saved; one who died says one of five random curses ("Argh, damn you!") | n/a | never started by the game |
| A villager who is not caught is not counted at all, and his fall is not watched | n/a | never started by the game |

## The result

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| None saved: the good advisor: "Tragic. You haven't caught a single person." then a line for Nemesis | n/a | never started by the game |
| One to four saved: evil: "You'll have to be faster than that."; good: "Oh this is terrible."; then a line for Nemesis | n/a | never started by the game |
| Five to nine saved: good: "Impressive stuff."; evil: "All those saved people. Lovely."; then a line for Nemesis | n/a | never started by the game |
| All ten: the evil advisor: "OK. So you're a hotshot at catching. Whatever." then a line for Nemesis | n/a | never started by the game |
| Nothing is logged, rewarded or marked complete beyond the completion flag; the gold scroll is never removed | n/a | never started by the game; no challenge log entry is ever made |
