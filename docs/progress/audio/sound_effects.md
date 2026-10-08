# Sound effects

The game's sound effects: the hand, miracles, the temple, buildings, trees, villagers, animals and the creature, each
from the game's sound banks and most placed in the world.

**Progress: 17/32 done, 3 partial — 58%**

## How effects play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An effect plays with its bank's volume, pitch, random pitch change, loop count and distances | done | `AudioManager::PlaySoundEffect` |
| Animation effects pick a sound by keys (action, size, surface, alignment …) at random | done | `src/Audio/AnimEffectTable.cpp`; test `test_anim_effects` |
| A sound marked to play once doesn't start again while its owner or its voice group plays it | done | `AudioManager::PlayAnimEffect` |
| Loops follow their owner and stop with it | done | `AudioManager::SetEmitterPosition`, `StopOwnedSounds` |
| Particle effects start, fade and let go of their sounds | done | `src/Particles/ParticleSounds.cpp`, `ParticleSoundRelease.cpp` |
| Sounds on objects (sound tags) restart while the camera is in reach | done | `SoundTagSystem` |
| A sound at a distant point arrives late, at the speed of sound | done | `SoundTagSystem::CreatePointSound` |
| A sound effects volume setting | done | `src/Gui/GameMenu.cpp` |
| Scripts play, stop and ask about sounds and attach them to objects | todo | stubs in `src/CHLApi.cpp` |

## By kind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand: gripping the land, picking up, dipping in water, passing through influence | partial | grip, water and influence sounds play (`Game.cpp`, `InfluenceSystem`); picking up, food and wood todo |
| Gestures and casting: recognised, failed, power up bands, bubbles popping | done | `MagicSystem`, `MiracleFxSystem` |
| Miracles: fireball, lightning, shields, forests growing, teleport, water | partial | the spells bank by particle rules (`ParticleSystem`); see ../miracles/ |
| Fire crackling and steam | done | `FireSystem` |
| Thunder claps with lightning | done | `src/3D/Lightning.cpp`; see ../weather/storms.md |
| The camera's whoosh when moving fast | done | `DefaultWorldCameraModel`, `TempleCameraModel` |
| The temple: doors, buttons, scrolls squeaking, the cave's waterfall and fire | done | see ../temple/ |
| The leash: attaching and clicking | done | `LeashSystem` |
| Lanterns crackling | done | `StreetLanternArchetype`, `SoundTagSystem` |
| Trees rustling, bending, falling, breaking and being turned to mulch | partial | rustling and bending (`VegetationSystem`, test `test_anim_effects`); falling, breaking and mulch todo |
| Planting trees, scaffolds appearing, ready and tapped, the workshop | todo | the workshop's work loop and ready horn, and the scaffold joining and tapping sounds: see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Food and wood piles and picking them up | todo |  |
| Villagers: footsteps, work (axe, saw, hammer), screams, babies crying, the village bell | todo | see ../villager/looks_and_voices.md |
| Crowds impressed, or losing belief | todo |  |
| Animals: their calls and footsteps by species | todo | see ../animal/ |
| The creature: breath, footsteps by ground, roars, snores, sneezes, eating, pooing, fight blows | done | `CreatureAudioSystem`, `src/Creature/CreatureAudio.cpp`; test `test_creature_audio` |
| Objects landing and breaking by what they are and what they hit | todo | see ../physics/ |
| Rewards, chests, the reward sting and the acknowledgement of a command | todo |  |
| The hand at the edge of the world | todo |  |
| The advisors' slapstick (knocking the glass, slapping, a fart, a gun) | todo | see voices_and_speech.md |
| Windmills and running water | todo |  |
| The story's own sound effects bank | todo | the script sound commands are stubs |
| The menu's button click | done | `GameInterface.cpp` |
