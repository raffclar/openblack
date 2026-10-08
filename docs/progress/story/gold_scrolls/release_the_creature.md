# Release the Creature

A cut gold-scroll story chapter in which the player's creature is found tied by its leash to a boulder in a prison on a
hill, watched by a hostile creature, and the player frees it by hitting the boulder with thrown rocks. It was never
compiled into the game and its title and dialogue were never written into the text table; the shipped third land frees
the creature from Lethys's prison statues instead
([so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md)).

**Land:** none (never started; an early form of the third land's creature rescue) · **Giver:** none (a cut scene when the camera finds the creature) · **Script:** ReleaseTheCreature · **Reward:** none (the creature itself) · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text and its single-quest test launcher, the compiler's challenge list and the game's
text table. Every row is n/a: nothing in the shipped game can start it.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is not among the files compiled into the shipped `challenge.chl` | n/a | never started by the game: only its own (uncompiled) test launcher runs it |
| Its challenge name keeps an entry in the compiler's list of challenges | n/a | never started by the game |
| Its title (title number 22), its reminder and all seven of its "save your creature" lines are missing from the shipped text table and from the development build's text source, so it could not even be compiled against the shipped text | n/a | never started by the game |
| The shipped third land's arrival script still has a commented-out log line with this same title and reminder, showing the chapter was folded into the third land's arrival | n/a | see [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| Its markers do not match a village on any shipped land closely (the nearest town to the prison is 110 away on the first land and 200 to 480 away on the others) | n/a | never started by the game |
| It waits on a "Wonder built" flag that is declared for every script but that no script, shipped or cut, ever sets, so even if started it would stop for good before its cut scene | n/a | never started by the game |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature is moved into the prison on the hill; a boulder is made beside it at one and a half times size, which can't be picked up or moved, and the creature's leash is tied to the boulder | n/a | never started by the game |
| The creature is made fully mature (marked in the source as something to remove) and made to look sad | n/a | never started by the game |
| An opposing creature is made beside the prison as a copy of the player's creature, but a cow, fully mature, wanting only to be angry (a note says it should become the opponent's real creature) | n/a | never started by the game |
| Five half-size rocks are put around the opponent, which looks at the player's creature and then at each rock in turn without approaching | n/a | never started by the game |
| Five rocks are put at a spot near the player's influence as ammunition; each one that is thrown away more than 20 from its spot is put back there and counted as used, and a lost one is made again, until the creature is freed | n/a | never started by the game |
| Once the Wonder flag is set, an area of influence of radius 80 is made at the ammunition spot | n/a | never started by the game; the flag is never set |

## The cut scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It starts when the camera comes within 150 of the creature and the creature is on screen; both advisors step out | n/a | never started by the game |
| The story log gets a gold scroll (title 22, reminder 21) at nothing done | n/a | never started by the game; neither text exists |
| Seven lines are spoken in turn | n/a | never started by the game; their words were never written, so who speaks them is unknown |

## Freeing the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When anything hits the boulder, it becomes movable and can be picked up, the leash is untied and the creature is free | n/a | never started by the game |
| Until then the creature cycles between looking sad, looking at the opponent and standing idle | n/a | never started by the game; a mistake in the counting (the idle step waits for a count that is never reached) means it is sad once, looks at the opponent once and then stops changing |
| Nothing marks the scroll complete or reports the rescue: the script just ends | n/a | never started by the game |
