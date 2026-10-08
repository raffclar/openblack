# Miracle effects and reactions

The machinery every miracle shares once it is cast: its life from creation to removal, the effect it applies to the
things it reaches, the deaths and damage it causes, how it moves alignment and belief, how people, animals and creatures
react to it, the challenge scripts' miracle commands, computer gods casting miracles, and saving active miracles.
Casting by hand is in [casting_and_globes.md](casting_and_globes.md), what a miracle costs in
[prayer_cost.md](prayer_cost.md), and creatures casting in [creature_spells.md](creature_spells.md).

**Progress: 68/97 done, 14 partial — 77%**

## A miracle's life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new miracle remembers who cast it and for which player; a miracle with no caster belongs to the neutral player | done | `src/ECS/Systems/Implementations/MagicSystem.cpp` (CastAtPoint) |
| Its strength, chants, duration and how many things it may make come from the cast | done | `MagicSystem.cpp`; `src/Magic/SpellRules.cpp` |
| A miracle cast from a seed lasts the seed's time for a player cast, times the seed's multiplier (one-off seeds too) | done | `src/Magic/SpellRules.cpp`; `test/test_magic_spells.cpp` |
| A miracle whose effect can't start is refused; one that has no effect (shield, teleport, flocks) still lives | done | `MagicSystem.cpp`; testbed `miracles.lifetime_without_effects` |
| A miracle cast on a thing starts at the thing and its effect follows it | done | `MagicSystem.cpp` (cast at an object) |
| Miracles are processed in a fixed order each turn: shields, seeds, then every miracle kept up, then every miracle run | done | `MagicSystem.cpp` (Update) |
| A miracle ages each turn and closes when it outlives its duration | done | `src/Magic/SpellRules.cpp` |
| When its caster is gone the miracle closes at once without paying for the turn | done | `MagicSystem.cpp`; fixed in fix-core |
| A miracle held in the hand is placed where the hand is, on the ground below it | done | `MagicSystem.cpp`; fixed in fix-core |
| A held miracle that leaves the ground where it may be cast is dropped from the hand | done | `src/ECS/Systems/Implementations/MagicHeldSeed.cpp`; land counts as a cell without water |
| A miracle whose power runs out closes; its effect finishes, then it is removed | done | `MagicSystem.cpp` (CoreProcess path) |
| The people's reactions to a miracle end as soon as its effect finishes, even for miracles kept on (shield, forest) | done | `MagicSystem.cpp`; fixed in fix-core |
| A seed goes with its miracle when the miracle is removed | done | `MagicSystem.cpp`; fixed in fix-core |
| The hand's stream of magic stops when a seed miracle the local player cast closes | done | `src/ECS/Systems/Implementations/MagicCreatureSpells.cpp` (StopHandGrain), now for every seed miracle |
| A miracle whose seed was left outside its player's influence closes | partial | Code path exists but is inert: only a seed from a worship-site icon can trigger it, and openblack has no spell icons (re-audit item 23 open) |
| Each miracle marks the map cell it stands in for the minimap, and the marks fade only while the minimap shows | partial | `src/Magic/SpellGrid.cpp` marks cells; fading always runs because openblack has no minimap |
| A miracle shows a blip on the minimap | todo | No minimap in openblack |
| The player's last cast (place, kind, time) and a count of each kind cast are kept | done | `src/ECS/Components/Player.h` (lastCast); fixed in fix-core |

## The effect on what it reaches

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each strike's effect is scaled by the chants it pays, the tribe's power and the strike's strength | done | `src/Magic/SpellBehaviours.cpp`, `src/Magic/AreaEffect.cpp` |
| An area strike reaches every object whose footprint overlaps its circle and whose height overlaps it, measured from the ground | done | `MagicSystem.cpp`, `src/Magic/AreaEffect.cpp`; point altitude fixed in fix-core |
| A strike that meets a shield is spent on the shield first (one more payment, a spark only if not absorbed) | done | `src/Magic/SpellBehaviours.cpp`; fixed in fix-core. See [physical_shield.md](physical_shield.md), [spiritual_shield.md](spiritual_shield.md) |
| The "can it destroy" question is answered after the shield test and falls through to a reaction with no target | done | `src/Magic/SpellBehaviours.cpp`; fixed in fix-core |
| A strike on one thing heals or harms it; healing cures poison | done | `src/Magic/SpellBehaviours.cpp`, `MagicSystem.cpp` |
| Healing passes through each thing's defence table, then damage, keeping life between none and full | done | `src/Magic/AreaEffect.cpp`, `src/Magic/SpellRules.cpp` |
| Burning strikes heat the thing's fire | done | `MagicSystem.cpp` into the fire system; see [fireball.md](fireball.md) |
| A strike pushes what it hits along the strike's movement | done | `src/Magic/SpellBehaviours.cpp` |
| The temple and its parts can't be destroyed by miracles | partial | openblack never gives the temple life, so strikes don't harm it, but there is no explicit rule (unconfirmed against the game's own rule) |

## Damage and kills

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal whose life reaches nothing from a miracle dies; a building is destroyed | done | `MagicSystem.cpp` → `src/ECS/WorldObjects.cpp` (DestroyedByEffect); testbed `miracles.miracle_kills` (verified in game) |
| A villager's life is kept finer than its shown health, so small strikes add up to a kill | done | `f79d0963` |
| Buildings, trees and features lose life to crushing and hitting strikes; fields take none | done | `MagicSystem.cpp` (ReceiverOf); fixed in fix-core |
| A creature struck down by a miracle faints, or is restored if it may not die | done | `src/ECS/Systems/Implementations/MagicLiving.cpp` |
| A creature's own miracle never harms it (only damaging effects are ignored) | done | `MagicLiving.cpp`; fixed in fix-core |
| In a fight, a creature's stamina takes the damage and healing, blocking softens it, and it faints at none | done | `MagicLiving.cpp`; fixed in fix-core |
| Outside fights a miracle leaves cuts and scars on a creature where it struck | partial | Damage applies, but placing the scar needs a mapping from a world point to the creature's skin (TODO in `MagicLiving.cpp`; re-audit item 10 open) |
| A creature caster counts the things its miracles destroy | done | `MagicSystem.cpp`; fixed in fix-core |
| The owner of a damaged thing keeps a tally of the damage each player did | done | `MagicSystem.cpp`; fixed in fix-core |
| A town remembers which player attacked it last | done | `MagicSystem.cpp`, `src/ECS/WorldObjects.cpp` |
| Something crushed starts a "struck" reaction from the casting creature (or itself) for its owner | done | `MagicSystem.cpp`; fixed in fix-core |
| A creature changes its opinion of a creature whose miracles hit it, at most every minute, even in fights and for its own | done | `MagicLiving.cpp`; fixed in fix-core |

## Alignment and belief from miracles

The player's alignment itself is in [../worship/](../worship/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Harming or healing a thing moves the caster's alignment by its kind (villager, building, plant, field, feature) and the life it changed | done | `src/Magic/AreaEffect.cpp`; weight fixed in fix-core |
| Burning counts by the harm its heat does | done | `src/Magic/AreaEffect.cpp`; fixed in fix-core |
| A change is softened the closer the alignment already is to that side | done | `src/Magic/AreaEffect.cpp` |
| Pending alignment moves the player by at most a small step each turn, and the rest is dropped | done | `src/Magic/AlignmentSystem.cpp`; `test/test_alignment.cpp`; fixed in fix-core |
| A creature caster's own alignment changes by its own step each creature turn | done | `MagicSystem.cpp`, `AlignmentSystem.cpp`; fixed in fix-core |
| A history of alignment deeds is kept for statistics | partial | Not kept: openblack has no statistics system (re-audit item 32 open) |
| A creature tribe's power for a miracle is its player's | done | `MagicSystem.cpp` |
| Tribal powers come from the worshipping tribes of each player | partial | Every tribe's power is 1.0 until worship exists; see [prayer_cost.md](prayer_cost.md) |

## People and animals reacting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle (on cast or on each strike, per its table) starts a reaction that spreads to the cells around it | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp`, `MagicSystem.cpp` |
| A reaction reaches further for a stronger miracle | done | `ReactionSystem.cpp` (Spread); fixed in fix-core |
| Reactions that grow start small and widen each turn | done | `ReactionSystem.cpp` |
| One reaction is re-spread each turn in turn | done | `ReactionSystem.cpp` |
| Villagers and creatures within reach decide by priority: kind of reaction, distance and species | done | `src/Magic/ReactionRules.cpp`, `VillagerReactionRules.cpp`; `test/test_reactions.cpp` |
| Villagers flee a nasty miracle, the closer the more urgently | done | `ReactionRules.cpp`; testbed `miracles.reactions_flee` |
| Villagers and creatures turn to look at a nice miracle | done | testbed `miracles.reactions_nice` |
| A creature never flees its own miracle | done | `ReactionRules.cpp` |
| Someone inside a shield doesn't react to a miracle outside it | done | `ReactionSystem.cpp`; `test/test_reactions.cpp` (ShieldRules) |
| A villager in a shield's reaction weighs other reactions against it like any other | partial | Villagers in a shield reaction still skip other reactions (TODO in `ReactionSystem.cpp`; re-audit item 15 open) |
| A reaction lasts its table's turns whatever the distance, and stops when what it reacts to goes | done | `ReactionSystem.cpp`; fixed in fix-core |
| A villager won't take the same reaction again too soon; the memory keeps three kinds | done | `VillagerReactionRules.cpp`; fixed in fix-core |
| A more urgent different reaction takes over only after ten seconds (one second after being picked up) | done | `ReactionRules.cpp`; `test/test_reactions.cpp` |
| Only villagers and creatures free to react take it up (not dead, scripted, in a fight …) | partial | States and the food exception are modelled; script-controlled, dance-editor and two unknown villager flags are TODO (`ReactionSystem.cpp`, `VillagerReactions.cpp`; re-audit items 16/33 open) |
| Villagers who can't reach the miracle, or are hiding, still gain belief without reacting | done | `ReactionSystem.cpp`; fixed in fix-core |
| Animals flee nasty miracles | todo | openblack animals take no reactions (re-audit item 17 open) |
| A reaction started by something picked up by the hand stops spreading while held | partial | Not reachable: the hand holds no objects yet (re-audit item 36 open) |
| A creature examines and learns from a nice miracle only if it is its own or an ally's, from a seed nobody has learnt from yet | done | `src/ECS/Systems/Implementations/CreatureMindReactions.cpp`; alliances and spell icons not in openblack |
| A creature runs from a nasty miracle, sometimes in fright, or goes to look at a nice one | done | `src/Creature/CreatureMiracleReactions.cpp`; `test/test_creature_miracle_reactions.cpp` |

## Impressiveness and belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| How much a miracle impresses depends on the land's balance, the reaction, the miracle, distance and how bored the watcher's town is | done | `src/Magic/Impressiveness.cpp`, `ReactionSystem.cpp` |
| A villager impressed adds belief in that player to its town, less in a bigger town | done | `ReactionSystem.cpp`, `VillagerReactions.cpp` |
| A town takes in its pending belief at its own turn, scaled by the town | done | `test/test_town_belief.cpp` |
| Each impression bores the town a little with that kind of miracle; the boredom wears off each town turn, faster on a lost land | done | `ReactionSystem.cpp`; `SET_LOST_TOWN_SCALE` in `src/LHScriptX/FeatureScriptCommands.cpp`; fixed in fix-core |
| Each reaction also moves the player's alignment (fleeing a little evil, admiring a little good), weighted by the town's desire | done | `ReactionSystem.cpp`, `src/Magic/Impressiveness.cpp`; fixed in fix-core |
| A creature watching its own player's miracle is impressed by it | done | `ReactionSystem.cpp`; fixed in fix-core |
| Belief symbols rise over impressed villagers | partial | Not seen in the audit runs; frame series still owed (`miracles.reactions_nice`) |

## Creatures watching the player cast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature, if it sees the point, feels the desires a miracle answers | partial | `MagicSystem.cpp`, `src/Creature/PerceivedDesires.cpp`; the live feeling works, but its 30-sample history needs a save system (re-audit item 18 open) |
| A creature copies a deed it sees the player do with a miracle (water on crops, feeding, attacking another's town …) | partial | Classifier and mimic are in (`src/Magic/SpellBehaviours.cpp` → creature mind); worship-site and building-site deeds aren't modelled and the mimic's own gates differ (re-audit item 19 open) |
| A creature that sees a miracle counts it towards learning it | done | See [creature_spells.md](creature_spells.md) |

## Challenge scripts and miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can cast a miracle at a point, from a point, with radius, duration and curl | done | `src/CHLApi.cpp` (SPELL_AT_POS), `src/Magic/ScriptCast.cpp` |
| A script can cast a miracle on a thing, on it or at its place by the miracle's rule | done | `src/CHLApi.cpp` (SPELL_AT_THING) |
| A script can find a miracle already at a point (or a shield over it) | done | `src/CHLApi.cpp` (SPELL_AT_POINT); fixed in fix-core |
| A script miracle with an unknown kind gives nothing back | partial | openblack hands back 0 instead of nothing, deliberately, to keep the script stack safe |
| A script miracle makes no cast sound | done | `src/Particles/ParticleCreatureSpellRules.cpp` |
| A script can stop spells on a creature wearing off | done | `src/CHLApi.cpp` (CREATURE_SPELL_REVERSION); fixed in fix-core |
| The land's balance set by script feeds how impressive miracles are | done | `src/LHScriptX/FeatureScriptCommands.cpp` |
| A script can ask a player's last cast miracle, where and when | todo | `PLAYER_SPELL_LAST_CAST`, `GET_LAST_SPELL_CAST_POS` are stubs in `src/CHLApi.cpp` (the record itself is kept) |
| A script can give or take a miracle from a player and ask whether the player has it | todo | `SET_PLAYER_MAGIC`, `HAS_PLAYER_MAGIC` are stubs |
| A script can ask whether a thing is under a miracle | todo | `IS_AFFECTED_BY_SPELL` is a stub |
| A script can ask whether wind magic (storm or tornado) is at a place | todo | `IS_WIND_MAGIC_AT_POS` is a stub |
| A script can put a miracle into an object or set a miracle's properties on it | todo | `SET_MAGIC_IN_OBJECT`, `SET_MAGIC_PROPERTIES` are stubs |
| A script can ask the prayer power a miracle needs and whether a miracle is charging | todo | `GET_MANA_FOR_SPELL`, `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING`, `CLEAR_PLAYER_SPELL_CHARGING` are stubs; charging belongs with worship |

## Computer gods casting miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rival god casts miracles to attack towns and creatures, choosing the best aggressive one it has | todo | No computer players in openblack |
| A rival god casts miracles to impress towns and helps its own (food, wood, water, heal into its store and building sites) | todo | No computer players |
| A rival god shields its towns and things against attacks | todo | No computer players |
| A rival god charges its worship sites' miracles and checks it has the prayer power | todo | No computer players or worship sites |
| A rival god teaches its creature miracles and casts on it for a laugh | todo | No computer players |
| A rival god's miracles last its own time | partial | The time is read from the miracle tables (`src/Magic/MagicTables.cpp`) but nothing casts as a computer player |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Active miracles, their reactions and their seeds are saved and loaded with the game | todo | openblack has no game save system; the fields are listed in the core audit |
| Reactions in progress are saved and loaded | todo | No game save system |
