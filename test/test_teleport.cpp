/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The teleport miracle's rules (Magic/Objects/MagicTeleport) and the ZR_SurfRevol mesh (Particles/Rules/SurfRevol).

#include <cmath>

#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Spell.h"
#include "InfoConstants.h"
#include "Magic/Core/Chants.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Particles/Rules/SurfRevol.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::psys;

TEST(Teleport, FastDistanceIsMaxPlusHalfMin)
{
	// FastDistance in MapCoords units (6553.6 a metre)
	const glm::vec3 a(0.0f);
	EXPECT_EQ(teleport::FastDistance(a, glm::vec3(10.0f, 0.0f, 0.0f)), 65536);
	EXPECT_EQ(teleport::FastDistance(a, glm::vec3(10.0f, 0.0f, 4.0f)), 65536 + (static_cast<int32_t>(4.0f * 6553.6f) >> 1));
	EXPECT_EQ(teleport::FastDistance(a, glm::vec3(-4.0f, 0.0f, 10.0f)),
	          teleport::FastDistance(a, glm::vec3(10.0f, 0.0f, 4.0f)));
}

TEST(Teleport, WorthTheDetour)
{
	// a villager at 0 going to 1000: stone A at 10, stone B at 990 -> 1.2 x (10 + 10) = 24 < 1000
	const glm::vec3 living(0.0f);
	const glm::vec3 destination(1000.0f, 0.0f, 0.0f);
	EXPECT_TRUE(teleport::IsWorthTheDetour(living, destination, glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(990.0f, 0.0f, 0.0f)));
	// B next to A: 1.2 x (10 + 990) = 1200 > 1000
	EXPECT_FALSE(teleport::IsWorthTheDetour(living, destination, glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(10.0f, 0.0f, 5.0f)));
	// the boundary: 1.2 x (a + b) == d is not enough (strictly less)
	EXPECT_FALSE(teleport::IsWorthTheDetour(glm::vec3(0.0f), glm::vec3(120.0f, 0.0f, 0.0f), glm::vec3(50.0f, 0.0f, 0.0f),
	                                        glm::vec3(70.0f, 0.0f, 0.0f)));
}

TEST(Teleport, ChooseTargetTakesTheBiggestSaving)
{
	const glm::vec3 living(0.0f);
	const glm::vec3 destination(100.0f, 0.0f, 0.0f);
	const std::vector<glm::vec3> others = {glm::vec3(50.0f, 0.0f, 0.0f), glm::vec3(90.0f, 0.0f, 0.0f),
	                                       glm::vec3(-20.0f, 0.0f, 0.0f)};
	float saving = 0.0f;
	EXPECT_EQ(teleport::ChooseTarget(living, destination, others, false, &saving), 1);
	// |dest - l| - |dest - T| = 100 - 10, each GetDistanceInMetres (its table root: not exactly 90).
	// Worked by hand: 100 m = 655360 units -> hypotenuse 655520 -> 100.0244140625 m (as in test_game_distance);
	// 10 m = 65536 -> InvSqrt(1) = 0.99951171875 -> 65568 -> 10.0048828125 m
	EXPECT_EQ(saving, 90.01953125f);
	// only a stone that brings it further: none unless forced (then the least bad one)
	const std::vector<glm::vec3> worse = {glm::vec3(-20.0f, 0.0f, 0.0f), glm::vec3(-50.0f, 0.0f, 0.0f)};
	EXPECT_EQ(teleport::ChooseTarget(living, destination, worse, false, &saving), -1);
	EXPECT_EQ(teleport::ChooseTarget(living, destination, worse, true, &saving), 0);
	// 120 m = 786432 units -> hypotenuse 786624 -> 120.029296875 m
	EXPECT_EQ(saving, -20.0048828125f);
	EXPECT_EQ(teleport::ChooseTarget(living, destination, {}, true, &saving), -1);
}

TEST(Teleport, JumpCostSign)
{
	// PayFor(-saving x costPerKilometer (200) x 0.001, forced): a useful 500 m jump gives 100 chants back
	EXPECT_FLOAT_EQ(teleport::JumpCost(500.0f, 200.0f), -100.0f);
	EXPECT_FLOAT_EQ(teleport::JumpCost(-20.0f, 200.0f), 4.0f);
}

TEST(Teleport, NegativePayForAddsChants)
{
	// PayFor: chants -= cost with no clamp; divideCostsByTribalPower divides by max(1, tp)
	GMagicEffectInfo effect {};
	effect.initialChants = 2000.0f;
	effect.costPerGameTurn = 1.0f;
	effect.costPerEvent = 1.0f;
	effect.divideCostsByTribalPower = 1;
	chants::Context context;
	context.effect = &effect;
	context.recharged = true;
	context.hasCreator = true;
	context.costToMaintain = 1.0f;
	context.maintain = [](float) { return 0.0f; }; // a normal player gives nothing
	ecs::components::Spell spell;
	chants::SetChants(spell, 2000.0f);
	chants::PayFor(spell, context, teleport::JumpCost(500.0f, 200.0f), true);
	EXPECT_FLOAT_EQ(spell.chants, 2100.0f);
	chants::PayFor(spell, context, teleport::JumpCost(-20.0f, 200.0f), true);
	EXPECT_FLOAT_EQ(spell.chants, 2096.0f);
}

TEST(Teleport, RouteStone)
{
	// FindRouteStone: the stone nearest the start if (nearest to start) + (nearest to target) < max distance
	const std::vector<glm::vec3> stones = {glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(500.0f, 0.0f, 0.0f)};
	EXPECT_EQ(teleport::FindRouteStone(stones, glm::vec3(0.0f), glm::vec3(505.0f, 0.0f, 0.0f), 100.0f), 0);
	EXPECT_EQ(teleport::FindRouteStone(stones, glm::vec3(0.0f), glm::vec3(700.0f, 0.0f, 0.0f), 100.0f), -1);
	// both minima start at the max distance: nothing nearer -> none
	EXPECT_EQ(teleport::FindRouteStone({}, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 100.0f), -1);
}

TEST(Teleport, RouteStoneBothEndsAreLookedForApart)
{
	// the two minima are kept apart, so one stone may serve both ends; the stone it gives back is always
	// the one nearest `from`
	const std::vector<glm::vec3> one = {glm::vec3(30.0f, 0.0f, 0.0f)};
	EXPECT_EQ(teleport::FindRouteStone(one, glm::vec3(0.0f), glm::vec3(40.0f, 0.0f, 0.0f), 100.0f), 0); // 30 + 10 < 100
	// 30 + 70 == 100 is not enough (strictly less)
	EXPECT_EQ(teleport::FindRouteStone(one, glm::vec3(0.0f), glm::vec3(100.0f, 0.0f, 0.0f), 100.0f), -1);
	// the list is the player's (newest first): the nearest to `from` wins even though the other is the nearest to `to`
	const std::vector<glm::vec3> two = {glm::vec3(60.0f, 0.0f, 0.0f), glm::vec3(10.0f, 0.0f, 0.0f)};
	EXPECT_EQ(teleport::FindRouteStone(two, glm::vec3(0.0f), glm::vec3(65.0f, 0.0f, 0.0f), 100.0f), 1);
}

TEST(SurfRevol, Profiles)
{
	EXPECT_FLOAT_EQ(surf_revol::Profile(0, 0.5f).x, 0.5f);
	EXPECT_FLOAT_EQ(surf_revol::Profile(0, 0.5f).y, 0.0f);
	EXPECT_FLOAT_EQ(surf_revol::Profile(1, 0.25f).y, 3.0f * (0.5f - 1.0f));
	EXPECT_FLOAT_EQ(surf_revol::Profile(2, 0.5f).x, 0.75f);
	EXPECT_FLOAT_EQ(surf_revol::Profile(2, 0.5f).y, 0.0f);
	EXPECT_FLOAT_EQ(surf_revol::Profile(3, 0.5f).y, 3.0f * (0.25f - 1.0f));
	EXPECT_FLOAT_EQ(surf_revol::Profile(7, 0.3f).x, 0.3f); // any other index: the disk
}

TEST(SurfRevol, TeleportVortexMesh)
{
	// SF_TeleportVortex: NumU 12, NumV 5, disk, FadeAlphas, fade in / out 0.4, ChangeSpecColor, player 1's colour
	const uint32_t red = surf_revol::PlayerColour(0);
	EXPECT_EQ(red, 0xFFFF4646u);
	EXPECT_EQ(surf_revol::PlayerColour(7), 0xFF000000u);
	const auto mesh = surf_revol::Build(12, 5, 0, true, 0.4f, 0.4f, true, red);
	ASSERT_EQ(mesh.positions.size(), 60u);
	EXPECT_EQ(mesh.indices.size(), static_cast<size_t>((5 - 1) * (2 * 12 - 2) * 3));
	// the centre row: radius 0, black and opaque, the specular is the whole player colour
	EXPECT_NEAR(glm::length(mesh.positions[0]), 0.0f, 1e-6f);
	EXPECT_EQ(mesh.colours[0], 0xFF000000u);
	EXPECT_EQ(mesh.speculars[0], 0xFFFE4545u); // x 255 >> 8
	// row 1 (t 0.25): RGB 255 x 0.25 / 0.4 = 159; row 2 (t 0.5): white opaque, no specular
	EXPECT_EQ(mesh.colours[12], 0xFF9F9F9Fu);
	EXPECT_EQ(mesh.colours[24], 0xFFFFFFFFu);
	EXPECT_EQ(mesh.speculars[24] & 0x00FFFFFFu, 0u);
	// row 3 (t 0.75): alpha 255 (1 - 0.15 / 0.4) = 159; the rim (t 1): alpha 0
	EXPECT_EQ(mesh.colours[36] >> 24, 159u);
	EXPECT_EQ(mesh.colours[48] >> 24, 0u);
	// the rim is the unit circle, u 0..1 around it (the seam doubled)
	EXPECT_NEAR(mesh.positions[48].x, 1.0f, 1e-5f);
	EXPECT_NEAR(glm::length(mesh.positions[59] - mesh.positions[48]), 0.0f, 1e-4f);
	EXPECT_FLOAT_EQ(mesh.uvs[59].x, 1.0f);
	EXPECT_FLOAT_EQ(mesh.uvs[59].y, 1.0f);
	// the first triangles: (U + i, i, U + 1 + i), (U + 1 + i, i, i + 1)
	EXPECT_EQ(mesh.indices[0], 12);
	EXPECT_EQ(mesh.indices[1], 0);
	EXPECT_EQ(mesh.indices[2], 13);
	EXPECT_EQ(mesh.indices[3], 13);
	EXPECT_EQ(mesh.indices[4], 0);
	EXPECT_EQ(mesh.indices[5], 1);
}

TEST(SurfRevol, Twists)
{
	auto mesh = surf_revol::Build(4, 3, 0, false, 0.4f, 0.4f, false, 0);
	EXPECT_EQ(mesh.colours[0], 0xFFFFFFFFu); // no FadeAlphas: white, specular 0
	EXPECT_EQ(mesh.speculars[0], 0u);
	const auto uvs = mesh.uvs;
	const auto positions = mesh.positions;
	// TwistUVs: u += (1 - t)^2 x MaxUVChange (1) x 1 -> the centre row +1, the middle +0.25, the rim 0
	surf_revol::TwistUVs(mesh, uvs, 1.0f, 1.0f);
	EXPECT_FLOAT_EQ(mesh.uvs[0].x, uvs[0].x + 1.0f);
	EXPECT_FLOAT_EQ(mesh.uvs[4].x, uvs[4].x + 0.25f);
	EXPECT_FLOAT_EQ(mesh.uvs[8].x, uvs[8].x);
	EXPECT_FLOAT_EQ(mesh.uvs[4].y, uvs[4].y);
	// TwistVertices: row j turned about Y by t_j x MaxVertexChange (0.35): the rim's first vertex -> (cos, 0, sin)
	surf_revol::TwistVertices(mesh, positions, 0.35f, 1.0f);
	EXPECT_NEAR(mesh.positions[8].x, std::cos(0.35f), 1e-5f);
	EXPECT_NEAR(mesh.positions[8].z, std::sin(0.35f), 1e-5f);
	EXPECT_NEAR(glm::length(mesh.positions[4] - positions[4] * 1.0f),
	            glm::length(positions[4]) * 2.0f * std::sin(0.175f / 2.0f), 1e-5f);
	// amount 0 gives the saved mesh back
	surf_revol::TwistVertices(mesh, positions, 0.35f, 0.0f);
	EXPECT_NEAR(mesh.positions[8].x, 1.0f, 1e-6f);
	// ScaleUVs: TextureWidth / 256, TextureHeight / 256
	surf_revol::ScaleUVs(mesh, 2.0f, 0.5f);
	EXPECT_FLOAT_EQ(mesh.uvs[11].y, 0.5f);
}
