# Positional audio

Where sounds are heard from: sounds placed in the world are heard from the camera, louder when near and panned to their
side, and fall silent beyond their distance.

**Progress: 8/10 done, 0 partial — 80%**

## 3D sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds placed in the world are heard from the camera's position and facing | done | `AudioManager` (OpenAL) |
| The listener has no motion, so moving the camera makes no Doppler | done | port notes (listener at rest) |
| Each sound fades from full at its near distance to nothing at its far one | done | `SoundEffectOptions` |
| A sound beyond its far distance isn't started at all | done | `AudioManager::PlayAnimEffect` |
| Loops move with their owner each turn | done | `AudioManager::SetEmitterPosition` |
| Interface sounds are heard centred, not placed | done | `AudioManager::PlaySound` |
| Music placed in the world, heard by distance | todo | see music.md |
| A master volume over everything | done | `AudioManager::SetGlobalVolume` |
| Too many sounds at once: the quietest or furthest are dropped | todo | (unconfirmed how the game limits voices) |
| The game runs without a sound device | done | `AudioManagerNoOp` |
