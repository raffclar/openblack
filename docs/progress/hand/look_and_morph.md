# Look and morph

How the god hand looks: it changes shape and colour with the player's alignment (gold for good, red and spiked for evil),
lights the land at night and casts a shadow.

**Progress: 10/11 done, 1 partial — 95%**

## Alignment morph

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The base hand is pulled towards the evil hand below alignment 0 and towards the good hand from 0, by how far the alignment is from 0 | done | `src/3D/HandMorph.cpp`, `Renderer::HandMorphTargets` (vertex shader); test `HandMorph.PullsTowardsEvilBelowZeroAndGoodFromZero` |
| The alignment shown is held between -1 and 1 | done | test `HandMorph.TargetIsTheAlignmentHeldToItsRange` |
| The skin is blended a 4-bit channel at a time, alpha too, in whole steps | done | `hand_morph::BlendSkin`; tests `HandMorph.SkinBlendsEveryChannelInWholeSteps`, `HandMorph.SkinWeightIsTruncated` |
| The hand doesn't ease: it jumps straight to the alignment once it has moved far enough from the one drawn | done | tests `HandMorph.StaysUntilTheAlignmentMovesFarEnough`, `HandMorph.JumpsStraightToTheAlignmentOnceFarEnough` |
| It starts neutral | done | `src/ECS/Components/HandMorph.h` |
| Going into or out of the player's influence blends the skin again | done | `HandSystem::UpdateAlignmentMorph`; test `HandMorph.CrossingTheInfluenceBlendsTheSkinForTheLastAlignmentTaken` |
| The influence is tested at the point picked under the cursor, a frame late, kept within reach of the map | done | The interface's resolved pick point (`PickingSystem`, a frame late as in the game); test `test_picking`; objects are picked in screen space as the game does, the first covering triangle of a moving model winning rather than the nearest (`src/3D/ScreenPick.cpp`) |

## Light and shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand casts a soft shadow on the land under it | partial | `src/Graphics/HandShadow.cpp`; follows the sun and fades at night, where the game's light stays in one place |
| The shadow fades as the camera pulls away from the ground under the hand | done | test `HandShadow.FadesWithCameraDistance` |
| At night the hand carries a light that brightens the land around it | done | `src/Graphics/HandLight.cpp`; tests `HandLight.*` |
| At night its light glows warm on water near it, never in the temple | done | `src/Graphics/HandWaterGlow.cpp`; tests `HandWaterGlow.*` |

The glows and effects the hand carries, and its effect on the world, are in [hand_effects_and_glows.md](hand_effects_and_glows.md).
