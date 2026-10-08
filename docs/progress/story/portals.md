# Portals between lands

At the end of each land a swirling vortex opens in the ground. Whatever it pulls in or is thrown into it (villagers,
animals, food and wood, one-shot miracles, rocks, toys, trees, scaffolds) is written to a crossing file, the player's
creature is carried over in its own files, and clicking the vortex's scroll flies the camera down into it and loads the
next land, where an outgoing vortex throws everything back out beside the new home town and tops the newcomers up to
30 villagers. The same kind of object makes the glowing crater of Land 5's volcano. Each land's own vortices (where,
when, what is said, what differs) are in [portals_per_land.md](./portals_per_land.md); the quests that open them are in
[gold_scrolls/](./gold_scrolls/); the lands in [land_1.md](./land_1.md) to [land_5.md](./land_5.md). Script commands in
general are in [../scripts/](../scripts/).

**Progress: 4/136 done, 13 partial — 8%**

Sources: the executable (the vortex objects, the crossing file, the land change, the creature's files), the shipped
challenge scripts' source text and the game's tables. openblack is judged on the physics work tree (`ob-wt-physics`):
it has no vortex object at all, so almost everything here is still to do.

## The three kinds of vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An incoming vortex (the exit from a land) pulls things in and writes each one to the crossing file for the next land | todo | no vortex object in openblack; `CreateScriptObject` in `src/CHLApi.cpp` logs it as not made |
| An outgoing vortex (the arrival in a land) reads the crossing file back, throws each thing out, then makes new villagers | todo | |
| A volcano vortex is the glowing mouth of Land 5's volcano; it pulls nothing in and throws nothing out, and flying things pass straight through it | todo | made by Land 5's set-up script at the crater; the creature's end walk to it is in [ending.md](./ending.md) |
| Every vortex is made with the same pull radius, 50 (five land cells), whatever its kind | todo | the radius is fixed by the game, not the script or the tables |
| A vortex is an object the creature cannot pick up, throw, eat, stomp on, set fire to, fight, examine, destroy by stoning or put in a store, and it cannot act as a container | todo | |
| A vortex is not touched by miracles or other effects | todo | |
| The hand never takes a vortex and does not rest on its model; a vortex can never be put in the hand | partial | `StaticOfItsOwn` in `src/ECS/Systems/Implementations/HandGrabSystem.cpp` and `hand_feel::Feel::Nothing` (`src/Hand/HandFeel.h`) already treat a vortex so, but no vortex is ever made |
| An incoming vortex is solid to flying things: a thrown object hits it rather than passing through, and is not lifted up over it | todo | the outgoing and volcano vortices are not solid; openblack has a vortex material row (`src/Physics/Materials.h`) but no vortex body |
| The creature keeps away from an incoming vortex unless it may go in (see [The creature](#the-creature)) | todo | |
| Scripts see a vortex as a vortex object and can find one at or near a place | todo | |
| The vortex is saved and loaded with a saved game, with its state, its fade, its counts and what it is in the middle of throwing | todo | see [Saved games](#saved-games) |

## The vortex table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack reads the vortex table (particles, starting state, fade, scale, limits) from the game's info file | partial | `GVortexInfo` in `src/InfoConstants.h` (three rows); nothing uses it yet |
| The incoming vortex starts by fading in; the outgoing and volcano vortices start fully open | todo | the table's starting state: fade in / active / active |
| Each kind has its base size: 0.28 for the land vortices, 0.75 for the volcano; the model and the ground ring are scaled by it | todo | the table's base scale |
| Each kind names its own four effects: before the land is drawn, after it, the object mover and the glow on the ground | partial | the effect names are mapped in `src/Particles/ParticleTypes.cpp`; nothing places them |
| The table's "fade when deleted" column (0 / 1 / 0) is never read: every vortex fades out only when a script tells it to | todo | quirk: the column has no effect |
| The table's limits (at most 1000 objects for the outgoing vortex, counts of food, wood, villagers and one-shots) are never read; the only limit is a fixed 30 villagers (see [Arriving](#arriving)) | todo | quirk: the columns have no effect |

## How it looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The vortex draws its own model, the spinning vortex cylinder, scaled by its base size | todo | the mesh is listed in `src/3D/AllMeshes.h`; nothing draws it as a vortex |
| Before the land is drawn, a spinning funnel of two swirl textures turns on the spot (the "before" effect) | partial | the effect file plays in the testbed (`particles.vortex_and_flies_sounds`, `src/Debug/TestbedScenarioRegistry.cpp`); not placed by any vortex |
| After the land is drawn, a cloud of stars spins over it (the "after" effect), the incoming and outgoing vortex sharing it | partial | same testbed scenario plays the outgoing "after" effect |
| The outgoing vortex uses the incoming vortex's "before" effect (the game's table points both at the same file) | done | `src/Particles/ParticleTypes.cpp` maps the outgoing "before" type to the incoming file |
| A light map paints a glow on the ground around the vortex | partial | the light-map effect is in the testbed's large-particles scenario; not tied to a vortex |
| A ring texture and its alpha mask are laid on the land block under the vortex, scaled to the vortex's base size: the multi-ring base for the land vortices, the volcano's own base and mask for the crater | todo | |
| An incoming or outgoing vortex levels the ground under it: in an 11-by-11-cell square around it the land is pulled towards the square's average height, fully within 50 of the middle and fading to nothing at 56 | todo | the heights are read once, when it first opens; earlier notes called this a funnel, but the game's curve adds no dip (its five points are all at zero height), so the ground is flattened, not sunk |
| The levelling grows with the fade-in, eased out (one minus the square of what is left), and is only ever added to, never undone, so the flattened pad stays after the vortex has gone | todo | while a vortex fades out the levelling counts as complete |
| The volcano vortex does not change the land | todo | |
| The volcano vortex has its own before, after and light-map effects (fire and rock) | partial | the effect files are mapped in `src/Particles/ParticleTypes.cpp`; nothing places them |
| Things being pulled in are carried by the object-mover effect: each spirals round the middle, closing in as it ages and rising on a curve, and vanishes when it reaches the middle | todo | the object-mover effect file; the tumble rule exists in `src/Particles/ParticleHandRules.cpp` but no vortex uses it |
| A vortex fades in over 7 seconds and is then fully open; told to fade out, it takes 7 seconds and then deletes itself | todo | the scripts wait 8 seconds after starting a fade-out, which matches |

## How it sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The vortex hums: a looping vortex sound starts with the "before" effect of the incoming vortex and the "after" effect of the outgoing one, released softly when they stop | partial | the hum plays in the testbed scenario; no vortex makes it in a land |
| The volcano vortex has a looping volcano rumble of its own, started when it is made | todo | |
| Villagers thrown out of an outgoing vortex scream only for their first 10 turns of flight (15 for an ordinary throw) | partial | the rule is `k_ThrownVortexSoundTurns` (`src/Audio/ClipSounds.h`, `src/ECS/ClipSoundPlayer.cpp`), but openblack gives the vortex clip to villagers carried by a tornado, never to ones from a vortex (see [Arriving](#arriving)) |
| A creature that reaches the middle of a vortex makes the teleport "energise" sound as it starts to sparkle away | todo | |
| Land 4's hidden vortex rumbles: a screen-rumble sound with every camera shake | todo | see [portals_per_land.md](./portals_per_land.md#land-4-exit); `ShakeCamera` is a stub |
| The hand over a vortex sends its own force-feedback effect | n/a | force-feedback mice are not supported (see ../pc_integration/online_services.md) |

## Being sucked in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only a fully open incoming vortex pulls: not while it fades in or out | todo | |
| Every turn it searches the cells around it in a spiral out from its own cell, stopping at the first cell beyond its reach (or after 9999 cells) | todo | |
| On two turns out of three it only reaches half its radius (25); on every third turn the whole 50 | todo | earlier notes said three quarters; the game uses one half. Things near the middle are taken three times as often as things at the edge |
| On each cell it takes every thing whose own cell it is and that may be sucked in; a thing that may be sucked in is anything that could be thrown or knocked flying (see [What goes in](#what-goes-in-kind-by-kind)) | todo | the creature has its own rule |
| A thing thrown into the vortex that hits its model while it is open is taken at once, keeping its speed and turn as it starts to spiral | todo | its flight is stopped; this is how things thrown by hand go in even before they land |
| Things merely carried over the vortex in the hand are not taken: nothing in the hand is on the ground | todo | a villager in the hand cannot be sucked in either |
| Food and wood piles too big to fly are drained instead: each time its cell is searched a pile gives up a pot of up to 1000 of its food or wood (of the kind dropped from the hand), and that pot is sucked in | todo | piles near the middle drain three times faster; whether a storehouse's own stock piles answer was not confirmed — buildings, fields and the storehouse itself never do |
| Pots made from a pile are drawn at 30% to 42% of their usual size at random as they fly in | todo | |
| Nothing walks into the vortex on purpose: no villager, disciple or animal is ever sent to it; only those that wander within reach, are dropped or thrown in, or stand where it opens are taken | todo | there is no "go to the vortex" job, gathering place or belief rule on the way in |
| Villagers hiding in a building (the run-and-hide state), villagers inside buildings and villagers in the hand are never taken | todo | |
| Each thing taken is handed to the object-mover effect; if the vortex has no object mover, nothing is taken | todo | |
| A thing is written to the crossing file the moment it is caught, before it is seen to fly in; a villager leaves its town and an animal drops everything that depends on it at that moment | todo | it vanishes when it reaches the middle |
| A thing that is indestructible (made so by a script, or one still being thrown out by an outgoing vortex) is pulled round and in like any other but is not written down: at the middle it is thrown back out at a random angle | todo | quirk |
| A creature close to the middle of the vortex (within seven tenths of the vortex's own width) sparkles away over 3 seconds instead; it is never written to the crossing file | todo | see [The creature](#the-creature) |
| Things in flight towards the vortex are not counted as thrown by anyone and do not react | todo | |
| The player can throw things in by hand; the advisors tell the player to send villagers, food, wood and followers in | todo | Land 1 and Land 4 lines; see [portals_per_land.md](./portals_per_land.md); Land 1's: [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md) |

## What goes in, kind by kind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers go in, alive or dead; each is written as its tribe and job (or its exact kind, for a villager of no tribe), its age, its place relative to the vortex and the number of its old town | todo | its health, hunger, job in progress, home, family, disciple role, poison and anything it carries are not kept |
| Disciples go in as plain villagers: their discipleship is not kept | todo | see [../villager/disciples.md](../villager/disciples.md) |
| Animals go in (cows, sheep, pigs, horses, wild animals and birds); each is written as its kind, its age, its flock and its flock's town | todo | its health and training are not kept |
| Doves are pulled in but never written down: they are lost | todo | |
| The player's creature, other gods' creatures and story creatures never go into the crossing file | todo | the player's creature crosses in its own files ([The creature](#the-creature)) |
| Food and wood dropped from the hand (hand piles) go in whole, as a pot of their kind and amount | todo | see [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md) |
| Big wood piles, food piles and magic food and wood piles cannot fly: they are drained into pots of up to 1000 (above) | todo | |
| A pot that is part of a structure is never written down | todo | |
| One-shot miracle globes go in, each written as its miracle with the command the lands use for powered-up globes | todo | whether an ordinary globe comes out powered up was not determined; the scripts' `CREATE_ONE_SHOT_SPELL_PU` is a stub (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| Seeds made from worship-site or village-centre icons and spell icons themselves never go in | todo | they can never fly |
| Rocks, fragments and other loose statics go in, each written as its kind, size and turn | todo | |
| Toys go in and come out in the next land as the same toy | todo | a toy is a loose static; see [../nature/toys.md](../nature/toys.md) |
| An artefact goes in and comes out still an artefact of the same god with the same worth; only its town is forgotten | todo | see [../town/artefacts.md](../town/artefacts.md) |
| Loose scaffolds go in and come out as scaffolds of the same kind; a scaffold in a workshop's yard, on a building site or tied to a building plan is never taken | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Living trees within reach are uprooted and go in, each written as its kind and size, and grow again in the next land | todo | trees count as things that can fly; nothing keeps them out |
| Dead and felled trees go in, keeping their kind, size and owning god | todo | |
| Field crops, reward chests that are ready to open, barrels, balls and other loose objects go in, each written as its kind, size and turn | todo | a chest comes out as a plain chest of its kind; its contents were not traced |
| A loose object stuck to something (carried, or fixed to another object) is pulled in but not written down | todo | |
| Buildings, fields, the temple, worship sites, village centres, wonders, spell dispensers, storehouses, lanterns, bonfires, fires, shields, fireballs, the Creed, whales and other vortices are never taken | todo | they cannot fly |
| Other gods' villagers, animals and things are taken just like the player's; a villager keeps nothing of its old god | todo | everything that comes out joins the arrival town |
| Objects are written in the order they are caught, and nothing limits how many | todo | |

## The crossing file

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What goes into an incoming vortex is written to a crossing file in the save folder, one line per object, in the same language as the lands' own set-up files | todo | openblack's land set-up reader (`src/LHScriptX/FeatureScriptCommands.cpp`) knows the commands but has no crossing file |
| Each line places the object relative to the vortex, so things come out spread round the arrival vortex as they lay round the exit | todo | |
| Every new incoming vortex empties the file when it opens, so only the last incoming vortex of a land counts | todo | on Land 2 this throws away anything sent through the earlier vortices ([portals_per_land.md](./portals_per_land.md#land-2-khazars-death-and-lethyss-vortex)) |
| An outgoing vortex opens the same file for reading when it is made; if there is no file it only makes new villagers | todo | |
| Nothing else about the land is written to the file: the creature, the hand, towns, buildings and statistics are handled elsewhere | todo | |

## Saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Saving a game while a vortex is open copies the crossing file beside the save and records how far it has been read | todo | |
| Loading that save copies the crossing file back, so a crossing can be saved and resumed from either side | todo | |
| A loaded incoming vortex carries on adding to the end of the file; a loaded outgoing vortex skips the lines it had already thrown out | todo | |
| The vortex's state, fade, gathering place, flock, town, counts and the list of things it is throwing are saved and restored | todo | |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature crosses in its own files: when the land is cleared its mind and its body are saved to the creature-mind folder under the player's name | partial | openblack can save and load a mind (`src/ECS/Systems/CreatureMindSystemInterface.h`, `src/Creature/CreatureMindModel.cpp`); nothing does it on a land change and the body file is not written; see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| The mind file keeps what it has learnt, its alignment, its age, size, energy and other body values and its tattoos; the body file keeps its species, its look, its strength and two more body values | todo | the mind file is read and written byte for byte by `MindFile`; the body file's last two values were not identified |
| A creature the game has marked as not the player's own to keep is not saved | todo | which creatures carry the mark was not determined |
| The creature does not physically go through: walking it into the vortex only makes it sparkle away; it crosses because it is saved when the land is cleared, wherever it stands | todo | |
| The creature may go near an incoming vortex only when the script allows it through and it is on the leash, or when a script is moving it; otherwise it keeps away | todo | the permission is a script property (`src/ScriptHeaders/ScriptEnums.h`); nothing reads it |
| A creature allowed near that comes within seven tenths of the vortex's width sparkles out over 3 seconds and stays invisible; nothing can make it sparkle back in that land | todo | |
| In the next land the arrival script loads the creature at a given spot, turned to face the vortex; it appears out of sparkles over 3 seconds | todo | `LoadMyCreature` in `src/CHLApi.cpp` is a stub |
| Loading the creature does nothing if the player already has one: Land 3 has already put it in Lethys's prison, so it does not come out of that land's vortex | todo | see [portals_per_land.md](./portals_per_land.md#land-3-arrival) |
| Only the spot's two map coordinates are used; the creature stands on the ground there | todo | |
| What the creature holds and its leash are not kept | todo | the hand and leash are reset with the land |

## What crosses and what is left

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clearing the land saves the player's creature, then deletes every object, town, building and effect of the old land | partial | `Game::LoadMap` in `src/Game.cpp` resets the land (weather, cinematic director and others per the port notes); not reached from a vortex |
| Whatever is in the hand when the land is cleared is lost (a held miracle too); only what went into the vortex crosses | todo | the hand is reset with the land |
| Buildings, towns, the temple, the worship site and the storehouse's stock stay behind; the next land's script builds a new temple and village centre part-built | todo | |
| Prayer power does not cross: the new temple starts with its own (unconfirmed) | todo | it belongs to the old temple, which is deleted; not traced further |
| The player's alignment crosses: later lands check it (for example Land 5's opening) | todo | the player's record is not reset when the land is cleared |
| Each player's power with each tribe is reset to the start, and each player's leftover miracle counts are cleared | todo | what those counts hold was not determined |
| Computer gods are restarted from scratch | todo | |
| Script globals survive the land change, so later lands can ask what happened (for example whether Lethys was killed) | todo | |
| Bookmarks, highlights, the camera's stack, physics, game statistics, fireflies, special villagers, the camera's no-go zones and the town-building help are cleared | todo | statistics: [../interface/statistics_counted.md](../interface/statistics_counted.md) |
| The help system's time played is reset with the land | todo | |
| The believers count, town happiness and the creature's place in the temple do not cross: they belong to the old towns and temple | todo | |

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The next land's script makes an outgoing vortex at its arrival point and tells it the home town, the gathering place, a distance and a radius, and an optional flock | todo | `VortexParameters` in `src/CHLApi.cpp` is a stub |
| If no town is given the outgoing vortex uses the nearest town within 200 | todo | |
| Every second turn the outgoing vortex brings out one thing: it reads the next line of the crossing file, makes the object 16 from the middle at a random angle and throws it outwards | todo | |
| Each thing is thrown out at 8 to 13 outwards and 10 to 15 upwards, spinning end over end faster the shorter it is, as if thrown by the vortex's god | todo | |
| When the file runs out it keeps making new villagers until 30 villagers in all have come out, counting those that came through | todo | a fixed 30, whatever the script asks for: sending 30 or more villagers through means no new ones |
| New villagers are of the home town's tribe (Norse when there is no town): half are housewives, the rest a forester, fisherman, farmer, shepherd or leader at random — never a trader | todo | earlier notes said three in four; the game uses one half |
| Every fifth new villager is a child (age 1 to 9); the others are adults (16 to 21) | todo | |
| Villagers that come out leave whatever town they were made in and join the vortex's town | todo | the crossing file still names their old land's town number |
| With a flock given, villagers that come out become disciples "from the vortex", standing in a crowd, and join the script's flock | todo | Land 2 only; see [../villager/disciples.md](../villager/disciples.md) |
| A villager that comes out while a script owns the flock is handed to the script | todo | |
| Animals that come out are put in one flock the vortex makes at the gathering place, with the given distance and radius; one flock serves every kind of animal | todo | |
| Doves that come out are not followed by the vortex | todo | |
| Everything thrown out is indestructible until the vortex has gone | todo | earlier notes said "marked so nothing will eat it"; the mark is the indestructible one |
| Villagers that came out are kept at full health every turn until the vortex has gone | todo | not only while flying |
| Villagers thrown out by a vortex play the thrown-from-a-vortex animation while they fly | partial | `living::VillagerThrownClip` (`src/Physics/LivingRules.cpp`) has the clip, but openblack chooses it for villagers carried by a tornado (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); the game only uses it for villagers from a vortex |
| A fade-out started while the vortex still has things to throw waits: the fade only begins 7 seconds after the last thing is out | todo | so a big crossing keeps the vortex open well past the scripts' 15 seconds |
| The arrival scripts start the fade-out 15 seconds after making the vortex and count it as closed 8 seconds later | todo | |
| Food and wood come out as loose pots beside the vortex, not in the storehouse; villagers carry them home like any loose pots | todo | |
| One-shot globes, toys, rocks, trees, scaffolds and artefacts come out as they went in | todo | |
| The outgoing vortex spreads newcomers: Land 2 27 within 48; Lands 3 and 4 20 within 80; Land 5 20 within 50 | todo | these two numbers only shape the vortex's animal flock |

## Going through

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player goes through by clicking the scroll over the vortex or the vortex itself | todo | `CreateHighlight` in `src/CHLApi.cpp` is a stub |
| On Lands 2 to 4 an advisor comes out and points at the scroll at most every 30 seconds while the camera is within 100 and it is in view; Land 1 instead repeats a reminder every 30 minutes | todo | see the per-land rows; Land 1's: [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md) |
| Each land then dives the camera into the vortex and fades to black, its own way | todo | `MoveCameraPosition` and `RunCameraPath` are stubs; see [portals_per_land.md](./portals_per_land.md) |
| Fades to and from black | done | `SetFade`, `SetFadeIn` in `src/CHLApi.cpp` through the cinematic director |
| When the crossing ends all the land's scripts are stopped except the story's control script, and the next land is loaded behind a loading screen | partial | stopping scripts is done (`StopAllScriptsExcluding` in `src/CHLApi.cpp`); loading the next map from a script is not (`LoadMap` is commented out) |
| The game clears the land, loads the next one's set-up file, then assigns the towns' features | todo | |
| The story's control script is started by the game itself and runs the five lands in turn | todo | |

## Script commands for vortices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Create a vortex of a kind (in, out, volcano) at a place | todo | `Create` in `src/CHLApi.cpp` |
| Set the outgoing vortex's town, gathering place, distance, radius and flock; a bad vortex or town is reported as an error | todo | `VortexParameters` stub |
| Start a vortex's fade-out; a thing that is not a vortex is reported as an error | todo | `VortexFadeOut` stub |
| Allow or forbid the creature to go through a vortex | todo | the property is in `src/ScriptHeaders/ScriptEnums.h`; nothing reads it |
| Load the player's creature at a place | todo | `LoadMyCreature` stub |
| Load another land | todo | `LoadMap` stub |
| The scripts keep a "vortex open" flag that other scripts wait on | done | ordinary script globals in the virtual machine |
| Stop all scripts but the named ones | done | `StopAllScriptsExcluding` |

## Opening the exit (per land)

Each land's exit, with its conditions, scenes, dialogue and quirks, is in
[portals_per_land.md](./portals_per_land.md).

## Arrival scenes

Each land's arrival scene is in [portals_per_land.md](./portals_per_land.md).
