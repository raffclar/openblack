# Workshop and scaffolds

The workshop turns wood into scaffolds, the building blocks the player carries by hand. A scaffold stands for one or
more scaffolds joined together; held over a town it shows the building the town most wants of that size, and put down
gently it becomes that building's site, which the villagers then build with wood. Scaffolds can be joined up to the
size of a wonder, tapped apart again, given to other towns, stolen by creatures and handed out by scripts and rewards.

**Progress: 1/115 done, 4 partial — 3%**

## The workshop building

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place workshops already built, with their starting wood, or as plans for the town to build | partial | built ones: `src/ECS/Archetypes/AbodeArchetype.cpp` (`CREATE_ABODE`) keeps the wood amount; `CREATE_PLANNED_ABODE` is not implemented |
| Each of the nine tribes has its own workshop model (the African one is stored under an "American" name) | partial | the tribe's model is drawn and follows the ground (`AbodeArchetype.cpp`); nothing else of the workshop exists |
| A workshop is a civic building raised from 3 joined scaffolds and 3500 wood (the Tibetan one 7000), by up to 6 builders | todo | building it is [construction.md](construction.md); the scaffold rules are below |
| While a workshop is being built, the wood brought for it lies on a wood pile at the workshop itself | todo | |
| When finished it joins its town's list of workshops; only finished workshops make scaffolds or are supplied | todo | |
| The hand feels a different surface over each tribe's workshop: straw (Celtic, Aztec), smooth (African, Egyptian, Greek, Tibetan), wood (Japanese, Norse) or canvas (Indian) | todo | See [../hand/](../hand/) |
| Its "?" help gives seven lines ("Get a Workshop and you can place Scaffolding." … "You can place more than one Scaffolding unit next to each other, too.") | todo | See [../interface/help_system.md](../interface/help_system.md) |
| A workshop is a store for wood only | todo | |
| Destroying a workshop frees its scaffolds: they stay where they are, no longer counted as the workshop's | todo | its wood pile is removed too (from Diego's port, unconfirmed); see [damage_and_repair.md](damage_and_repair.md) |

## Wood for the workshop

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wood dropped on the workshop by the hand (a handful, a tree), thrown at it or poured on it by the wood miracle goes into it | todo | See [../resources/resource_handling.md](../resources/resource_handling.md) and [../miracles/wood.md](../miracles/wood.md) |
| The workshop keeps its wood on a wood pile beside it, made the first time wood arrives | todo | |
| There is no limit to the wood a workshop can hold | todo | |
| A wood flag beside the workshop rises with the wood still missing for the next scaffold (1 − wood ÷ 2500, at most 3.9 high) | todo | the in-game help: "The flag outside shows the amount of wood needed to create a Scaffold" |
| Ordinary villagers never supply the workshop: the town's wish to supply it is always nil | done | `src/ECS/TownDesire.cpp` (`DesireToSupplyWorkshop`); see [../town/town_desires.md](../town/town_desires.md) |
| A villager of the player's own town dropped by the workshop becomes a craftsman disciple | todo | a town condition can refuse it (undetermined what it means); see [../villager/disciples.md](../villager/disciples.md) |
| A craftsman carrying wood takes it to the workshop; otherwise he goes to the storage pit when it has wood and picks up as much as he can carry | todo | the in-game help: "if you place a person next to the workshop they will keep collecting wood from the Village Store" |
| The craftsman takes the wood to the town's best finished workshop: the one in need of wood, nearer ones preferred (out to 500 m) | todo | a workshop needs wood while it has room for another scaffold or less than 2500 wood |
| Craftsmen raise the town's desire for wood by their share of the town's adults | partial | the formula counts craftsmen (`src/ECS/TownDesire.cpp`); nothing makes craftsmen supply a workshop yet. See [../town/town_desires.md](../town/town_desires.md) |
| The creature learns from seeing the player put wood in the workshop, and can do it itself | todo | See [building_by_creature.md](building_by_creature.md) and [../creature/town_actions.md](../creature/town_actions.md) |

## Making scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A finished workshop with room in its yard and at least 2500 wood starts a scaffold, taking the 2500 wood at once | todo | |
| Each scaffold takes 200 game turns to make | todo | |
| A working sound plays at the workshop while it makes a scaffold, stopping when it is done | todo | See [../audio/sound_effects.md](../audio/sound_effects.md) |
| The workshop's chimney smokes while a scaffold is being made | partial | the workshop has its own grey smoke (`ChimneySmokeSystem`), but it is lit only while someone is at home, which never happens for a workshop |
| Over the workshop the hand shows how far the current scaffold is made ("Production Completed: …%") | todo | See [../hand/pointing_and_tooltips.md](../hand/pointing_and_tooltips.md) |
| A finished scaffold (worth one) appears at the first free of three places in the yard, turned to that place, and belongs to the workshop's town | todo | the places are marked on each tribe's model |
| A horn sounds at the workshop when a scaffold is ready | todo | |
| The yard holds three: room = 3 − one being made − scaffolds the workshop still counts as its own | todo | the table's "most to produce" figure is not used |
| Scaffolds sitting in the yard cannot be knocked about: they only move when picked up | todo | |
| A scaffold taken from the yard keeps its place reserved: the workshop still counts it as its own | todo | |
| Once it has lain somewhere else for 150 game turns (and is not a script's scaffold), the workshop lets it go and its place is free again | todo | |
| A scaffold put down within 2 m of a yard place snaps into it, joining that workshop if it has room | todo | any workshop of the town it is dropped by |
| No scaffold can make a building in the workshop's yard: within 10 m of the yard or 5 m of each of its three places | todo | |
| Every tribe's workshop makes scaffolds the same way; only the model, its cost and the hand's feel differ | todo | |

## What a scaffold is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scaffold is a movable wooden frame worth one or more scaffolds ("Scaffold Value …" under the hand) | todo | |
| Its size shows its worth: drawn at 0.5 + 0.2 × its worth (0.7 for one, 1.9 for seven) | todo | |
| All tribes share one scaffold model | todo | the model is listed in `src/3D/AllMeshes.h`; nothing uses it |
| A scaffold is worth 2500 wood for each scaffold it stands for | todo | |
| It weighs 400 and has its own physics material and collision sound | todo | the material row exists in `src/Physics/Materials.h`, unused; see [../physics/object_dynamics.md](../physics/object_dynamics.md) |
| The hand feels wood over it | todo | |
| A scaffold remembers the town whose workshop made it | todo | |
| The first time the player holds a scaffold the help system gives its message, once | todo | See [../interface/help_system.md](../interface/help_system.md) |
| Its "?" help: "A piece of Scaffolding." | todo | |
| Scaffolds never wear out or rot while they lie about | todo | |
| An "old scaffold" object exists in the tables but nothing places it | n/a | no land or script uses it |

## Which building a scaffold makes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town is that of the nearest building within 50 m; failing that, the nearest town or planned building within 50 m, whoever it belongs to | todo | |
| It offers every kind of building that needs scaffolds and needs no more than its worth, keeping the one that fits the ground there and the town most wants | todo | See [../town/town_desires.md](../town/town_desires.md) |
| Each building is tried at eight angles 45° apart from a random start, the first that fits being kept | todo | |
| The two smallest house kinds need one scaffold everywhere; the other four need, by tribe: Celtic, Greek and Tibetan 1, 2, 2, 2; African 1, 1, 2, 2; Aztec 1, 2, 2, 1; Japanese, Indian and Norse 2, 2, 2, 1; Egyptian 2, 1, 2, 2 | todo | |
| A storage pit, crèche, workshop or graveyard needs 3; a field 4; a village centre 5; a miracle dispenser 6; a wonder 7 | todo | the in-game help lists 1 small house, 2 bigger house, 3 civic, 4 field, 5 village centre, 7 wonder |
| A football pitch needs 8, possible only with football turned on; a totem is never made from scaffolds | todo | see [../town/football.md](../town/football.md) |
| The wonder takes the tribe of the town the scaffold came from; other buildings take the receiving town's tribe | todo | See [../town/wonder.md](../town/wonder.md) |
| The wonder's size comes from the town's wonder power at that spot | todo | See [../town/wonder.md](../town/wonder.md) |
| Away from every town, a scaffold of five or more with a home town offers a village centre that will found a new town of its home town's tribe | todo | tried at eight angles; the in-game help: "Five Scaffolds make a new Village Centre (which you'll need to create a new Village)" |
| Each tribe's buildings cost their own wood (houses 1000 to 4800, Tibetan dearest; civic 2000 to 4000; village centre 6000, Tibetan 12000; field 2000; dispenser 5500; football pitch 6000; wonder 24000) | todo | building with it is [construction.md](construction.md) |
| A script can fix which building a scaffold makes; such a scaffold offers only that one | todo | |

## Holding a scaffold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose scaffold can always be picked up | todo | See [../hand/picking_up.md](../hand/picking_up.md) |
| A scaffold on a building site can be picked up only before building starts and within 150 game turns of putting it down (a script's scaffold: any time before building starts) | todo | |
| Picking one up from a site cancels the site and its unbuilt building, and the town does not keep the plan | todo | |
| Held over land, a see-through ghost of the building it would make stands under the hand, seen only by the holder | todo | |
| The ghost turns slowly all the time (0.06 radians a turn, smoothed between turns) | todo | |
| When the building on offer changes, the old ghost shrinks away and the new one grows in, over four turns | todo | |
| The new ghost's sound is pitched by the kind of building; the old one has its own vanishing sound | todo | See [../hand/hand_sounds.md](../hand/hand_sounds.md) |
| Held over another scaffold it can join, the ghost shows the building the two would make there ("Combine Scaffolds") | todo | |
| Where no building can go there is no ghost, and a small puff can appear under the hand | todo | exactly when the puff shows is undetermined |
| The drop tooltip names the building on offer (Abode, Village Store, Crèche, Workshop, Wonder, Graveyard, Village Centre, Football Pitch, Miracle Dispenser, Field) or says "Build" | todo | See [../hand/pointing_and_tooltips.md](../hand/pointing_and_tooltips.md) |

## Putting a scaffold down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Put down gently with a building on offer, the scaffold becomes that building's site in the town | todo | |
| The building faces the way the ghost was turned when let go; if it doesn't fit that way, the way chosen with the plan | todo | the advisor: "When you're happy with its location and angle, click the Action Button." |
| A scaffold worth more than the building needs puts down only what it needs: the rest stays in the hand as a new scaffold | todo | |
| A planting sound plays: for the player who dropped it as a plain sound, for others at the scaffold | todo | |
| The town's own planned buildings under the new site are removed | todo | |
| The scaffold stays on the site, drawn at the building's scale and shrinking as the building rises until it is gone at two-thirds built | todo | the building's own partial look is in [construction.md](construction.md) |
| The ghost of the planned building stays drawn see-through over the site while it is built | todo | |
| Builders build it with wood as any site | todo | See [construction.md](construction.md) |
| Wood dropped on a scaffold that stands on a site goes to the site | todo | |
| When the building is finished, its scaffold is gone | todo | |
| If the unfinished building is destroyed, its scaffold at once makes a fresh site there | todo | See [damage_and_repair.md](damage_and_repair.md) |
| A scaffold put down where no building is on offer just lies there with a little puff | todo | |
| A thrown scaffold never builds: it flies, lands and lies there, with a little puff where it comes down | todo | See [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| A founding village centre first makes the new town: it gets the forests around it and the player's starting belief there | todo | See [../town/growth_and_housing.md](../town/growth_and_housing.md) |
| A scaffold a script marks to destroy what is under it pushes movable things out of the way and removes the rest when it is let go | todo | |

## Belief, reactions and alignment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Planting a scaffold makes the villagers around react to it (out to 60 m) | todo | See [../villager/reactions.md](../villager/reactions.md) |
| Every town within 300 m is impressed: a tenth of its people (at least one) gain belief | todo | See [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| How impressive it is grows with its worth and with the building it stands for | todo | |
| The reaction leans the player a little towards good, as caring for homes | todo | the reaction's alignment figures; see [../worship/](../worship/) |
| Any town takes a scaffold, believers or not: giving scaffolds to other villages impresses them | todo | the in-game help says so |
| The creature learns building from seeing the player drop a scaffold | todo | See [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |

## Joining and breaking scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Let go onto another scaffold (not held, not in flight), the two join when together they are worth at most 7 (8 with football), neither is a script's and the other's building hasn't started | todo | in the yard too; see [../town/football.md](../town/football.md) |
| The joined scaffold is the one on the ground, worth both, with the held one's building on offer; the held one is gone | todo | |
| The joined scaffold at once tries to become its building's site where it lies | todo | |
| Joining shows a puff, plays one of four sounds in turn at the hand and gives a force-feedback jolt | todo | See [../hand/hand_sounds.md](../hand/hand_sounds.md) |
| A scaffold worth more than one, at rest, not a script's and with no started building can be tapped ("Tap To Break") | todo | See [../hand/clicking_and_activating.md](../hand/clicking_and_activating.md) |
| A tap breaks one single scaffold off at a random quarter turn, the two moving apart; any site it stood for is cancelled; one of four sounds plays in turn at the hand | todo | |
| The piece broken off takes a place in its workshop's yard if there is room | todo | |
| A burning scaffold passes its fire to the piece broken off | todo | |
| Let go onto a storage pit, a scaffold is taken in as its wood (2500 for each scaffold it stands for) | todo | See [../resources/resource_handling.md](../resources/resource_handling.md) |
| A scaffold is not taken in by the workshop | todo | |

## Scaffolds in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose scaffold flies, rolls and lands as a movable object; one in the yard or on a site never does | todo | See [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| Blows don't hurt a scaffold | todo | See [../physics/impact_damage.md](../physics/impact_damage.md) |
| A scaffold catches fire (it lights at 200, heat capacity 400) and fire hurts it at a fifth of full strength | todo | what a burnt-out scaffold leaves is undetermined; see [damage_and_repair.md](damage_and_repair.md) |
| A loose scaffold standing in a vortex's pull is sucked in and carried to the next land like other loose objects, coming out as a scaffold of the same kind; one in a workshop's yard, on a building site or tied to a building plan is neither taken nor written down | todo | no vortex in openblack; See [../story/portals.md](../story/portals.md#what-goes-in-kind-by-kind) |
| A saved land writes each loose scaffold not owned by a workshop as a scaffold line (town, place, angle, size) | todo | See [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |

## The creature and other gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can pick up, carry, inspect and build with scaffolds, give them away and bring them home, but not attack or play with them | todo | See [building_by_creature.md](building_by_creature.md) and [../creature/object_actions.md](../creature/object_actions.md) |
| The creature steals scaffolds made for other gods' towns (never its own god's): it picks one up, walks off and puts it down | todo | See [../creature/town_actions.md](../creature/town_actions.md) |
| The hand can take another god's scaffold where it may pick things up | todo | See [../hand/picking_up.md](../hand/picking_up.md) |
| Rival gods fill their workshops, join scaffolds and place them in their towns | todo | See [../rival_gods/ai.md](../rival_gods/ai.md) and [../rival_gods/towns_and_influence.md](../rival_gods/towns_and_influence.md) |
| Villagers never carry scaffolds: the game leaves that empty | n/a | See [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md) |

## Scripts and rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land files place scaffolds (town, place, angle, size): the two-god playground has twelve on an Egyptian building site | todo | `CREATE_SCAFFOLD` is empty (`src/LHScriptX/FeatureScriptCommands.cpp`); see [../multiplayer/maps/two_gods.md](../multiplayer/maps/two_gods.md) |
| Challenge scripts make scaffolds and set their building, size and "destroys when placed" | todo | `SET_SCAFFOLD_PROPERTIES` logs "not implemented" (`src/CHLApi.cpp`); see [../scripts/challenge_natives_towns_and_players.md](../scripts/challenge_natives_towns_and_players.md) |
| A script can make a scaffold build at once where it stands ("enable … active") | todo | |
| Scripts can stop a scaffold being picked up or moved | todo | |
| Land 2: Khazar places a village centre scaffold (size 5) for the player, then a storage pit (3) and a house (2), and later a workshop (3) that clears its ground | todo | See [../rival_gods/khazar.md](../rival_gods/khazar.md) |
| Lands 3 and 5: the home town's storage pit and workshop are laid out as scaffolds and built on arrival | todo | See [../story/land_3.md](../story/land_3.md) and [../story/land_5.md](../story/land_5.md) |
| A reward chest opened in a town with a workshop gives a single scaffold, counted as that workshop's | todo | See [../story/rewards.md](../story/rewards.md) |

## Advisors, help and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 2's "The Workshop" scroll: an engineer explains the workshop, asks for wood, makes a scaffold, the advisors show where to place it, and he gives the forest miracle once the homeless are housed | todo | See [../story/land_2.md](../story/land_2.md); every step: [../story/gold_scrolls/the_workshop.md](../story/gold_scrolls/the_workshop.md) |
| Land 4's opening: "Excellent. The Workshop is built. Now let's concentrate on making a Village Store." "You'll need three combined Scaffolds to build it." | todo | See [../story/land_4.md](../story/land_4.md) |
| Did-you-know pages on the workshop, joining scaffolds and giving them to other villages | todo | See [../interface/help_system.md](../interface/help_system.md) |
| The sounds: the workshop at work, the ready horn, the ghost appearing and vanishing, planting, joining (four) and tapping (four) | todo | named in `src/Audio/Sound.h`; nothing plays them. See [../audio/sound_effects.md](../audio/sound_effects.md) |
