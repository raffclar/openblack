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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// The puzzle totems that rise out of the land and sink back into it (components::PuzzleTotem), such as the two totems
/// of the undead village. A script sets their height, a creature raises them, and the hand takes hold of one and slides
/// it up and down.
class PuzzleTotemSystemInterface
{
public:
	virtual ~PuzzleTotemSystemInterface() = default;

	/// A totem takes a height: a script, a creature or the hand sets it. The totem is put at the place that the height
	/// gives it.
	virtual void SetHeight(entt::entity totem, int32_t height) = 0;
	/// The totem is put where its height puts it: under the land by a part of its sink depth
	virtual void Place(entt::entity totem) = 0;

	/// The hand of a player, at a point, takes hold of a totem. Only a totem that rises and sinks and that no hand holds
	/// can be taken, with the hand in the player's influence. Whether it took hold.
	virtual bool Grip(entt::entity totem, PlayerNames player, glm::vec3 hand) = 0;
	/// The mouse moved up the screen by some pixels this frame while the hand holds a totem: the hand slides it. Each new
	/// height that the hand gives it is sent to the turn.
	virtual void Slide(float upPixels, float screenHeight) = 0;
	/// The hand lets go of the totem: the height that it was left at is sent to the turn
	virtual void LetGo() = 0;
	/// The totem the hand of this computer holds, if any
	[[nodiscard]] virtual std::optional<entt::entity> GetGripped() const = 0;
	/// How the hand holds the totem: where it is, how far it tips, and how far it closes round the totem
	[[nodiscard]] virtual std::optional<VillageTotemSystemInterface::HandHold> GetHandHold() const = 0;

	/// At the turn: a hand takes hold of the totem, or lets go of it (player_commands::HandLock and HandUnlock)
	virtual void DoHeld(entt::entity totem, bool held) = 0;
	/// At the turn: the totem takes the height for a share of its greatest height that the hand gave it
	/// (player_commands::TotemShare)
	virtual void DoShare(entt::entity totem, float share) = 0;
};

} // namespace openblack::ecs::systems
