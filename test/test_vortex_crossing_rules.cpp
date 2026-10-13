/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Magic/VortexCrossingRules.h"
#include "Magic/VortexPullRules.h"

using namespace openblack;
using namespace openblack::vortex::crossing;

namespace
{
map_coords::MapCoords Offset(float x, float z)
{
	return {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z)};
}

float FlatGround(glm::vec2 /*xz*/)
{
	return 10.0f;
}
} // namespace

TEST(VortexCrossing, TheFullReachSweepsTheSquareOfSevenCells)
{
	const auto centre = map_coords::FromMetres({1005.0f, 2005.0f});
	const auto cells = vortex::CellsSearched(centre, 50.0f);
	// The 7 by 7 square about the vortex's cell, out to 42.4 m at its corners; the next ring's first cell, 4 across and 3
	// along, measures a hair over 50 m by the game's distances, so the search stops there
	ASSERT_EQ(cells.size(), 49u);
	const auto has = [&cells](int dx, int dz) {
		return std::ranges::find(cells, glm::ivec2(100 + dx, 200 + dz)) != cells.end();
	};
	EXPECT_TRUE(has(3, 3));
	EXPECT_TRUE(has(-3, -3));
	for (int d = -3; d <= 3; ++d)
	{
		// A thing 40 to 45 m away is never reached
		EXPECT_FALSE(has(-4, d));
		EXPECT_FALSE(has(4, d));
		EXPECT_FALSE(has(d, 4));
		EXPECT_FALSE(has(d, -4));
	}
	// The half reach: the 3 by 3 square and three cells of the -x side
	EXPECT_EQ(vortex::CellsSearched(centre, 25.0f).size(), 12u);
}

TEST(VortexCrossing, APileGivesUpAPotOfASmallerSize)
{
	EXPECT_FLOAT_EQ(PileShare(0.0f), 0.3f);
	EXPECT_FLOAT_EQ(PileShare(k_PileShareRandom), 0.42f);
	EXPECT_EQ(k_PileTake, 1000u);
}

TEST(VortexCrossing, AThingJustTakenStaysWhereItWas)
{
	const AttractAtom atom {.position = {40.0f, 12.0f, 0.0f}, .startHeight = 2.0f, .age = 0.0f};
	const auto step = Attract({}, atom, glm::vec3(0.0f), 0.1f, FlatGround);
	ASSERT_TRUE(step.has_value());
	EXPECT_FLOAT_EQ(step->position.x, 40.0f);
	EXPECT_FLOAT_EQ(step->position.y, 12.0f);
	EXPECT_FLOAT_EQ(step->position.z, 0.0f);
	EXPECT_FLOAT_EQ(step->velocity.y, 0.0f);
}

TEST(VortexCrossing, ABlendedThingSpiralsInOntoTheFunnel)
{
	const AttractAtom atom {.position = {40.0f, 50.0f, 0.0f}, .startHeight = 2.0f, .age = 8.0f};
	const auto step = Attract({}, atom, glm::vec3(0.0f), 0.1f, [](glm::vec2) { return 0.0f; });
	ASSERT_TRUE(step.has_value());
	// A tenth of its distance gone, swung round by 3 / 36 of a radian, and on the funnel: sqrt(36 * 640) - 80 above
	// the land
	const float angle = 3.0f / 36.0f;
	EXPECT_NEAR(step->position.x, 36.0f * std::cos(angle), 1e-4f);
	EXPECT_NEAR(step->position.z, 36.0f * std::sin(angle), 1e-4f);
	EXPECT_NEAR(step->position.y, std::sqrt(36.0f * 640.0f) - 80.0f, 1e-3f);
}

TEST(VortexCrossing, AtTheMiddleItIsGoneOnceBlendedIn)
{
	const AttractSettings settings;
	// Inside the deletion radius and fully blended: gone
	EXPECT_FALSE(Attract(settings, {.position = {3.5f, 0.0f, 0.0f}, .age = 8.0f}, glm::vec3(0.0f), 0.1f, FlatGround));
	// Half blended, it stays, drawn in by half the step; one that can't be destroyed goes from half way
	const auto waiting = Attract(settings, {.position = {3.5f, 0.0f, 0.0f}, .age = 4.0f}, glm::vec3(0.0f), 0.1f, FlatGround);
	ASSERT_TRUE(waiting.has_value());
	EXPECT_NEAR(glm::length(glm::vec2(waiting->position.x, waiting->position.z)), 3.325f, 1e-3f);
	// Never nearer than the least radius
	const auto least = Attract(settings, {.position = {3.0f, 0.0f, 0.0f}, .age = 4.0f}, glm::vec3(0.0f), 0.1f, FlatGround);
	ASSERT_TRUE(least.has_value());
	EXPECT_NEAR(glm::length(glm::vec2(least->position.x, least->position.z)), 3.0f, 1e-3f);
	EXPECT_FALSE(Attract(settings, {.position = {3.5f, 0.0f, 0.0f}, .age = 4.0f, .indestructible = true}, glm::vec3(0.0f), 0.1f,
	                     FlatGround));
	// Outside it, it stays whatever its age
	EXPECT_TRUE(Attract(settings, {.position = {30.0f, 0.0f, 0.0f}, .age = 20.0f}, glm::vec3(0.0f), 0.1f, FlatGround));
}

TEST(VortexCrossing, AThingThatCantBeDestroyedIsThrownBackOut)
{
	const auto throwOut = ThrowOutOf(0.0f, 0.0f, 0.0f, 2.0f, 4.0f);
	EXPECT_FLOAT_EQ(throwOut.velocity.x, 0.0f);
	EXPECT_FLOAT_EQ(throwOut.velocity.y, 20.0f);
	EXPECT_FLOAT_EQ(throwOut.velocity.z, -16.0f);
	EXPECT_FLOAT_EQ(throwOut.spin.x, 8.0f);
	EXPECT_FLOAT_EQ(throwOut.spin.z, 0.0f);
}

TEST(VortexCrossing, TheCommandsAreWrittenAsTheLandScriptsRead)
{
	EXPECT_EQ(CommandFormat("CREATE_POT", "ANNN"), "CREATE_POT(%s, %d, %d, %d)\n");
	EXPECT_EQ(CommandFormat("CREATE_DEAD_TREE", "ALNFFFF"), "CREATE_DEAD_TREE(%s, \"%s\", %d, %f, %f, %f, %f)\n");
	EXPECT_EQ(PlaceText(Offset(-20.0f, 5.0f)), "\"-20.00,5.00\"");
}

TEST(VortexCrossing, EachKindWritesItsLine)
{
	EXPECT_EQ(Text(VillagerLine {.town = 2, .offset = Offset(10.0f, 20.0f), .villagerType = "CELTIC_FARMER", .age = 25}),
	          "CREATE_TOWN_VILLAGER(2, \"10.00,20.00\", CELTIC_FARMER, 25)\n");
	EXPECT_EQ(Text(SpecialVillagerLine {.town = -1, .offset = Offset(0.0f, 0.0f), .row = 7, .age = 40}),
	          "CREATE_SPECIAL_TOWN_VILLAGER(-1, \"0.00,0.00\", 7, 40)\n");
	EXPECT_EQ(Text(AnimalLine {.offset = Offset(-5.0f, 0.0f), .type = 3, .flock = -1, .town = 1, .age = 2}),
	          "CREATE_NEW_ANIMAL(\"-5.00,0.00\", 3, -1, 1, 2)\n");
	EXPECT_EQ(Text(PotLine {.offset = Offset(10.0f, -10.0f), .row = 3, .resource = 1, .amount = 1000}),
	          "CREATE_POT(\"10.00,-10.00\", 3, 1, 1000)\n");
	EXPECT_EQ(Text(TreeLine {.offset = Offset(0.0f, 0.0f), .row = 4, .yAngle = 1.5f, .scale = 0.8f, .maxScale = 1.0f}),
	          "CREATE_NEW_TREE(-1, \"0.00,0.00\", 4, 0, 1.500000, 0.800000, 1.000000)\n");
	EXPECT_EQ(Text(MobileStaticLine {.offset = Offset(0.0f, 0.0f), .row = 9, .yAngle = 0.25f, .scale = 2.0f}),
	          "CREATE_MOBILE_STATIC(\"0.00,0.00\", 9, 0.000000, 0.000000, 0.250000, 0.000000, 2.000000)\n");
	EXPECT_EQ(Text(MobileObjectLine {.offset = Offset(0.0f, 0.0f), .row = 6, .yAngle = 1.2345f, .scale = 1.0f}),
	          "CREATE_MOBILEOBJECT(\"0.00,0.00\", 6, 1234, 1000)\n");
	EXPECT_EQ(Text(DeadTreeLine {.offset = Offset(0.0f, 0.0f), .player = "PLAYER_ONE", .row = 2, .life = 1.0f, .yAngle = 0.5f}),
	          "CREATE_DEAD_TREE(\"0.00,0.00\", \"PLAYER_ONE\", 2, 1.000000, 0.000000, 0.500000, 0.000000)\n");
	EXPECT_EQ(Text(OneOffSpellLine {.offset = Offset(-10.0f, 0.0f), .magic = "FOOD"}),
	          "CREATE_ONE_SHOT_SPELL_PU(\"-10.00,0.00\", \"FOOD\")\n");
}

TEST(VortexCrossing, ThingsComeOutAboutSixteenMetresAwayByWholeCells)
{
	// A vortex at cell (100, 200), 3.5 m and 7.25 m into it
	const auto vortex = map_coords::FromMetres({1003.5f, 2007.25f});
	const auto fraction = [](int32_t fixed) { return static_cast<uint32_t>(fixed) & 0xffffu; };
	const auto cell = [](int32_t fixed) { return static_cast<uint32_t>(fixed) >> 16u; };
	// Straight along -z: 16 m out is 1.6 cells, cut to one whole cell back from 198.4
	const auto north = ThrowOutPlace(vortex, 0.0f);
	EXPECT_EQ(cell(north.x), 100u);
	EXPECT_EQ(cell(north.z), 198u);
	EXPECT_EQ(fraction(north.x), fraction(vortex.x));
	EXPECT_EQ(fraction(north.z), fraction(vortex.z));
	// A quarter turn: along +x, 101.6 cut to 101
	const auto east = ThrowOutPlace(vortex, glm::half_pi<float>());
	EXPECT_EQ(cell(east.x), 101u);
	EXPECT_EQ(cell(east.z), 200u);
}

TEST(VortexCrossing, TheFadeWaitsForTheFileAndThirtyVillagers)
{
	EXPECT_TRUE(HoldsFade(true, 40));
	EXPECT_TRUE(HoldsFade(false, 29));
	EXPECT_FALSE(HoldsFade(false, 30));
}

TEST(VortexCrossing, OnlyAVortexIntoTheLandKeepsWhatItThrew)
{
	const std::array thrown {entt::entity {3}, entt::entity {7}};
	EXPECT_TRUE(KeepsThrown(VortexType::Out, thrown, entt::entity {7}));
	EXPECT_FALSE(KeepsThrown(VortexType::Out, thrown, entt::entity {5}));
	EXPECT_FALSE(KeepsThrown(VortexType::In, thrown, entt::entity {7}));
	EXPECT_FALSE(KeepsThrown(VortexType::Volcano, thrown, entt::entity {7}));
}

TEST(VortexCrossing, EveryFifthNewVillagerIsAChild)
{
	EXPECT_EQ(NewVillagerAgeLimit(5), 9u);
	EXPECT_EQ(NewVillagerAge(5, 0), 1u);
	EXPECT_EQ(NewVillagerAge(10, 8), 9u);
	EXPECT_EQ(NewVillagerAgeLimit(1), 6u);
	EXPECT_EQ(NewVillagerAge(1, 0), 16u);
	EXPECT_EQ(NewVillagerAge(4, 5), 21u);
}

TEST(VortexCrossing, HalfTheNewVillagersAreHousewives)
{
	// Norse (7) rows start at 49: the housewife, then forester, fisherman, farmer, shepherd and leader
	EXPECT_EQ(NewVillagerRow(7, 0.49f, 4), 49);
	EXPECT_EQ(NewVillagerRow(7, 0.5f, 0), 50);
	EXPECT_EQ(NewVillagerRow(7, 0.9f, 4), 54);
	EXPECT_EQ(NewVillagerRow(0, 0.7f, 2), 3);
}
