/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "FakeWebSocketClient.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/NetworkGameSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"

using namespace openblack;

namespace
{

using Hands = std::array<entt::entity, static_cast<size_t>(ecs::systems::HandSystemInterface::Side::_Count)>;
using HandPositions =
    std::array<std::optional<glm::vec3>, static_cast<size_t>(ecs::systems::HandSystemInterface::Side::_Count)>;

/// The hands of a computer, with the god that a network game gives it
class FakeHands final: public ecs::systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] Hands GetPlayerHands() const noexcept override { return {entt::null, entt::null}; }
	[[nodiscard]] HandPositions GetPlayerHandPositions() const noexcept override { return {}; }
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
	void SetOwnPlayer(PlayerNames player) override { own = player; }
	void ClearOwnPlayer() override { own.reset(); }
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return own.value_or(PlayerNames::PLAYER_ONE); }
	[[nodiscard]] bool IsOwnPlayer(PlayerNames player) const noexcept override { return own == player; }

	std::optional<PlayerNames> own {PlayerNames::PLAYER_ONE};
};

class NetworkGameEnd: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The network game writes its log lines to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::time::emplace<ecs::systems::TimeSystem>([] { return uint32_t {0}; });
		Locator::handSystem::emplace<FakeHands>();
	}
	void TearDown() override
	{
		Locator::handSystem::reset();
		Locator::time::reset();
	}

	/// A join from the front end, as a player makes it
	static ecs::systems::NetworkJoin Join()
	{
		return {.url = "wss://example.org/game",
		        .ticket = "ticket",
		        .gamePassword = {},
		        .name = "Player",
		        .testAuthorities = std::nullopt,
		        .fromCommandLine = false,
		        .partAt = std::nullopt,
		        .partAgain = false,
		        .testCreatureFile = std::nullopt,
		        .origin = online::GameOrigin::Lan};
	}

	static FakeHands& Hands() { return static_cast<FakeHands&>(Locator::handSystem::value()); }

	test_fakes::FakeWebSocketClient sockets;
};

} // namespace

TEST_F(NetworkGameEnd, AfterTheNetworkGameThePlayerOfThisComputerIsTheFirstPlayerAgain)
{
	ecs::systems::NetworkGameSystem network(sockets, {}, [] { return std::vector<uint8_t> {}; });
	ASSERT_TRUE(network.Join(Join()));
	Locator::time::value().SetNetworkGame(true);
	// The network game gave this computer the god of the second slot
	Hands().SetOwnPlayer(PlayerNames::PLAYER_TWO);

	network.EndNetworkGame();
	EXPECT_FALSE(Locator::time::value().IsNetworkGame());
	EXPECT_EQ(Hands().GetOwnPlayer(), PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(Hands().IsOwnPlayer(PlayerNames::PLAYER_ONE));
}

TEST_F(NetworkGameEnd, AnObserverIsTheFirstPlayerAgainAfterTheNetworkGame)
{
	ecs::systems::NetworkGameSystem network(sockets, {}, [] { return std::vector<uint8_t> {}; });
	ASSERT_TRUE(network.Join(Join()));
	// An observer has no god of its own in the network game
	Hands().ClearOwnPlayer();

	network.EndNetworkGame();
	EXPECT_TRUE(Hands().IsOwnPlayer(PlayerNames::PLAYER_ONE));
}
