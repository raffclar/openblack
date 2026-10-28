/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "Input/LocalPlayer.h"

using namespace openblack;

namespace
{

using Hands = std::array<entt::entity, static_cast<size_t>(ecs::systems::HandSystemInterface::Side::_Count)>;
using HandPositions =
    std::array<std::optional<glm::vec3>, static_cast<size_t>(ecs::systems::HandSystemInterface::Side::_Count)>;

/// The hands of a computer, with the god that a network game can give it
class FakeHands final: public ecs::systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] Hands GetPlayerHands() const noexcept override { return {entt::null, entt::null}; }
	[[nodiscard]] HandPositions GetPlayerHandPositions() const noexcept override { return {}; }
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
	void SetOwnPlayer(PlayerNames player) override
	{
		own = player;
		hasOwn = true;
	}
	void ClearOwnPlayer() override { hasOwn = false; }
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return own; }
	[[nodiscard]] bool IsOwnPlayer(PlayerNames player) const noexcept override { return hasOwn && player == own; }

	PlayerNames own {PlayerNames::PLAYER_ONE};
	bool hasOwn {true};
};

} // namespace

TEST(LocalInputPlayer, WithNoHandsTheInputActsForTheFirstPlayer)
{
	EXPECT_EQ(input::LocalPlayer(nullptr), PlayerNames::PLAYER_ONE);
}

TEST(LocalInputPlayer, InASinglePlayerGameTheInputActsForTheFirstPlayer)
{
	const FakeHands hands;
	EXPECT_EQ(input::LocalPlayer(&hands), PlayerNames::PLAYER_ONE);
}

TEST(LocalInputPlayer, InANetworkGameTheInputActsForTheGodOfTheSlotOfThisComputer)
{
	FakeHands hands;
	hands.SetOwnPlayer(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(input::LocalPlayer(&hands), PlayerNames::PLAYER_TWO);
	hands.SetOwnPlayer(PlayerNames::PLAYER_FOUR);
	EXPECT_EQ(input::LocalPlayer(&hands), PlayerNames::PLAYER_FOUR);
}

TEST(LocalInputPlayer, WithNoHandsOnlyTheFirstPlayerIsTheGodOfThisComputer)
{
	EXPECT_TRUE(input::IsLocalPlayer(nullptr, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(input::IsLocalPlayer(nullptr, PlayerNames::PLAYER_TWO));
}

TEST(LocalInputPlayer, InANetworkGameOnlyTheGodOfTheSlotOfThisComputerIsItsGod)
{
	FakeHands hands;
	hands.SetOwnPlayer(PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(input::IsLocalPlayer(&hands, PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(input::IsLocalPlayer(&hands, PlayerNames::PLAYER_ONE));
}

TEST(LocalInputPlayer, TheGameServerHasNoGodOfItsOwn)
{
	FakeHands hands;
	hands.ClearOwnPlayer();
	EXPECT_FALSE(input::IsLocalPlayer(&hands, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(input::IsLocalPlayer(&hands, PlayerNames::PLAYER_TWO));
}
