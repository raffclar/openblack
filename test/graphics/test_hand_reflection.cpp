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
#include "Visual/RenderModes.h"
#include "Visual/RenderPass.h"

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

// The game draws the held thing in the hand after the sea, and each draw keeps the light of the thing. The sea mirrors
// the thing in the light of the frame before. On its first frame in the hand, the thing takes the light of that frame.
// Here the light changes on each frame: the point, the haze distance and the colour of a fire.
TEST(HandReflection, TheHeldThingIsMirroredInTheLightOfTheFrameBefore)
{
	using hand_reflection::HeldLook;
	using hand_reflection::StepHeldLight;
	const HeldLook first {.point = {10.0f, 20.0f, 30.0f}, .viewDepth = 400.0f};
	const auto taken = StepHeldLight(std::nullopt, first);
	EXPECT_EQ(taken.mirrored.point, first.point);
	EXPECT_EQ(taken.mirrored.viewDepth, first.viewDepth);
	EXPECT_EQ(taken.kept.point, first.point);

	const HeldLook second {.point = {12.0f, 21.0f, 33.0f}, .viewDepth = 410.0f, .tint = 0xA0A0A0u, .added = 0x402000u};
	const auto next = StepHeldLight(taken.kept, second);
	EXPECT_EQ(next.mirrored.point, first.point);
	EXPECT_EQ(next.mirrored.viewDepth, first.viewDepth);
	EXPECT_EQ(next.mirrored.tint, 0u);
	EXPECT_EQ(next.mirrored.added, 0u);
	EXPECT_EQ(next.kept.tint, second.tint);

	const HeldLook third {.point = {14.0f, 22.0f, 36.0f}, .viewDepth = 420.0f};
	const auto last = StepHeldLight(next.kept, third);
	EXPECT_EQ(last.mirrored.point, second.point);
	EXPECT_EQ(last.mirrored.tint, second.tint);
	EXPECT_EQ(last.mirrored.added, second.added);
}

// The land's light, the haze and the trees' brightness that the mirror takes are those of the frame before. When the
// light changes between two frames, the mirror shows the light of the first frame on the second frame.
TEST(HandReflection, TheLightOfTheFrameBeforeIsKept)
{
	hand_reflection::FrameBefore<float> brightness;
	brightness.StartFrame(std::nullopt);
	EXPECT_FALSE(brightness.Has());
	EXPECT_EQ(brightness.Or(0.8f), 0.8f);
	// The first frame works out 0.8, the second 0.9
	brightness.StartFrame(0.8f);
	EXPECT_EQ(brightness.Or(0.9f), 0.8f);
	brightness.StartFrame(0.9f);
	EXPECT_EQ(brightness.Or(1.0f), 0.9f);
}

// A thing other than a tree takes its light where the hand holds it. A fire on it multiplies its light by the grey of
// the charring and adds its glow. Poison tints a pot, but a fire takes the place of the poison.
TEST(HandReflection, AHeldThingTakesTheColoursOfAFireOrPoison)
{
	using hand_reflection::HeldLookOf;
	using hand_reflection::HeldThing;
	const HeldThing plain {.drawnAt = {5.0f, 30.0f, 100.0f}, .takenAt = {0.0f, 2.0f, 0.0f}, .eye = {5.0f, 30.0f, 0.0f}};
	const auto plainLook = HeldLookOf(plain);
	EXPECT_EQ(plainLook.point, plain.drawnAt);
	EXPECT_EQ(plainLook.viewDepth, 100.0f);
	EXPECT_EQ(plainLook.tint, 0u);
	EXPECT_EQ(plainLook.added, 0u);

	auto poisoned = plain;
	poisoned.poisonedPot = true;
	EXPECT_EQ(HeldLookOf(poisoned).tint, 0xE8FFDDu);
	EXPECT_EQ(HeldLookOf(poisoned).added, 0x001000u);

	auto burning = poisoned;
	burning.onFire = true;
	burning.charredGrey = 0x80;
	burning.glow = 0x603010u;
	EXPECT_EQ(HeldLookOf(burning).tint, 0x808080u);
	EXPECT_EQ(HeldLookOf(burning).added, 0x603010u);
}

// A tree takes its light at its place on the map, where the hand took it, with its haze there at the height of the sea.
// A fire on it scales its light down to the grey of the fire.
TEST(HandReflection, AHeldTreeTakesTheLightOfItsPlaceOnTheMap)
{
	using hand_reflection::HeldLookOf;
	using hand_reflection::HeldThing;
	HeldThing tree {.drawnAt = {50.0f, 40.0f, 150.0f},
	                .takenAt = {20.0f, 7.0f, 120.0f},
	                .eye = {20.0f, 60.0f, 0.0f},
	                .forward = {0.0f, -0.6f, 0.8f},
	                .tree = true};
	const auto look = HeldLookOf(tree);
	EXPECT_EQ(look.point, tree.takenAt);
	EXPECT_FLOAT_EQ(look.viewDepth, (-60.0f * -0.6f) + (120.0f * 0.8f));
	EXPECT_EQ(look.treeGrey, 0u);
	EXPECT_EQ(look.tint, 0u);
	tree.onFire = true;
	tree.treeGrey = 120u;
	EXPECT_EQ(HeldLookOf(tree).treeGrey, 120u);
	EXPECT_EQ(HeldLookOf(tree).added, 0u);
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
