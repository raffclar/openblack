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
#include "ECS/ComputerHandObjectsWorld.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems::computer_world
{

/// What the god's hand does with things in the game's world
class GameHandObjects final: public computer_hand::ObjectsWorld
{
public:
	GameHandObjects(PlayerNames player, components::ComputerPlayer& state);

	[[nodiscard]] bool HandHasRoom() const override;
	void PutDownHeld(glm::vec3 hand) override;
	[[nodiscard]] bool CanPickUp(entt::entity thing) const override;
	void PickUp(entt::entity thing, glm::vec3 hand) override;
	void UseHeldOn(entt::entity target, glm::vec3 hand) override;
	void DropHeld(glm::vec3 hand) override;
	void ThrowHeld(glm::vec3 hand, glm::vec3 velocity) override;
	[[nodiscard]] std::optional<uint32_t> HeldPotHolds(ResourceType type) const override;
	[[nodiscard]] uint32_t ResourceIn(entt::entity thing, ResourceType type) const override;
	bool TakeFromContainer(entt::entity container, ResourceType type, uint32_t amount, glm::vec3 hand) override;

private:
	/// The god places a fish farm in its hand: the hand gets a pot of food
	void PickUpFishFarm(entt::entity farm, glm::vec3 hand);

	PlayerNames _player;
	components::ComputerPlayer& _state;
};

} // namespace openblack::ecs::systems::computer_world
