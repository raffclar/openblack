/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::particles
{
class LightSheet;
}

namespace openblack::ecs::components
{

/// A place creatures fight in, a circle on the land (see creature_arena). Its middle is the entity's position.
struct CreatureArena
{
	/// Its middle in 16.16 map units (x, z), as arenas are measured from
	glm::ivec2 place {0};
	float radius {0.0f};
	/// Made for one fight, it goes when the creature that made it leaves the fight; the land's own arenas stay
	bool temporary {false};
	/// A fight is on in it, between these two, the first the creature that took the arena
	bool fightOn {false};
	entt::entity first {entt::null};
	entt::entity second {entt::null};
	/// The ring of light standing round it, shown only while its fight is on
	std::shared_ptr<particles::LightSheet> ring;
};

} // namespace openblack::ecs::components
