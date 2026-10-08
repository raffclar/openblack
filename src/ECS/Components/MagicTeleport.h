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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The invisible teleport stone each TELEPORT cast leaves (a mobile static). No mesh; its vortex PSys
/// (SF_TeleportVortex) is its only visual. Magic/Objects/MagicTeleport.
struct MagicTeleport
{
	/// {the living, the map coordinates it wants to reach} of each registered traveller (newest first)
	struct Traveller
	{
		entt::entity living {entt::null};
		glm::vec3 destination {0.0f}; ///< Map coordinates as metres (y above the land)
	};
	std::vector<Traveller> travellers;
	uint32_t reaction {0};             ///< REACTION 20 REACT_TO_TELEPORT (ECS/Effects/Reactions.h id)
	uint32_t psys {0};                 ///< The SF_TeleportVortex effect (psys::manager id), 0 none
	entt::entity spell {entt::null};   ///< The SpellTeleport
	std::optional<PlayerNames> player; ///< The spell's player, if it has one
	float scale {1.0f};                ///< The object's scale: x 0.01 at Create
};

} // namespace openblack::ecs::components
