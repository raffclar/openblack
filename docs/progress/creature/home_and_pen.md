# Home and pen

A creature has a home: the pen by its player's citadel (temple), where it is kept while young, where it is carried
when it passes out, and which it goes back to when it wants to rest or feels safe. The game also has a creature building
a home of its own and bringing things back to it.

**Progress: 8/36 done, 7 partial — 32%**

## The pen at the citadel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each citadel has a pen for its player's creature, made by the land's script | partial | openblack takes the place by the player's temple as the pen (`CreatureFightSystem::HomeOf`, `creature_mode::PenOf`); the land script's pen command is a stub (`FeatureScriptCommands::CreateCreaturePen`) |
| A planned pen is shown as a plan before the citadel is built | todo | |
| The pen's size and look come from the game's pen tables | todo | |
| The creature's home is where a script sets it | todo | `SET_CREATURE_HOME` is a stub in `CHLApi.cpp` |
| With no temple, the creature's home is where it stands (as on the testbed) | done | `LeashSystem::ConfineToHome`, `PenOf`; test `PenIsTheHomeThenTheTempleThenTheFallback` |
| Scripts can put the creature inside the temple or bring it out | todo | `SET_CREATURE_IN_TEMPLE` is a stub |
| The creature shrinks to fit as it nears the citadel, and grows back as it leaves | todo | see [growth_and_size.md](growth_and_size.md) |
| Enter the citadel | todo | |
| Pray at the citadel | todo | |
| A creature without a citadel behaves as if it had no home | partial | `FreeOfHome` needs the player to have a temple; other effects todo |

## Kept at home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While it starts to grow up the creature is kept within a short distance of its home | done | `LeashSystem::ConfineToHome` (within 12); test `Confinement` |
| Straying out of the area it is kept in, it walks back | done | `LeashSystem::ProcessTurn` (confinement) |
| On a working leash it may go further than the area | done | `IsConfined`; test `Confinement` |
| It is free to roam only within reach of home while its player has a temple | done | `FreeOfHome`; test `Confinement` |
| Once mature enough it may leave home | partial | the tutorial script call that keeps it home exists; the stage that frees it is todo ([development_phases.md](development_phases.md)) |
| Its plans weigh being too far from home, or on the leash | todo | see [decision_making.md](decision_making.md) |
| Scripts can confine a creature to an area | partial | the confinement exists; the script call for an arbitrary area is a stub |
| Tying the leash to a house teaches it to stay there | partial | see [leash.md](leash.md) |

## Going home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hang around at home: walks somewhere nearby and sits | partial | plan action "hang around at home" walks somewhere near where it is, not near home; test `HangingAroundWalksSomewhereNearbyThenSits` |
| Go home | todo | the wish to go home is a desire with no action yet ([desires.md](desires.md)) |
| Run home when frightened | todo | |
| Go home to recover when hurt | todo | |
| Rest at home | todo | |
| Nothing scary near home makes home a safe place to go | todo | |
| Is it near home, is it not | partial | distance to home is measured for roaming (`FreeOfHome`); not used by plans |

## Carried home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature that passes out (from damage, hunger or tiredness) is carried to its pen to come round | done | `CreatureModeSystem`, `creature_mode::PassesOut`; test `PassesOutWhenAStatusReachesAHundredPercent` |
| A creature knocked out in a fight lies out cold, then fades out and in at home | done | `CreatureFightSystem::KnockOut`, `HomeOf` |
| It rests at home until healthy enough, then gets up | done | `CreatureKnockedOut` rest |
| The player is told their creature was carried home | todo | see [lessons_and_help.md](lessons_and_help.md) |
| Scripts can call the player's creature home | todo | `CALL_PLAYER_CREATURE` is a stub |

## Its own home (unconfirmed whether used in the five lands)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature wants to build a home of its own | todo | the desire exists with nothing to act on ([desires.md](desires.md)) |
| It creates the home where it chooses | todo | |
| It builds it from rocks, and is told when more rocks are needed | todo | |
| It knows whether its home is built, being built, or missing | todo | |
| Bring things home, bring food home, take food or fish home, take a toy home | todo | |
| Go out to look for food and bring it back | todo | |
