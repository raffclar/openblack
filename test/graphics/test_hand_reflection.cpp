/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/HandReflection.h"
#include "Graphics/RenderModes.h"

using namespace openblack::graphics;

namespace
{
constexpr float k_Grey = 160.0f / 255.0f;
} // namespace

// The game draws the mirrored hand in grey 0xA0 with nothing added
TEST(HandReflection, TheColoursAreTheGames)
{
	EXPECT_EQ(hand_reflection::k_Colour, 0x65A0A0A0u);
	EXPECT_EQ(hand_reflection::Colour(), glm::vec3(k_Grey));
	EXPECT_EQ(hand_reflection::Added(), glm::vec3(0.0f));
}

// The hand is mirrored while it shows and has a model, and at no other time
TEST(HandReflection, OnlyAShownHandWithAModelIsMirrored)
{
	EXPECT_TRUE(hand_reflection::Shown(true, true));
	EXPECT_FALSE(hand_reflection::Shown(false, true));
	EXPECT_FALSE(hand_reflection::Shown(true, false));
	EXPECT_FALSE(hand_reflection::Shown(false, false));
}

// The hand comes first, then what it holds, then the creature
TEST(HandReflection, TheMirroredThingsComeInTheGamesOrder)
{
	using hand_reflection::MirroredThing;
	using hand_reflection::SortKey;
	EXPECT_GT(SortKey(MirroredThing::Hand), 0u);
	EXPECT_LT(SortKey(MirroredThing::Hand), SortKey(MirroredThing::Held));
	EXPECT_LT(SortKey(MirroredThing::Held), SortKey(MirroredThing::Creature));
}

// The hand's material takes its alpha from its texture alone, so the alpha of the mirrored colour has no effect. It is
// blended over what is behind it and writes its depth.
TEST(HandReflection, TheHandsMaterialKeepsItsTextureAlpha)
{
	const auto& mode = render_modes::Desc(render_modes::Mode::AlphaTextured);
	EXPECT_FALSE(mode.alphaModulate);
	EXPECT_EQ(mode.blend, render_modes::Blend::Standard);
	EXPECT_TRUE(mode.zWrite);
	EXPECT_FALSE(mode.alphaTest);
	// The hand's cull byte draws only the front faces
	EXPECT_FALSE(render_modes::DrawsBothSides(0x04u));
}

// The lion's one chroma primitive also takes its alpha from its texture alone: the mirrored body's alpha has no effect
// on it, and the alpha test drops the same pixels as in the world
TEST(HandReflection, TheChromaPrimitiveKeepsItsTextureAlpha)
{
	const auto& mode = render_modes::Desc(render_modes::Mode::TexturedChroma);
	EXPECT_FALSE(mode.alphaModulate);
	EXPECT_TRUE(mode.alphaTest);
	EXPECT_EQ(mode.blend, render_modes::Blend::Standard);
}
