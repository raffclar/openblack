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
#include <optional>

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
	void ProcessTurn() override;
	void FillEmptySlots(std::span<const PlayerNames> gods, std::span<const PlayerNames> humans) override;
	void WaitForHumans() override;
	void HumansJoined(std::span<const PlayerNames> humans) override;
	void SetAiSeed(std::optional<uint32_t> seed) override { _aiSeed = seed; }

private:
	/// The computer mind of the player, made with the player when the land has none
	components::ComputerPlayer& MindOf(PlayerNames player);

	std::unique_ptr<computer_player::World> _world;
	std::optional<uint32_t> _aiSeed;
};

} // namespace openblack::ecs::systems
