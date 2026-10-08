# Statistics: what is counted

Every figure the game counts for its statistics: when each is counted, for whom, how the Game Statistics box shows it,
and what is kept in saved games and the profile. The box itself (tabs, list, graphs, controls) is in
[statistics.md](statistics.md). Rows follow the box's tabs in the order the list shows them.

**Progress: 0/73 done, 1 partial — 1%**

## How the figures are kept

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player has a statistics record of their own, made with the player | todo | openblack keeps no statistics; the temple's scrolls use made-up figures (`src/3D/TempleScrolls.cpp`) |
| Clearing the map (a new land or a new game) clears every player's counters | todo | see [../story/portals.md](../story/portals.md) for the land change |
| The influence and population histories are not cleared with the counters: they run on from land to land | todo | only the record's making clears them |
| At the start of a land the game notes each player's alignment, their creature's alignment, the real time and the number of towns | todo | the real time is saved but never shown; perhaps the online record's time played (unconfirmed) ([../pc_integration/real_clock.md](../pc_integration/real_clock.md)) |
| Town totals (population, homes, towns and buildings) are summed from the player's towns at most once a game turn, when a figure needs them | todo | |
| The global figures (lines of code, frame rates, objects created) are updated every 10 game turns, every 5 in one game mode | todo | (unconfirmed which mode halves it) |
| Everything is kept in saved games: the counters, both histories, the town totals and the global figures | todo | see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The hand and camera figures on Tech Stats are the player's help record, kept in their profile across every game | todo | see [help_system.md](help_system.md) and [profiles.md](profiles.md) |
| In a network game each machine counts every player's figures itself; nothing extra is sent | n/a | see [../multiplayer/network_play.md](../multiplayer/network_play.md) |

## The influence and population histories

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every game turn the player's influence is added to the influence history: their citadel's, each of their towns' and each influence ring they own | todo | |
| Every game turn the player's population (adults and children in all their towns) is added to the population history | todo | |
| Each history point is the average of a run of turns: 50 at first | todo | |
| When 500 points are full, neighbouring pairs are averaged into 250 and each later point covers twice as many turns | todo | so a history always spans the whole game |

## Player Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Final Area Of Influence": the player's influence now, as for the history; a whole number | todo | see [../worship/influence.md](../worship/influence.md) |
| "Number Of Gods Defeated": when a god loses, each human god still playing counts one, if the loser was human or one game-wide setting is on; shown out of the number of gods less one | todo | (unconfirmed what the setting is) see [../multiplayer/multiplayer_rules.md](../multiplayer/multiplayer_rules.md) |
| "Alignment Change": the player's alignment now less their alignment at the land's start, two decimals, always a number | todo | see [../worship/alignment.md](../worship/alignment.md) |
| "Total People Killed": villagers whose death is put down to this player | todo | counted with the town's death, below |
| "Final Total Belief": the belief in the player summed over every town in the world, times 1000; a whole number | todo | see [../worship/belief.md](../worship/belief.md) |
| "Max Total Belief" and "Min Total Belief": every 10 game turns the summed belief is checked; a new highest skips the lowest test, and the lowest starts at the largest number | todo | |
| "Number Of Artifacts": the artifacts given to towns that belong to the player, counted over every town now | todo | see [../town/artefacts.md](../town/artefacts.md) |
| "Creature Growth": the creature's size now over its size when first noted, less one, as a percentage with two decimals | todo | the size is noted the first time the creature is processed after the counters are cleared; see [../creature/growth_and_size.md](../creature/growth_and_size.md) |
| "Creature Alignment Change": the creature's alignment now less at the land's start, two decimals, always a number | todo | |
| "Buildings Destroyed": a building the player puts out of action counts when its state is 200 or more | todo | (unconfirmed what that state means) |

## Creature Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Damage", "Hunger" and "Tiredness": the creature's damage, hunger and tiredness now, as percentages with two decimals | todo | the same figures as the status panel ([../creature/creature_mode.md](../creature/creature_mode.md)) |
| "Age:": the creature's age now, a whole number | todo | see [../creature/development_phases.md](../creature/development_phases.md) |
| "Alignment:": the creature's alignment now, two decimals, its bar out of 1 | todo | |
| "Animals Killed:" | todo | counted by the creature ([../creature/fighting.md](../creature/fighting.md)) |
| "Battles Fought:" and "Battles Won:" | todo | counted by the creature's fights ([../creature/fighting.md](../creature/fighting.md)) |
| "Poos Done:" and "Mushrooms Eaten:" | todo | counted by the creature ([../creature/physiology.md](../creature/physiology.md)) |
| A player without a creature shows 0 for every creature figure | todo | |
| The creature's people killed and creatures defeated are counted and sent online, but the box never lists them | todo | the labels "People Killed:" and "Creatures Defeated:" exist unused |

## Town Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Total Births": a child born in one of the player's towns | todo | see [../villager/life_cycle.md](../villager/life_cycle.md) |
| "Total Deaths": a villager dying in one of the player's towns; the player held to blame gets one "Total People Killed" | todo | see [../villager/death.md](../villager/death.md) |
| "Final Total Population", "Final Male Population" and "Final Female Population": the people in the player's towns now; men and women shown out of the total | todo | |
| "Final Total Population Capacity": the places in the player's homes now | todo | |
| "Maximum Total/Male/Female Population": checked every game turn; men and women shown out of the highest total | todo | |
| "Minimum Total/Male/Female Population": a turn that sets a new highest skips the lowest test, and the lowest starts at the largest number; men and women out of the lowest total | todo | |
| "Total People Converted": when the player takes over a town, its adults and children are added | todo | see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| "Average Satisfaction Of Villagers": each time a town weighs its desires, one less its average desire is added; shown as the mean of all these, two decimals, its bar out of 1 | todo | see [../town/town_desires.md](../town/town_desires.md) |
| "Towns Owned": the player's towns now | todo | |
| "Total Buildings": over the player's towns a running count of homes and civic buildings is added town by town, so with more than one town the earlier towns count again | todo | as the game does |
| "Total Abodes", "Total Civic Buildings" and "Total Wonders": the player's homes, civic buildings and wonders now | todo | |
| "Total Buildings Built": a home finished in one of the player's towns; also "Total Abodes Built", "Total Civic Buildings Built" or "Total Wonders Built" by its kind | todo | |
| "Number Of Disciple Farmers, Foresters, Fishermen, Builders, Breeders, Traders, Missionaries, Craftsmen": a villager dropped and made a disciple of that kind | todo | protection and worship disciples aren't counted; see [../villager/disciples.md](../villager/disciples.md) |
| "Total Food Eaten": food a town uses | todo | |
| "Wood Used": wood a town uses | todo | |

## Miracle Stats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle the player casts counts once for its kind and power, when it is made | todo | see [../miracles/miracle_core.md](../miracles/miracle_core.md) |
| "Total Aggressive Miracles Cast": the fireballs, lightnings and explosions of every power, the two storms, the tornado and the ground flock | todo | |
| "Total Nice Miracles Cast": the two heals, teleport, forest, the two foods, the two shields, wood, the two waters and the flying flock | todo | |
| "Total Creature Miracles Cast": every creature miracle | todo | |
| "Number Of Fireballs / Bigger Fireballs / Biggest Fireballs Cast", and the same for Lightnings and Explosions | todo | |
| "Number Of Heals / Bigger Heals Cast", "Number Of Teleport Cast", "Number Of Miracle Forests Cast", "Number Of Miracle Foods / Bigger Miracle Foods Cast" | todo | |
| "Number Of Storms / Bigger Storms Cast", "Number Of Tornados Cast", "Number Of Spiritual / Physical Shields Cast" | todo | |
| "Number Of Miracle Woods Cast", "Number Of Miracle Waters / Bigger Miracle Waters Cast", "Number Of Flying Flock Cast", "Number Of Ground Flocks Cast" | todo | |
| "Number Of Creature Freeze, Small, Big, Weak, Strong, Fat, Thin, Invisible, Compassion, Angry Miracles Cast" | todo | |
| "Number Of Creature Itchy Miracles Cast" | todo | |
| The Hungry, Frightened, Tired, Ill and Thirsty creature miracles are neither counted nor listed, though their labels exist | todo | |
| "Total Chants Used": the chants a worship site spends for the player, one decimal | todo | see [../worship/worship_sites.md](../worship/worship_sites.md) and [../worship/prayer_power.md](../worship/prayer_power.md) |
| "Total Number Of Sacrifices": a villager or an animal sacrificed for the player | todo | |

## Tech Stats

These figures are shown as one value, in the Total column only.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Total Lines Of Code Executed" is invented: each update adds the land's town count times 10,130, plus a random fifth of that again | todo | |
| "Max Frame Rate": each update the frame rate is compared with it, one decimal | partial | openblack measures its frame rate for its own frame statistics ([../debug/diagnostics.md](../debug/diagnostics.md)), not for this |
| "Min Frame Rate" always reads 0: it starts at 0 and is only tested when no new highest is set | todo | as the game does |
| "Total Objects Created": every game object made since the counters were cleared | todo | |
| "Time Played": game turns so far over 10, a whole number of seconds of game time | todo | |
| "Script VM Instructions": the script engine's count of instructions run | todo | see [../scripts/](../scripts/) |
| "Total Number Of Statistics" always reads 113 | todo | |
| "Distance Dragged": the help record's dragging figure times 3 | todo | from the profile's help record, see [help_system.md](help_system.md) |
| "Time Looking At Sky" and "Time Looking At Ground" | todo | from the help record |
| "Gestures Done" | todo | from the help record; see [../gesture/miracle_gestures.md](../gesture/miracle_gestures.md) |
| "Zoom Count" (the help record's figure times 9), "Rotate Count" and "Pitch Count" | todo | from the help record; see [../camera/](../camera/) |
| "Total Interface Actions", "Total Objects Thrown" and "Total Number Of Things Picked Up" | todo | from the help record; see [../hand/](../hand/) |
| "Number Of Fight Attacks", "Number Of Fight Blocks" and "Number Of Fight Steps" | todo | from the help record; see [../creature/fighting.md](../creature/fighting.md) |
| "Total Polygons Drawn" has a label but is neither counted nor listed | n/a | nothing to do |

## Counted but not shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When a human god loses they get the next finishing place; when one god is left, it gets the place after | todo | sent online only |
| The number of players who have left the game | todo | sent online only |
| At the end of an online game one record goes to Lionhead's database: the game's version and map, the places, the players' accounts and clan, and every figure above by name | n/a | the server is gone; see [../pc_integration/online_services.md](../pc_integration/online_services.md) and [../multiplayer/clans.md](../multiplayer/clans.md) |
| If the database can't be reached the player is told "Could not connect to database. Your creature and/or statistics will not be updated." | n/a | the server is gone |
