/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/PlayerSystem.h"

using namespace openblack;
using namespace openblack::ecs;

class PlayerSystemLands: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<components::Player>(entity, name);
		return entity;
	}

	/// The land goes, its players with it, as a new land is loaded
	static void ClearLand() { Locator::entitiesRegistry::value().Reset(); }

	systems::PlayerSystem _players;
};

TEST_F(PlayerSystemLands, KnowsTheLandsPlayers)
{
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_TWO), two);
}

TEST_F(PlayerSystemLands, APlayerWhoHasNotJoinedYetIsNotFound)
{
	// A network land has only the first player until the first turn makes the players of the humans
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(one);
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_TWO), std::nullopt);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.AddPlayer(two);
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_TWO), two);
}

TEST_F(PlayerSystemLands, TheFirstPlayerMadeKeepsItsName)
{
	// The land is set up with its player added, then every player on it registered
	const auto added = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(added);
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_ONE), added);
}

TEST_F(PlayerSystemLands, ANewLandsPlayersTakeTheLastLandsPlaces)
{
	MakePlayer(PlayerNames::PLAYER_ONE);
	MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();

	ClearLand();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(one);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();

	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_TWO), two);
	EXPECT_TRUE(registry.Valid(_players.FindPlayer(PlayerNames::PLAYER_ONE).value_or(entt::null)));
	EXPECT_TRUE(registry.Valid(_players.FindPlayer(PlayerNames::PLAYER_TWO).value_or(entt::null)));
}

TEST_F(PlayerSystemLands, APlayerNotOnTheNewLandIsForgotten)
{
	MakePlayer(PlayerNames::PLAYER_THREE);
	_players.RegisterPlayers();

	ClearLand();
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();

	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_THREE), std::nullopt);
}

TEST_F(PlayerSystemLands, AGodSlotThatNobodyJoinedYetGetsItsPlayerWhenTheLandNamesIt)
{
	// A network land has only the first player before the humans join, but a land script can name every god slot
	_players.AddPlayer(MakePlayer(PlayerNames::PLAYER_ONE));
	const auto two = _players.EnsurePlayer(PlayerNames::PLAYER_TWO);
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_TRUE(registry.Valid(two));
	EXPECT_EQ(registry.Get<components::Player>(two).name, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_TWO), two);
}

TEST_F(PlayerSystemLands, AGodSlotKeepsTheSamePlayerWhenTheLandNamesItAgain)
{
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(one);
	EXPECT_EQ(_players.EnsurePlayer(PlayerNames::PLAYER_ONE), one);
	const auto two = _players.EnsurePlayer(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_players.EnsurePlayer(PlayerNames::PLAYER_TWO), two);
	auto count = 0;
	Locator::entitiesRegistry::value().Each<const components::Player>(
	    [&count](const components::Player& player) { count += player.name == PlayerNames::PLAYER_TWO ? 1 : 0; });
	EXPECT_EQ(count, 1);
}

TEST_F(PlayerSystemLands, AGodSlotOfALandThatIsGoneGetsANewPlayerOnTheNextLand)
{
	_players.EnsurePlayer(PlayerNames::PLAYER_TWO);
	ClearLand();
	const auto now = _players.EnsurePlayer(PlayerNames::PLAYER_TWO);
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(registry.Valid(now));
	EXPECT_EQ(registry.Get<components::Player>(now).name, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_players.FindPlayer(PlayerNames::PLAYER_TWO), now);
}

TEST_F(PlayerSystemLands, APlayerKeepsTheirAlignmentAndWhatIsTheirsOnTheNextLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	registry.Assign<components::Alignment>(one, -0.6f, 0.05f);
	auto& player = registry.Get<components::Player>(one);
	player.windResistance = 1;
	player.damageFrom.at(1) = 3.5f;
	_players.AddPlayer(one);

	_players.KeepForNextLand();
	ClearLand();
	const auto again = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(again);

	const auto* alignment = registry.TryGet<components::Alignment>(again);
	ASSERT_NE(alignment, nullptr);
	EXPECT_EQ(alignment->value, -0.6f);
	EXPECT_EQ(alignment->pending, 0.05f);
	EXPECT_EQ(registry.Get<components::Player>(again).windResistance, 1u);
	EXPECT_EQ(registry.Get<components::Player>(again).damageFrom.at(1), 3.5f);
}

TEST_F(PlayerSystemLands, APlayerAwayFromALandKeepsWhatTheyHadOnTheLastTheyWereOn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	registry.Assign<components::Alignment>(two, 0.9f, 0.0f);
	_players.KeepForNextLand();

	// A land without them, then one with them again
	ClearLand();
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.KeepForNextLand();
	ClearLand();
	const auto again = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.TakeUpKept(again);
	EXPECT_EQ(registry.Get<components::Alignment>(again).value, 0.9f);
}

TEST_F(PlayerSystemLands, ANewPlayerHasNothingToTakeUp)
{
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(one);
	EXPECT_EQ(Locator::entitiesRegistry::value().TryGet<components::Alignment>(one), nullptr);
}

TEST_F(PlayerSystemLands, ANewGamesPlayersStartWithNothingOfTheLastGame)
{
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<components::Alignment>(one, components::Alignment {.value = -0.5f, .pending = 0.0f});
	registry.Get<components::Player>(one).miracles.everEnabled.at(3) = true;
	_players.RegisterPlayers();
	_players.KeepForNextLand();
	// A new game forgets what the last game's players kept
	_players.ForgetKept();
	ClearLand();
	const auto again = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(again);
	EXPECT_EQ(registry.TryGet<components::Alignment>(again), nullptr);
	EXPECT_FALSE(registry.Get<components::Player>(again).miracles.everEnabled.at(3));
}

TEST_F(PlayerSystemLands, TheAlignmentIsKeptWithTheProfileEverySixHundredTurnsFromLandToLand)
{
	EXPECT_FALSE(_players.KeepAlignmentAt(0));
	EXPECT_FALSE(_players.KeepAlignmentAt(599));
	EXPECT_TRUE(_players.KeepAlignmentAt(600));
	EXPECT_FALSE(_players.KeepAlignmentAt(601));
	EXPECT_TRUE(_players.KeepAlignmentAt(1200));
	// The next land's turns start again from nothing: kept at once, then every six hundred turns from there
	EXPECT_TRUE(_players.KeepAlignmentAt(0));
	EXPECT_FALSE(_players.KeepAlignmentAt(599));
	EXPECT_TRUE(_players.KeepAlignmentAt(600));
	// A new game starts the count again
	_players.ForgetKept();
	EXPECT_FALSE(_players.KeepAlignmentAt(0));
	EXPECT_FALSE(_players.KeepAlignmentAt(599));
}

TEST_F(PlayerSystemLands, APlayersEnabledMiraclesAndTribalPowerGoWithTheLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	auto& miracles = registry.Get<components::Player>(one).miracles;
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Heal, true);
	miracles.allEnabled = true;
	miracles.tribalPower.at(static_cast<size_t>(Tribe::NORSE)) = 2.0f;
	miracles.maxTribalPower.at(static_cast<size_t>(Tribe::NORSE)) = 2.0f;
	_players.AddPlayer(one);

	_players.KeepForNextLand();
	ClearLand();
	const auto again = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(again);

	const auto& after = registry.Get<components::Player>(again).miracles;
	EXPECT_EQ(after.enabled.at(static_cast<size_t>(MagicType::Heal)), 0u);
	EXPECT_EQ(after.tribalPower, components::Player::k_UsualTribalPower);
	EXPECT_EQ(after.maxTribalPower, components::Player::k_UsualTribalPower);
	// What was ever enabled, and every type being enabled, stay
	EXPECT_TRUE(after.everEnabled.at(static_cast<size_t>(MagicType::Heal)));
	EXPECT_TRUE(after.allEnabled);
}

TEST(PlayerMiracles, AMagicTypeStaysEnabledWhileAnythingEnablesIt)
{
	components::Player::Miracles miracles;
	EXPECT_FALSE(player_miracles::IsMagicTypeEnabled(miracles, MagicType::Fireball));
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Fireball, true);
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Fireball, true);
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Fireball, false);
	EXPECT_TRUE(player_miracles::IsMagicTypeEnabled(miracles, MagicType::Fireball));
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Fireball, false);
	EXPECT_FALSE(player_miracles::IsMagicTypeEnabled(miracles, MagicType::Fireball));
	// Never fewer than none
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Fireball, false);
	EXPECT_EQ(miracles.enabled.at(static_cast<size_t>(MagicType::Fireball)), 0u);
	player_miracles::SetMagicTypeEnabled(miracles, MagicType::Fireball, true);
	EXPECT_TRUE(player_miracles::IsMagicTypeEnabled(miracles, MagicType::Fireball));
	EXPECT_TRUE(miracles.everEnabled.at(static_cast<size_t>(MagicType::Fireball)));
}

// A globe taken into the hand marks its miracle as ever enabled, without enabling it
TEST(PlayerMiracles, AMagicTypeMarkedEverEnabledIsNotEnabled)
{
	components::Player::Miracles miracles;
	player_miracles::MarkMagicTypeEverEnabled(miracles, MagicType::Heal);
	EXPECT_TRUE(miracles.everEnabled.at(static_cast<size_t>(MagicType::Heal)));
	EXPECT_FALSE(player_miracles::IsMagicTypeEnabled(miracles, MagicType::Heal));
}

TEST(PlayerMiracles, EveryTypeIsEnabledWhenAllAre)
{
	components::Player::Miracles miracles;
	miracles.allEnabled = true;
	EXPECT_TRUE(player_miracles::IsMagicTypeEnabled(miracles, MagicType::Tornado));
	EXPECT_FALSE(miracles.everEnabled.at(static_cast<size_t>(MagicType::Tornado)));
}

TEST(PlayerMiracles, APlayerStartsWithNothingEnabledAndUsualTribalPower)
{
	const components::Player player {PlayerNames::PLAYER_ONE};
	EXPECT_FALSE(player_miracles::IsMagicTypeEnabled(player.miracles, MagicType::Heal));
	EXPECT_EQ(player.miracles.tribalPower, components::Player::k_UsualTribalPower);
	EXPECT_EQ(player.miracles.maxTribalPower.at(static_cast<size_t>(Tribe::TIBETAN)), 1.0f);
}

TEST_F(PlayerSystemLands, TheHandKeepsWhatItKeptPastTheBorderOnTheNextLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	auto& kept = registry.Assign<components::VirtualInfluence>(one).state;
	kept.fraction = 0.4f;
	kept.disabled = true;
	kept.anchor = glm::vec3(10.0f, 0.0f, 20.0f);
	_players.KeepForNextLand();
	ClearLand();

	const auto again = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.TakeUpKept(again);
	const auto* virtualInfluence = registry.TryGet<components::VirtualInfluence>(again);
	ASSERT_NE(virtualInfluence, nullptr);
	EXPECT_EQ(virtualInfluence->state.fraction, 0.4f);
	EXPECT_TRUE(virtualInfluence->state.disabled);
	ASSERT_TRUE(virtualInfluence->state.anchor.has_value());
	EXPECT_EQ(virtualInfluence->state.anchor->z, 20.0f);
	EXPECT_TRUE(virtualInfluence->hum == entt::null);
}

TEST_F(PlayerSystemLands, OnTheNextLandTheStatisticsCountersStartAgainButTheHistoriesGoOn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	auto& statistics = registry.Assign<components::PlayerStatistics>(one);
	statistics.births = 4;
	statistics.startAlignment = 0.3f;
	// One finished point of influence, of fifty samples, and part of the first point of population
	for (int i = 0; i < 50; ++i)
	{
		statistics.influenceHistory.Add(2.0f);
	}
	statistics.populationHistory.Add(5.0f);
	const float point = statistics.influenceHistory.Finished()[0];
	_players.KeepForNextLand();
	ClearLand();

	// The player is made again on the next land with statistics that start from nothing
	const auto again = MakePlayer(PlayerNames::PLAYER_ONE);
	registry.Assign<components::PlayerStatistics>(again);
	_players.TakeUpKept(again);
	const auto& next = registry.Get<components::PlayerStatistics>(again);
	EXPECT_EQ(next.births, 0u);
	EXPECT_EQ(next.startAlignment, 0.0f);
	ASSERT_EQ(next.influenceHistory.Finished().size(), 1u);
	EXPECT_EQ(next.influenceHistory.Finished()[0], point);
	EXPECT_EQ(next.populationHistory.samplesInPoint, 1u);
	EXPECT_EQ(registry.Context().globalStatistics.townsAtStart, 0u);
}

TEST_F(PlayerSystemLands, ANewGameStartsTheStatisticsHistoriesAgain)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	auto& history = registry.Assign<components::PlayerStatistics>(one).influenceHistory;
	for (int i = 0; i < 50; ++i)
	{
		history.Add(1.0f);
	}
	_players.KeepForNextLand();
	_players.ForgetKept();
	ClearLand();

	const auto again = MakePlayer(PlayerNames::PLAYER_ONE);
	registry.Assign<components::PlayerStatistics>(again);
	_players.TakeUpKept(again);
	EXPECT_TRUE(registry.Get<components::PlayerStatistics>(again).influenceHistory.Finished().empty());
}
