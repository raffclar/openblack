# Dance scripts

Villagers dance at worship sites, around totems and artefacts, when buildings are finished, at the town's celebrations and with the creature. Each dance is a choreography file in `Scripts/Dance/` (dancers' steps and actions key-framed to the beat), chosen through the dance table in `info.dat`. openblack reads the table (`InfoConstants`, its dance entries) but has no dances: the villager and creature dance states are listed as to-do in `LivingActionSystem.cpp`. What dancing does for worship is the worship domain's ([../worship/](../worship/)).

**Progress: 0/32 done, 1 partial — 2%**

## The machinery

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dance table in `info.dat` names, for each of 25 dances, its file, length, beats, whether it starts on its own, the room it needs and its fewest and most dancers | partial | read into `InfoConstants` (dance entries); nothing uses it |
| A dance file is loaded once and kept: its groups of dancers, their key frames on the beat, the shape they dance in and the actions they play | todo | no reader |
| Dancers first walk to their starting places, wait for each other, then dance in step to the beat | todo | the move-to-dance and dancing states are to-do entries in `LivingActionSystem.cpp` |
| Dancers rotate between places and swap partners as the file says | todo | nothing in openblack |
| Coloured dance lights (red, green, yellow, blue, white) light the creature's dance | todo | nothing in openblack |
| A dance camera can follow the creature's dance | todo | nothing in openblack |
| A dance stops when there are no villagers left to dance with | todo | nothing in openblack |
| The challenge scripts can start a dance of a kind around a place for a time | todo | `DanceCreate` in `CHLApi.cpp` logs "not implemented" (called twice by the shipped scripts) |
| The map script can load a tribe's dance | todo | `LoadTribeDance` in `MapScriptCommands.cpp` throws; never used by the shipped map script |
| The developers' dance editing (a dance-for-editing state and an editor state) | n/a | a developer tool; the test land for it (`dance.txt`) loads a landscape that isn't shipped |

## The 25 dances of the dance table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers dancing at a worship site while they pray: file `NewWorship2`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |
| Villagers dancing with the creature, with lights and the camera: file `CreatureDanceWithLightsAndCamera`, 120 beats, no set length, 8 to 20 dancers | todo | no dances in openblack |
| Children dancing together in town: file `ChildrenDancing`, 120 beats, length 12000 (units unconfirmed), any number of dancers, starts on its own | todo | no dances in openblack |
| A housewives' dance in town: file `Test`, 120 beats, length 240 (units unconfirmed), any number of dancers, starts on its own | todo | no dances in openblack |
| Celebrating a small building finished: file `SmallBuilding`, 120 beats, length 300 (units unconfirmed), 0 to 12 dancers, starts on its own | todo | no dances in openblack |
| Celebrating a large building finished: file `LargeBuilding`, 120 beats, length 500 (units unconfirmed), 0 to 20 dancers, starts on its own | todo | no dances in openblack |
| Villagers dancing around a person: file `DanceAroundPerson`, 120 beats, no set length, 0 to 8 dancers, starts on its own | todo | no dances in openblack |
| Villagers praying facing inwards: file `DancePrayInwards`, 120 beats, no set length, any number of dancers, starts on its own | n/a | the file isn't shipped (only `DancePrayingInwards.dat` is), so this dance can't be loaded in the game either (unconfirmed what the game does) |
| The town's celebration around its village centre: file `DanceAroundTownCentre`, 120 beats, length 50000 (units unconfirmed), 0 to 20 dancers, starts on its own | todo | no dances in openblack |
| The creature telling villagers a story: file `CreatureTellingStory`, 100 beats, no set length, 4 to 20 dancers | todo | no dances in openblack |
| The creature playing a game with villagers: file `CreaturePlayingWithVillagers`, 90 beats, no set length, 8 to 20 dancers | todo | no dances in openblack |
| The creature dancing while villagers watch: file `CreatureDanceWithVillagersWatching`, 120 beats, no set length, 8 to 20 dancers | todo | no dances in openblack |
| The creature dancing to impress villagers: file `CreatureImpressingVillagers`, 90 beats, no set length, 8 to 20 dancers | todo | no dances in openblack |
| The creature dancing amorously with villagers: file `CreatureDanceAmorous`, 120 beats, no set length, 8 to 8 dancers | todo | no dances in openblack |
| Villagers dancing around a person: file `NewDanceAroundPerson`, 120 beats, no set length, 0 to 8 dancers, starts on its own | todo | no dances in openblack |
| Villagers dancing around an object narrower than 7 (a totem or artefact): file `DanceAroundObjectDiameterLess7`, 90 beats, length 900 (units unconfirmed), 14 to 20 dancers | todo | no dances in openblack |
| Villagers dancing around an object narrower than 10: file `DanceAroundObjectDiameterLess10`, 120 beats, length 1200 (units unconfirmed), 14 to 20 dancers | todo | no dances in openblack |
| Villagers dancing around an object narrower than 20: file `DanceAroundObjectDiameterLess20`, 90 beats, length 900 (units unconfirmed), 14 to 20 dancers | n/a | never chosen: objects 10 or wider get the "20 or wider" dance (see ../town/artefacts.md) |
| Villagers dancing around an object 20 or wider: file `DanceAroundObjectDiameterGreater20`, 120 beats, length 1200 (units unconfirmed), 14 to 20 dancers | todo | no dances in openblack |
| Worshippers' dance at the temple (one of six, using NewWorship2): file `NewWorship2`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |
| Worshippers' dance at the temple (one of six, using NewWorship1): file `NewWorship1`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |
| Worshippers' dance at the temple (one of six, using NewWorship): file `NewWorship`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |
| Worshippers' dance at the temple (one of six, using NewWorship2): file `NewWorship2`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |
| Worshippers' dance at the temple (one of six, using NewWorship1): file `NewWorship1`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |
| Worshippers' dance at the temple (one of six, using NewWorship): file `NewWorship`, 120 beats, no set length, any number of dancers, starts on its own | todo | no dances in openblack |

## Shipped dance files the game doesn't use

39 files are shipped; 20 are used by the table, the 19 below are not (older versions, tests and dances that were cut: funeral, marriage, totem, line dancing and others).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `Creature1` (656 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `CreatureImpressVillagers` (2,729 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `CreaturePlayingGame` (8,123 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `DancePrayingInwards.dat` (396 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `FourDancers` (3,808 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `LineDancing1` (2,628 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `NewCelebration1` (7,628 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `NewCelebration2` (11,896 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `NewEightDancers` (7,840 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `NewFourDancers` (5,056 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `OnlyDanceThatWorks` (5,912 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `TwoDancers` (1,876 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `Worship1` (6,140 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `Worship2` (9,724 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `Worship3` (684 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `funeral1` (5,916 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `marraige1` (8,061 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `totem1` (4,408 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
| `totem2` (3,870 bytes) is shipped but no dance in the table uses it | n/a | left over from development; nothing to do |
