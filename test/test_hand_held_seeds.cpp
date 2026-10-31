/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <map>
#include <optional>
#include <set>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/SpellIconSystem.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using openblack::ecs::components::HandHeldSeed;
using openblack::ecs::components::SpellSeed;
using openblack::ecs::systems::HandSystemInterface;
using openblack::ecs::systems::MagicSystem;
using openblack::ecs::systems::PlayerSystemInterface;
using openblack::ecs::systems::SpellIconSystem;

namespace
{

constexpr auto k_Left = static_cast<size_t>(HandSystemInterface::Side::Left);

/// The hands of the test: the players that have hands, and the player of this computer
class FakeHands final: public HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, 2> GetPlayerHands() const noexcept override
	{
		return GetHandsOf(PlayerNames::PLAYER_ONE).value_or(std::array {entt::entity {entt::null}, entt::entity {entt::null}});
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetPlayerHandPositions() const noexcept override { return {}; }
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
	[[nodiscard]] std::optional<std::array<entt::entity, 2>> GetHandsOf(PlayerNames player) const noexcept override
	{
		const auto found = hands.find(player);
		return found != hands.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return own; }

	std::map<PlayerNames, std::array<entt::entity, 2>> hands;
	PlayerNames own {PlayerNames::PLAYER_ONE};
};

/// The players of the test: the players that the computer plays
class FakePlayers final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames /*name*/) const override { return std::nullopt; }
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames /*name*/) const override { return std::nullopt; }
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] bool IsComputerPlayer(PlayerNames name) const override { return computers.contains(name); }
	[[nodiscard]] bool IsHumanPlayer(PlayerNames name) const override
	{
		return !computers.contains(name) && !nobody.contains(name);
	}

	std::set<PlayerNames> computers;
	/// The players that nobody plays
	std::set<PlayerNames> nobody;
};

class HandHeldSeeds: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_hands = &static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
		for (const auto player : {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO})
		{
			_hands->hands[player] = {registry.Create(), registry.Create()};
		}
		_magic = &static_cast<MagicSystem&>(Locator::magicSystem::emplace<MagicSystem>());
	}
	void TearDown() override
	{
		Locator::playerSystem::reset();
		Locator::magicSystem::reset();
		Locator::handSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	/// Puts a new seed of the player in the left hand of that player
	entt::entity HoldSeed(PlayerNames player)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto seed = registry.Create();
		registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal, .player = player});
		registry.AssignOrReplace<HandHeldSeed>(_hands->hands.at(player)[k_Left], seed);
		return seed;
	}

	FakeHands* _hands {nullptr};
	MagicSystem* _magic {nullptr};
};

} // namespace

// Each player's hand holds its own seed. The hand of this computer is the hand of its own player only.
TEST_F(HandHeldSeeds, EachPlayerHasTheSeedOfItsOwnHand)
{
	const auto seed = HoldSeed(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_magic->GetHeldSeedOf(PlayerNames::PLAYER_TWO), seed);
	EXPECT_TRUE(_magic->IsHandBusyOf(PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(_magic->GetHeldSeedOf(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(_magic->IsHandBusy());

	// The computer of the second player has that seed in its own hand
	_hands->own = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(_magic->GetHeldSeed(), seed);
	EXPECT_TRUE(_magic->IsHandBusy());
}

// A player with no hands, such as a computer god, holds nothing
TEST_F(HandHeldSeeds, APlayerWithNoHandsHoldsNothing)
{
	EXPECT_FALSE(_magic->GetHeldSeedOf(PlayerNames::PLAYER_THREE).has_value());
	EXPECT_FALSE(_magic->IsHandBusyOf(PlayerNames::PLAYER_THREE));
}

// A seed that goes from the world leaves the hand of the player that held it, and the other hand keeps its seed
TEST_F(HandHeldSeeds, ADeletedSeedLeavesOnlyTheHandThatHeldIt)
{
	const auto first = HoldSeed(PlayerNames::PLAYER_ONE);
	const auto second = HoldSeed(PlayerNames::PLAYER_TWO);
	_magic->DeleteSeed(second);
	EXPECT_FALSE(_magic->GetHeldSeedOf(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(second));
	EXPECT_EQ(_magic->GetHeldSeedOf(PlayerNames::PLAYER_ONE), first);
}

// A reset of the miracles empties the hand of every player
TEST_F(HandHeldSeeds, AResetEmptiesEveryHand)
{
	HoldSeed(PlayerNames::PLAYER_ONE);
	HoldSeed(PlayerNames::PLAYER_TWO);
	_magic->Reset();
	EXPECT_FALSE(_magic->IsHandBusyOf(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_magic->IsHandBusyOf(PlayerNames::PLAYER_TWO));
}

// The icons give a miracle to the empty hand of any human player, also on the computer of another player. The turns
// read this, so it must not follow the player of this computer.
TEST_F(HandHeldSeeds, TheIconsServeTheEmptyHandOfEveryHuman)
{
	const SpellIconSystem icons;
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_TWO));
	// A computer god has no hand
	EXPECT_FALSE(icons.IsHandReady(PlayerNames::PLAYER_THREE));

	HoldSeed(PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(icons.IsHandReady(PlayerNames::PLAYER_TWO));

	// The computer of the second player reads the same
	_hands->own = PlayerNames::PLAYER_TWO;
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(icons.IsHandReady(PlayerNames::PLAYER_TWO));
}

// A computer god gets hands to show where it acts, but the icons never give a miracle to its hand. The god casts from
// the store of the icon.
TEST_F(HandHeldSeeds, TheIconsServeNoHandOfAComputerGod)
{
	auto& players = static_cast<FakePlayers&>(Locator::playerSystem::emplace<FakePlayers>());
	auto& registry = Locator::entitiesRegistry::value();
	_hands->hands[PlayerNames::PLAYER_THREE] = {registry.Create(), registry.Create()};
	players.computers.insert(PlayerNames::PLAYER_THREE);
	const SpellIconSystem icons;
	EXPECT_FALSE(icons.IsHandReady(PlayerNames::PLAYER_THREE));
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_TWO));

	// A player that the computer does not play gets the miracle again
	players.computers.clear();
	EXPECT_TRUE(icons.IsHandReady(PlayerNames::PLAYER_THREE));

	// The hand of a player that nobody plays gets no miracle
	players.nobody.insert(PlayerNames::PLAYER_THREE);
	EXPECT_FALSE(icons.IsHandReady(PlayerNames::PLAYER_THREE));
}

// A cast or a let go of a player whose hand holds nothing changes nothing, also when another hand holds a seed
TEST_F(HandHeldSeeds, ACommandOfAPlayerWithAnEmptyHandLeavesTheOtherHand)
{
	const auto seed = HoldSeed(PlayerNames::PLAYER_ONE);
	_magic->DoCastUnlock(PlayerNames::PLAYER_TWO, seed);
	_magic->DoCast(PlayerNames::PLAYER_TWO, player_commands::HandCast {.point = {1.0f, 2.0f, 3.0f}});
	EXPECT_EQ(_magic->GetHeldSeedOf(PlayerNames::PLAYER_ONE), seed);
	EXPECT_FALSE(_magic->IsHandBusyOf(PlayerNames::PLAYER_TWO));
}

// A tool drops the seed of a named player. The result is the same on the computer of each player, and the other hand
// keeps its seed.
TEST_F(HandHeldSeeds, ADropOfANamedPlayersSeedIsTheSameOnEachComputer)
{
	for (const auto own : {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO})
	{
		_hands->own = own;
		HoldSeed(PlayerNames::PLAYER_ONE);
		const auto other = HoldSeed(PlayerNames::PLAYER_TWO);
		_magic->DiscardHeldSeedOf(PlayerNames::PLAYER_ONE);
		EXPECT_FALSE(_magic->IsHandBusyOf(PlayerNames::PLAYER_ONE));
		EXPECT_EQ(_magic->GetHeldSeedOf(PlayerNames::PLAYER_TWO), other);
		_magic->DiscardHeldSeedOf(PlayerNames::PLAYER_TWO);
		EXPECT_FALSE(_magic->IsHandBusyOf(PlayerNames::PLAYER_TWO));
	}
}
