/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Creature/CreatureLayers.h"
#include "ECS/Components/ShownCreatureFace.h"

namespace openblack::ecs::shown_creature_faces
{

/// Changes the face of the world of a creature. The game has one face slot, so the face that this computer shows
/// changes in the same way: a face that the world pulls starts from the shown face. Only the computer that shows a face
/// changes it, and the world does not read it, so the synced state stays the same on all computers.
template <typename Registry, typename Change>
void ChangeFace(Registry& registry, entt::entity creature, creature_layers::FaceLayer& world, Change&& change)
{
	world = change(world);
	if (auto* shown = registry.template TryGet<components::ShownCreatureFace>(creature))
	{
		shown->face = change(shown->face);
		shown->worldFace = world;
	}
}

} // namespace openblack::ecs::shown_creature_faces
