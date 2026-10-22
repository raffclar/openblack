/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <span>

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/AlignmentSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace std::chrono_literals;

namespace
{
/// Each player's influence wherever it is asked
class FakeInfluence final: public systems::InfluenceSystemInterface
{
public:
	void Reset() override {}
	void ProcessTurn(uint32_t) override {}
	void BordersChanged() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	void ShowHandInfluence(std::chrono::duration<float, std::milli>) override {}
	[[nodiscard]] float PlayerInfluence(PlayerNames player, const map_coords::MapCoords&) const override
	{
		return influence.at(static_cast<size_t>(player));
	}
	[[nodiscard]] float HandPointInfluence(PlayerNames player, const map_coords::MapCoords&) const override
	{
		return influence.at(static_cast<size_t>(player));
	}
	[[nodiscard]] float PlayerRawInfluence(PlayerNames player, const map_coords::MapCoords&) const override
	{
		return influence.at(static_cast<size_t>(player));
	}
	void HeldThingUsedOnLand(PlayerNames) override {}
	void SetInGameTurn(bool) override {}
	void FixCitadelReaches() override {}
	[[nodiscard]] std::span<const influence::Circle> GetCircles() const override { return {}; }
	[[nodiscard]] bool IsBorderShown(PlayerNames) const override { return false; }
	[[nodiscard]] glm::vec2 GetScrollOffset() const override { return {}; }
	[[nodiscard]] std::span<const influence::Ripple> GetRipples() const override { return {}; }

	std::array<float, 8> influence {};
};
} // namespace

class Alignment: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		registry.Assign<components::Player>(registry.Create(), PlayerNames::PLAYER_ONE);
		registry.Assign<components::Player>(registry.Create(), PlayerNames::PLAYER_TWO);
		// The camera is in player one's influence
		_influence = &static_cast<FakeInfluence&>(Locator::influenceSystem::emplace<FakeInfluence>());
		_influence->influence.at(0) = 1.0f;
	}
	void TearDown() override
	{
		Locator::influenceSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	FakeInfluence* _influence {nullptr};
	systems::AlignmentSystem _alignment;
};

TEST_F(Alignment, StartsNeutral)
{
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.0f);
	EXPECT_EQ(_alignment.GetCameraAlignment(), 0.0f);
	EXPECT_EQ(_alignment.GetSkyAlignment(), 0.0f);
}

TEST_F(Alignment, PlayersAreHeldBetweenEvilAndGood)
{
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 3.0f);
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 1.0f);
	_alignment.AddPlayerAlignment(PlayerNames::PLAYER_ONE, -0.5f);
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f);
	_alignment.AddPlayerAlignment(PlayerNames::PLAYER_ONE, -4.0f);
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), -1.0f);
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_TWO), 0.0f);
}

TEST_F(Alignment, PlayersNotInTheGameHaveNone)
{
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_SIX, 1.0f);
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_SIX), 0.0f);
}

TEST_F(Alignment, TheCameraTakesTheMostInfluentialPlayersAtTheTurn)
{
	auto& influence = *_influence;
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, -0.75f);
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_TWO, 0.5f);
	influence.influence.at(0) = 0.3f;
	influence.influence.at(1) = 0.2f;
	EXPECT_EQ(_alignment.GetCameraAlignment(), 0.0f);
	_alignment.UpdatePlayersTurn({});
	EXPECT_FLOAT_EQ(_alignment.GetCameraAlignment(), -0.75f);
	influence.influence.at(1) = 0.4f;
	_alignment.UpdatePlayersTurn({});
	EXPECT_FLOAT_EQ(_alignment.GetCameraAlignment(), 0.5f);
	// Where no player has any influence the place is the neutral player's
	influence.influence = {};
	_alignment.UpdatePlayersTurn({});
	EXPECT_EQ(_alignment.GetCameraAlignment(), 0.0f);
}

TEST(AlignmentRules, TheMostInfluentialPlayerHoldsAPlace)
{
	using systems::alignment::MostInfluentialAlignment;
	using systems::alignment::PlayerAtPlace;
	EXPECT_EQ(MostInfluentialAlignment({}), 0.0f);
	const std::array none {PlayerAtPlace {.influence = 0.0f, .alignment = 1.0f}};
	EXPECT_EQ(MostInfluentialAlignment(none), 0.0f);
	// A tie keeps the earlier player
	const std::array tie {PlayerAtPlace {.influence = 0.5f, .alignment = -1.0f},
	                      PlayerAtPlace {.influence = 0.5f, .alignment = 1.0f}};
	EXPECT_EQ(MostInfluentialAlignment(tie), -1.0f);
	const std::array more {PlayerAtPlace {.influence = 0.5f, .alignment = -1.0f},
	                       PlayerAtPlace {.influence = 0.6f, .alignment = 0.25f}};
	EXPECT_EQ(MostInfluentialAlignment(more), 0.25f);
}

TEST(AlignmentRules, AScriptChangesAnAlignmentOnlyByAnAmountFromMinusOneToOne)
{
	using systems::alignment::ScriptChangeInRange;
	EXPECT_TRUE(ScriptChangeInRange(-1.0f));
	EXPECT_TRUE(ScriptChangeInRange(1.0f));
	EXPECT_TRUE(ScriptChangeInRange(0.25f));
	EXPECT_FALSE(ScriptChangeInRange(std::nextafter(1.0f, 2.0f)));
	EXPECT_FALSE(ScriptChangeInRange(-1.5f));
	EXPECT_FALSE(ScriptChangeInRange(std::numeric_limits<float>::quiet_NaN()));
}

TEST(AlignmentRules, AnAmountAddedAtOnceIsHeldBetweenEvilAndGood)
{
	using systems::alignment::AddedAlignment;
	EXPECT_EQ(AddedAlignment(0.25f, 0.5f), 0.75f);
	EXPECT_EQ(AddedAlignment(0.75f, 0.5f), 1.0f);
	EXPECT_EQ(AddedAlignment(-0.75f, -0.5f), -1.0f);
	// A sum that is not a number goes to evil
	EXPECT_EQ(AddedAlignment(std::numeric_limits<float>::quiet_NaN(), 0.5f), -1.0f);
}

TEST(AlignmentRules, TheLandShowsAnAlignmentHeldBetweenEvilAndGood)
{
	using systems::alignment::LandAlignment;
	EXPECT_FLOAT_EQ(LandAlignment(0.5f), 0.5f);
	EXPECT_FLOAT_EQ(LandAlignment(-3.0f), -1.0f);
	EXPECT_FLOAT_EQ(LandAlignment(2.0f), 1.0f);
}

TEST_F(Alignment, TheSkyTurnsAWholeASecondOfGameTime)
{
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, -1.0f);
	_alignment.UpdatePlayersTurn({});
	_alignment.Update(500ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyAlignment(), -0.5f);
	// Not while paused
	_alignment.Update(0ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyAlignment(), -0.5f);
	_alignment.Update(2000ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyAlignment(), -1.0f);
	// And back the other way
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 1.0f);
	_alignment.UpdatePlayersTurn({});
	_alignment.Update(1500ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyAlignment(), 0.5f);
}

TEST_F(Alignment, WhatTheirDeedsChangeScalesOneTurnsChange)
{
	// The tables are large: made on the heap
	auto info = std::make_unique<InfoConstants>();
	info->player.maxAlignmentChangePerGameTurn = 0.01f;
	Locator::infoConstants::emplace(*info);
	_alignment.AddPendingAlignment(PlayerNames::PLAYER_ONE, -0.025f);
	EXPECT_FLOAT_EQ(_alignment.GetPendingAlignment(PlayerNames::PLAYER_ONE), -0.025f);
	// Setting the alignment keeps what is still to come
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 0.5f);
	// The turn's change is the limit times what waits, and the rest of what waited is gone
	_alignment.UpdatePlayersTurn({});
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f - (0.025f * 0.01f));
	EXPECT_FLOAT_EQ(_alignment.GetPendingAlignment(PlayerNames::PLAYER_ONE), 0.0f);
	_alignment.UpdatePlayersTurn({});
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f - (0.025f * 0.01f));
	Locator::infoConstants::reset();
}

TEST_F(Alignment, ThePlanOfACreatureMovesItsAlignmentEachTurn)
{
	// The tables are large: made on the heap
	auto info = std::make_unique<InfoConstants>();
	auto& species = info->creature.at(static_cast<size_t>(CreatureType::Cow));
	species.alignmentChangePerTurn = 0.01f;
	species.alignmentChangeScale = 0.2f;
	info->creatureInitialDesire.at(static_cast<size_t>(creature_desires::Desire::Compassion)).alignmentChange = 1.0f;
	// The first row of the alignment table gives the weight of an eaten villager
	info->alignment.at(0).villager = -1.0f;
	for (auto& row : info->villager)
	{
		row.alignmentType = AlignmentType::Villager;
	}
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	registry.Assign<components::Villager>(villager);
	const auto creature = registry.Create();
	registry.Assign<components::Creature>(creature).species = CreatureType::Cow;
	auto& mind = registry.Assign<components::CreatureMindState>(creature);

	// Without a plan the alignment stays
	_alignment.UpdateCreaturesTurn();
	EXPECT_FLOAT_EQ(registry.Get<components::Creature>(creature).alignment, 0.0f);

	// A plan to be nice moves it to good
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Compassion};
	_alignment.UpdateCreaturesTurn();
	EXPECT_FLOAT_EQ(registry.Get<components::Creature>(creature).alignment, 0.2f * 0.01f);

	// A plan to eat a villager moves it to evil, more strongly against the good alignment
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .object = entt::to_integral(villager)};
	const float before = registry.Get<components::Creature>(creature).alignment;
	_alignment.UpdateCreaturesTurn();
	EXPECT_FLOAT_EQ(registry.Get<components::Creature>(creature).alignment, before - (0.2f * (1.0f + (before * 0.5f)) * 0.01f));

	// A plan that is not active does not count
	mind.planActive = false;
	const float after = registry.Get<components::Creature>(creature).alignment;
	_alignment.UpdateCreaturesTurn();
	EXPECT_FLOAT_EQ(registry.Get<components::Creature>(creature).alignment, after);
	Locator::infoConstants::reset();
}
