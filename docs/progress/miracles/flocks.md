# Flocks

Two flock miracles are swept out of the hand. The flying flock makes doves for a good god or bats for an evil one; they
fly off together, leave a trail and frighten creatures (bats). The ground flock makes a pack of wolves that run down a
corridor, hunting and eating villagers in their way. Wild animals and livestock are in [../animal/](../animal/).

**Progress: 40/50 done, 7 partial — 87%**

## Flying flock: casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Doves for a caster whose alignment is good enough, bats otherwise | done | `src/Magic/FlockMiracle.cpp` |
| Twelve birds times tribal power, rounded half to even | done | `src/Magic/FlockMiracleRules.cpp`; `test/test_flock_miracles.cpp` |
| Birds are made twelve a second along the hand's sweep, their height eased along it, each nudged a little at random | done | `FlockMiracle.cpp`, `FlockMiracleRules.cpp` |
| Birds that would start off the map, or outside the human caster's influence, are skipped but still counted | done | `FlockMiracle.cpp` |
| Birds keep being made until all are, even after the miracle is over | done | `FlockMiracle.cpp` |
| They fan out across the camera's heading (or the creature's throw), side chosen by which way the hand swept | done | `FlockMiracleRules.cpp` |
| Each flies for a point up to 800 m away in whole map cells, halving the distance down to 12.5 m until it is on the map | done | `FlockMiracleRules.cpp` (destination) |
| Birds fly at their kind's normal height; followers take the leader's goal | done | `FlockMiracle.cpp` |
| Every bird is the same size, 2.8 to 3.0 times the model, whatever its birth size | done | `FlockMiracleRules.cpp` (spawn scale) |
| A bird faces its goal from where it actually appeared | done | `FlockMiracle.cpp` |
| The local caster's hand leaves a trail of sparkles (good) or smoke (evil) until all birds are made | partial | `FlockMiracle.cpp`; the trail is moved to the hand once a turn instead of every frame |

## Flying flock: the birds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The leader wanders from wherever it is now, so the flock roams away from the cast point | done | `FlockMiracle.cpp` (flock centre follows the leader), `src/ECS/Systems/Implementations/AnimalSystem.cpp` |
| Each leg's height varies within the kind's limits | done | `AnimalSystem.cpp` |
| Random wander points avoid places the map's walls block | partial | `AnimalSystem.cpp`; every other check is there, but openblack has no map collision |
| Birds bank into their turns and flap | done | `AnimalSystem.cpp` (bank, flap clips) |
| Birds are lit by the brightest land light | done | `AnimalSystem.cpp`, renderer |
| A bird crossing a shield into it strikes the shield, sparks, and fades if the shield holds | done | `FlockMiracle.cpp` (shield test with the bird's radius) |
| When the miracle ends every bird fades over 20 turns, but stays opaque until it vanishes | done | `AnimalSystem.cpp`; scenario `miracles.flock_doves_vanish` |
| The miracle lasts 25 s for a player and costs 40 a turn while its birds or effect last | done | `src/Magic/SpellBehaviours.cpp`, effect tables |
| Bats frighten creatures | partial | `AnimalSystem.cpp` answers that bats are frightening; the creature's mind doesn't read it yet. See [../creature/](../creature/) |

## Flying flock: effects and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each bird trails pale sparkles (doves) or black smoke (bats), sized to the bird | done | `src/Particles/ParticleFlockRules.cpp` |
| A trail stays where its bird was when the bird goes, and fades with the effect | done | `ParticleFlockRules.cpp` |
| One coo (doves) or screech (bats) per cast, not one per bird | partial | `ParticleFlockRules.cpp` (one sound only); the sound-name fix lets the sounds start (logged), but hearing them with the camera close is still owed |

## Ground flock: casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Always wolves, whatever the caster's alignment | done | `FlockMiracle.cpp` |
| Fourteen wolves times tribal power, rounded half to even | done | `FlockMiracleRules.cpp` |
| Made twelve a second along the sweep on the ground, skipped off the map or outside influence | done | `FlockMiracle.cpp` |
| A puff of magic appears where each wolf appears | done | `FlockMiracle.cpp` (spot visual) |
| Every wolf is 1.5 to 2.0 times the model, whatever its birth size | done | `FlockMiracleRules.cpp` |
| Wolves are born grown up and hungry, owned by the caster | done | `AnimalSystem.cpp` |

## Ground flock: running and hunting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack runs down a 45 m-wide corridor to its goal at the wolves' run speed | done | `FlockMiracleRules.cpp` (corridor), `AnimalSystem.cpp` |
| Wolves reaching their goal fade away | done | `AnimalSystem.cpp` |
| A hungry wolf hunts villagers inside the corridor and not behind it | done | `AnimalSystem.cpp`, `FlockMiracleRules.cpp` |
| Once a wolf remembers prey, a new prey must be much closer to take its place | done | `AnimalSystem.cpp` |
| It chases, pounces when close and facing, and gives up when the prey gets too far | done | `AnimalSystem.cpp`; seen in game |
| Within a metre of the prey the pounce brings it down, again each turn it stays that close | done | `AnimalSystem.cpp` |
| The downed villager plays its attacked fall | partial | the fall's timing is kept, but villager clips aren't played yet. See [../villager/](../villager/) |
| The villager is eaten over 300 turns and then dies the normal villager death, killed by an animal | done | `AnimalSystem.cpp`; checked in game (dies about 31 s after being brought down) |
| The wolf takes 15 to 24 mouthfuls, then runs on | done | `AnimalSystem.cpp` |
| Things that can't be eaten are never hunted | todo | the uneatable flag isn't in openblack |
| A remembered prey that can no longer be reached is dropped | partial | `AnimalSystem.cpp`; the other validity checks are there, reachability isn't |

## Ground flock: look, sound and end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wolves leave a land-tinted dust trail | done | `ParticleFlockRules.cpp`, `assets/shaders/` (land light on sprites) |
| One howl per cast | partial | `ParticleFlockRules.cpp`; the sound-name fix lets it start, hearing it with the camera close is still owed |
| Wolves bank into their turns and play the right clip in each state | done | `AnimalSystem.cpp` |
| When the miracle ends the wolves turn translucent white and fade over 2 s while running on | done | `AnimalSystem.cpp`; scenario `miracles.flock_wolves_fade` |
| A burning wolf is drawn charred with flames | todo | fire doesn't reach animals yet. See [../physics/](../physics/) |
| The player can't pick up the miracle's wolves | done | `AnimalSystem.cpp` (pick-up from the info) |
| The miracle lasts 60 s for a player and costs 35 a turn | done | effect tables |

## Both flocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The leader is a miracle event each turn with no effect on anything | done | `FlockMiracle.cpp` |
| A running flock and its animals are kept in a saved game | todo | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
| A creature can cast either flock, throwing it the way it faces | done | `FlockMiracleRules.cpp` (creature direction) |
