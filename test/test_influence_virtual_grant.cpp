/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/Influence.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/InfluenceSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The player's one hand, wherever the test puts it
class FakeHands final: public systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override
	{
		return {hand, entt::null};
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override
	{
		return {};
	}
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords>, bool) override {}

	entt::entity hand {entt::null};
};

/// The player's own influence reaches 100 m round the middle of the land
constexpr glm::vec3 k_Middle {1000.0f, 0.0f, 1000.0f};
constexpr float k_Reach = 100.0f;
} // namespace

class VirtualGrant: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_hands = &static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
		Locator::playerSystem::emplace<systems::PlayerSystem>();
		_player = registry.Create();
		registry.Assign<components::Player>(_player, PlayerNames::PLAYER_ONE);
		const auto source = registry.Create();
		registry.Assign<components::Transform>(source, k_Middle, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<components::InfluenceSource>(source, PlayerNames::PLAYER_ONE, k_Reach);
		_hands->hand = registry.Create();
		registry.Assign<components::Transform>(_hands->hand, k_Middle, glm::mat3(1.0f), glm::vec3(1.0f));
	}
	void TearDown() override
	{
		Locator::playerSystem::reset();
		Locator::handSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The hand, at the last turn and now, past the border with a share of the player's influence left
	virtual_influence::State& HandOut(float fraction, glm::vec3 turnHand)
	{
		auto& state = Locator::entitiesRegistry::value().Assign<components::VirtualInfluence>(_player).state;
		state.fraction = fraction;
		state.turnHand = turnHand;
		state.anchor = k_Middle;
		return state;
	}
	void MoveHand(glm::vec3 to) { Locator::entitiesRegistry::value().Get<components::Transform>(_hands->hand).position = to; }

	FakeHands* _hands {nullptr};
	entt::entity _player {entt::null};
	systems::InfluenceSystem _influence;
};

TEST_F(VirtualGrant, NearTheHandPastTheBorderThePlayerHasTheShareLeft)
{
	const glm::vec3 out {1300.0f, 0.0f, 1000.0f};
	HandOut(0.5f, out);
	MoveHand(out);
	EXPECT_FLOAT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, out + glm::vec3(3.0f, 0.0f, 0.0f)), 0.5f);
	// Half the share reaches only half as far
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, out + glm::vec3(7.0f, 0.0f, 0.0f)), 0.0f);
	// Its own influence knows nothing of it
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_ONE, out), 0.0f);
}

TEST_F(VirtualGrant, NearTheHandTheShareTakesThePlaceOfTheOwnInfluence)
{
	// Just back inside with a little left: near the hand the player has that little, further off their own
	HandOut(0.3f, k_Middle);
	EXPECT_FLOAT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, k_Middle), 0.3f);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, k_Middle + glm::vec3(20.0f, 0.0f, 0.0f)), 1.0f);
}

TEST_F(VirtualGrant, BetweenTurnsItIsMeasuredFromTheHandNowAndDuringOneFromTheHandThen)
{
	const glm::vec3 then {1300.0f, 0.0f, 1000.0f};
	const glm::vec3 now {1300.0f, 0.0f, 1200.0f};
	HandOut(1.0f, then);
	MoveHand(now);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, now), 1.0f);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, then), 0.0f);
	_influence.SetInGameTurn(true);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, now), 0.0f);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, then), 1.0f);
}

TEST_F(VirtualGrant, TheHandItselfIsInWhileAnyShareIsLeft)
{
	const glm::vec3 far {1900.0f, 0.0f, 1900.0f};
	auto& state = HandOut(0.1f, {1300.0f, 0.0f, 1000.0f});
	EXPECT_TRUE(_influence.IsHandInInfluence(PlayerNames::PLAYER_ONE, far));
	state.fraction = 0.0f;
	EXPECT_FALSE(_influence.IsHandInInfluence(PlayerNames::PLAYER_ONE, far));
	// Inside, the hand has the player's own influence
	EXPECT_TRUE(_influence.IsHandInInfluence(PlayerNames::PLAYER_ONE, k_Middle));
}

TEST_F(VirtualGrant, AnotherPlayersShieldKeepsTheShareOut)
{
	const glm::vec3 out {1300.0f, 0.0f, 1000.0f};
	HandOut(1.0f, out);
	MoveHand(out);
	auto& registry = Locator::entitiesRegistry::value();
	const auto shield = registry.Create();
	registry.Assign<components::Transform>(shield, out, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::AntiInfluence>(shield, PlayerNames::PLAYER_TWO, 20.0f);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, out), 0.0f);
	EXPECT_FALSE(_influence.IsHandInInfluence(PlayerNames::PLAYER_ONE, out));
}

TEST_F(VirtualGrant, APlayerWithoutAHandShareHasTheirOwnInfluenceOnly)
{
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, k_Middle), 1.0f);
	EXPECT_EQ(_influence.PlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(1300.0f, 0.0f, 1000.0f)), 0.0f);
}

TEST_F(VirtualGrant, WithinAScriptsAntiInfluenceThePlayerHasNone)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto anti = registry.Create();
	registry.Assign<components::Transform>(anti, k_Middle, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::InfluenceSource>(anti, PlayerNames::PLAYER_ONE, 30.0f, true);
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_ONE, k_Middle), 0.0f);
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_ONE, k_Middle + glm::vec3(29.0f, 0.0f, 0.0f)), 0.0f);
	// Outside it the player's own influence is whole, and the other players keep theirs
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_ONE, k_Middle + glm::vec3(50.0f, 0.0f, 0.0f)), 1.0f);
}

TEST_F(VirtualGrant, AScriptsInfluenceGoesAboutWithItsObject)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	const glm::vec3 away {1500.0f, 0.0f, 1500.0f};
	registry.Assign<components::Transform>(object, away, glm::mat3(1.0f), glm::vec3(1.0f));
	const auto ring = registry.Create();
	registry.Assign<components::Transform>(ring, k_Middle, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::InfluenceSource>(ring, PlayerNames::PLAYER_TWO, 20.0f, false, object);
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_TWO, away), 1.0f);
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_TWO, k_Middle), 0.0f);
	// Once its object has gone it stays where it was put
	registry.Destroy(object);
	EXPECT_EQ(_influence.PlayerRawInfluence(PlayerNames::PLAYER_TWO, k_Middle), 1.0f);
}
