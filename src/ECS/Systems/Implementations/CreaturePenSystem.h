/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

class CreaturePenSystem final: public CreaturePenSystemInterface
{
public:
	/// What the pens need from the rest of the game
	struct World
	{
		/// Where a temple's pen is (x, z), from its mesh; none when its mesh marks no pen
		std::function<std::optional<glm::vec2>(entt::entity temple)> penPlace;
		/// The height of the land at a point (x, z)
		std::function<float(glm::vec2)> groundAt;
		/// The scale a creature's body is drawn at for a size
		std::function<float(CreatureType species, float size)> drawnScale;
	};

	/// With the game's temples' meshes, land and creature meshes
	CreaturePenSystem();
	explicit CreaturePenSystem(World world);

	void ProcessTurn() override;
	/// A turn over the given registry
	void ProcessTurn(Registry& registry) const;

private:
	World _world;
};

} // namespace openblack::ecs::systems
