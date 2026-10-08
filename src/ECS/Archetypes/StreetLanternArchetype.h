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
	/// A street lantern from its mobile static info (CREATE_STREET_LANTERN, CHL CREATE 7 and 59)
	/// @return entt::null when another MobileStatic is within 0.5 m
	static entt::entity Create(const glm::vec3& position, MobileStaticInfo info);
	StreetLanternArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
