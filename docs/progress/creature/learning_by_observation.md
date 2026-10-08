# Learning by observation

Besides the hand's rewards, the creature learns by watching. It picks up ordinary skills (building, fishing, dancing and
so on) by watching villagers practise them, learns miracles by seeing them cast often enough, and copies what its god
does: it notices a deed, does it itself, and may come to want what its god seemed to want. What it sees its god do also
tells it what its god wants.

**Progress: 40/84 done, 9 partial — 53%**

## Skills learnt from villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There are six ordinary skills to learn by watching: building, using a field, using a totem, using the storehouse, fishing and dancing | done | read from the game's tables in `src/Creature/CreatureMindTables.cpp` |
| Each skill needs the creature grown up to a stage first: fields, fishing and dancing early, the totem later, building and the storehouse late | done | `SkillRule::minPhase`, `creature_watching::SeeSkill` |
| A skill is learnt once the creature has watched it for six or seven seconds from first seeing it | done | `SkillRule::watchSeconds`; test `CreatureWatching.LearnsASkillOnceWatchedLongEnough` |
| How often it has seen each skill and when it first did are kept and saved in its mind | done | `creature_watching::Knowledge`, `src/Creature/CreatureMindModel.cpp` |
| Villagers practising a skill where creatures can see it teach them | partial | `CreatureMindSystem::SeeSkill` works, but no villager job reports what it practises; only the debug spawner and testbed call it |
| Only creatures near enough and able to see it learn | partial | openblack uses a fixed reach of 150 (`k_ViewDistance`); the original goes by the creature's sight (unconfirmed) |
| Knowing a skill lets it do the actions that need it (help build, fish, dance, use the storehouse or a totem) | todo | those actions aren't carried out yet; see [town_actions.md](town_actions.md) |
| A skill seen too young to learn is ignored | done | `Progress::ignored` |
| The player is told when it has learnt a skill, nearly has, or is too young to | todo | see [lessons_and_help.md](lessons_and_help.md) |

## Miracles learnt by seeing them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature learns a miracle by seeing it cast near it, whoever casts it | partial | `CreatureMindSystem::WatchMiracle` from the miracle reactions (`src/ECS/Systems/Implementations/CreatureMindReactions.cpp`); whether every caster and every miracle reaches it is unconfirmed |
| Each miracle needs seeing a set number of times (water 9, food 12, heal and wood 15, fireball 18, lightning and shield 21, tornado 39; the explosion so many it only comes from a script) | done | `MiracleRule::timesToSee` from the tables |
| Some species need to see miracles more often than others | done | `creature_mind_tables::MiracleMultiplier`, `creature_watching::TimesNeeded`; test `CreatureWatching.LearnsAMiracleBySightingsAndSpecies` |
| A sighting counts again only five seconds after the last | done | `k_MiracleSightingTurns` |
| On the learning leash each sighting counts three times | done | `mind.leash.miracleSightingWeight`; see [leash.md](leash.md) |
| Miracles need the creature grown up to a stage (the explosion line later than the rest) | done | `MiracleRule::minPhase` |
| A power-up teaches nothing until its miracle is known; the storm with lightning needs the storm, the tornado needs that; the thirst and itch spells need the building skill | done | `creature_watching::MiraclePrerequisite`; test `CreatureWatching.APowerUpTeachesNothingBeforeItsMiracle` |
| The lightning bolt's second power-up is never learnt by watching | done | `k_NotLearnt` in `CreatureMindReactions.cpp` |
| A frozen creature learns nothing from what it sees | done | `mind.paused` check in `WatchMiracle` |
| Some miracles are flagged as known from the start, which only the computer gods use to choose what to teach | done | `MiracleRule::knownAtStart` read and kept out of the creature's knowledge |
| Once it has seen a miracle half the times it needs, it may try it; tries count as sightings | done | see [creature_casting.md](creature_casting.md) |
| It keeps counting sightings after it has learnt a miracle | done | `creature_watching::SeeMiracle` |
| Three quarters of the way there it is "nearly" there, and a meter of how far along it is moves | partial | `LearningEvent::NearlyLearnt`, `Progress::meter` are worked out but shown nowhere |
| Learnt miracles appear on the Creature Cave's magic scroll | done | `src/ECS/Systems/Implementations/CreatureCaveSystem.cpp`; see [creature_cave.md](creature_cave.md) |

## Copying the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Copying goes in stages: noticing the deed, doing it itself, then (for some deeds) wanting what the player wanted | done | `creature_watching::StepMimicry`; test `CreatureWatching.MimicryStages` |
| It copies only once past the third stage of growing up | done | `k_MinMimicPhase` |
| It copies only while the player holds the learning leash in the hand that did the deed, except playing with a toy | done | `MimicRule::needsLearningLeash`, `MimicConditions::learningLeashInHand` |
| It doesn't copy while it reacts to something pressing | partial | the rule exists (`k_MaxReactionPriorityToMimic`) but openblack always passes no reaction |
| It must be able to see where the deed was done | partial | a fixed reach of 150 rather than the creature's sight |
| Each deed has its own chance of being copied (stealing and sacrifice very likely, planting a tree unlikely) | done | `MimicRule::chance` from the tables |
| Already copying, a new deed takes over only if it is as likely or more, and different | done | `creature_watching::StartMimicry` |
| Each stage lasts a number of steps from the tables, plus a little at random, a step a second | done | `MimicRule::stageSteps`, `k_MimicStepTurns` |
| Noticing, it plays an action for the kind of deed: helpful, aggressive, neutral, stealing or playful | partial | openblack only turns to look at the spot for two seconds (`PlayerDid`) |
| It does the deed itself, to the same thing or one like it | partial | `CreatureMindSystem::LearnTurn`; only with actions openblack carries out |
| For some deeds it then comes to want the desire behind it above all | done | `MimicStage::CopyDesire` → `MakeFullyDominant` |
| Stroked while copying, it skips straight to doing the deed and wants to follow its god more | done | `creature_watching::StrokedWhileMimicking`, `k_MimicStrokeBoost` |
| Only the player's own leashable creature copies that player | done | `PlayerDid` |
| Being too young to copy is shown to the player | todo | see [lessons_and_help.md](lessons_and_help.md) |

## Deeds the creature notices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Putting food in a worship site by hand | todo | not reported |
| Casting food at a worship site | todo | `src/Magic/MiracleDeeds.h` has it, but worship sites aren't on the land, so it never fires |
| Putting food in the storehouse by hand | partial | Giving by hand goes through `ResourceStoreSystem`; whether it reports the deed to the creature isn't checked |
| Casting food into the storehouse | done | `MagicSystem.cpp` reports it to `CreatureMindSystem::PlayerDid` |
| Putting wood in the storehouse by hand | todo | not reported |
| Casting wood into the storehouse | done | `MagicSystem.cpp` |
| Building a house | todo | not reported |
| Putting wood on a building site by hand | todo | not reported |
| Casting wood by a building site | todo | the deed exists but buildings going up aren't on the land, so it never fires |
| Putting wood in the workshop, or casting wood by it | todo | not reported |
| Planting a tree | todo | not reported |
| Shielding a town | todo | not reported |
| Bringing people to worship | todo | not reported |
| Making an artefact | todo | not reported |
| Damaging something by throwing, or throwing something at it | done | `PhysicsGameHooks::ImpactFeedback` → `PlayerDid` |
| Damaging something with fire | done | `MagicSystem.cpp` (fire miracles) |
| Damaging something with magic | done | `MagicSystem.cpp` |
| Impressing by throwing | todo | not reported |
| Impressing with magic | done | `MagicSystem.cpp` |
| Throwing something into the sea | done | `villager_physics::Sink`, `PhysicsGameHooks::HasSunk` → `PlayerDid` |
| Making a disciple of each kind: farmer, forester, fisherman, builder, breeder, protector, missionary, craftsman, house changer, worshipper | todo | not reported; see [../villager/](../villager/) |
| Taking something home | todo | not reported |
| Casting water on crops | done | `MagicSystem.cpp` |
| Casting water to put out a fire | done | `MagicSystem.cpp` |
| Stealing something and putting it in a town, or by the temple | todo | not reported |
| Breaking rocks | todo | not reported |
| Throwing the football into the goal, or catching it | todo | not reported; see [../town/football.md](../town/football.md#the-player-and-the-creature) |
| Sacrificing | todo | not reported |
| Playing with a toy | todo | not reported; the toys: [../nature/toys.md](../nature/toys.md) |
| Healing | done | `MagicSystem.cpp` |
| Stealing food from a farm | todo | not reported |

## What it thinks its god wants

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle it sees its god cast shows it the desires the miracle answers, and the town desire it helps | done | `MagicSystem.cpp` → `creature_perceived_desires::Increase`, `IncreaseTown` |
| It only takes in what it can see: within about two thirds of a half turn of where it looks, or in its own patch of land | done | `creature_perceived_desires::CanSeePos`; test `PerceivedDesires.ACreatureSeesTwoThirdsOfAHalfTurnEitherWayOrInItsCell` |
| What it thinks fades slowly | done | `creature_perceived_desires::Fade`; test `PerceivedDesires.SeenDesiresAddUpHeldToOneAndFade` |
| What it thinks its god wants drives its desire to follow its god's wishes | todo | nothing feeds that desire's source yet; see [desires.md](desires.md) |
| What it thinks its god wants most is shown in the Creature Cave | done | `CreatureCaveSystem.cpp`; see [creature_cave.md](creature_cave.md) |

## Watching others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Watching its god be nice, nasty or impressive stirs its own compassion, anger or wish to impress | todo | the desire sources exist but nothing feeds them from watching; see [desires.md](desires.md) |
| Watching villagers play or eat stirs its own wish to play or eat | todo | as above |
| Seeing an action done, it updates its desires as if it had done it | todo | not modelled |
| A friendly creature can teach it what it knows, and it can ask a friend to teach it | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| It follows a friend doing something worth copying | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |

## Teaching by scripts and computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can make it learn everything, or everything but some actions | todo | `CREATURE_LEARN_EVERYTHING`, `CREATURE_LEARN_EVERYTHING_EXCLUDING` are stubs in `src/CHLApi.cpp` |
| Scripts can set whether it knows a single action | todo | `CREATURE_SET_KNOWS_ACTION` is a stub |
| Scripts can give it the most skill at one miracle or power-up | todo | not modelled |
| The developer cheats to learn everything, learn ordinary things, or move to the next stage | todo | not present; the debug spawner can show a skill, cast a miracle or do a deed near a creature (`src/Debug/CreatureSpawnerMind.cpp`) |
| Computer gods teach their creatures skills, miracles and the use of totems, and lead them on the leash to things | todo | no computer gods; see [../story/](../story/) and [../multiplayer/](../multiplayer/) |
| A computer god's creature's knowledge is balanced to the land's difficulty | todo | not modelled |
