# Swap To Cow

A herding challenge written for Land 4: a lame herder asks for the player's creature to drive ten frightened cattle into
a pen by the player's home village, and a trader there pays with a strength miracle seed, or with a cow creature to swap
for when the player's creature is not already a cow. It is compiled into the shipped game but nothing ever starts it.

**Land:** 4 (written for it; never started) · **Giver:** a Greek herder by a Japanese house, then a trader by the pen · **Script:** SwapToCow · **Reward:** a creature strength miracle seed (if already a cow) or a cow creature to swap for · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text and the decompile of the compiled challenge file, Land 4's map script, the game's text
table. Every row is n/a: the game itself never runs this quest, so there is nothing for openblack to match. If openblack
ever wants to restore it, it needs the same commands as the other swap quests (dialogue, flocks, animal states, timers,
challenge records, the swap) which are all stubs in `src/CHLApi.cpp`.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is compiled into the shipped challenge file (it is in Land 4's file list) | n/a | never started by the game: compiled, but no script in the compiled file runs it; Land 4's control script starts the ogre, the fish puzzle, the breeder and others, never this |
| Only a single-quest test launcher, which is not compiled, ever runs it | n/a | never started by the game |
| A note at its top says it was to be triggered after the ogre fight on Land 4 | n/a | never started by the game |
| The herder's house is a real Japanese house in Land 4's neutral Japanese village; the pen is about 190 from the player's Norse home village; the trader's hut position matches no building on Land 4 | n/a | never started by the game: checked against `Land4.txt` |
| The silver scroll over the herder's house and its advisor nag ("Whew! As if you ain't busy enough, here's something for you to do.") are commented out, so even if it were run it would start straight with the cut scene, with no scroll | n/a | never started by the game: the compiled version has no highlight at all |
| Its title "Swap To Cow" and reminder "Let's get these cows back to the Village." exist in the text table, as do all its dialogue lines | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene: a Greek farmer comes out of the house and walks a few steps; the camera glides to him over 3 seconds | n/a | never started by the game |
| Out of view, eight short Celtic fence pieces are set in a ring of radius 10 around the pen spot, each turned to face it | n/a | never started by the game |
| A still flock is made at the pen (inner 5, outer 10) and a moving flock at the cattle's start (inner 15, outer 20); ten cows are made at the start, scattered up to 10 either way, and join the moving flock | n/a | never started by the game |
| The herder turns to the camera, prays, and says: "Mighty one. I need to herd my cattle to your home Village." then "But I have injured my ankle and cannot walk that far." | n/a | never started by the game |
| The camera cuts to the cattle; the challenge record opens; the herder: "But as my beasts are afraid of your Creature I had an idea." | n/a | never started by the game |
| The camera cuts to the pen: "You could get him to shepherd the cattle to the pen near your Village." and "Please try and chase as many into the pen as possible." | n/a | never started by the game |
| The herder walks back home and is removed as the camera glides back | n/a | never started by the game |

## Herding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cows can't be picked up by the hand | n/a | never started by the game |
| Every second, a cow within 50 of the player's creature, not held, runs from it: it picks a spot 4 to 20 away on the far side (within 60 degrees of straight away), retrying until the ground there is within 2 of the height where it stands, and walks there at speed 0.3 for 3 seconds | n/a | never started by the game |
| A cow more than 60 from the creature goes back to moving with the flock | n/a | never started by the game |
| The moving flock's centre follows the cow that has got closest to the pen so far, so the herd drifts along behind the leader | n/a | never started by the game: the best distance is shared by all the cows and never reset |
| A cow that reaches within 20 of the pen, alive, not held and not flying, counts as arrived, joins the pen's flock and sparkles for 3 seconds | n/a | never started by the game |
| A cow that dies or disappears counts as lost | n/a | never started by the game |
| Every 3 seconds the record updates when the counts change: progress is a tenth per cow arrived or lost, alignment a tenth up per arrived and a tenth down per lost | n/a | never started by the game |
| There is no timer and no failure: the challenge only moves on when all ten cows are either penned or dead | n/a | never started by the game: a cow that never arrives and never dies keeps it open forever |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It then waits until the camera is within 150 of the pen and looking at it | n/a | never started by the game |
| A cut scene: a man walks to the pen and faces the camera; the record closes as a success with the final alignment | n/a | never started by the game |
| If the player's creature is already a cow, the trader thanks by result and gives a creature strength miracle seed beside himself with a sparkle: all ten ("Thank you, lordly one. You got the herd here safely."), more penned than lost ("You got most of the cattle here safely."), at least one penned ("Well, some cows made it, Mighty One."), each followed by "Here's an offering to show my gratitude." | n/a | never started by the game |
| If all ten died, a cow owner gets nothing: "None of the cows made it. I shall look elsewhere for my herding requirements in future." | n/a | never started by the game |
| Otherwise a cow creature appears near the trader's hut, looks at the camera and waves; the lines are "A billion thanks. All the cattle got to the pen safely." / "A hundred thanks. You managed to get most of the cattle here safely." / "Thanks, Holy One. A few cows did make it, in the end." each followed by "Take this Miraculous cow as an offering from me to you." | n/a | never started by the game |
| Even with every cow dead, a non-cow owner still gets the cow: "Lordly spirit, none of my herd survived. I sense a famine coming." "Here. Take this special beast." "We might have to eat it otherwise." | n/a | never started by the game |
| The camera returns, the trader is left in the world, and the cow is offered through the shared swap offer | n/a | never started by the game: see [creature_swaps.md](./creature_swaps.md) |

## Bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the creature is already a cow, the swap offer is still run with no creature | n/a | never started by the game |
| Only a cow owner can get the miracle seed; everyone else always gets the cow, whatever the result | n/a | never started by the game |
