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
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHandCastsWorld.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems::computer_world
{

/// The charge, the gesture and the cast of a miracle of the god's hand in the game's world
class GameHandCasts final: public computer_hand::CastsWorld
{
public:
	GameHandCasts(PlayerNames player, components::ComputerPlayer& state);

	[[nodiscard]] bool IconHasSite(entt::entity icon) const override;
	[[nodiscard]] bool IconCharging(entt::entity icon) const override;
	bool StartChant(entt::entity icon, MagicType type) override;
	[[nodiscard]] float ChantNeeded(entt::entity icon) const override;
	[[nodiscard]] bool Angry(MagicType type) const override;
	bool Cast(MagicType type, entt::entity target, entt::entity icon, glm::vec3 hand) override;
	[[nodiscard]] float CastSeconds(MagicType type) const override;
	[[nodiscard]] uint32_t SeedGesture(MagicType type, int32_t stage) const override;
	[[nodiscard]] float GestureLength(uint32_t gesture) const override;
	[[nodiscard]] glm::vec3 GesturePoint(uint32_t gesture, float share) const override;
	void ShowGesture(uint32_t gesture, glm::vec3 corner, float size) override;

private:
	PlayerNames _player;
	components::ComputerPlayer& _state;
};

} // namespace openblack::ecs::systems::computer_world
