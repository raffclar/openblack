/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

#include "Common/VirtualInfluence.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
#include "ECS/Systems/PlayerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class PlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override;
	void AddPlayer(entt::entity playerEntity) override;
	[[nodiscard]] entt::entity GetPlayer(PlayerNames playerName) const override;
	void AddCreature(entt::entity creature) override;
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const override;
	void KeepForNextLand() override;
	void TakeUpKept(entt::entity playerEntity) override;

private:
	/// What a player keeps from land to land
	struct Kept
	{
		std::optional<components::Alignment> alignment;
		std::array<float, 8> damageFrom {};
		uint32_t windResistance {0};
		components::Player::Miracles miracles;
		/// What their hand keeps of their influence past the border, and whether a script switched it off
		std::optional<virtual_influence::State> virtualInfluence;
	};

	std::unordered_map<PlayerNames, entt::entity> _players;
	/// Each player's as of the last land they were on
	std::unordered_map<PlayerNames, Kept> _kept;
};
} // namespace openblack::ecs::systems
