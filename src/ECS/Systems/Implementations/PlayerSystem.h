/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <unordered_map>

#include "ECS/Systems/PlayerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class PlayerSystem final: public PlayerSystemInterface
{
public:
	static constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

	void RegisterPlayers() override;
	void AddPlayer(entt::entity playerEntity) override;
	[[nodiscard]] entt::entity GetPlayer(PlayerNames playerName) const override;
	/// There is one interface, the first player's: the game makes that player first on every land
	[[nodiscard]] PlayerNames LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	void ClearPlayers() override;

	[[nodiscard]] components::Alignment& Alignment(PlayerNames name) override;
	[[nodiscard]] components::PlayerMagic& MagicWithoutEntity(PlayerNames name) override;
	void ClearMagicWithoutEntity() override;

private:
	std::unordered_map<PlayerNames, entt::entity> _players;
	std::array<components::Alignment, k_Players> _alignment {};
	std::array<components::PlayerMagic, k_Players> _magicWithoutEntity {};
};
} // namespace openblack::ecs::systems
