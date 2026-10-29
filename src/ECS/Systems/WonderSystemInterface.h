/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The wonders and the power that their tribes give the players. A finished wonder adds its power to the most power that
/// its tribe gave the player of its town. At each turn, the power of each tribe for each player becomes that most power,
/// and at least 1.
class WonderSystemInterface
{
public:
	virtual ~WonderSystemInterface() = default;

	/// The game makes a wonder of a size: the wonder gets the power of its size. A finished wonder adds its power to
	/// its player.
	virtual void Made(entt::entity building, float scale) = 0;
	/// The builders finish a wonder: it adds its power to its player
	virtual void Built(entt::entity building) = 0;
	/// A wonder goes from the world: a finished wonder removes its power from its player
	virtual void Lost(entt::entity building) = 0;
	/// A town goes from its player: each finished wonder of the town removes its power from that player
	virtual void TownLeavesPlayer(entt::entity town) = 0;
	/// A town goes to its new player: each finished wonder of the town adds its power to that player
	virtual void TownJoinsPlayer(entt::entity town) = 0;
	/// A scaffold starts a wonder: the wonder gets a power. Nothing changes for a building that is not a wonder.
	virtual void SetPower(entt::entity building, float power) = 0;
	/// The power of a wonder of a town at a place: see wonder::Power
	[[nodiscard]] virtual float PowerAt(entt::entity town, glm::vec3 place) const = 0;
	/// The size of a wonder of a town at a place: see wonder::Scale
	[[nodiscard]] virtual float ScaleAt(entt::entity town, glm::vec3 place) const = 0;
	/// Each turn, the power of each tribe for each player becomes the most power that the tribe gave the player, and at
	/// least 1
	virtual void ProcessTurn() = 0;
};

} // namespace openblack::ecs::systems
