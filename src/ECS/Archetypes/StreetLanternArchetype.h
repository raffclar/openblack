/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class StreetLanternArchetype
{
public:
	/// A lantern of the given kind: the street lantern of the towns, or any other kind for a country lantern on a campfire
	static entt::entity Create(const glm::vec3& position, MobileStaticInfo info);
	/// A street light of a land script at the place. Nothing shows it, and nothing finds it on the map.
	static entt::entity CreateStreetLight(const glm::vec3& position);
	StreetLanternArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
