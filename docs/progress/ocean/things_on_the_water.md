# Things on the water

What happens on and under the sea's surface: rings where things splash, the hand's light at night, fish, boats and
nets, and what floats or sinks.

**Progress: 7/19 done, 2 partial — 42%**

## Rings and splashes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ring of the smoke texture spreads and fades on the surface over 0.7 seconds where something splashes | done | `src/3D/WaterRings.cpp`, `WaterRingSystem`; `test/test_water_rings.cpp` |
| Rings are capped; when full a new one is not made | done | `src/3D/WaterRings.h`; test |
| Rings keep the land's light of when they were made | done | `src/3D/WaterRings.cpp` |
| Rings stop growing while the game is paused | done | `WaterRingSystem` |
| The hand splashes where it grips the land at the water | done | 1ec9d2c9 |
| Things thrown or dropped into the sea splash and ring | todo | port notes: with physics; see ../physics/ |
| Rain makes rings on the water | todo | port notes: rain rings todo; see ../weather/rain.md |
| The water miracle splashes and ripples where it falls | partial | see ../miracles/water.md |
| Big splashes of spray when something heavy lands in the sea | todo | |

## Light on the water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At night the hand's light glows warm on the water under it | done | `src/Graphics/HandWaterGlow.cpp` (49331df5); `test/graphics/test_hand_water_glow.cpp` |
| The glow only shows where there is water near the hand and the hand's light is strong enough | done | `src/Graphics/HandWaterGlow.cpp` |
| Lightning lights the sea as it lights the land | partial | the sea takes the land's light, which the flash lifts; not checked against the game (unconfirmed) |

## Life and objects at sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Objects thrown into the sea float or sink | todo | See ../physics/ |
| Villagers thrown into the sea drown | todo | See ../villager/ |
| Sharks swim off the coast and take what falls in | todo | See ../animal/ |
| Seagulls fly over the coast | todo | See ../animal/ |
| Shoals of fish scatter round things in the water | todo | the game has fish that rush away from what falls in; not ported |
| Fishermen's nets and fish farms stand in the water, cut by the sea plane | todo | See ../resources/ and ../building/ |
| A boat sits on the water where the story calls for one (unconfirmed which lands) | todo | port notes list a boat among things to draw; see ../story/ |
