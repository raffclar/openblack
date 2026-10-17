/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Graphics/HandReflection.h"
#include "Graphics/RenderModes.h"
#include "Graphics/RenderPass.h"

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

// The game draws the held thing in the hand after the sea, and each draw keeps the light at the point of the thing. The
// sea mirrors the thing in the light of the frame before. On its first frame in the hand, the thing takes the light where
// it is.
TEST(HandReflection, TheHeldThingIsMirroredInTheLightOfTheFrameBefore)
{
	using hand_reflection::HeldLightPoint;
	using hand_reflection::StepHeldLight;
	const HeldLightPoint first {.point = {10.0f, 20.0f, 30.0f}, .viewDepth = 400.0f};
	const auto taken = StepHeldLight(std::nullopt, first);
	EXPECT_EQ(taken.mirrored.point, first.point);
	EXPECT_EQ(taken.mirrored.viewDepth, first.viewDepth);
	EXPECT_EQ(taken.kept.point, first.point);

	const HeldLightPoint second {.point = {12.0f, 21.0f, 33.0f}, .viewDepth = 410.0f};
	const auto next = StepHeldLight(taken.kept, second);
	EXPECT_EQ(next.mirrored.point, first.point);
	EXPECT_EQ(next.mirrored.viewDepth, first.viewDepth);
	EXPECT_EQ(next.kept.point, second.point);
	EXPECT_EQ(next.kept.viewDepth, second.viewDepth);
}

// The mirrored held thing is flat: each vertex takes all of the colour, as the sun does not shade it
TEST(HandReflection, TheMirroredHeldThingIsFlat)
{
	EXPECT_EQ(hand_reflection::k_HeldShade, 256.0f);
}

// The game draws the mirrored hand, the held thing and the creature before the sharks under the sea. The mirrored land
// writes no depth, so the sharks test their depth only against these mirrored things.
TEST(HandReflection, TheSharksUnderTheSeaComeAfterTheMirroredThings)
{
	EXPECT_LT(static_cast<int>(RenderPass::ReflectionMirrored), static_cast<int>(RenderPass::UnderSea));
	EXPECT_LT(static_cast<int>(RenderPass::Reflection), static_cast<int>(RenderPass::ReflectionMirrored));
}
