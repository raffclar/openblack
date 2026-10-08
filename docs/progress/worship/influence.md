# Influence

The area where a player's hand can act and their miracles can be cast: round their temple and round each town that
believes in them, shown by a glowing border on the land. Khazar teaches it on the second land:
[Impress Village](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence).

**Progress: 15/22 done, 2 partial — 73%**

## How far it reaches

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's reach is set per land of the story, fixed the first time, times the land's player multiplier | done | `src/ECS/Systems/Implementations/InfluenceSystem.cpp` |
| A town's reach is a base for the land plus each building's share by its size and its people, times the land's multiplier | done | same |
| Only a player's own towns give them influence; the neutral player has none | done | same |
| Influence is full near the middle and falls away to nothing at the edge | done | test `test_influence_circle` (InfluenceFallsWithDistance) |
| A town's reach grows as it builds and fills, and shrinks as it loses buildings | partial | worked out each turn from its buildings, but towns don't grow yet (see `../town/`) |
| Land scripts set influence multipliers for one town, all towns and the players | done | `src/LHScriptX/FeatureScriptCommands.cpp`, kept in the map's script globals |
| Under another player's shield a player has no influence | done | `InfluenceSystem::PlayerInfluence` |
| Scripts make influence rings round places and objects, which follow the objects they are on | todo | `CreateInfluenceRing` and the challenge script's influence functions are stubs |
| Anti-influence rings shut a player out of an area | todo | |
| The most influential player at a place decides the land's alignment there | partial | crops and trees sum each player's influence times alignment (`VegetationSystem.cpp`); the sky still takes player one's (`AlignmentSystem::UpdateTurn`) |
| Scripts read a player's influence at a place | todo | stub in `src/CHLApi.cpp` |
| Prayer power can buy influence elsewhere (virtual influence; unconfirmed what it costs) | todo | reaching past the border draws motes of light from it to the hand, see [../rendering/light_beams.md](../rendering/light_beams.md) |
| Allies share influence | n/a | multiplayer only, see `../multiplayer/` |

## The border

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A curtain of light round the temple and each town, in the player's colour, following the land | done | `InfluenceSystem`, test `test_influence_circle` |
| It is drawn again every ten turns, and only once a reach has moved | done | same |
| Circles inside others and the insides of overlapping ones are hidden | done | same |
| It shows only from high enough above the land | done | same |
| Its texture scrolls with game time | done | same |
| A player's border shows once their temple stands | done | same |
| The hand crossing a border sends out a ripple and makes a sound | done | same |

## What it allows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Miracles land only in the caster's influence; a press outside does nothing | done | `src/Magic/CastInput.cpp`, fixed after the casting audit |
| Shaking a miracle off the hand needs influence (except while powering up) | done | gesture requests, fixed after the casting audit |
| The hand may pick up and use things only inside influence | todo | the hand can't hold objects yet, see `../hand/` |
