# Beams and lights of worship

The light the game puts over the places of belief: the faint column of light standing over every village centre a
player owns, the temple's own column that beats with its owner's heart, the curtain of light round each worship site's
altar, the trails of prayer power that run to a miracle, the glow of disciples, the beams over challenge scrolls and the
vortex, and the arcs of light the temple throws when it is struck. The influence border is in
[../worship/influence.md](../worship/influence.md), the village lanterns in [special_effects.md](special_effects.md). The
game's bursts of light are drawn only in the falling-spell film ([../video/bink_playback.md](../video/bink_playback.md)),
and a newly won town gets fireworks and a fountain of its owner's symbols rather than a beam
([../town/belief_and_conversion.md](../town/belief_and_conversion.md)).

**Progress: 0/37 done, 0 partial — 0%**

## The column over each village centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every village centre owned by a player has a tall column of light standing over it | todo | nothing in openblack draws it; the column mesh is only listed (`src/3D/AllMeshes.h`, `SpellColumn`) |
| It is made with the village centre and drawn every frame while the village centre is in play | todo | |
| It shows only once its owner's influence border shows (their temple stands); towns of the neutral player have none | todo | same per-player switch as the border in `InfluenceSystem::IsBorderShown` |
| It stands at the middle of the village centre's model, turned every frame about the upright to face the camera | todo | |
| It takes its owner's colour and is very faint: drawn additively at about 8% strength (20 of 255), steady, with no pulse | todo | |
| Its mesh is the column of the mesh pack: three faces of a half tube 5.8 across and 800 high, with the light band of mesh-pack texture 94 (0x5E) | todo | mesh 530 in `AllMeshes.g3d` |
| The column is stretched three times upright and drawn at three times its size, additively and without writing depth | todo | |
| It is drawn every frame in the world view, day and night and at any distance; it makes no sound | todo | the game's drawing of it has no time, distance or sound check |

## The temple's column

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player's temple has the same column of light in its owner's colour, made with the temple | todo | |
| It stands at the temple's position, turned every frame to face the camera, drawn like the village centre's | todo | |
| It shows only once its owner's influence border shows | todo | |
| Its strength beats with its owner's heartbeat, from 55 to 110 of 255, rising and falling smoothly with each beat | todo | the heartbeat isn't in openblack (`src/Audio/Sound.h` lists its sound only) |
| The heartbeat quickens with danger, from 0.75 beats a second with none to 2.5 at the most, eased in by a fifth each turn | todo | the danger is worked out every ten turns from the towns' wish for protection, the falling share of the world's people who believe, and enemy creatures near the player's towns |
| During the temple's destruction the column stays for the first 21.8 seconds and then goes | todo | |

## The light round a worship site's altar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ring of light stands round each built worship site's altar, in the player's colour | todo | see [../worship/worship_sites.md](../worship/worship_sites.md) |
| The ring is ten points on a circle of radius 3 about the site's altar point, closed back on itself | todo | |
| It rises as a curtain: brightest just above the ground and fading to nothing at the top, its top flared out to 1.5 times the ring | todo | |
| Its height ripples round the ring, up to about 10, the ripple running round as time passes | todo | |
| Its brightness follows how full the site's prayer battery is (stored prayer over the battery's size), the strength travelling round the ring as it changes | todo | the battery is in `src/Magic/WorshipBattery.cpp` (debug sandbox only) |
| It is drawn additively with `S_LightSheetStars.raw`, the texture sliding along the curtain as fast as the site is strained | todo | |
| When the miracles ask more than the dancers give, the ring's colour throbs with the site's strain pulse | todo | the pulse is (1 + cos phase) / 2, its phase quickening with the strain (Diego's port notes) |

## Trails of prayer power

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a miracle spends prayer power, a mote of light runs from its caster to the miracle for every 100 prayer power spent | todo | only when the caster has a worship site; `src/Magic/SpellChants.cpp` has no listener (see [../miracles/prayer_cost.md](../miracles/prayer_cost.md)) |
| The motes take the caster's player colour, hug the land 0.4 above it, weave from side to side and travel at a steady 40 a second | todo | `SF_ManaPathNew` spell file; particle type "Mana Path" in `src/Particles/ParticleTypes.cpp` is listed only |
| Reaching beyond the influence border with prayer to spare, the same motes run from the border to the hand (unconfirmed what it costs) | todo | see [../worship/influence.md](../worship/influence.md) |

## Disciples

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A disciple glows: a soft orange-gold light (alpha 128), sized by the villager and raised above it by its size, when it is drawn at a distance | todo | every disciple but worshippers and those from the vortex; up to 1,024 glows a frame. Which near detail levels also add it is unconfirmed |
| The disciple held in the hand, or the one under the hand, shows its disciple sign over its glow: a bright sprite five times its size above it, kept the same size on screen | todo | this is the one glowing villager; the sign is picked by the kind of disciple and drawn only where its glow is |

## Scrolls and the vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A started silver scroll has a faint pale blue column of light over it, facing the camera | todo | see [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
| A started gold scroll has a yellow column that only shows for an instant at the top of each pulse | todo | the game rounds its strength down, so it is 80 of 255 at the very top of the pulse and nothing otherwise |
| The scroll columns use the blast-centre mesh (a half tube 400 high), stretched three times upright, at the middle of the scroll | todo | mesh 527 in `AllMeshes.g3d` (`SpellBlastCentre`) |
| All scrolls share one pulse, (1 − cos phase) / 2, its phase going round about once every 1.26 seconds | todo | |
| The "see this" beam: a yellow column (alpha 180, 1.5 times size) facing the camera, beating with the scroll pulse, fading out over 2 seconds when ended | todo | `SF_SeeThisBeam` spell file; only the testbed starts it (`src/Debug/TestbedScenarioRegistry.cpp`) |
| On land 1, once the way out is opened, a "see this" beam marks where the vortex will open, and goes when the vortex appears | todo | the land 1 vortex script; special effects from scripts are stubs in `src/CHLApi.cpp` |

## The temple struck

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle that hits the temple passes its harm to one of the temple's parts, and an arc of crackling light runs from the temple's heart to it | todo | a part above a quarter of its life is taken at once; otherwise the first part with life left; with none, the heart itself |
| The arcs are two wiggling beams of `S_Beam.raw`, drawn additively, with one of five crackle sounds in turn | todo | `SF_SimpleBeamCitadel` spell file ("Magic Beam On Citadel") |
| A temple below full life throws arcs of light over itself from time to time | todo | (unconfirmed when: it happens when the harm falls on the heart itself) |
| The temple's destruction plays a timed show of arcs, magic and explosions over 21.8 seconds | todo | (unresearched in detail) |

## Other lights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two lanterns light the land at the temple's door at night | todo | the same village light as the street lanterns (`src/3D/VillageLights.cpp`); they come on when the temple's light value passes 0.07 (unconfirmed what that measures); where they stand is unconfirmed |
| Coloured lights in red, green, yellow, blue or white follow the dancers of a dance | n/a | the game keeps them but never draws them: the routine that would light the land under them (`DanceLight*.raw`, 6 by 6) is never called in any PC version |
