# The Attackers

A cut raid challenge: a band of ten marauders camps by a bonfire outside a Norse village and raids it three times,
first driving off a herd of cattle, then robbing the village store, then setting fire to the crèche, eating their
plunder at camp between raids. The player can kill them or keep them fed; a villager who begs for help promises a
fireball miracle seed if the marauders are wiped out. It was never compiled into the game.

**Land:** 2 most likely (never started) · **Giver:** a Norse farmer from a house by the store (only if no marauder has been killed by the time of the third raid) · **Script:** Marauder · **Reward:** a fireball miracle seed, only if the farmer begged and every marauder is then dead · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text and its single-quest test launcher, the script compiler's project and quest menu
lists, the land map scripts, the shared notify, reminder and reward scripts, and the game's text table. Every row is
n/a: the game never runs this quest. To restore it openblack would need, besides the usual dialogue, camera and
challenge record commands, villager and animal flocks, food stores and resources, dances, building fire and
anti-influence areas, which are all stubs in `src/CHLApi.cpp` (its `Create` only makes mobile statics and rocks, so not
even the marauders or the cattle would appear).

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file: it is missing from the compiler's project list, and no land's control script or challenge list starts it | n/a | never started by the game |
| Only its single-quest test launcher starts it; the developers' quest menu compiles that launcher together with Land 2's setup script | n/a | never started by the game: the launcher is not compiled into the shipped game either |
| The launcher itself builds a storage pit, a crèche and a house at the quest's spots, marks them fully built and fills the storage pit with 10,000 food and 10,000 wood, so no shipped land has these buildings ready | n/a | never started by the game |
| Its spots sit 60 to 100 from Land 2's town 0, a Norse town site that starts with no buildings, and the marauder camp is near that town's fish farms; no shipped land has a building within 15 of any of its spots | n/a | never started by the game: checked against every `Land<N>.txt`; Land 2 is the best fit (Norse, and the test menu pairs it with Land 2's setup) but not certain |
| Its title "The Attackers" and its reminders exist in the text table, and so do all its spoken lines except the three in the cut opening scene | n/a | never started by the game |
| There is no silver scroll: the line that makes the scroll over the farmer's house is commented out, so where the script waits for "the scroll" it really waits for the house itself to be clicked | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## The marauders arrive

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An area of anti-influence, radius 30, is placed on the marauders' camp, so the player's hand has no power there | n/a | never started by the game |
| A bonfire is made at the camp | n/a | never started by the game |
| The script waits until the camp is out of view, then makes ten marauders there, each one fleeing from objects thrown at it, gathered in a flock (inner and outer radius 10) | n/a | never started by the game |
| A food store is made at the camp, about 20 from the bonfire | n/a | never started by the game |
| A herd of five cattle is made in a flock (inner 5, outer 10) about 180 from the camp and 170 west of the village buildings | n/a | never started by the game |
| The marauders set off for the cattle at speed 0.5 | n/a | never started by the game |
| The challenge record opens as "The Attackers", success and alignment 0, reminder "Cattle are missing and those Marauders are responsible." | n/a | never started by the game |
| A cut opening scene is commented out: the Norse farmer would have come out of his house, pointed towards the camp and said two lines (neither is in the text table), with the camera following the marauders | n/a | never started by the game |

## Feeding the marauders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All through the quest, every 2 seconds while any marauder lives: any food store within 50 of the camp that the script didn't make is emptied into the camp's store | n/a | never started by the game |
| Any cow within 40 of the camp that the script didn't make is killed and adds 500 food to the camp's store | n/a | never started by the game |
| Either way, the good advisor says "This food should keep the Marauders quiet for a while.", at most once every 30 seconds | n/a | never started by the game |
| If the camp's store has gone, a new one is made at the camp | n/a | never started by the game |

## Raid one: the cattle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the marauders are within 30 of the herd, the herd moves onto them | n/a | never started by the game |
| Once the camera is within 100 of the marauders and looking at them, the good advisor points at them: "These folk are stealing our cattle! Something must be done!" | n/a | never started by the game |
| The marauders head home at speed 0.5, the herd following at 0.4; the record's reminder is set again to the cattle line | n/a | never started by the game |
| Every 4 seconds while the herd keeps up (within 30) the marauders are sent on again; if the herd falls behind they stop and wait until it is within 10 | n/a | never started by the game |
| Every 5 seconds the script looks for a cow more than 75 from the herd; one found before the marauders get home is taken out of the herd and walks back to the herd's starting spot, joining a rescued herd there (inner 5, outer 30) | n/a | never started by the game: this is how the player saves cattle, by carrying or scaring a cow away from the herd |
| When the marauders are within 30 of the camp, the herd is led in; one cow at a time walks to the camp's store at 0.4 and, once within 20, or dead, or picked up, or thrown, is killed and fades away, adding 100 food to the camp's store | n/a | never started by the game: a comment says 300 food per cow; a cow the player is holding is killed in the hand |
| The raid is called off at once if two marauders are killed (fewer than nine left) or the herd is empty: the marauders go home and the herd goes back to its starting spot | n/a | never started by the game |

## Between raids

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After a raid, if the camp's store holds more than 100 food, the marauders stay at camp: one is put at the camp and the band dances round it (a 30-second dance) | n/a | never started by the game |
| Meanwhile one marauder at a time walks to a spot by the store, faces it, eats 100 food and rejoins the band, then waits 30 seconds | n/a | never started by the game: so each 100 food buys a little over 30 seconds of peace |
| When the store is down to 100 or less, the dance stops and the next raid begins | n/a | never started by the game |

## Raid two: the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The marauders walk at 0.5 to a spot beside the village storage pit and wait until within 15 of it | n/a | never started by the game |
| Once the camera is within 100 and the pit is in view, the evil advisor points: "Argh! These godless thieves are attacking the Village Store. I say kill every last one of them!" and the good advisor answers "No don't kill them. They're just hungry. That's all." | n/a | never started by the game |
| 500 food is taken from the storage pit; after 2 seconds the raid is over | n/a | never started by the game: the header comment says this raid destroys the storage pit, but the script only takes food |
| The raid ends early if two marauders are killed during it | n/a | never started by the game |
| The record's reminder becomes "Food's been stolen from the Village Store. The Marauders are to blame."; the marauders go home | n/a | never started by the game |
| If two or more died in this raid, the evil advisor: "The marauding band have retreated. We've given them a good pasting!" | n/a | never started by the game |
| 500 food is added to the camp's store, then the band waits 40 seconds (a comment says this was lowered for testing) | n/a | never started by the game: the 500 is added even when the raid was cut short before anything was taken |
| If the camp has more than 100 food, the band dances and eats again as above | n/a | never started by the game |

## The farmer begs for help

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if all ten marauders are still alive at this point | n/a | never started by the game |
| The good advisor points at the farmer's house: "The man who asked for your help needs to speak with you, Leader." | n/a | never started by the game: nobody has asked yet, since the opening scene where he would have was cut |
| It then waits until the house is clicked; the evil advisor would nag "A Silver Reward Scroll. Let's see what it's all about." every 30 seconds while the camera is within 100, but the nag hangs on a scroll that is never made | n/a | never started by the game: whether the nag ever fires with no scroll can't be told from the source |
| A Norse farmer comes out of the house and walks to a spot in front of it; the camera glides to 10 above and 10 off him over 4 seconds | n/a | never started by the game |
| The record is set again (success 0) and the player's alignment drops by 0.4 | n/a | never started by the game: the player is marked down for having let the raids happen |
| The farmer: "Please help us, massive one. We'll reward you!" | n/a | never started by the game |
| After 2 seconds the camera returns (3 seconds) and the farmer is released | n/a | never started by the game |

## Raid three: the crèche

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whether or not the farmer begged, the marauders walk at 0.5 to the crèche and wait until within 15 (or all dead) | n/a | never started by the game |
| The good advisor points: "Now they're attacking the crèche! If I wasn't so good I'd go down there and pull out their innards!" | n/a | never started by the game: said even if every marauder is dead by then |
| After 3 seconds, five sparkles appear around and above the crèche for 10 seconds; after 2 more the crèche is set on fire | n/a | never started by the game |
| The raid ends early if two marauders are killed during it, before the fire | n/a | never started by the game |
| The record's reminder becomes "People have been taken from the Village. Those outrageous Marauders did it!"; the marauders go home | n/a | never started by the game |
| If two or more died in this raid: good advisor "They took losses in that last raid. If they've got sense they'll leave us alone now.", evil advisor "It was glorious to see." | n/a | never started by the game |

## After the raids

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From now on the band loops for ever: eat at camp while it has more than 100 food, then raid the storage pit for another 500 food and carry it home | n/a | never started by the game |
| These later raids have no advisor lines and don't change the record | n/a | never started by the game |
| The loop only ends when every marauder is dead; there is no timer, no failure and no other way out | n/a | never started by the game: a player who keeps them fed never finishes the challenge |

## All dead and the reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the last marauder dies: "Sweet. The marauders are all dead." | n/a | never started by the game: the good advisor is brought out for a line voiced by the evil advisor |
| If the farmer did not beg (a marauder was killed before the third raid), the record closes as a success with no reward | n/a | never started by the game |
| If he did, it waits for his house to be clicked again (the same scroll-less nag as before) | n/a | never started by the game |
| The farmer walks out again; the camera glides to 10 above and 10 off his spot over 3 seconds; he faces the camera and prays once | n/a | never started by the game |
| The player's alignment rises by 0.2 and the record closes as a success with alignment -0.2 | n/a | never started by the game |
| The farmer: "Oh thank you, invisible one. We will worship harder for you." then "We humbly offer you this. You know, just to say thanks." | n/a | never started by the game |
| A fireball miracle seed falls from the sky beside him, with no reward camera; the reward help shows when it is clicked | n/a | never started by the game: the shared reward script, run without its camera |
| After 2 seconds the camera returns over 4 seconds; the farmer is left in the script's control (a comment wonders whether to make him homeless) | n/a | never started by the game |
| Last of all the record is closed again with alignment 0, overwriting the alignment just set | n/a | never started by the game |

## Bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every closing record uses the reminder "Do you want to swap your Creature with one from this person?", which belongs to the swap quests | n/a | never started by the game |
| The reward only comes to a player who killed none of the band before the third raid and then all of it afterwards; killing them early, the advisors' first suggestion, gives nothing | n/a | never started by the game |
| The header comment (three raids, retreat after two deaths, storage pit and crèche destroyed) is copied word for word into "Food for Thought"'s source | n/a | never started by the game: see [food_for_thought.md](./food_for_thought.md) |
| Marauders also appear, unrelated, as a single villager in one of Land 5's cut scenes | n/a | never started by the game: not this quest |
