# Fireflies

Small glowing lights that leave the trees and rocks at nightfall, hover around the houses and street lights all night
and hide again in the nearest tree or rock at dawn. By day they can't be seen; lifting a tree or rock with the hand while
a firefly hides in it removes the firefly and leaves a one-shot miracle seed where it stood, drawn from the land's
firefly reward table. This is the "miracles hidden under trees and rocks" players remember: no seed is ever placed under
a tree; it comes from a hiding firefly (see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md)).

The game hints at it three ways: the Hermit on Land 1 saw "a firefly a-heading under that there rock at break of dawn"
and the seed under his rock is a scripted version of it ([../story/silver_scrolls/the_hermit.md](../story/silver_scrolls/the_hermit.md));
a Land 2 "did you know" scroll; and a tip of the day (rows below). The game keeps no statistic of fireflies caught, and
neither the creature nor the villagers take any notice of them.

**Progress: 0/43 done, 0 partial — 0%**

## What a firefly is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A firefly is a single small white glowing sprite, 0.3 m in size, from the game's third sprite sheet; it has no model of its own | todo | openblack has no fireflies; the only trace is a TODO in `src/ECS/Systems/Implementations/HandGrabSystem.cpp` |
| It makes no sound, has no light of its own on the land, and takes no part in physics: nothing can hit it or push it | todo | |
| The game's object table lists it as a nice animal named "Firefly", with a bat model that is never drawn | todo | the type and info index exist in `src/Enums.h` only |
| It is drawn only while out of its hiding place, and not at all beyond 300 m from the camera | todo | |
| Within 100 m it is drawn at about three-quarters opacity (190 of 255); further away it fades, by the square of the distance, to nothing at 300 m | todo | |
| Its drawn place is blended between the last two game turns' places, so it moves smoothly between turns | todo | |

## Where they come from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land holds at most 50 fireflies | todo | |
| At the first nightfall after a land is loaded, and at each nightfall that follows a morning, the game makes new fireflies to bring the count back up to 50 | todo | so caught or lost fireflies come back the next night |
| Each new firefly is hidden, by a coin toss, either in a random tree anywhere on the land or in a rock: a random place in the land's list of fixed objects, then the first rock from there (none from there makes nothing this time) | todo | a land with no trees, or with no fixed objects at all, stops the top-up |
| Each firefly draws its own speed, between 0.6 and 1.4 times the normal, for its flights and its drifting, and random starting points for its drift | todo | |
| The land scripts can also place fireflies at a spot; the story lands place none and rely on the top-up | todo | `CREATE_FIRE_FLY` is an empty stub in `src/LHScriptX/FeatureScriptCommands.cpp`; see [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |
| Three Gods places 43, the demo map 38 and Death Comes To Those That Wait 86 (two lists of 43); most stand exactly on one of the map's trees (35 of 43, 26 of 38, but only 35 of 86 on Death Comes, whose lists partly repeat Three Gods' spots) | todo | see [../multiplayer/maps/](../multiplayer/maps/) |
| At nightfall a firefly whose spot no longer holds a tree or rock, or that shares its spot with another firefly, is removed instead of coming out | todo | |

## Night and day

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the evening, once the sky leaves dusk and starts to darken towards night, the fireflies come out, one each game turn | todo | the sky's stages are in `src/3D/DayNightClock.cpp`; see [../sky/day_night_cycle.md](../sky/day_night_cycle.md) |
| In the morning, once the sky leaves dawn and starts to brighten towards day, they go back into hiding, one each game turn | todo | |
| Between those times (late morning to dusk, and the dark hours until dawn) nothing changes: hidden ones stay hidden and those out stay out | todo | |
| Only the shown time of day matters: weather, rain, season, the land's alignment and the date make no difference | todo | they follow the shown clock, so a script that changes the time of day moves them too |

## Flying and hovering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Coming out, a firefly flies to a house or street light within 300 m of its hiding place, roughly the nearest (it searches a random half of the ground squares around it and takes the nearest it finds), and hovers 2 m above its top | todo | houses and street lanterns exist in openblack (`Abode`, `StreetLantern`) |
| With no house or street light in reach, it hovers 15 m to one side of its hiding place, 4 m up | todo | |
| A flight takes its length over 3 m a second times the firefly's own speed, never less than half a second, and eases in and out | todo | |
| Hovering, it drifts around its spot on a slow loop of 8 m (4 m up and down), turning 0.06 to 0.14 radians a second by its speed, with a quick loop of 1 m (half a metre up and down) on top turning 0.6 to 1.7 radians a second | todo | |
| The drift grows over the first fifth of the flight out and dies away over the last fifth of the flight home, so it rests still in its hiding place | todo | |
| Going home, it hides in a tree or rock within 300 m of where it hovered, roughly the nearest; with none it settles on the ground 15 m to the side (and is removed at nightfall) | todo | so it need not return to the tree it came from |
| So fireflies gather in the trees and rocks next to houses and street lights; a tip of the day says "If you place rocks and trees next to houses they will attract fireflies more readily." | todo | whether that tip is ever shown in the shipped game is unconfirmed |
| They follow nothing: not the hand, the creature, villagers or the camera | todo | |

## Catching one

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A firefly is never picked up or tapped itself; it is caught when the player's hand picks up the tree or rock it is hiding in, uprooting the tree or lifting the rock | todo | tree uprooting is done ([../hand/tug.md](../hand/tug.md)); see [../hand/picking_up.md](../hand/picking_up.md) |
| It must sit exactly at that tree or rock's spot, so only one at rest counts: in practice from dawn until it is sent out after dusk; one already out hovering by a house is not caught | todo | |
| The firefly is removed and a one-shot miracle seed appears where the tree or rock stood, at full strength and at the power-up level of the miracle drawn | todo | no sound or effect of its own; the seed is an ordinary one-shot globe, tapped into the hand ([../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md)) |
| One pick-up catches at most one firefly | todo | |
| The miracle is drawn by weight: each kind's chance is its weight over the sum of all weights; with every weight at zero the firefly is lost and nothing appears | todo | `FIRE_FLY_SPELL_REWARD_PROB` is an empty stub in `src/LHScriptX/FeatureScriptCommands.cpp` |
| Only the god's hand catches them: the creature lifting a tree or rock, foresters felling, the tug shaking a tree, fire, or a tree knocked down never give a seed; the firefly that lost its home is removed at nightfall | todo | |
| Any player's hand can catch them in a multiplayer game, and the fireflies' choices use the game's shared random numbers so every machine agrees | todo | see [../multiplayer/](../multiplayer/) |

## The land's reward table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every land starts with all weights at zero, then its script sets them one miracle at a time; a weight for a kind the game doesn't have is ignored | todo | `FIRE_FLY_SPELL_REWARD_PROB` stub |
| Land 1: heal 20 of 26 (77%); fireball, lightning, forest, food, wood and water 1 each (about 4%) | todo | [../scripts/land1_script.md](../scripts/land1_script.md) |
| Land 2: fireball, lightning, heal, teleport, forest, food, storm, spiritual shield, physical shield, wood and water about 8% each; flying flock, and the creature's freeze, small, big, weak, strong, invisible, compassion, angry and itchy under 1% each; ground flock about 0.1% | todo | [../scripts/land2_script.md](../scripts/land2_script.md) |
| Land 3: the same eleven at about 9% each and both flocks under 1%; no creature miracles | todo | [../scripts/land3_script.md](../scripts/land3_script.md) |
| Land 4: the eleven, with stronger heal and stronger food, about 7% each; stronger fireball, lightning and water about 1.4%; strongest fireball and lightning and the storm with lightning about 0.7%; the nine creature miracles about 0.7% each; no flocks | todo | [../scripts/land4_script.md](../scripts/land4_script.md) |
| Land 5: the eleven and stronger food about 6% each; both flocks about 3%; strongest fireball, stronger lightning, stronger heal and the nine creature miracles about 1.3%; stronger fireball, strongest lightning, the explosion miracle, the storm with lightning and the tornado about 0.6% | todo | [../scripts/land5_script.md](../scripts/land5_script.md); the only story land whose table offers the explosion miracle ([../miracles/blast.md](../miracles/blast.md)) |
| The tutorial land's weights are all zero, so a firefly caught there gives nothing | todo | [../scripts/landT_script.md](../scripts/landT_script.md) |
| No table ever names the fat, thin, hungry, frightened, tired, ill or thirsty creature miracles, so fireflies never give them | todo | see [../miracles/creature_spells.md](../miracles/creature_spells.md) |
| Skirmish: Two Gods and Four Gods share one table (fireball, lightning, heal, forest, food, storm, both shields, wood and water about 8% each, both flocks about 4%, teleport and the nine creature miracles about 1.5%, the explosion under 1%); Death Comes To Those That Wait ends with Land 2's table; Three Gods, Island Wars and Firestorm set every weight to zero, so their fireflies give nothing | todo | every map still gets fireflies from the top-up, whether its script places any or not; see [../multiplayer/maps/](../multiplayer/maps/) |

## Saving and editing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps every firefly (its spot, whether it is out, hiding or flying, and its flight), the reward table, the limit of 50 and whether the next nightfall tops them up | todo | openblack has no saved games |
| The land editor writes each firefly into the land's script as a placement at its spot | n/a | openblack has no land editor |

## Hints in the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Land 2 "did you know" scroll: "Fireflies can be seen at night. They hide under rocks during the day, and become rewards if you find them." | todo | see [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
