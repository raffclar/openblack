/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player system's state (every player's alignment, the magic of the players without an entity, the local player)
// and the magic::players and creature::LocalPlayer functions that reach it

#define LOCATOR_IMPLEMENTATIONS

#include <stdexcept>

#include <gtest/gtest.h>

#include "Creature/LocalPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::Alignment;
using openblack::ecs::components::Player;
using openblack::ecs::components::PlayerMagic;
using openblack::ecs::systems::PlayerSystem;
using openblack::ecs::systems::PlayerSystemInterface;
using openblack::lhscriptx::FeatureScriptCommands;

namespace
{
/// Records which player each call asked for, and hands out its own state
class FakePlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity) override {}
	[[nodiscard]] entt::entity GetPlayer(PlayerNames) const override { return entt::null; }
	[[nodiscard]] PlayerNames LocalPlayer() const override { return local; }
	void ClearPlayers() override { ++playerClears; }
	[[nodiscard]] ecs::components::Alignment& Alignment(PlayerNames name) override
	{
		alignmentAsked = name;
		return alignment;
	}
	[[nodiscard]] PlayerMagic& MagicWithoutEntity(PlayerNames name) override
	{
		magicAsked = name;
		return magic;
	}
	void ClearMagicWithoutEntity() override { ++clears; }

	ecs::components::Alignment alignment {};
	PlayerMagic magic {};
	PlayerNames alignmentAsked {PlayerNames::_COUNT};
	PlayerNames magicAsked {PlayerNames::_COUNT};
	PlayerNames local {PlayerNames::PLAYER_ONE};
	int clears {0};
	int playerClears {0};
};
} // namespace

TEST(PlayerSystem, startsAtTheNewGameValues)
{
	PlayerSystem players;
	for (size_t p = 0; p < PlayerSystem::k_Players; ++p)
	{
		const auto name = static_cast<PlayerNames>(p);
		EXPECT_EQ(players.Alignment(name).value, 0.0f) << p;
		EXPECT_EQ(players.Alignment(name).pending, 0.0f) << p;
		EXPECT_EQ(players.MagicWithoutEntity(name).tribalPower[0], 1.0f) << p;
		EXPECT_EQ(players.MagicWithoutEntity(name).playerType, 0) << p;
	}
}

TEST(PlayerSystem, eachPlayerHasItsOwnState)
{
	PlayerSystem players;
	players.Alignment(PlayerNames::PLAYER_TWO).value = 0.5f;
	players.MagicWithoutEntity(PlayerNames::PLAYER_THREE).chantsUsed = 3.0f;
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_ONE).value, 0.0f);
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_TWO).value, 0.5f);
	EXPECT_EQ(players.MagicWithoutEntity(PlayerNames::PLAYER_ONE).chantsUsed, 0.0f);
	EXPECT_EQ(players.MagicWithoutEntity(PlayerNames::PLAYER_THREE).chantsUsed, 3.0f);
}

TEST(PlayerSystem, aNameOutOfRangeIsTheNeutralPlayer)
{
	PlayerSystem players;
	players.Alignment(PlayerNames::_COUNT).value = -0.25f;
	players.MagicWithoutEntity(PlayerNames::_COUNT).allMagicCheat = true;
	EXPECT_EQ(&players.Alignment(PlayerNames::_COUNT), &players.Alignment(PlayerNames::NEUTRAL));
	EXPECT_EQ(players.Alignment(PlayerNames::NEUTRAL).value, -0.25f);
	EXPECT_TRUE(players.MagicWithoutEntity(PlayerNames::NEUTRAL).allMagicCheat);
}

TEST(PlayerSystem, aLandLoadClearsTheMagicButKeepsTheAlignment)
{
	PlayerSystem players;
	auto& magic = players.MagicWithoutEntity(PlayerNames::NEUTRAL);
	magic.remainder[3] = 2;
	magic.everEnabled[3] = true;
	magic.tribalPower[1] = 0.5f;
	magic.wonderPower[1] = 4.0f;
	magic.teleportStones.push_back(entt::entity {7});
	players.Alignment(PlayerNames::NEUTRAL) = Alignment {.value = 0.75f, .pending = 0.125f};

	players.ClearMagicWithoutEntity();

	EXPECT_EQ(magic.remainder[3], 0);
	EXPECT_FALSE(magic.everEnabled[3]);
	EXPECT_EQ(magic.tribalPower[1], 1.0f);
	EXPECT_EQ(magic.wonderPower[1], 0.0f);
	EXPECT_TRUE(magic.teleportStones.empty());
	EXPECT_EQ(players.Alignment(PlayerNames::NEUTRAL).value, 0.75f);
	EXPECT_EQ(players.Alignment(PlayerNames::NEUTRAL).pending, 0.125f);
}

TEST(PlayerSystem, thePlayerFunctionsUseTheLocatorsPlayerSystem)
{
	const test::RestoreService<Locator::playerSystem> restore;
	auto& fake = static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
	const test::RestoreService<Locator::entitiesRegistry> noRegistry;
	Locator::entitiesRegistry::reset();

	EXPECT_EQ(&magic::players::AlignmentOf(PlayerNames::PLAYER_FOUR), &fake.alignment);
	EXPECT_EQ(fake.alignmentAsked, PlayerNames::PLAYER_FOUR);
	// no player entity: the magic is the player system's
	EXPECT_EQ(&magic::players::MagicOf(PlayerNames::NEUTRAL), &fake.magic);
	EXPECT_EQ(fake.magicAsked, PlayerNames::NEUTRAL);
	magic::players::Reset();
	EXPECT_EQ(fake.clears, 1);
	// a land load also empties the list of the player entities
	EXPECT_EQ(fake.playerClears, 1);
}

TEST(PlayerSystem, theCreaturesLocalPlayerIsThePlayerSystems)
{
	const test::RestoreService<Locator::playerSystem> restore;
	// the game's player service: the first player's interface
	EXPECT_EQ(PlayerSystem().LocalPlayer(), PlayerNames::PLAYER_ONE);
	// with no player service, the first player
	Locator::playerSystem::reset();
	EXPECT_EQ(creature::LocalPlayer(), PlayerNames::PLAYER_ONE);
	// with one, whichever player it names
	auto& fake = static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
	EXPECT_EQ(creature::LocalPlayer(), PlayerNames::PLAYER_ONE);
	fake.local = PlayerNames::PLAYER_THREE;
	EXPECT_EQ(creature::LocalPlayer(), PlayerNames::PLAYER_THREE);
}

TEST(PlayerSystem, aLandLoadListsOnlyThePlayersTheNewLandMakes)
{
	const test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	PlayerSystem players;
	const auto lastLand = registry.Create();
	registry.Assign<Player>(lastLand, PlayerNames::PLAYER_ONE);
	players.AddPlayer(lastLand);
	ASSERT_EQ(players.GetPlayer(PlayerNames::PLAYER_ONE), lastLand);

	// the next land: the registry is cleared and the land makes its own PLAYER_ONE
	players.ClearPlayers();
	registry.Reset();
	const auto newLand = registry.Create();
	registry.Assign<Player>(newLand, PlayerNames::PLAYER_ONE);
	players.AddPlayer(newLand);
	players.RegisterPlayers();

	EXPECT_EQ(players.GetPlayer(PlayerNames::PLAYER_ONE), newLand);
	EXPECT_THROW(static_cast<void>(players.GetPlayer(PlayerNames::PLAYER_TWO)), std::out_of_range);
}

TEST(PlayerSystem, aComputerPlayerTheLandScriptMakesIsListed)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	auto& players = static_cast<PlayerSystem&>(Locator::playerSystem::emplace<PlayerSystem>());

	// Land 2's TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1), after LOAD_LANDSCAPE
	const auto human = registry.Create();
	registry.Assign<Player>(human, PlayerNames::PLAYER_ONE);
	players.AddPlayer(human);
	FeatureScriptCommands::ToggleComputerPlayer("PLAYER_TWO", 1);

	const auto entity = players.GetPlayer(PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(registry.Valid(entity));
	EXPECT_EQ(registry.Get<Player>(entity).name, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(registry.Get<PlayerMagic>(entity).playerType, 0);
	EXPECT_EQ(players.GetPlayer(PlayerNames::PLAYER_ONE), human);
}
