# Choose Your Creature (BAFTA draft)

A cut, shortened draft of the first land's "Choose Your Creature" gold scroll: clicking the scroll at the creature gate
skips the hunt for the gate stones and plays straight into the stones sinking, the gates opening, the flight to the
valley and the choice of cow, ape or tiger. It sits in an uncompiled file and is started only by an uncompiled "BAFTA"
control script, apparently made for an awards demonstration. The shipped quest belongs to the first land (see
[../land_1.md](../land_1.md)); this file only covers the draft.

**Land:** 1 by its markers (never started) · **Giver:** the gold scroll at the creature gate · **Script:** ChooseYourCreature (in the draft file RussYourCreature) · **Reward:** the chosen creature · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the draft's source text, the uncompiled demo control script, the shipped quest's source and the game's text
table. Every row is n/a: the shipped game has only the full version.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The draft file is not among the files compiled into the shipped `challenge.chl`; the shipped game has the full "Choose Your Creature" from its own file instead (same script name, so both could never be compiled together) | n/a | never started by the game |
| The only thing that would run it is the uncompiled demo control script, which starts "old" versions of the Lost Brother, the Pied Piper and Choose Your Creature; none of those "old" names exist in any script, so even that script would fail | n/a | never started by the game; see [../../easter_eggs/unused_content.md](../../easter_eggs/unused_content.md) |
| All its lines are in the shipped text table (they are the shipped quest's lines); its markers are the first land's creature gate, plinth and valley | n/a | never started by the game |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll is put at the creature gate, 14 up; the evil advisor points it out when the camera comes near: "There's a Silver Reward Scroll down here, Boss." (the shipped line, wrong scroll colour and all) | n/a | never started by the game |
| It never opens a story-log entry: it only updates one (to 0.75, reminder "We need to choose a Creature!"), so the log entry would have to come from elsewhere | n/a | never started by the game |
| Two helper scripts for the gate-stone hunt (the advisor spotting the second gate stone, the trainer's reminder that the stone isn't carved) are in the file but never run; a "start russ" note marks where the hunt was cut out | n/a | never started by the game |

## The gates open

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three creatures are made in the valley; the trainer is made at her hut, walks to her door and to a talking spot, and turns to the camera | n/a | never started by the game |
| Trainer: "The Gate Stones are together. Pass through and claim your Creature!" (a comment holds an earlier joke line) | n/a | never started by the game |
| The camera moves to the plinth; the gate-stone sound plays, the camera shakes (amplitude 0.1 for 7 seconds within 20) and the plinth opens with the stones sinking | n/a | never started by the game |
| The camera looks at the gate chain; the gates open with the bolt sound; the trainer beckons; the gate-open sound plays as the camera pulls back | n/a | never started by the game |
| The camera flies to the valley along the prepared path with the creature-choosing music; the creatures turn to the camera | n/a | never started by the game |

## Choosing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors talk as the creatures act up: good "Look at them. Just look at them. These are Miraculous Creatures indeed."; evil "They certainly are. But not quite as big as I expected."; good "Not yet, maybe. But they can become the most powerful Creatures in the world."; evil "Now that I'd like to see. We must have one. Which should we choose?"; good "Any. They're all special."; evil "If rather small at the moment."; good "Shh." | n/a | never started by the game |
| The music stops and the camera is kept to the valley | n/a | never started by the game |
| Holding the hand over a creature makes it beg to be picked; the first time over each, the advisors pitch it: cow (good "We could have the cow. A strong and noble beast." evil "What? Not the fierce, lethal tiger? Click the Action Button on him!"), ape (good "Hmm. How about the ape. Intelligent and quick to learn?"), tiger (evil "I'm up for the tiger. Look at those claws.") | n/a | never started by the game |
| Each creature turns to the camera again every 5 seconds | n/a | never started by the game |
| Clicking a creature once asks "Are you sure you want this Creature? …" (good); clicking the same one again chooses it; clicking another resets the count | n/a | never started by the game |
| On choosing: a 10-second close-up, evil "We've chosen a Creature.", a 3-second fade to black, the creature looks at the camera, becomes the player's creature, is moved to its pen and given it as home; the other two are deleted | n/a | never started by the game |
| The creature development scripts are started and the scroll is marked complete with the reminder "We should speak to the sculptor about the Gate Stones." (a leftover from the full version's order) | n/a | never started by the game |
