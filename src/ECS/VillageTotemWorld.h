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

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::village_totem
{

/// What the village totems need of the rest of the game: the land, how big things stand, the players' temples and
/// creatures, and sounds
class WorldInterface
{
public:
	virtual ~WorldInterface() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// The model of the icon on a player's town totems: the player's creature's species, or the hand without one
	[[nodiscard]] virtual entt::id_type IconMeshFor(PlayerNames player) const = 0;
	/// How big an object stands: its radius across the land and its height
	struct Size
	{
		float radius {0.0f};
		float height {0.0f};
	};
	[[nodiscard]] virtual Size SizeOf(entt::entity object) const = 0;
	[[nodiscard]] virtual float LandHeightAt(glm::vec2 point) const = 0;
	/// Whether a player's temple stands built
	[[nodiscard]] virtual bool TempleBuilt(PlayerNames player) const = 0;
	/// Whether a building stands built and working
	[[nodiscard]] virtual bool Built(entt::entity building) const = 0;
	/// The totem's moving sound loops while on
	virtual void SetMovingSound(entt::entity totem, bool on) = 0;
	/// The village bell sounds once at a point
	virtual void RingBell(glm::vec3 position) = 0;
	/// The hand takes hold of a player's totem: the living near it react to the hand using it
	virtual void ReactToHandUsingTotem(entt::entity totem, PlayerNames player, glm::vec3 position) = 0;
	/// The player's creature, if it can see the totem, takes it the player wants to impress, by half
	virtual void EmpathiseWithPlayer(PlayerNames player, glm::vec3 position) = 0;
	/// A number floats up from a point, in a colour (0xAARRGGBB)
	virtual void FloatNumber(glm::vec3 position, float value, uint32_t colour) = 0;
};

} // namespace openblack::ecs::village_totem
