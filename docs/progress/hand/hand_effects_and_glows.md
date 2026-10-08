# Hand effects and glows

The effects the god hand carries: the bands and bracelets and the flowing glow of a miracle held in it, the miracle's own
effect riding in the hand, the pulse of a miracle charging, the tribal power ring, the flash of a recognised gesture,
and the glows scripts put on the creature's hands. How the hand's own skin and shape show alignment, its light and its
shadow are in [look_and_morph.md](look_and_morph.md).

**Progress: 31/45 done, 2 partial — 71%**

## Bands and bracelets of a held miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking a miracle into the hand, five bands fly from in front of the camera onto the hand, a tenth of a second apart | done | `src/Magic/MiracleVisuals.cpp` (fly-in); `MiracleFxSystem::SeedInHand` |
| The bands that come with a miracle still settling wait before flying in | done | `MiracleVisuals.h` (settle delay 2.4 s) |
| Each fly-in plays the power-up band sound | done | `MiracleFxSystem::SeedInHand` (power-up band sound) |
| The hand wears one bracelet for the plain miracle and one more for each power-up, five at most | done | `MiracleVisuals.h` (most bracelets 5); `visuals::SetBracelets` |
| Powering up adds the newest bracelets; powering down takes the newest off | done | `visuals::SetBracelets` |
| The bracelets sit along the hand and arm, each further one further up the arm and spinning a little faster | done | `MiracleVisuals.h` (band radius 10, spin 12 plus 0.2 per band, offsets 10 plus 40 per band) |
| Letting the miracle go takes the bracelets off | done | `MiracleFxSystem::SeedLeftHand` |
| Shaking the miracle off sends one band flying off the hand back to the camera over a second | done | `visuals::AddFlyOff`; `MiracleFxSystem::SeedShakenOff` |
| Shaking a miracle off plays the shake-off sound | done | `MagicSystem.cpp` (shake sound, volume 35) |
| The announcer names the first, second or third power-up as it comes to the hand | done | `visuals::PowerUpVoiceSample` |
| Bands and bracelets stand still while the game is paused | done | `MiracleFxSystem` steps by game time |

## The glow of a held miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a miracle, a flowing glow in the player's colour runs over the hand | done | `src/Graphics/RendererMiracles.cpp` (flow texture), `HandMiracleFx` |
| The glow is drawn at 0.8 strength, and not at all below a hundredth | done | `MiracleVisuals.h` (glow share 0.8) |
| The glow's texture steps through a sheet of cells an eighth of the sheet across and down | done | `MiracleVisuals.h` (cell size 0.125, 32 cells at 20 a second) |
| The glow also shows while the hand holds a miracle's living creation, or anything else that counts as magic | todo | (unconfirmed which objects besides miracles); nothing else can be held yet |
| The glow takes the player's colour, with its own strength as alpha | done | `RendererMiracles.cpp` |

## A miracle charging in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a miracle charges from worship into the hand, the hand pulses, more often as the charge fills | todo | charging from worship is not done; see [../worship/](../worship/) |
| The first pulse comes as soon as charging starts | todo | |
| With a force-feedback mouse the charge is also felt, its beat quickening with the charge (charge less a tenth) | todo | see [clicking_and_activating.md](clicking_and_activating.md) |
| The charging feel stops when the charging stops | todo | |

## The miracle's own effect in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A held miracle's effect (flames, water, sparks, the storm's cloud …) rides with the hand | done | `MagicSystem::PlaceHandEffect`; see [../miracles/](../miracles/) |
| The effect sits at the hand, or at a point of the hand's mesh where the miracle asks for one | partial | openblack places it at the hand; the per-miracle point on the mesh is not used |
| The effect is scaled with the hand's size, so it keeps its size on screen | done | `MagicSystem::PlaceHandEffect` (hand scale) |
| The effect steps by the game's time, at least a millisecond a frame, and ends when it says it has finished | partial | steps by game time; the end-of-effect hand-off is the particle system's (see [../rendering/](../rendering/)) |
| The fireball held in the hand rolls its flames with the hand's smoothed movement on screen | done | particle rule for in-hand fireball; see [../miracles/](../miracles/) |

## Tribal power ring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spinning the hand with a tribal power miracle starts a ring in the player's colour, fully opaque | done | `src/Magic/TribalPowerSpin.cpp`, `MiracleFxSystem` |
| The ring flies in from the camera and settles round the hand, above it | done | tests `TribalPowerSpin.ItFliesInFromTheCamera`, `TribalPowerSpin.TheRingSettlesRoundTheHandAboveIt` |
| Nothing shows until the ring has two samples of the spin | done | test `TribalPowerSpin.NothingShowsUntilTheRingHasTwoSamples` |
| Letting go leaves the ring rising as a column where the hand was | done | test `TribalPowerSpin.LetGoItRisesAsAColumnAndIsGoneAfterFourSecondsAndAHalf` |
| Letting go with no ring started raises the column straight away, in the player's colour | done | `MiracleFxSystem::UpdateTribalPower` |
| The column fades after three seconds and is gone after four and a half | done | test `TribalPowerSpin.TheColumnFadesAfterThreeSeconds` |

## Gesture flash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A recognised gesture makes the hand flash in its player's colour | done | `src/ECS/Components/HandGlow.h`; see [../gesture/gesture_effects.md](../gesture/gesture_effects.md) |
| The sparkles of a recognised gesture settle on the gesture's shape | done | see [../gesture/gesture_effects.md](../gesture/gesture_effects.md) |

## Glows on the creature's hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts light a glow on the creature's left hand, right hand, or above its hands (the creed on land 4) | todo | `SET_CREATURE_CREED_PROPERTIES` is a stub in `src/CHLApi.cpp` |
| Each glow has a scale, a power and a time to reach it; a power of 0 puts the glow out | todo | |
| The glow is a sprite on the hand's point, scaled by the glow's scale, its brightness twice its power | todo | |
| While any of its glows is lit the creature carries a looping glow sound, which stops when the last goes out | todo | the creed glow sounds are listed in `src/Creature/CreatureAudio.cpp` but never played |
| In the opening film the creature's hand glows rise and fall with the film | todo | see [../story/](../story/) |

## The hand's effect on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees within the hand's reach bend away from it, from below their tops | done | `VegetationSystem.cpp` |
| Moving the hand quickly blows chimney smoke about | done | `ChimneySmokeSystem.cpp` (hand wind) |
| Gripping the land throws up a puff of dust | done | `src/3D/GripLandscapeEffect.cpp` |
| Gripping the sea splashes a ring on the water | done | `WaterRingSystem`, `Game::PlayHandGrabSound` |
| Crossing an influence border ripples the border | done | `InfluenceSystem.cpp`; see [../worship/](../worship/) |
| A held object carried through trees bends them too | todo | The hand holds objects now (`HandGrabSystem`); they don't bend trees yet |
| Objects whizzing past the camera after a throw make a whoosh (rocks, fireballs) | todo | see [hand_sounds.md](hand_sounds.md) |
