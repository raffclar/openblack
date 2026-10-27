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

#include <memory>

#include <entt/entity/entity.hpp>

#include "ECS/ComputerPlayerStart.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class ComputerPlayerSystem final: public ComputerPlayerSystemInterface
{
public:
	/// In the game's own world
	ComputerPlayerSystem();
	/// In a world of its own, as tests give it
	explicit ComputerPlayerSystem(std::unique_ptr<computer_player::World> world);
	~ComputerPlayerSystem() override;

	void SwitchOn(PlayerNames player, bool on) override;
	void SetCreatureLike(PlayerNames player, PlayerNames like) override;
	void SetActive(PlayerNames player, bool on) override;
	void SetPaused(PlayerNames player, bool paused) override;
	void SetPersonality(PlayerNames player, std::string_view name, float weight) override;
	void SetSuppression(PlayerNames player, std::string_view name, float suppression) override;
	[[nodiscard]] bool Ready(PlayerNames player) override;
	void SetAttitude(PlayerNames player, PlayerNames other, float attitude) override;
	[[nodiscard]] float Attitude(PlayerNames player, PlayerNames other) const override;
	void ProcessTurn() override;
	void FillEmptySlots(std::span<const PlayerNames> gods, std::span<const PlayerNames> humans) override;
	void WaitForHumans() override;
	void HumansJoined(std::span<const PlayerNames> humans) override;

private:
	/// The computer mind of the player, made with the player when the land has none
	components::ComputerPlayer& MindOf(PlayerNames player);
	/// The turn of the mind of one player, when the player has a mind
	void MindTurn(PlayerNames name, entt::entity entity, uint32_t turn);

	std::unique_ptr<computer_player::World> _world;
};

} // namespace openblack::ecs::systems
