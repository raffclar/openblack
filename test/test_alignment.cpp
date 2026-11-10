/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <span>

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
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
	[[nodiscard]] float PlayerInfluence(PlayerNames player, const map_coords::MapCoords& position) const override
	{
		askedAt = position;
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
	/// The last place where the influence was asked
	mutable std::optional<map_coords::MapCoords> askedAt;
};

/// The hands of the player of this computer, with the camera that the player last reported
class FakeHands final: public systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override
	{
		return {entt::null, entt::null};
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override
	{
		return {};
	}
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords>, bool) override {}
	[[nodiscard]] std::optional<ReportedCamera> GetReportedCamera(PlayerNames player) const noexcept override
	{
		return player == own ? reported : std::nullopt;
	}
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return own; }

	std::optional<ReportedCamera> reported;
	PlayerNames own {PlayerNames::PLAYER_ONE};
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
	EXPECT_EQ(_alignment.GetSkyLand(), 1.0f);
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
	// A value that is not a number is wholly evil
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, std::numeric_limits<float>::quiet_NaN());
	EXPECT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), -1.0f);
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
	_alignment.UpdatePlayersTurn();
	EXPECT_FLOAT_EQ(_alignment.GetCameraAlignment(), -0.75f);
	influence.influence.at(1) = 0.4f;
	_alignment.UpdatePlayersTurn();
	EXPECT_FLOAT_EQ(_alignment.GetCameraAlignment(), 0.5f);
	// Where no player has any influence the place is the neutral player's
	influence.influence = {};
	_alignment.UpdatePlayersTurn();
	EXPECT_EQ(_alignment.GetCameraAlignment(), 0.0f);
}

TEST_F(Alignment, TheCameraIsWhereThePlayerLastReportedIt)
{
	// Before the first report the camera is at the middle of the world's axes
	_alignment.UpdatePlayersTurn();
	ASSERT_TRUE(_influence->askedAt.has_value());
	EXPECT_EQ(_influence->askedAt->x, 0);
	EXPECT_EQ(_influence->askedAt->z, 0);

	// After a report the turn reads the reported camera, not the camera that the frames move
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.reported = systems::HandSystemInterface::ReportedCamera {.origin = {120.5f, 300.0f, 64.25f}, .focus = {}};
	_alignment.UpdatePlayersTurn();
	const auto expected = map_coords::FromMetres({120.5f, 64.25f});
	EXPECT_EQ(_influence->askedAt->x, expected.x);
	EXPECT_EQ(_influence->askedAt->z, expected.z);

	// A computer that plays another god reads the camera of its own god
	hands.own = PlayerNames::PLAYER_TWO;
	hands.reported->origin = {8.0f, 0.0f, 16.0f};
	_alignment.UpdatePlayersTurn();
	const auto own = map_coords::FromMetres({8.0f, 16.0f});
	EXPECT_EQ(_influence->askedAt->x, own.x);
	EXPECT_EQ(_influence->askedAt->z, own.z);
	Locator::handSystem::reset();
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

TEST(AlignmentRules, TheLandKeepsTwiceTheEvilnessHeldBetweenGoodAndEvil)
{
	using systems::alignment::LandValue;
	EXPECT_EQ(LandValue(1.0f), 0.0f);
	EXPECT_EQ(LandValue(0.0f), 1.0f);
	EXPECT_EQ(LandValue(-1.0f), 2.0f);
	EXPECT_EQ(LandValue(3.0f), 0.0f);
	EXPECT_EQ(LandValue(-3.0f), 2.0f);
	// A goodness that is not a number is fully evil
	EXPECT_EQ(LandValue(std::numeric_limits<float>::quiet_NaN()), 2.0f);
}

TEST(AlignmentRules, TheTempleAndTheAudioTakeTheirAlignmentFromTheLandValue)
{
	using systems::alignment::AudioAlignment;
	using systems::alignment::LandValue;
	using systems::alignment::TempleLightAlignment;
	// The evilness rounds, so the temple's alignment is not two times the goodness less 1. The bits come from the
	// game's values in test/oracle/data/alignment_store.txt.
	const auto land = LandValue(std::bit_cast<float>(0xbf66df91u));
	EXPECT_EQ(std::bit_cast<uint32_t>(land), 0x3ff36fc8u);
	EXPECT_EQ(std::bit_cast<uint32_t>(TempleLightAlignment(land)), 0xbf66df90u);
	const auto nearNeutral = LandValue(std::bit_cast<float>(0xbbd1f67au));
	EXPECT_EQ(std::bit_cast<uint32_t>(nearNeutral), 0x3f80d1f6u);
	EXPECT_EQ(std::bit_cast<uint32_t>(TempleLightAlignment(nearNeutral)), 0xbbd1f600u);
	// A goodness of 1e-8 has an evilness of 1
	const auto almostEvil = LandValue((1e-8f * 2.0f) - 1.0f);
	EXPECT_EQ(almostEvil, 2.0f);
	EXPECT_EQ(TempleLightAlignment(almostEvil), -1.0f);
	EXPECT_EQ(AudioAlignment(almostEvil), -1.0f);
	EXPECT_EQ(AudioAlignment(LandValue(0.5f)), 0.5f);
}

TEST(AlignmentRules, TheSkyMovesAHundredthOfATenthEachMillisecondAndStopsAtTheLand)
{
	using systems::alignment::SkyLandValue;
	EXPECT_EQ(SkyLandValue(1.0f, 2.0f, 500), 1.5f);
	EXPECT_EQ(SkyLandValue(1.0f, 0.0f, 500), 0.5f);
	EXPECT_EQ(SkyLandValue(1.0f, 1.2f, 500), 1.2f);
	EXPECT_EQ(SkyLandValue(1.0f, 0.8f, 500), 0.8f);
	EXPECT_EQ(SkyLandValue(1.0f, 2.0f, 0), 1.0f);
	// The bits come from the game's sky in test/oracle/data/sky_land.txt
	EXPECT_EQ(std::bit_cast<uint32_t>(SkyLandValue(std::bit_cast<float>(0x3f2f2939u), std::bit_cast<float>(0x3e92bbfeu), 74)),
	          0x3f1c378fu);
}

TEST_F(Alignment, TheCameraAndTheSkyStartNeutral)
{
	EXPECT_EQ(_alignment.GetCameraLand(), 1.0f);
	EXPECT_EQ(_alignment.GetSkyLand(), 1.0f);
	EXPECT_EQ(_alignment.GetTempleLightAlignment(), 0.0f);
}

TEST_F(Alignment, TheSkyTurnsAWholeASecondOfGameTime)
{
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, -1.0f);
	_alignment.UpdatePlayersTurn();
	_alignment.Update(500ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyLand(), 1.5f);
	// Not while paused
	_alignment.Update(0ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyLand(), 1.5f);
	_alignment.Update(2000ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyLand(), 2.0f);
	// And back the other way
	_alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 1.0f);
	_alignment.UpdatePlayersTurn();
	_alignment.Update(1500ms);
	EXPECT_FLOAT_EQ(_alignment.GetSkyLand(), 0.5f);
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
	_alignment.PlayerTurn(PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f - (0.025f * 0.01f));
	EXPECT_FLOAT_EQ(_alignment.GetPendingAlignment(PlayerNames::PLAYER_ONE), 0.0f);
	_alignment.PlayerTurn(PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f - (0.025f * 0.01f));
	Locator::infoConstants::reset();
}

// Each player moves its alignment in its own turn. The turn of one player does not move the alignment of another player.
TEST_F(Alignment, ThePlayerTurnMovesOnlyTheAlignmentOfThatPlayer)
{
	auto info = std::make_unique<InfoConstants>();
	info->player.maxAlignmentChangePerGameTurn = 0.5f;
	Locator::infoConstants::emplace(*info);
	_alignment.AddPendingAlignment(PlayerNames::PLAYER_ONE, 1.0f);
	_alignment.AddPendingAlignment(PlayerNames::PLAYER_TWO, -1.0f);
	_alignment.PlayerTurn(PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.0f);
	EXPECT_FLOAT_EQ(_alignment.GetPendingAlignment(PlayerNames::PLAYER_ONE), 1.0f);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_TWO), -0.5f);
	_alignment.PlayerTurn(PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f);
	// The camera turn moves no alignment
	_alignment.AddPendingAlignment(PlayerNames::PLAYER_ONE, 1.0f);
	_alignment.UpdatePlayersTurn();
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::PLAYER_ONE), 0.5f);
	Locator::infoConstants::reset();
}

// The neutral player is neither good nor evil after its turn, also when a change waited or a script set its alignment
TEST_F(Alignment, TheNeutralPlayerHasNoAlignmentAfterItsTurn)
{
	auto info = std::make_unique<InfoConstants>();
	info->player.maxAlignmentChangePerGameTurn = 0.5f;
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<components::Player>(registry.Create(), PlayerNames::NEUTRAL);
	_alignment.SetPlayerAlignment(PlayerNames::NEUTRAL, 0.75f);
	_alignment.AddPendingAlignment(PlayerNames::NEUTRAL, 1.0f);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::NEUTRAL), 0.75f);
	_alignment.PlayerTurn(PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(_alignment.GetPlayerAlignment(PlayerNames::NEUTRAL), 0.0f);
	EXPECT_FLOAT_EQ(_alignment.GetPendingAlignment(PlayerNames::NEUTRAL), 0.0f);
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
