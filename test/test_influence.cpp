/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Influence/InfluenceState.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// GInfluenceInfo from info.dat: 0.4, 0.2, 0.2
GInfluenceInfo ShippedInfluenceInfo()
{
	return {0.4f, 0.2f, 0.2f};
}

class InfluenceTest: public ::testing::Test
{
protected:
	static void SetLand(int32_t land) { influence::detail::MapGlobals().landNumber = land; }
	static void SetTownMultiplier(float value) { influence::detail::MapGlobals().townInfluenceMultiplier = value; }
	static void SetPlayerMultiplier(float value) { influence::detail::MapGlobals().playerInfluenceMultiplier = value; }

	void SetUp() override
	{
		influence::detail::MapGlobals() = MapScriptGlobals {};
		auto info = std::make_unique<InfoConstants>();
		info->influence = ShippedInfluenceInfo();
		info->town.influence = 25.0f;
		info->town.storyInfluence = {25.0f, 25.0f, 25.0f, 50.0f, 25.0f};
		info->citadelHeart.influence = 125.0f;
		info->citadelHeart.storyInfluence = {750.0f, 450.0f, 250.0f, 450.0f, 450.0f};
		auto& house = info->abode.at(0);
		house.abodeNumber = AbodeNumber::A;
		house.tribeType = Tribe::NORSE;
		house.influence = 5.0f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		Locator::infoConstants::reset();
	}

	/// A town with that influence radius, and the circles rebuilt at once (the dirty byte set, turn 0)
	static entt::entity MakeTownCircle(uint32_t id, const glm::vec3& position, PlayerNames owner, float radius)
	{
		const auto town = MakeTown(id, position, owner);
		Locator::entitiesRegistry::value().Get<TownInfluence>(town).radius = radius;
		game_clock::SetTurn(0);
		influence::ForceNeedUpdateInfluence();
		influence::Update3DInfluence();
		return town;
	}

	static entt::entity MakeTown(uint32_t id, const glm::vec3& position, PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, id).owner = owner;
		registry.Assign<Tribe>(town, Tribe::NORSE);
		registry.Assign<Transform>(town, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<TownInfluence>(town);
		registry.Context().towns.insert({id, town});
		return town;
	}
};
} // namespace

TEST(Influence, rangeGradient)
{
	// r = 100: 1 up to 40, 0.8 -> 0 up to 60, 0.2 -> 0 up to 100
	const auto info = ShippedInfluenceInfo();
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(0.0f, 100.0f, info), 1.0f);
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(40.0f, 100.0f, info), 1.0f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(40.001f, 100.0f, info), 0.8f, 1e-3f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(50.0f, 100.0f, info), 0.4f, 1e-5f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(60.0f, 100.0f, info), 0.0f, 1e-5f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(60.001f, 100.0f, info), 0.2f, 1e-4f);
	EXPECT_NEAR(influence::CalculateInfluenceOnRange(80.0f, 100.0f, info), 0.1f, 1e-5f);
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(100.0f, 100.0f, info), 0.0f);
	EXPECT_FLOAT_EQ(influence::CalculateInfluenceOnRange(150.0f, 100.0f, info), 0.0f);
}

TEST_F(InfluenceTest, townRadiusAndPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	SetLand(4);
	SetTownMultiplier(0.8f);
	const auto town = MakeTown(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE);
	// a house of scale 2 with no villagers: 5 x 2 x 1 x (0 + 0 + 1) = 10
	const auto house = registry.Create();
	registry.Assign<Abode>(house, AbodeNumber::A, 0u, 0u, 0u);
	registry.Assign<Transform>(house, glm::vec3(1010.0f, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(2.0f));
	influence::ProcessTowns();
	// (story[3] = 50 + 10) x 0.8
	EXPECT_FLOAT_EQ(influence::TownRadius(town), 48.0f);
	// inside: the town gives its radius, clamped to 1; outside (strict d < r) and for another player: 0
	EXPECT_FLOAT_EQ(influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1040.0f, 0.0f, 1000.0f)), 1.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1048.0f, 0.0f, 1000.0f)), 0.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_TWO, glm::vec3(1000.0f, 0.0f, 1000.0f)), 0.0f);
	// the height does not count (GetDistanceInMetres is x,z)
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1000.0f, 500.0f, 1040.0f)), 1.0f);
}

TEST_F(InfluenceTest, citadelStoryInfluence)
{
	auto& registry = Locator::entitiesRegistry::value();
	SetLand(1);
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple, PlayerNames::PLAYER_ONE);
	registry.Assign<Transform>(temple, glm::vec3(2000.0f, 0.0f, 2000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FLOAT_EQ(influence::CitadelRadius(temple), 750.0f);
	// fixed with the heart: a later multiplier only scales it
	SetPlayerMultiplier(0.5f);
	EXPECT_FLOAT_EQ(influence::CitadelRadius(temple), 375.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(2000.0f, 0.0f, 2374.0f)), 1.0f);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(2000.0f, 0.0f, 2376.0f)), 0.0f);
}

TEST_F(InfluenceTest, ringsAndAntiRings)
{
	const glm::vec3 centre(500.0f, 0.0f, 500.0f);
	const auto ring = influence::CreateRing(centre, PlayerNames::PLAYER_ONE, 100.0f, false);
	EXPECT_NEAR(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, centre + glm::vec3(50.0f, 0.0f, 0.0f)), 0.4f,
	            1e-5f);
	// two rings add up and clamp to 1
	influence::CreateRing(centre + glm::vec3(20.0f, 0.0f, 0.0f), PlayerNames::PLAYER_ONE, 100.0f, false);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, centre + glm::vec3(50.0f, 0.0f, 0.0f)), 1.0f);
	// an anti ring of the player makes it 0 (a shield's ring for its enemies)
	influence::CreateRing(centre, PlayerNames::PLAYER_ONE, 10.0f, true);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, centre), 0.0f);
	EXPECT_TRUE(influence::IsInAntiInfluence(PlayerNames::PLAYER_ONE, centre));
	EXPECT_FALSE(influence::IsInAntiInfluence(PlayerNames::PLAYER_TWO, centre));
	EXPECT_FALSE(influence::IsInAntiInfluence(PlayerNames::PLAYER_ONE, centre + glm::vec3(11.0f, 0.0f, 0.0f)));
	influence::DeleteRing(ring);
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(ring));
}

TEST_F(InfluenceTest, ringFollowsItsObject)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	registry.Assign<Transform>(object, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto ring = influence::CreateRingOnObject(object, PlayerNames::PLAYER_ONE, 30.0f, false);
	ASSERT_TRUE(ring != entt::null);
	registry.Get<Transform>(object).position = glm::vec3(300.0f, 0.0f, 100.0f);
	influence::ProcessRings();
	EXPECT_FLOAT_EQ(registry.Get<InfluenceRing>(ring).position.x, 300.0f);
	registry.Destroy(object);
	influence::ProcessRings();
	EXPECT_FALSE(registry.Valid(ring));
}

TEST_F(InfluenceTest, everywhere)
{
	influence::SetInfluenceEverywhere(true);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_THREE, glm::vec3(0.0f)), 1.0f);
	influence::SetInfluenceEverywhere(false);
	EXPECT_FLOAT_EQ(influence::CalculatePlayerInfluence(PlayerNames::PLAYER_THREE, glm::vec3(0.0f)), 0.0f);
}

TEST_F(InfluenceTest, influencePowerAndRatio)
{
	// the citadel's influence + the towns' radii + the player's rings'
	// radius (anti ones too); the ratio: the active players' and the neutral one's sum over the own
	auto& registry = Locator::entitiesRegistry::value();
	SetLand(1);
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple, PlayerNames::PLAYER_ONE);
	registry.Assign<Transform>(temple, glm::vec3(2000.0f, 0.0f, 2000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	influence::CreateRing(glm::vec3(0.0f), PlayerNames::PLAYER_ONE, 100.0f, false);
	influence::CreateRing(glm::vec3(0.0f), PlayerNames::PLAYER_ONE, 10.0f, true);
	influence::CreateRing(glm::vec3(0.0f), PlayerNames::PLAYER_TWO, 40.0f, false);
	influence::CreateRing(glm::vec3(0.0f), PlayerNames::PLAYER_THREE, 1000.0f, false);
	for (const auto name : {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO})
	{
		registry.Assign<Player>(registry.Create(), name); // the land made them: active
	}
	EXPECT_FLOAT_EQ(influence::InfluencePower(PlayerNames::PLAYER_ONE), 0.0f); // before the first turn
	influence::CalculateInfluencePowers();
	EXPECT_FLOAT_EQ(influence::InfluencePower(PlayerNames::PLAYER_ONE), 860.0f); // 750 (Land 1) + 100 + 10
	EXPECT_FLOAT_EQ(influence::InfluencePower(PlayerNames::PLAYER_TWO), 40.0f);
	EXPECT_FLOAT_EQ(influence::InfluencePower(PlayerNames::PLAYER_THREE), 1000.0f);
	// PLAYER_THREE was not made: not summed
	EXPECT_FLOAT_EQ(influence::InfluencePowerRatio(PlayerNames::PLAYER_ONE), 900.0f / 860.0f);
	EXPECT_FLOAT_EQ(influence::InfluencePowerRatio(PlayerNames::PLAYER_TWO), 900.0f / 40.0f);
	EXPECT_FLOAT_EQ(influence::InfluencePowerRatio(PlayerNames::PLAYER_FOUR), 0.0f); // its own is 0
}

TEST_F(InfluenceTest, circlesRebuildEveryTenTurns)
{
	// a move over 0.01 sets the dirty flag; Update3DInfluence only rebuilds on a turn that
	// is a multiple of 10, then writes the radii it drew
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = MakeTown(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE);
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple, PlayerNames::PLAYER_TWO);
	registry.Assign<Transform>(temple, glm::vec3(3000.0f, 0.0f, 3000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	game_clock::SetTurn(3);
	influence::ProcessTurn(); // the town's 25 and the citadel's 125 against 0: dirty
	EXPECT_TRUE(influence::detail::GlobalsOrDefault().circlesDirty);
	influence::Update3DInfluence();
	EXPECT_TRUE(influence::Circles().empty()); // turn 3
	game_clock::SetTurn(10);
	influence::Update3DInfluence();
	ASSERT_EQ(influence::Circles().size(), 2u);
	// player by player from the first, its citadel then its towns, every Add at the head: the newest first
	EXPECT_EQ(influence::Circles()[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(influence::Circles()[0].radius, 125.0f);
	EXPECT_EQ(influence::Circles()[1].player, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(registry.Get<TownInfluence>(town).drawnRadius, 25.0f);
	EXPECT_FLOAT_EQ(registry.Get<CitadelInfluence>(temple).drawnRadius, 125.0f);
	EXPECT_FALSE(influence::detail::GlobalsOrDefault().circlesDirty);
	// the next turn: nothing moved
	influence::ProcessTurn();
	EXPECT_FALSE(influence::detail::GlobalsOrDefault().circlesDirty);
	// strictly more than 0.01
	influence::NoteInfluence(25.005f, 25.0f);
	EXPECT_FALSE(influence::detail::GlobalsOrDefault().circlesDirty);
	influence::NoteInfluence(25.02f, 25.0f);
	EXPECT_TRUE(influence::detail::GlobalsOrDefault().circlesDirty);
	// the citadel's player has a temple: its border is shown ((inferred) openblack has no temple fade); the town's not
	EXPECT_TRUE(influence::BoundaryShown(PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(influence::BoundaryShown(PlayerNames::PLAYER_ONE));
	game_clock::SetTurn(0);
}

TEST_F(InfluenceTest, circlesOverlapOfOnePlayer)
{
	// a circle inside another one of the same player is deleted
	MakeTownCircle(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE, 100.0f);
	MakeTownCircle(1, glm::vec3(1010.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE, 20.0f);
	ASSERT_EQ(influence::Circles().size(), 1u);
	EXPECT_FLOAT_EQ(influence::Circles()[0].radius, 100.0f);
	// another player's circle across its edge: left alone, and so is the first one
	MakeTownCircle(2, glm::vec3(1090.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_TWO, 50.0f);
	ASSERT_EQ(influence::Circles().size(), 2u);
	for (const auto& circle : influence::Circles())
	{
		for (const auto hidden : circle.hidden)
		{
			EXPECT_EQ(hidden, 0);
		}
	}
}

TEST_F(InfluenceTest, circlesHideTheirContact)
{
	// the columns inside the other circle of the player go transparent; the closing column N turns white
	MakeTownCircle(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE, 50.0f);
	MakeTownCircle(1, glm::vec3(1060.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE, 50.0f);
	const auto circles = influence::Circles();
	ASSERT_EQ(circles.size(), 2u);
	const auto& older = circles[1]; // the town at x = 1000: its columns around angle 0 are inside the other
	const auto n = older.Segments();
	ASSERT_EQ(older.hidden.size(), n + 1);
	EXPECT_EQ(older.hidden.front(), 1);
	EXPECT_EQ(older.hidden.back(), 1);
	EXPECT_EQ(older.hidden.at(n / 2), 0);                    // angle pi, outside
	EXPECT_EQ(older.curtain.colours.at(3 * n), 0x00FFFFFFu); // the white closing column
	EXPECT_EQ(older.curtain.colours.at(0), influence::k_CircleColours[0]);
	const auto& newer = circles[0];
	EXPECT_EQ(newer.hidden.at(newer.Segments() / 2), 1); // its column at angle pi is inside the older one
	EXPECT_EQ(newer.hidden.back(), 0);
	// the distance is 3D: a smaller circle far above the other is not inside it
	MakeTownCircle(2, glm::vec3(3000.0f, 0.0f, 3000.0f), PlayerNames::PLAYER_THREE, 100.0f);
	MakeTownCircle(3, glm::vec3(3010.0f, 200.0f, 3000.0f), PlayerNames::PLAYER_THREE, 20.0f);
	EXPECT_EQ(influence::Circles().size(), 4u);
}

TEST_F(InfluenceTest, curtainAlphaAndLatch)
{
	// the curtain: nothing at or under y = 100; 120 from 200 up; the ramp truncated between
	EXPECT_FALSE(influence::CurtainAlpha(100.0f).has_value());
	EXPECT_EQ(influence::CurtainAlpha(200.0f).value_or(0), 120);
	EXPECT_EQ(influence::CurtainAlpha(1000.0f).value_or(0), 120);
	EXPECT_EQ(influence::CurtainAlpha(199.9f).value_or(0), 119);
	EXPECT_NEAR(influence::CurtainAlpha(150.0f).value_or(0), 60, 1);
	// only the middle row, and only once the player's border is shown
	MakeTownCircle(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE, 50.0f);
	influence::SetCurtainAlpha(120);
	EXPECT_EQ(influence::Circles()[0].curtain.colours.at(1) >> 24, 0u);
	influence::ShowBoundary(PlayerNames::PLAYER_ONE);
	influence::SetCurtainAlpha(120);
	const auto& circle = influence::Circles()[0];
	EXPECT_EQ(circle.alphaCache, 120u);
	EXPECT_EQ(circle.curtain.colours.at(1), 0x78000000u | influence::k_CircleColours[0]);
	EXPECT_EQ(circle.curtain.colours.at(0) >> 24, 0u);
	EXPECT_EQ(circle.curtain.colours.at(2) >> 24, 0u);
}

TEST_F(InfluenceTest, handCrossingMakesARipple)
{
	// the edge by halves
	const auto edge = influence::detail::CrossingPoint(glm::vec3(0.0f), 50.0f, glm::vec3(0.0f), glm::vec3(100.0f, 0.0f, 0.0f));
	EXPECT_NEAR(edge.x, 50.0f, 1.0f);
	EXPECT_FLOAT_EQ(edge.z, 0.0f);
	// a ripple only when the player's border is shown
	influence::detail::ResetHandCrossing();
	MakeTownCircle(0, glm::vec3(1000.0f, 0.0f, 1000.0f), PlayerNames::PLAYER_ONE, 50.0f);
	EXPECT_FALSE(influence::HandCrossedInfluence(glm::vec3(1100.0f, 0.0f, 1000.0f)));
	EXPECT_FALSE(influence::HandCrossedInfluence(glm::vec3(1000.0f, 0.0f, 1000.0f)));
	EXPECT_TRUE(influence::Ripples().empty());
	influence::ShowBoundary(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(influence::HandCrossedInfluence(glm::vec3(1100.0f, 5.0f, 1000.0f)));
	ASSERT_EQ(influence::Ripples().size(), 1u);
	const auto& ripple = influence::Ripples()[0];
	EXPECT_NEAR(ripple.point.x, 1050.0f, 1.0f);
	EXPECT_FLOAT_EQ(ripple.point.y, 5.0f); // max(the land's 0, the hand's 5)
	EXPECT_EQ(ripple.life, 2000);
	EXPECT_EQ(ripple.colour, influence::k_CircleColours[0]);
	EXPECT_NEAR(ripple.matrix[0].z, 1.0f, 1e-3f); // at angle 0 the border's tangent is +z
	EXPECT_FLOAT_EQ(ripple.sizes[0], 0.0001f);
	EXPECT_FLOAT_EQ(ripple.sizes[3], 6.0f);
	// the life, the fade under 1000 ms, the growth and the wrap at 14
	influence::UpdateRipples(1500);
	EXPECT_EQ(influence::Ripples()[0].life, 500);
	const auto sprites = influence::DrawRipple(0, 100); // g = 1
	EXPECT_FLOAT_EQ(sprites[3].size, 7.0f);
	EXPECT_EQ(sprites[3].argb >> 24, 63u); // (1 - 7 / 14) x 0.5 x 255, truncated
	EXPECT_NEAR(sprites[6].size, 13.0f, 1e-5f);
	(void)influence::DrawRipple(0, 200); // sprite 6: 15 -> 1
	EXPECT_NEAR(influence::Ripples()[0].sizes[6], 1.0f, 1e-5f);
	EXPECT_EQ(influence::Ripples()[0].flags[6], 0);
	influence::UpdateRipples(501);
	EXPECT_TRUE(influence::Ripples().empty());
}
