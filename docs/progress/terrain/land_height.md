# Land height and mountains

The shape of the land: a height for every cell corner, interpolated over each cell's two triangles, with the sea level
flattened, and the queries everything uses to stand on the ground.

**Progress: 10/12 done, 2 partial — 92%**

## Height

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every cell corner has a height of 0 to 255 units | done | `components/lnd` cell altitude |
| Heights between corners follow the cell's two triangles, split one way or the other per cell | done | `LandIsland::GetAltitude` (`src/3D/Implementations/LandIsland.cpp`) |
| Height is worked out in the game's fixed-point units, so objects stand exactly where the game puts them | done | `LandIsland::GetAltitude`, `src/3D/MapCoords.h` |
| Land at or below altitude 3 next to the sea is drawn and treated as flat at sea level | done | `LandIsland::GetDrawnAltitude`; port notes (sea flattening) |
| The land's normal is the flat normal of the triangle a point is on, worked out through the game's lookup tables | done | `src/3D/LandNormal.cpp`; `test/test_land_normal.cpp` |
| Mountains and hills are just high land: there are no separate cliff or rock-face meshes in the land itself | done | rock features are objects, see ../nature/ |
| The highest point of each block is known for drawing and the creature's view of the land | partial | the field is read from the file but not used |
| Scripts ask the height of the land at a point | done | `CHLApi.cpp` GET_LAND_HEIGHT |
| Things dropped or thrown land on the ground and objects stand on it | done | land collision shape per block (`src/3D/LandBlock.cpp`); see ../physics/ |
| A point on the land is found under the mouse by the game's line test over the land's cells | done | The game's own land line test over the cells (`src/3D/LandLine.cpp`, `PickingSystem::LandUnderPixel`); test `test_picking`. Its quirks are kept: every cell is split on the same fixed diagonal whatever the drawn split, and the line is always extended to the map's edge (the game compares a float with a double that never match) |
| Particles and spells can be kept at a fixed height above the land or forced onto it | partial | particle rules in `src/Particles` follow the land; see ../rendering/ |
| The land never changes shape in play; buildings sit on it as they are | done | the original has no terraforming in the single-player game; the land script's height change is in countries.md |

## The test land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack's flat test land, with a lake, shallows, a bank and patches of sand and snow | n/a | openblack-only, see ../debug/ (`src/3D/FlatLand.cpp`, `test/test_flat_land.cpp`) |
