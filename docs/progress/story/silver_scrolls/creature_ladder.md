# Creature Ladder

A cut fighting tournament: fifteen stone creature statues stand in a half circle around an arena, and each silver scroll
there brings one statue to life as a copy of the player's own creature in another species, a little stronger each
time, to be beaten in a fight. It was never compiled into the game, has no title or dialogue in the text table, and its
reward (a "black belt" tattoo for the creature) was never written.

**Land:** none identified (never started) · **Giver:** a silver scroll over the arena · **Script:** CreatureLadder · **Reward:** none (an empty reward step, meant to be a tattoo) · **Repeatable:** no (each rung once; a lost fight is retried)

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, the script compiler's project and quest menu lists, the land map scripts and the
script grammar. Every row is n/a: the game never runs this quest. To restore it openblack would need, besides the usual
scroll and camera commands, making a creature from another creature, creature fights and fight health, which are all
stubs in `src/CHLApi.cpp`.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file, and no land's control script, challenge list or the extra-menu list starts it | n/a | never started by the game: only listed in the developers' quest menu and project files, where it would run itself when compiled alone |
| It runs itself at the top of its own file, the way single-quest test files do | n/a | never started by the game |
| No title, reminder or dialogue: the only line it uses is the shared "Your attention is required here." | n/a | never started by the game |
| Its arena sits on open ground at sea level; the "citadel heart" it faces is not near any shipped land's citadel (the closest is Land 4's, about 150 away), so it can't be placed on a shipped land with confidence | n/a | never started by the game: checked against every `Land<N>.txt` |
| It uses real script commands of the shipped language (making a creature from a creature, stone creature icon features), so it would compile | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## The statues

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fifteen stone creature icons are placed on a half circle of radius 80 round the arena, 12 degrees apart, each facing the next; they can't be destroyed | n/a | never started by the game |
| From one end to the middle: sheep, cow, zebra, ape, wolf, leopard, polar bear, lion; from the other end back to the middle: ape, ape, horse, tortoise, ape, bear, tiger; sizes grow from 1 at the ends to 2.4 for the lion in the middle | n/a | never started by the game: the chimp, mandrill and gorilla statues use the ape's icon, there being no icon for them |
| The comments list the rungs in a different order (sheep, chimp, cow, ape...) from the one the statues and levels actually use | n/a | never started by the game |

## Each rung

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rung waits until the player's creature is within 100 of the arena, then a silver scroll appears over it and the good advisor nags ("Your attention is required here.") until it is clicked | n/a | never started by the game |
| If the creature is within 90 when clicked, it is turned and walked to its starting spot on the west side of the arena | n/a | never started by the game |
| Cut scene: the camera rises 50 above the player's side and looks at the rung's statue | n/a | never started by the game |
| The opponent is made at the statue as a copy of the player's creature (its mind) but in the rung's species, fading in over 6 seconds, auto-scaled to 0.6, fully grown and taught everything | n/a | never started by the game |
| The rungs go sheep, chimp, cow, ape, zebra, horse, mandrill, tortoise, wolf, gorilla, leopard, bear, polar bear, tiger, lion, alternating ends of the half circle towards the middle | n/a | never started by the game |
| Strength rises by 0.066 a rung, from 0.066 for the sheep to 1 for the lion; odd rungs are fully good and even rungs fully evil | n/a | never started by the game |
| The statue fades away and the opponent walks to its spot on the east side, the camera following; the two face each other if the player's creature is in place, otherwise the opponent faces the citadel | n/a | never started by the game |
| The camera settles on a fight view over the arena | n/a | never started by the game |

## The fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player's creature is within 50 of the arena, the two are made to fight each other; the leash comes off | n/a | never started by the game |
| When neither is fighting any more, the one with more fight health wins | n/a | never started by the game |
| Losing: the player's creature is knocked out, the camera pulls back, the opponent returns to its spot fully healed and the player's creature is moved to a spot outside the arena; the rung is fought again once the creature comes back within 50 | n/a | never started by the game |
| Winning: the opponent walks back to its statue's place, idles and prays, is frozen for 3 seconds, then an explosion miracle falls on it from above (radius 50) and it is removed; the camera returns to the fight view and the creature is released | n/a | never started by the game |
| There is no timer and no way to give up; the next rung starts as soon as one is won | n/a | never started by the game |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the lion, a reward step meant to give "a unique tattoo" (a black belt) for the creature runs, but it is empty | n/a | never started by the game |
| No challenge record is ever opened or closed | n/a | never started by the game |
