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

#include <entt/fwd.hpp>

#include "3D/StormClouds.h"

namespace openblack::ecs::archetypes
{
class StormCloudArchetype
{
public:
	/// The next of a storm's own clouds, a puff of mist that shrinks edge on, its animation started at random from the C
	/// runtime's numbers before the cloud draws its own
	static entt::entity Create(entt::entity storm, uint8_t index, float cloudHeight, const storm_clouds::Random& random);
	StormCloudArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
