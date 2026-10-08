/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the Creature Spawner debug window works out before it acts, with hand-made values

#include <cmath>

#include <bitset>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Debug/CreatureSpawnerModel.h"

using namespace openblack;
using namespace openblack::debug::creature_spawner;
using creature_desires::Desire;

namespace
{
constexpr float k_Tolerance = 1e-4f;

using Known = std::bitset<creature_leash::k_Types.size()>;

/// Desires all switched off, but for the ones given their values
creature_desires::Desires DesiresOf(std::initializer_list<std::pair<Desire, float>> values)
{
	creature_desires::Desires desires;
	for (auto& state : desires.desires)
	{
		state.activated = false;
	}
	for (const auto& [desire, value] : values)
	{
		desires[desire].activated = true;
		desires[desire].value = value;
	}
	return desires;
}
} // namespace

TEST(CreatureSpawnerModel, NamesTheSpeciesFromTheCow)
{
	EXPECT_EQ(SpeciesName(CreatureType::Cow), "Cow");
	EXPECT_EQ(SpeciesName(CreatureType::GiantApe), "Giant Ape");
	EXPECT_EQ(SpeciesName(CreatureType::Unknown), "Unknown");
	EXPECT_EQ(SpeciesName(CreatureType::_COUNT), "Unknown");
	EXPECT_EQ(SpeciesAt(0), CreatureType::Cow);
	EXPECT_EQ(SpeciesAt(k_SpeciesCount - 1), CreatureType::GiantApe);
}

TEST(CreatureSpawnerModel, NamesTheOwners)
{
	EXPECT_EQ(OwnerName(PlayerNames::PLAYER_ONE), "Player One");
	EXPECT_EQ(OwnerName(PlayerNames::NEUTRAL), "Neutral");
	EXPECT_EQ(OwnerName(PlayerNames::_COUNT), "Nobody");
}

TEST(CreatureSpawnerModel, DrawnScaleIsThePosesWhenItHasOne)
{
	EXPECT_EQ(DrawnScale(std::nullopt, glm::vec3(1.5f)), glm::vec3(1.5f));
	EXPECT_EQ(DrawnScale(glm::vec3(0.22f), glm::vec3(1.5f)), glm::vec3(0.22f));
}

TEST(CreatureSpawnerModel, PhaseStaysWithinTheStages)
{
	EXPECT_EQ(ClampPhase(-3, 13), 0);
	EXPECT_EQ(ClampPhase(7, 13), 7);
	EXPECT_EQ(ClampPhase(20, 13), 13);
}

TEST(CreatureSpawnerModel, ListsTheKnownLeashesInTheGamesOrder)
{
	EXPECT_EQ(KnownLeashes(Known {}), "none");
	Known known;
	known.set(2);
	known.set(0);
	EXPECT_EQ(KnownLeashes(known), std::string(creature_leash::Name(creature_leash::k_Types[0])) + ", " +
	                                   creature_leash::Name(creature_leash::k_Types[2]));
}

TEST(CreatureSpawnerModel, DesiresStrongestFirstEqualOnesInTheGamesOrder)
{
	const auto desires = DesiresOf({{Desire::Hunger, 0.2f}, {Desire::Anger, 0.7f}, {Desire::Impress, 0.2f}});
	const std::vector<Desire> expected {Desire::Anger, Desire::Impress, Desire::Hunger};
	EXPECT_EQ(DesiresByStrength(desires), expected);
}

TEST(CreatureSpawnerModel, LeavesOutDesiresSwitchedOffOrEmpty)
{
	auto desires = DesiresOf({{Desire::Play, 0.0f}, {Desire::Fear, 0.4f}, {Desire::Poo, 0.0f}});
	desires[Desire::Poo].sources.push_back({});
	desires[Desire::Curiosity].value = 0.9f; // switched off
	const std::vector<Desire> expected {Desire::Fear, Desire::Poo};
	EXPECT_EQ(DesiresByStrength(desires), expected);
}

TEST(CreatureSpawnerModel, SummarisesThePlayersCreature)
{
	const auto desires = DesiresOf({{Desire::Hunger, 0.5f}, {Desire::Anger, 0.9f}, {Desire::Play, 0.1f}});
	creature_physiology::Needs needs;
	needs.energy = 0.3f;
	Known known;
	known.set(1);
	const creature_planner::Plan plan {.desire = Desire::Anger, .action = 4, .priority = 2.0f};
	const auto summary = Summarise(
	    {
	        .entity = entt::entity {7},
	        .species = CreatureType::Lion,
	        .size = 1.2f,
	        .alignment = -0.5f,
	        .leashable = true,
	        .position = {10.0f, 2.0f, 30.0f},
	        .ownScale = glm::vec3(0.8f),
	        .poseScale = glm::vec3(0.3f),
	        .developmentPhase = 4u,
	        .home = glm::vec3(5.0f, 0.0f, 6.0f),
	        .wornLeash = LeashType::Rope,
	        .knownLeashes = known,
	        .desires = &desires,
	        .plan = plan,
	        .needs = &needs,
	    },
	    2);
	EXPECT_EQ(summary.entity, entt::entity {7});
	EXPECT_EQ(summary.species, CreatureType::Lion);
	EXPECT_FLOAT_EQ(summary.size, 1.2f);
	EXPECT_EQ(summary.drawnScale, glm::vec3(0.3f));
	EXPECT_EQ(summary.developmentPhase, 4u);
	ASSERT_TRUE(summary.home.has_value());
	EXPECT_EQ(*summary.home, glm::vec3(5.0f, 0.0f, 6.0f));
	EXPECT_EQ(summary.wornLeash, LeashType::Rope);
	EXPECT_EQ(summary.knownLeashes, creature_leash::Name(creature_leash::k_Types[1]));
	EXPECT_TRUE(summary.leashable);
	const std::vector<Desire> top {Desire::Anger, Desire::Hunger};
	EXPECT_EQ(summary.desires, top);
	ASSERT_TRUE(summary.plan.has_value());
	EXPECT_EQ(summary.plan->action, 4u);
	ASSERT_TRUE(summary.needs.has_value());
	EXPECT_FLOAT_EQ(summary.needs->energy, 0.3f);
}

TEST(CreatureSpawnerModel, SummaryOfACreatureWithoutMindLeashOrBody)
{
	const auto summary = Summarise({.species = CreatureType::Cow, .ownScale = glm::vec3(2.0f)}, 5);
	EXPECT_EQ(summary.drawnScale, glm::vec3(2.0f));
	EXPECT_FALSE(summary.developmentPhase.has_value());
	EXPECT_FALSE(summary.home.has_value());
	EXPECT_FALSE(summary.wornLeash.has_value());
	EXPECT_EQ(summary.knownLeashes, "none");
	EXPECT_TRUE(summary.desires.empty());
	EXPECT_FALSE(summary.plan.has_value());
	EXPECT_FALSE(summary.needs.has_value());
}

TEST(CreatureSpawnerModel, PointsAroundACreatureAcrossTheLand)
{
	const glm::vec3 at {100.0f, 5.0f, 200.0f};
	const glm::mat3 facing {1.0f};
	// Straight ahead is down -z
	const auto ahead = PointAround(at, facing, 0.0f, 10.0f);
	EXPECT_NEAR(ahead.x, 100.0f, k_Tolerance);
	EXPECT_NEAR(ahead.y, 5.0f, k_Tolerance);
	EXPECT_NEAR(ahead.z, 190.0f, k_Tolerance);
	const auto right = PointAround(at, facing, std::numbers::pi_v<float> / 2.0f, 10.0f);
	EXPECT_NEAR(right.x, 110.0f, k_Tolerance);
	EXPECT_NEAR(right.z, 200.0f, k_Tolerance);
	// Turned half round, ahead is behind; a tilt stays on the creature's height
	const auto turned = glm::mat3(glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float>, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto behind = PointAround(at, turned, 0.0f, 10.0f);
	EXPECT_NEAR(behind.z, 210.0f, k_Tolerance);
	const auto tilted = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.5f, glm::vec3(1.0f, 0.0f, 0.0f)));
	const auto flat = PointAround(at, tilted, 0.0f, 10.0f);
	EXPECT_NEAR(flat.y, 5.0f, k_Tolerance);
	EXPECT_NEAR(glm::distance(glm::vec2(flat.x, flat.z), glm::vec2(at.x, at.z)), 10.0f, k_Tolerance);
}

TEST(CreatureSpawnerModel, OnTheGround)
{
	EXPECT_EQ(OnGround({1.0f, 50.0f, 2.0f}, 7.5f), glm::vec3(1.0f, 7.5f, 2.0f));
}

TEST(CreatureSpawnerModel, RandomPicksAreTheWindowsOwnAndRepeat)
{
	RandomEngine first {k_RandomSeed};
	RandomEngine second {k_RandomSeed};
	for (int i = 0; i < 100; ++i)
	{
		const auto facing = RandomFacingDegrees(first);
		EXPECT_GE(facing, 0.0f);
		EXPECT_LT(facing, 360.0f);
		EXPECT_EQ(facing, RandomFacingDegrees(second));
		const auto texel = RandomSkinTexel(first);
		EXPECT_GE(texel, 16);
		EXPECT_LE(texel, 239);
		EXPECT_EQ(texel, RandomSkinTexel(second));
		const auto index = RandomIndex(first, 3);
		EXPECT_LT(index, 3u);
		EXPECT_EQ(index, RandomIndex(second, 3));
	}
}

TEST(CreatureSpawnerModel, NarrowsNamesToAscii)
{
	EXPECT_EQ(Narrow(u"Bobby"), "Bobby");
	EXPECT_EQ(Narrow(u"Ren\u00e9e"), "Ren?e");
	EXPECT_EQ(Narrow(u""), "");
}

TEST(CreatureSpawnerModel, MindFilesLeaveOutTheSavedBodies)
{
	EXPECT_TRUE(IsMindFileName("Tiger"));
	EXPECT_TRUE(IsMindFileName("MyCreature.erc"));
	EXPECT_FALSE(IsMindFileName("Physique0"));
	EXPECT_FALSE(IsMindFileName(""));
}
