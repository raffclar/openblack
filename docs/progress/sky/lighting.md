# Lighting by time of day

The light the world is seen in: a palette of colours for the land through the day for each alignment, a dark colour, a
warm colour and the moon's colour, which light the land, the models and the sea, and close the haze in at dusk.

**Progress: 14/16 done, 2 partial — 94%**

## The land's light

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather palette holds the good, neutral and evil colours of the land through the day | done | `src/3D/LandLightTable.cpp` (34b26e64, 1942811f); `test/test_land_light_table.cpp` |
| The land's colour of the moment comes from the palette by the sky type and the alignment the sky shows | done | `LandLightTable::Build`, `src/3D/LandLightFrame.cpp` |
| An overcast at the camera darkens the land's colour | done | ab345ff0 |
| A flash of lightning takes every light and the haze towards white | done | 6c9dff13; `src/3D/LandLightTable.cpp` |
| The dark, warm and moon colours change with the alignment | done | `src/3D/LandLightTable.h` |
| The land's light of the moment is kept by things made in it, such as a splash on the water | done | `land_light::Current` (`src/3D/LandLightFrame.h`) |

## Models and the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit by the land's light where they stand, and by the sun's direction | done | `src/Graphics/ModelLight.cpp`; `test/graphics/test_model_light.cpp`; see ../rendering/ |
| Trees take the land's brightness where they stand | done | `src/Graphics/TreeBrightness.cpp`; see ../nature/ |
| Mists and clouds take the land's light | done | `src/3D/Clouds.cpp`, `shaders/fs_mist.sc` |
| The sea takes the land's light at full luminosity | done | see ../ocean/sea_surface.md |
| Distant things fade into a haze that is a third of the land's colour and closes in at dusk | done | 4b1a1cc5; `shaders/haze.sh` |
| Under a storm the haze thickens | done | ab345ff0 (storm haze) |
| At night, village lights, lanterns, the hand and fires light the cells around them | partial | a7e48d8f; abode and citadel lanterns still blocked (port notes); see ../town/ and ../rendering/ |
| The light at night is the moon's colour | done | `src/3D/LandLightTable.cpp` |
| Inside the temple the light comes from the temple's own lights, not the time of day | done | `src/3D/TempleLight.cpp`; see ../temple/ |
| Shadows of objects fall away from the sun | partial | see ../rendering/ (object shadows exist; whether they follow the sun's hour is unconfirmed) |
