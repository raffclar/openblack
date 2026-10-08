# Town belief and conversion

Every town believes a little in each player. Impressive deeds near it raise its belief in their doer; once belief in a
player passes what the town needs, the town is won over and becomes that player's, its influence and people with it.
A town can be lost again if belief in its owner falls behind another's.

**Progress: 7/25 done, 4 partial — 36%**

## Belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town keeps its belief in every player, starting with belief in the neutral player from the tables | done | `src/Magic/TownBelief.h`, `src/ECS/Components/MiracleImpression.h`; test `test_town_belief.cpp` |
| What its people are impressed by waits until the town's turn and is believed all at once, times the town's belief scale | done | `src/Magic/TownBelief.cpp`, `src/ECS/Systems/Implementations/ReactionSystem.cpp` |
| Belief in a player is capped, and scripts can set the cap | partial | the cap is kept (`TownBelief.h`) but SET_TOWN_BELIEF_CAP is not implemented |
| Scripts set a town's belief in a player | partial | SET_TOWN_BELIEF writes a separate map on the town (`FeatureScriptCommands.cpp`) that the belief rules don't read |
| A symbol of belief gained rises from the town centre in the player's colour | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp` |
| Towns grow bored of the same kind of deed seen again and again | done | `ReactionSystem.cpp` |
| Recent belief given fades every turn | done | `TownBelief.cpp` |
| Belief from miracles, from the creature, from artefacts, from scaffolds and buildings given, from disciples and missionaries | partial | miracles and the creature's deeds (`src/Magic/Impressiveness.cpp`); the rest todo |
| Taking a town's resources lowers its belief for a while | todo | |
| A town believing in a player moves faster as the land's belief speed scale says | done | `src/ECS/VillagerSpeed.cpp` |
| How impressed a town is shows over it (the belief bar or tooltip) | todo | See ../interface/ |
| Every ten turns the believers count for the tooltip | todo | |

## Conversion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each town needs a set amount of belief to be won over, by its size | todo | |
| When belief in a player passes what is needed, the town becomes that player's | todo | the owner is only set at creation |
| A won town celebrates, with fireworks and its villagers cheering | todo | at the town centre the game starts the town fireworks and a fountain of the new owner's symbols (not for the neutral player); the local player hears a sound when the town becomes theirs |
| A won town joins its new owner's worship site and influence, and its spells go to the player | todo | See ../worship/ |
| The previous owner loses the town and its influence | todo | |
| Losing a town lowers belief elsewhere by the lost-town scale | todo | SET_LOST_TOWN_SCALE is not implemented |
| Claiming a town multiplies its belief by the claimed-town multiplier | todo | |
| A town's attitude to each creature changes how its villagers react to it | todo | |
| The take-town cheat gives the player a town at once | todo | |
| Scripts ask who owns a town and how much it believes | partial | See ../story/ |

## Influence multipliers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts scale every town's influence | done | SET_TOWN_INFLUENCE_MULTIPLIER (`FeatureScriptCommands.cpp`, read by `InfluenceSystem`); see ../worship/ |
| Scripts scale one town's influence | todo | SET_A_TOWNS_INFLUENCE_MULTIPLIER is not implemented |
| Scripts set the town's belief balance scale | todo | SET_TOWN_BALANCE_BELIEF_SCALE is not implemented |
