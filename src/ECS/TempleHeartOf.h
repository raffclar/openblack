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

#include "ECS/Components/Temple.h"
#include "ECS/Registry.h"

namespace openblack::ecs::temple_heart
{

/// The heart of a temple, none when the temple has no heart
[[nodiscard]] inline entt::entity Of(const Registry& registry, entt::entity temple)
{
	entt::entity found = entt::null;
	registry.Each<const components::TempleHeart>([temple, &found](entt::entity entity, const components::TempleHeart& heart) {
		if (heart.temple == temple && found == entt::null)
		{
			found = entity;
		}
	});
	return found;
}

/// The thing that a creature acts on for a thing that it believes in: the temple for the temple's heart, else the thing
[[nodiscard]] inline entt::entity Acted(const Registry& registry, entt::entity thing)
{
	const auto* heart = registry.Valid(thing) ? registry.TryGet<const components::TempleHeart>(thing) : nullptr;
	return heart != nullptr ? heart->temple : thing;
}

} // namespace openblack::ecs::temple_heart
