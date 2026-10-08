# Tornado

The tornado is the storm's second power-up: a storm whose centre grows a spinning funnel that wanders over the land,
sucks up trees, people, animals and loose objects, carries them round and flings them away. The storm's clouds, rain
and in-cloud lightning are described in [storm.md](storm.md).

**Progress: 27/33 done, 2 partial — 85%**

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tornado comes with the storm's clouds and rain; its in-cloud lightning does not set fires | done | `src/Particles/ParticleStormRules.cpp`, effect tables from the game's info |
| Upkeep each turn grows with the square of the radius, and each object picked up costs prayer power | done | `src/Particles/StormMaths.cpp`, `src/Magic/SpellBehaviours.cpp` |
| The funnel's foot pushes and hurts what it passes over a little, every step | done | generic miracle effect (`SpellBehaviours.cpp`) |

## The funnel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The funnel fades in over its fade-in time and fades out after the storm ends | done | `src/Particles/ParticleTornadoRules.cpp`, `src/Particles/TornadoMaths.cpp` |
| The foot wanders on a smooth noise path; the top follows the storm once the delay before moving has passed | done | `ParticleTornadoRules.cpp`, `TornadoMaths.cpp`; `test/test_tornado.cpp` |
| The foot keeps to the land's height, the top stands a size-scaled height above it | done | `ParticleTornadoRules.cpp` |
| The funnel bends between foot and top and wiggles with height, the wiggle following the bend | done | `TornadoMaths.cpp` |
| Two translucent spinning shells, tinted by the land, make up the funnel | partial | creators from the effect file exist; their drawing has not been checked against the game in the renderer |
| Dust is thrown up from the ground at the foot, coloured by the land's material, snow-coloured on snowy ground, at most 50 pieces | done | `src/ECS/Systems/Implementations/ParticleSystemTornado.cpp` |
| Make-believe bushes and chickens are tossed up only over dry land | done | `ParticleTornadoRules.cpp`, `ParticleSystemTornado.cpp` (dry land: more than 3 m up) |
| A roaring loop starts with the funnel and fades out softly when it ends | done | `ParticleTornadoRules.cpp`, `src/ECS/Systems/Implementations/ParticleSystem.cpp`; heard in game (`miracles.tornado_village`) |

## Sucking things up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing is picked up until the funnel has faded in; then at most one thing every three turns | done | `ParticleTornadoRules.cpp`; checked in game (first pickup 8.2 s, then every 0.3 s) |
| The reach grows with the funnel's size and with tribal power, up to five times | done | `TornadoMaths.cpp` |
| The search goes outward cell by cell from the foot, fixed things before moving things, and the first thing taken ends it | done | `src/ECS/Systems/Implementations/TornadoSystem.cpp` (cell spiral) |
| Trees, villagers (dead ones too, but not those hidden in buildings), animals, rocks and other loose things, and small pots can be lifted; buildings never | done | `TornadoSystem.cpp`; scenario `miracles.tornado_animals` |
| Only things small enough for the funnel are lifted | done | `TornadoMaths.cpp` (fit test), `TornadoSystem.cpp` (object radius) |
| A food or wood pile too big to lift gives up a pot of 150 to 650 by the funnel's size; a pile dropped from the hand is lifted whole | done | `TornadoSystem.cpp`, `ParticleTornadoRules.cpp`; scenario `miracles.tornado_hand_pile` |
| Storage pits are never emptied | done | `TornadoSystem.cpp` (only piles give pots) |
| Only things the miracle can destroy are taken | done | `src/ECS/WorldObjects.cpp`, `MagicSystem.cpp` |
| Field crops are lifted too | todo | openblack has no field-crop objects |
| Things a script marks as immovable stay put | todo | script object flags are not in openblack |

## Carrying and flinging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is caught spirals up around the funnel, pulled round the bend and wiggle | done | `ParticleTornadoRules.cpp`, `TornadoMaths.cpp` |
| Things near the top are flung out; anything still carried is let go 15 s after it was caught, wherever it is | done | `ParticleTornadoRules.cpp`; checked in game |
| Carried things follow their place on the funnel smoothly between turns | done | `TornadoSystem.cpp` |
| Villagers let go die where they land, killed by the caster's miracle | done | `TornadoSystem.cpp` (spell death) |
| Trees and pots let go vanish | done | `TornadoSystem.cpp` |
| Something no longer there when let go is simply dropped | done | `TornadoSystem.cpp` |
| Animals let go play their dying clip, lie dead and are removed later | partial | `src/ECS/Systems/Implementations/AnimalSystem.cpp`; missing: the grey puff of smoke when the body goes, the dying clips of goats, zebras and other plain animals, birds falling as physical objects. See [../animal/](../animal/) |
| When the storm ends, what is still carried is flung out | done | `ParticleTornadoRules.cpp` |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature in reach is never lifted; it faints once, the leash lets go, and it lies a while before getting up | done | `TornadoSystem.cpp`, `src/ECS/Systems/Implementations/CreatureFightSystem.cpp`; `test/test_tornado_creature.cpp`; scenario `miracles.tornado_creature` |
| After the faint it carries on from where it lay, waiting for spells on it to end | done | checked in game (back home and standing) |
| The player is told by the creature help that the creature has fainted | todo | no creature help messages in openblack |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A running tornado and what it carries are kept in a saved game | todo | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
