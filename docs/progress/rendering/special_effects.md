# Special effects

The game's smaller visual effects that aren't particle effects: beams and glows of light, the hand's and the villages'
lights at night, footprints and rings on the water, highlights placed by challenges, chimney smoke and the effects laid
over the whole picture. The sea's reflections are in [../ocean/](../ocean/), rain splashes in
[../weather/](../weather/), gesture trails in [../gesture/](../gesture/).

**Progress: 11/18 done, 2 partial — 67%**

## Lights and glows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spot light's beam: an open cone of eight sides drawn additively from the light, fading to black, its texture drifting | done | `src/Graphics/LightBeams.cpp`; test graphics/test_light_beams |
| Windows shed a volume of light drawn out from their edges, fading to nothing | done | `LightBeams.cpp`; test graphics/test_light_beams |
| At night the hand carries a light that brightens the land around it, coming up as the land darkens | done | `src/Graphics/HandLight.cpp`; test graphics/test_hand_light |
| At night the hand's light glows warm on the water beneath it | done | `src/Graphics/HandWaterGlow.cpp`; test graphics/test_hand_water_glow |
| Village lanterns and campfires light the land around them at night, flickering | partial | `src/3D/VillageLights.cpp`, `VillageLightSystem`; test_village_lights. Lanterns of homes and the temple are still missing (port notes) |

The columns of light over village centres and temples, the light round worship sites' altars, the glow of disciples,
the scroll and vortex beams and the falling-spell bursts are in [light_beams.md](light_beams.md).

## Marks on the land and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creatures leave prints as their footsteps fall, fading over about five seconds; they are not saved | done | `FootprintSystem`, `src/Creature/CreatureFootprints.h`; test_creature_footprints |
| Each species leaves its own print at its own size; the crocodile's has no size | done | `CreatureFootprints.h`; test_creature_footprints |
| On the first of April every creature leaves smiley faces | done | `CreatureFootprints.h`; test_creature_footprints |
| Prints stop being laid when there are as many as there can be | done | `FootprintSystem` |
| Creatures leave wet prints after walking out of water, placed by species | todo | |
| Rings spread on the water where something splashes, growing and fading over 0.7 seconds | done | `src/3D/WaterRings.cpp`, `WaterRingSystem`; test_water_rings. The hand's splash and the physics' hits and bobbing make them (`DynamicsSystem`) |

## Highlights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Challenges place highlights that pulse with a beam and can be tapped | todo | the beams are in [light_beams.md](light_beams.md); the scrolls in [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
| Highlighted things are drawn in a highlight material | todo | unconfirmed which things use it |

## Smoke

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smoke rises from the chimneys of homes | done | `src/3D/ChimneySmoke.cpp`, `ChimneySmokeSystem`; test_chimney_smoke |

## Over the whole picture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts fade the picture to a colour and back over whole seconds | done | `CinematicDirectorSystem` |
| Cinema bars slide in for cut scenes, hiding the dialogs | partial | `CinematicDirectorSystem`; snapping the bars for videos and picking inside them are todo (port notes) |
| Motion blur | todo | the renderer keeps a motion blur amount (unconfirmed when the game uses it) |
| Scripts take screenshots of the world for the challenge room's pictures | todo | see [../temple/](../temple/) |
