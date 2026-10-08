# Teleport

Each teleport cast leaves a stone with a swirling pool on the ground. A player's stones form a network: villagers and
the creature walking past one jump to the stone that leaves them nearest their goal when it saves enough walking, and a
villager dropped on a stone jumps at once. There is no camera or hand travel through stones.

**Progress: 21/29 done, 5 partial — 81%**

## The stone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each cast makes one stone at the cast point, owned by the caster; there is no power-up | done | `src/Magic/SpellBehaviours.cpp`, `src/ECS/Systems/Implementations/TeleportSystem.cpp` |
| A stone can't be cast within 6 m of buildings, fields, features, rocks, dead trees, forests, the temple, miracle dispensers or other stones | partial | `TeleportSystem.cpp`, `src/Magic/TeleportRules.cpp`; worship sites, totems, football pitches and fish farms are not counted (openblack has none yet) |
| The pool is a land-textured disc in the owner's colour at the middle, fading at the rim and slowly turning | done | `src/Particles/ParticleSurfaceRules.cpp`; checked in game (`miracles.teleport_two_points`) |
| A humming loop plays at each stone while it stands | done | particle sound of the pool; logged in game |
| The hum fades out softly when the stone goes | partial | let-go path exists (`ParticleSystem.cpp`); not yet heard in game |
| In the hand the seed is a spinning sparkle in the player's colour | done | generic in-hand effect; see [casting_and_globes.md](casting_and_globes.md) |
| The stone lives while its prayer power lasts (one a turn), and costs to create as the game's tables say | done | `src/ECS/Systems/Implementations/MagicSystem.cpp`, effect tables |
| When the miracle ends the stone, its pool and its reaction go | done | `TeleportSystem.cpp` |
| New buildings can't be placed on a stone | todo | the rule exists but nothing places new buildings yet |
| A stone can't be burned, picked up, thrown or used by the creature | done | stones are left out of burning and miracle targets (`FireSystem.cpp`, `src/ECS/WorldObjects.cpp`) |

## Who travels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers and the creature of the stone's player walking within reach of a stone may react; animals never | done | `TeleportSystem.cpp` |
| They turn aside only when going via the stones beats the walk by enough (walk more than 1.2 times the stone route) | done | `TeleportRules.cpp`; `test/test_teleport_rules.cpp` |
| Taking up the stone competes with any other reaction they hold, by the reaction table's priority | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp`, `TeleportSystem.cpp` |
| A villager heading off to decide what to do never turns aside | done | follows from the reaction contest |
| Slow villagers walk to the stone, fast ones run, by the game's speed threshold | done | `TeleportSystem.cpp` |
| A villager re-checks each turn whether it has reached the stone | done | `src/ECS/Systems/Implementations/VillagerTeleport.cpp` |
| Walkers stop being travellers once they leave, give up or jump | done | `TeleportSystem.cpp` (traveller pruning) |
| Leaving the stone's reaction also takes a villager off its town's list of those on the way to worship | todo | needs worship |

## The jump

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The traveller comes out of the player's stone that leaves it nearest its goal; newer stones win ties | done | `TeleportRules.cpp` |
| Distances are measured with the game's own approximate distance | done | shared game distance (`src/Common/GUtilsDistance.cpp`) |
| Yellow sparkles appear where the traveller leaves and at the stone it arrives at | done | `TeleportSystem.cpp` (spot visuals); seen in game |
| A villager is moved at once, with a "go" sound where it leaves and an "arrive" sound where it lands | partial | `TeleportSystem.cpp`; both sounds are started but have not been confirmed audible in game |
| Each jump gives back prayer power for the walking saved (less with tribal power) and tops the miracle up from the store | done | `TeleportRules.cpp` (jump cost), `MagicSystem.cpp` (forced payment) |
| The creature walks to within 5 m of the stone, fades out over 20 turns, moves, plays the arrive sound and fades back in, then walks on to its goal | done | `TeleportSystem.cpp`; checked in game (`miracles.teleport_creature`) |
| Each jump is a miracle event with no damage or alignment effect | done | generic miracle effect |

## Hand and worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager dropped on a stone of the dropping player, who owns more than one stone, decides where it is going and jumps at once | partial | `TeleportSystem.cpp` (drop on stone); the hand now picks villagers up and lets them go (`HandGrabSystem`), but letting one go onto a stone doesn't apply it to the stone yet. See [../hand/](../hand/) |
| Worshippers going to a far worship site are routed through the stones | partial | the stone part works in game (`miracles.teleport_worship`); worship sites and worshipping are missing. See [../worship/](../worship/) |
| A creature casting teleport gets a shorter-lived stone than the player's | done | effect timers per caster |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stones and their travellers are kept in a saved game | todo | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
