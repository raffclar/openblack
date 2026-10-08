# The Sculptor (cut silver version)

An early, silver-scroll version of Land 1's sculptor: a sculptor offers to carve the missing creature-gate stone if the
player brings him the right rock from the old hermit's quarry, then hammers at it for twenty minutes. The shipped game
replaced it with the gold-scroll story quest of the same title (its own script, in Land 1's story; see
[../land_1.md](../land_1.md)); this version was never compiled.

**Land:** 1 (an early version of it; never started) · **Giver:** a Norse farmer, the sculptor, by his house · **Script:** CreatureRetrieve (file), TheSculptor (its main script) · **Reward:** a lightning bolt miracle seed from the sky, then the carved gate stone · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, the shipped sculptor script that superseded it, the uncompiled early Land 1 control
script, the land map scripts and the game's text table. Every row is n/a: the game never runs this version. Its
behaviour as shipped belongs to the gold scroll ([../gold_scrolls/the_sculptor.md](../gold_scrolls/the_sculptor.md)) and is
out of this file's scope.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| This file is not in the compiled challenge file | n/a | never started by the game |
| The shipped Land 1 sculptor script defines scripts with the same names (the main script and the "rock returned" step, which takes one more argument there), and the shipped story calls those, never this file's | n/a | never started by the game: the compiled file only has the shipped versions |
| The uncompiled early Land 1 control script asks to start a script by this file's name, but no script of that name exists (it is only a variable here), so even that early version could not start it | n/a | never started by the game |
| It used a silver scroll; the shipped quest uses a gold scroll and records itself as a story quest rather than a challenge | n/a | never started by the game |
| The title "The Sculptor", the reminder and most of the sculptor's lines are in the text table (shared with the shipped quest); five advisor lines and one reminder of this version are missing | n/a | never started by the game |
| It calls the scroll's nag helper with one argument too many for the shipped helper | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sculptor (a Norse farmer) is made by his house; a silver scroll appears over the house and the good advisor nags "There's someone who might need your attention, Leader." | n/a | never started by the game |
| On click, a cut scene: he walks out, the camera glides in (4 seconds) and the challenge record opens, reminder "We need to get a carveable stone for the sculptor from the Hermit's Quarry over here." | n/a | never started by the game |
| The sculptor: "Holy one, I hear you're looking for a Gate Stone." then "I am a sculptor. If you provide me with the right rock I'll carve one for you." | n/a | never started by the game |
| The camera flies a path to the quarry: "You'll find the rock I need in the Old Hermit's Quarry. Please bring it here." | n/a | never started by the game |
| Both advisors then speak five lines while the camera flies a second, 15 second path back | n/a | never started by the game: lines missing from the text table |
| He then wanders around his house (radius 6) | n/a | never started by the game |

## The rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 seconds, any ordinary rock within 30 of him is checked: he walks to it, says "Hmm. Actually this rock isn't right for carving. Try another." and it is removed | n/a | never started by the game |
| The right rock (the quarry's gate-stone rock) put down within 30 of him, not held or flying: "Oh that's excellent. Exactly the right kind of rock to work with." the record goes to complete with full good alignment, "Thank you. Come back later and I'll have it finished." | n/a | never started by the game |
| A lightning bolt miracle seed then falls from the sky about 14 from him, with the reward camera swoop | n/a | never started by the game |
| He walks to the rock, faces it and hammers at it; the record is set back to half done and a 20 minute carving timer starts | n/a | never started by the game: the record goes from complete back to half, a quirk of this version |

## Carving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking the rock up while he works: "Hey! I haven't finished working on carving the Gate Stone yet!" and the timer pauses with the time left | n/a | never started by the game |
| Putting it back within 30: "Thank you. Now, if you don't mind. I've got carving to do." he walks to it and hammers again, and the timer resumes | n/a | never started by the game |
| When the timer reaches its last second: the rock is replaced by the carved gate stone, "I've finished! It's ready to be placed with the others by the Gate." the record completes, he walks home and is removed | n/a | never started by the game: the check is for exactly one second left, which could be missed |

## Hurting the sculptor

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thrown with almost no life left: the camera follows him in flight at half game speed while he screams "Aaaaaarrrrrrrggggggghhhh!" and carving stops | n/a | never started by the game |
| Killed: the good advisor points at him, "What a senseless waste of human life!", the evil advisor "Come on. Let's do another!", and the record closes with full evil alignment; the quest ends | n/a | never started by the game |
