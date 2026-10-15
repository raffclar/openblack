/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/PosedDraws.h"

using openblack::graphics::posed_draws::BlendedPassDrawsAllPlaced;
using openblack::graphics::posed_draws::MeshPlacers;
using openblack::graphics::posed_draws::PosedThingIsBlended;

namespace
{

/// How many times a posed thing of a mesh is drawn in one frame: once by itself in the solid or the blended pass, and
/// once more for each pass that draws all of the mesh's placed things together
int DrawsOfPosedThing(bool fading, const MeshPlacers& placers)
{
	const bool blended = PosedThingIsBlended(fading, placers);
	const bool solidPassDrawsAllPlaced = !placers.translucent && !placers.perEntity;
	const int solidDraws = (blended ? 0 : 1) + (solidPassDrawsAllPlaced ? 1 : 0);
	const int blendedDraws = (blended ? 1 : 0) + (BlendedPassDrawsAllPlaced(placers) ? 1 : 0);
	return solidDraws + blendedDraws;
}

} // namespace

TEST(PosedDraws, AnOpenedRewardChestIsDrawnOnce)
{
	// The opened chest blends by its materials (its light) and is posed by its clip (its lid)
	const MeshPlacers chest {.translucent = true, .perEntity = true};
	EXPECT_FALSE(BlendedPassDrawsAllPlaced(chest));
	EXPECT_TRUE(PosedThingIsBlended(false, chest));
	EXPECT_EQ(DrawsOfPosedThing(false, chest), 1);
	EXPECT_EQ(DrawsOfPosedThing(true, chest), 1);
}

TEST(PosedDraws, SolidPosedThingsAreDrawnWithTheSolidThings)
{
	const MeshPlacers gate {.translucent = false, .perEntity = true};
	EXPECT_FALSE(PosedThingIsBlended(false, gate));
	EXPECT_EQ(DrawsOfPosedThing(false, gate), 1);
}

TEST(PosedDraws, FadingPosedThingsAreDrawnWithTheBlendedThings)
{
	const MeshPlacers gate {.translucent = false, .perEntity = true};
	EXPECT_TRUE(PosedThingIsBlended(true, gate));
	EXPECT_EQ(DrawsOfPosedThing(true, gate), 1);
}

TEST(PosedDraws, TranslucentMeshesNotPosedAreDrawnTogether)
{
	EXPECT_TRUE(BlendedPassDrawsAllPlaced({.translucent = true, .perEntity = false}));
	EXPECT_FALSE(BlendedPassDrawsAllPlaced({.translucent = false, .perEntity = false}));
}
