# Water

A held miracle: a small rain cloud follows the hand and drops water on the land below for a few seconds. It puts out
fires, sows and ripens fields, grows young trees and plants new ones in forests. The power-up ("extreme") version
rains over a wider cone and grows any tree past its normal size, but never plants.

Given by gold scroll: the monk's two water one-shot miracles in
[Fire! Fire! I'm on Fire!](../story/gold_scrolls/fire_fire_im_on_fire.md#the-monk-helps), if his quest is done.

**Progress: 35/41 done, 4 partial — 90%**

## Casting and lifetime

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rain is held: while the player keeps it, the drops fall under the hand and follow it each turn | done | `src/ECS/Systems/Implementations/MagicSystem.cpp` (held miracle follows the hand); scenario `miracles.water_trees` |
| It lasts 6 s for a player, 10 s for a computer player; a creature's lasts until it lets go | done | `src/Magic/SpellRules.cpp` timers from the info tables |
| It costs 5000 (7000 extreme) to cast and 10 prayer power per drop; nothing per turn | done | `src/Magic/SpellRules.cpp`, `src/Magic/SpellChants.cpp`; test `test_spell_chants.cpp`. See [prayer_cost.md](prayer_cost.md) |
| While the player casts, the hand bobs up 8 m and back over an 8 s loop, and stops when the rain ends | done | `src/Particles/ParticleHandRules.cpp` (hand sprinkle rule) |
| A scripted water miracle rains from a fixed point for nobody, with no hand bob | done | `src/Magic/ScriptCast.cpp`; test `test_script_cast.cpp` |
| A creature can cast water from above, its hand raised over the target | partial | The casting path raises food, wood and water above the target (`src/ECS/Systems/Implementations/MagicCreatureCasting.cpp`), but the creature's plans only cast lightning, heal and the creature spells (`src/Creature/CreaturePlanActions.cpp`), so it never chooses water. See [creature_spells.md](creature_spells.md) |
| A creature judges whether a field or a growing tree would gain from water before watering it | todo | No creature reasoning about watering in `src/Creature/` |
| The rain and the forests' shared planting delay are kept in a saved game | todo | openblack has no save games |

## Drops

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One drop falls each turn at a random point of the cone under the cloud: 0.3–4.5 m out (0.3–8.7 m extreme) | done | `src/Magic/SpellBehaviours.cpp` |
| Each drop pays its 10 and soaks a 1 m circle at ground level | done | `src/Magic/SpellBehaviours.cpp`, `MagicSystem.cpp` (burn applied from the ground point, fixed in the firewater round) |
| A drop reaches every object in the nine cells around it whose edge lies within reach across the ground | done | `MagicSystem.cpp` (water reach loop) |
| A field counts as 5 m wide for the drop's reach | done | `MagicSystem.cpp` (`k_FieldWaterRadius`); scenario `miracles.water_fields` |
| Water hurts nothing, changes no weather and raises no water level | done | Effect values are all zero apart from the cooling |
| The miracle's own drops don't count as rain, so a fireball in it doesn't steam from rain | done | Rain state comes only from the weather system |

## Putting out fires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A drop cools anything already burning; light things go out at once, heavy ones need several drops | done | `src/ECS/Systems/Implementations/FireSystem.cpp`; scenario `miracles.water_fire` (trees out within 3–5 drops) |
| A drop never sets anything alight or makes a fire where there was none | done | `FireSystem.cpp` (negative heat only acts on an existing fire) |
| A burning villager under the rain stops burning and goes back to what it was doing | done | `src/ECS/Systems/Implementations/VillagerFire.cpp` |
| People nearby come to watch the water put a fire out, one reaction per miracle at a time | done | `MagicSystem.cpp` (watchers' reaction); scenario `miracles.water_fire_watchers` |
| The water puts out a fireball rolling in it | done | `FireSystem.cpp`; scenario `miracles.water_fire` |

## Fields

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An unsown or half-sown field is sown fully by one drop | done | `src/Magic/WaterRules.cpp` called from `MagicSystem.cpp`; scenario `miracles.water_fields` |
| A sown field ripens faster: each drop ages the crop and adds food (about 0.58 per drop) until harvest age | done | Same; test `WaterRules.AFieldIsSownAtOnceThenRipensWithEachDrop` (`test/test_fire.cpp`) |
| A burning field's crop is left as it is | done | `MagicSystem.cpp` |
| The player's own creature may learn to water crops by watching each drop | done | `MagicSystem.cpp` per-drop mimic; the leash and mind gates belong to the creature's mind. See ../creature/ |

## Trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Normal water grows a young, still-growing tree a little with each drop, up to its full size | done | `src/Magic/WaterRules.cpp`, `MagicSystem.cpp`; scenario `miracles.water_forest`; test `WaterRules.TheWaterGrowsYoungTreesAndTheExtremeAnyTree` |
| Extreme water grows any tree, even a full-grown one, past its normal size, ever more slowly up to about three times | done | Same (growth falloff for full-grown trees) |
| A growing tree rustles with one of the tree-growth sounds | done | `MagicSystem.cpp` (tree-grow samples from the in-game bank) |
| Normal water on a full-grown forest tree plants a new young tree nearby, at most once every 40 turns across the whole island; natural forest spread shares the same delay | done | `src/ECS/Systems/Implementations/ForestSystem.cpp` (`AddTreeNear`); scenarios `miracles.water_forest`, `miracles.water_magic_forest` |
| The new tree's spot is searched over up to 160 tries 5–9 m away, spots on water allowed | done | `ForestSystem.cpp` |
| A spot is refused only where it hits a fixed object's own collision shape | partial | openblack has no per-object collision shapes; the fixed-obstacle circle stands in |
| The new tree starts tiny and grows to 0.8–1.2 of its kind's size, turned at random | done | `ForestSystem.cpp` |
| Trees grown by the forest miracle plant their seedlings into that miracle's forest, which removes them with it | done | `ForestSystem.cpp`, `MagicSystem.cpp`; scenario `miracles.water_magic_forest` |
| Planting a tree counts as a good deed for the caster | partial | The alignment change is made (`MagicSystem.cpp` via the alignment system); the player's statistics and alignment history aren't kept in openblack |

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pale blue mist cloud forms at the gesture point, kept within 58 m of the land; bigger when extreme | done | `src/Particles/ParticleHandRules.cpp`, particle data `SF_Water`/`SF_WaterPU1` |
| A rain cone streams from the cloud to the land, wider and taller when extreme | done | `src/Particles/` (rain cone mesh, data driven) |
| Cloud and cone fade out over 1 s when the rain ends and are gone 2.2 s after | done | `src/Particles/ParticleUpdateRules.cpp` |
| Every 0.1 s a ring spreads on the land or water where a drop fell, size 2 (4 extreme), in a random shade | done | `src/ECS/Systems/Implementations/WaterRingSystem.cpp`; test `test_water_rings.cpp` |
| Old rings keep drifting with the wind the way a reused ring slot did in the game | partial | Original quirk not reproduced: openblack has no fixed ring pool |
| The held seed shows a mist and a sparkling trail on the hand (bigger when extreme) | done | Particle data `SF_WaterInHand`/`SF_WaterInHandPU1`; in-hand effect stepped every drawn frame |
| A water seed on a dispenser shows its holder effect | done | `src/Debug/TestbedScenarioRegistry.cpp` registers `SF_WaterOnHolder`. See [dispensers_and_seeds.md](dispensers_and_seeds.md) |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rain loop plays while the water falls and fades out when it stops | done | Particle sound rule; the sound-name parser counts implicit values (`components/psys/src/EnumHeader.cpp`), confirmed in game |
| There is no cast, drop, splash or end sound | done | Matches the particle data |
