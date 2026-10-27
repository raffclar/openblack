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

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHandCreatureWorld.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems::computer_world
{

/// What the hand of the god does with the god's creature in the game's world
class GameHandCreature final: public computer_hand::CreatureWorld
{
public:
	GameHandCreature(PlayerNames player, components::ComputerPlayer& state);

	bool ForceAction(entt::entity creature, uint32_t action, entt::entity object) override;
	[[nodiscard]] bool PlanForced(entt::entity creature) const override;
	void ForceDominantDesire(entt::entity creature, int32_t desire, float seconds) override;
	[[nodiscard]] std::optional<computer_hand::CreatureSpeeds> SpeedsOf(entt::entity creature) const override;
	void DrawForShortJourney() override;
	void SetRequiredSpeed(entt::entity creature, float speed) override;

private:
	[[maybe_unused]] PlayerNames _player;
	[[maybe_unused]] components::ComputerPlayer& _state;
};

} // namespace openblack::ecs::systems::computer_world
