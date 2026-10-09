/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <stdexcept>

#include <gtest/gtest.h>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
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
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_TWO), two);
}

TEST_F(PlayerSystemLands, TheFirstPlayerMadeKeepsItsName)
{
	// The land is set up with its player added, then every player on it registered
	const auto added = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(added);
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), added);
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
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_TWO), two);
	EXPECT_TRUE(registry.Valid(_players.GetPlayer(PlayerNames::PLAYER_ONE)));
	EXPECT_TRUE(registry.Valid(_players.GetPlayer(PlayerNames::PLAYER_TWO)));
}

TEST_F(PlayerSystemLands, APlayerNotOnTheNewLandIsForgotten)
{
	MakePlayer(PlayerNames::PLAYER_THREE);
	_players.RegisterPlayers();

	ClearLand();
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();

	EXPECT_THROW(static_cast<void>(_players.GetPlayer(PlayerNames::PLAYER_THREE)), std::out_of_range);
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
