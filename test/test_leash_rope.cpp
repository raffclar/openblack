/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/LeashRope.h"

using namespace openblack;
using namespace openblack::leash_rope;

namespace
{
constexpr float k_Tolerance = 1e-4f;
// Flat land at height 0, well below the ropes
float Flat(glm::vec2 /*point*/)
{
	return 0.0f;
}
// A rope 32.5 long at rest and 77 at full length, as held in the hand for a creature of size 1
constexpr float k_Slack = 32.5f;
constexpr float k_Max = 77.0f;
const glm::vec3 k_Start {100.0f, 50.0f, 100.0f};
} // namespace

TEST(LeashRope, StartsStraightAndEvenlySpread)
{
	const auto end = k_Start + glm::vec3(41.0f, 0.0f, 0.0f);
	const auto rope = Create(k_Start, end, k_Slack, k_Max, {});
	for (size_t i = 0; i < k_NodeCount; ++i)
	{
		EXPECT_NEAR(rope.nodes.at(i).position.x, k_Start.x + static_cast<float>(i + 1), k_Tolerance);
		EXPECT_EQ(rope.nodes.at(i).velocity, glm::vec3(0.0f));
	}
	EXPECT_NEAR(RestLength(41.0f), 1.0f, k_Tolerance);
}

TEST(LeashRope, ASlackRopeHasNoTension)
{
	// The ends closer together than the rope's rest length
	const auto rope = Create(k_Start, k_Start + glm::vec3(20.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	EXPECT_FLOAT_EQ(rope.tension, 0.0f);
}

TEST(LeashRope, StretchedToItsFullLengthIsTaut)
{
	const auto atMax = Create(k_Start, k_Start + glm::vec3(k_Max, 0.0f, 0.0f), k_Slack, k_Max, {});
	EXPECT_NEAR(atMax.tension, 1.0f, k_Tolerance);
	const auto beyond = Create(k_Start, k_Start + glm::vec3(2.0f * k_Max, 0.0f, 0.0f), k_Slack, k_Max, {});
	EXPECT_FLOAT_EQ(beyond.tension, 1.0f);
	// Halfway between the rest length and the full length, half taut
	const auto half = Create(k_Start, k_Start + glm::vec3((k_Slack + k_Max) * 0.5f, 0.0f, 0.0f), k_Slack, k_Max, {});
	EXPECT_NEAR(half.tension, 0.5f, 1e-3f);
}

TEST(LeashRope, SagsUnderItsWeight)
{
	auto rope = Create(k_Start, k_Start + glm::vec3(25.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	for (int frame = 0; frame < 120; ++frame)
	{
		Step(rope, rope.start, rope.end, 1.0f / 30.0f, Flat);
	}
	// The middle hangs below the ends, the ends exactly where they were put
	EXPECT_LT(rope.nodes.at(k_NodeCount / 2).position.y, k_Start.y - 1.0f);
	EXPECT_EQ(rope.start, k_Start);
	// The weight of the rope stretches the end segments a very small distance only
	EXPECT_LT(rope.tension, 0.05f);
}

TEST(LeashRope, PullingTheEndAwayTightensIt)
{
	auto rope = Create(k_Start, k_Start + glm::vec3(25.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	auto end = rope.end;
	for (int frame = 0; frame < 60; ++frame)
	{
		end.x += 2.0f;
		Step(rope, rope.start, end, 1.0f / 30.0f, Flat);
	}
	// The ends are 145 apart, far beyond the rope's full length
	EXPECT_GT(rope.tension, 0.8f);
}

TEST(LeashRope, StaysOffTheGround)
{
	const auto high = [](glm::vec2 /*point*/) { return 45.0f; };
	auto rope = Create(k_Start, k_Start + glm::vec3(10.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	for (int frame = 0; frame < 120; ++frame)
	{
		Step(rope, rope.start, rope.end, 1.0f / 30.0f, high);
	}
	const auto floor = 45.0f + Look {}.halfWidth + k_GroundClearance;
	EXPECT_TRUE(std::ranges::all_of(rope.nodes, [floor](const Node& node) { return node.position.y >= floor - 1e-3f; }));
}

TEST(LeashRope, NothingFasterThanItsSpeedCap)
{
	auto rope = Create(k_Start, k_Start + glm::vec3(10.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	// Yanked far away in one frame
	Step(rope, rope.start, k_Start + glm::vec3(3000.0f, 0.0f, 0.0f), 0.05f, Flat);
	EXPECT_TRUE(
	    std::ranges::all_of(rope.nodes, [](const Node& node) { return glm::length(node.velocity) <= k_MaxSpeed + 1e-2f; }));
}

TEST(LeashRope, EachFrameStepsOnceMoreThanTheWholeStepsInIt)
{
	const auto start = Create(k_Start, k_Start + glm::vec3(10.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	const auto stepped = [&start](std::initializer_list<float> frames) {
		auto rope = start;
		for (const float seconds : frames)
		{
			Step(rope, rope.start, rope.end, seconds, Flat);
		}
		return rope;
	};
	const auto same = [](const Rope& a, const Rope& b) {
		return std::ranges::equal(a.nodes, b.nodes, [](const Node& x, const Node& y) {
			return x.position == y.position && x.velocity == y.velocity;
		});
	};
	// No time and less than one step time both step once
	const auto once = stepped({0.0f});
	EXPECT_NE(once.nodes.at(k_NodeCount / 2).position, start.nodes.at(k_NodeCount / 2).position);
	EXPECT_TRUE(same(stepped({0.0049f}), once));
	// One step time steps twice, two step times three times
	EXPECT_TRUE(same(stepped({k_StepSeconds}), stepped({0.0f, 0.0f})));
	EXPECT_TRUE(same(stepped({0.01f}), stepped({0.0f, 0.0f, 0.0f})));
}

TEST(LeashRope, EndsAreKeptWithinTheWorld)
{
	EXPECT_EQ(ClampEnd({-5000.0f, -3.0f, 9000.0f}), glm::vec3(k_MinAcross, k_MinHeight, k_MaxAcross));
}

TEST(LeashRope, RibbonFacesTheEyeAndShadowLiesOnTheLand)
{
	const auto rope = Create(k_Start, k_Start + glm::vec3(41.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	// Looking straight down on a rope running along x: the ribbon spreads along z, half its width each side
	const auto ribbon = BuildRibbon(rope, k_Start + glm::vec3(20.0f, 100.0f, 0.0f), Flat);
	const auto& a = ribbon.rope.at(2);
	const auto& b = ribbon.rope.at(3);
	EXPECT_NEAR(glm::distance(a.position, b.position), 2.0f * Look {}.halfWidth, k_Tolerance);
	EXPECT_NEAR(a.position.x, b.position.x, k_Tolerance);
	EXPECT_FLOAT_EQ(a.uv.y, Look {}.v1);
	EXPECT_FLOAT_EQ(b.uv.y, Look {}.v0);
	// The texture runs along the rope's length: one unit is 2.5 x 0.05 of it
	EXPECT_NEAR(a.uv.x, 0.125f, k_Tolerance);
	EXPECT_NEAR(ribbon.rope.back().uv.x, 41.0f * 0.125f, 1e-3f);
	// The shadow sits just over the land, faint, and fades out at the ends
	for (const auto& corner : ribbon.shadow)
	{
		EXPECT_NEAR(corner.position.y, k_ShadowLift, k_Tolerance);
	}
	EXPECT_FLOAT_EQ(ribbon.shadow.front().alpha, 0.0f);
	EXPECT_FLOAT_EQ(ribbon.shadow.back().alpha, 0.0f);
	EXPECT_NEAR(ribbon.shadow.at(10).alpha, 65.0f / 255.0f, k_Tolerance);
}

TEST(LeashRope, RibbonTrianglesCoverEverySegment)
{
	const auto indices = RibbonIndices();
	EXPECT_EQ(indices.size(), (k_PointCount - 1) * 6);
	EXPECT_EQ(*std::ranges::max_element(indices), k_RibbonVertexCount - 1);
}

TEST(LeashRope, AScriptTakingTheCameraAloneHidesNoRope)
{
	EXPECT_TRUE(Shown({.scriptShows = false}));
	EXPECT_TRUE(Shown({.scriptShows = false, .local = false}));
}

TEST(LeashRope, AScriptsCinemaBarsHideTheRopeUnlessTheScriptsShowLeashes)
{
	EXPECT_FALSE(Shown({.scriptShows = false, .scriptCinema = true}));
	EXPECT_FALSE(Shown({.scriptShows = false, .scriptCinema = true, .local = false}));
	EXPECT_TRUE(Shown({.scriptShows = true, .scriptCinema = true}));
}

TEST(LeashRope, ThePutAwayHandHidesItsOwnRopeOnlyWhileTheScriptsHideLeashes)
{
	EXPECT_FALSE(Shown({.scriptShows = false, .handHidden = true}));
	EXPECT_TRUE(Shown({.scriptShows = true, .handHidden = true}));
	EXPECT_TRUE(Shown({.scriptShows = false, .local = false, .handHidden = true}));
}

TEST(LeashRope, AHandHoldingATiedRopeDrawsNoneOfItsUsualRope)
{
	EXPECT_FALSE(Shown({.handHoldsTiedRope = true}));
}

TEST(LeashRope, AHandHoldingATiedRopeDrawsItsTwoRopesWhileTheScriptsShowLeashes)
{
	EXPECT_TRUE(HeldRopesShown({.handHoldsTiedRope = true}));
	EXPECT_FALSE(HeldRopesShown({.scriptShows = false, .handHoldsTiedRope = true}));
	EXPECT_FALSE(HeldRopesShown({.local = false, .handHoldsTiedRope = true}));
	EXPECT_FALSE(HeldRopesShown({}));
}

TEST(LeashRope, AnotherPlayersRopeGoesAsTheirCreatureFizzesAway)
{
	EXPECT_TRUE(Shown({.local = false, .creatureFizz = 0.39f}));
	EXPECT_FALSE(Shown({.local = false, .creatureFizz = k_HiddenFizz}));
	EXPECT_TRUE(Shown({.local = true, .creatureFizz = 1.0f}));
}

TEST(LeashRope, HangingFromAHandAboveItIsSlackAtTheCollarButNotAtTheHand)
{
	// The collar on the land, the hand 12 m up and away: the rope hangs from the hand, its weight stretching the top
	// segments, while the end at the collar lies slack. The tension is taken at the collar, the rope's start.
	const glm::vec3 collar {100.0f, 1.0f, 100.0f};
	const auto hand = collar + glm::vec3(8.0f, 9.0f, 0.0f);
	// The masses swing up and down a little, so the test takes the largest tension in the last second
	const auto largestTension = [](Rope rope, const glm::vec3& start, const glm::vec3& end) {
		float largest = 0.0f;
		for (int frame = 0; frame < 300; ++frame)
		{
			Step(rope, start, end, 1.0f / 30.0f, Flat);
			if (frame >= 270)
			{
				largest = std::max(largest, rope.tension);
			}
		}
		return largest;
	};
	const auto atCollar = largestTension(Create(collar, hand, k_Slack, k_Max, {}), collar, hand);
	EXPECT_LT(atCollar, 0.8f);
	const auto atHand = largestTension(Create(hand, collar, k_Slack, k_Max, {}), hand, collar);
	EXPECT_GT(atHand, atCollar);
}

// The ribbon's corners come from float steps in the game's order: the line from the eye and the rope's direction are
// each made one long first, and the side across them is made one long with its squares added z, x, y
TEST(LeashRope, RibbonCornersFollowTheGameOrder)
{
	auto rope = Create(k_Start, k_Start + glm::vec3(41.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	rope.nodes.at(3).position = k_Start + glm::vec3(3.3f, 0.7f, -0.9f);
	rope.nodes.at(4).position = k_Start + glm::vec3(4.1f, 1.3f, 0.35f);
	rope.nodes.at(5).position = k_Start + glm::vec3(5.7f, 0.2f, 1.1f);
	const glm::vec3 eye {113.7f, 171.3f, 61.9f};
	const auto ribbon = BuildRibbon(rope, eye, Flat);

	// Point 5 is the fourth mass (index 4); the points either side are the third and fifth masses
	const auto point = rope.nodes.at(4).position;
	const auto before = rope.nodes.at(3).position;
	const auto after = rope.nodes.at(5).position;
	glm::vec3 v = point - eye;
	const float vShare = 1.0f / std::sqrt(((v.z * v.z) + (v.y * v.y)) + (v.x * v.x));
	v = glm::vec3(vShare * v.x, v.y * vShare, v.z * vShare);
	glm::vec3 t = after - before;
	const float tShare = 1.0f / std::sqrt(((t.x * t.x) + (t.y * t.y)) + (t.z * t.z));
	t = glm::vec3(t.x * tShare, t.y * tShare, t.z * tShare);
	const glm::vec3 across {(v.z * t.y) - (v.y * t.z), (t.z * v.x) - (v.z * t.x), (v.y * t.x) - (t.y * v.x)};
	const float aShare = 1.0f / std::sqrt(((across.z * across.z) + (across.x * across.x)) + (across.y * across.y));
	const float half = Look {}.halfWidth;
	const glm::vec3 side {(across.x * aShare) * half, (across.y * aShare) * half, (across.z * aShare) * half};
	EXPECT_EQ(ribbon.rope.at(10).position, point + side);
	EXPECT_EQ(ribbon.rope.at(11).position, point - side);
}

// A rope that runs straight at the eye has no width there: the game keeps the side at zero
TEST(LeashRope, RibbonHasNoWidthWhereTheRopePointsAtTheEye)
{
	const auto rope = Create(k_Start, k_Start + glm::vec3(41.0f, 0.0f, 0.0f), k_Slack, k_Max, {});
	const auto ribbon = BuildRibbon(rope, k_Start + glm::vec3(-10.0f, 0.0f, 0.0f), Flat);
	EXPECT_EQ(ribbon.rope.at(10).position, ribbon.rope.at(11).position);
}
