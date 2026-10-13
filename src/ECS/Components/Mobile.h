/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Mobile
{
	char dummy;
};

struct MobileStatic
{
	MobileStaticInfo type; ///< This is 32 bits but could be 8 bits if stored in uint8_t
};

struct MobileObject
{
	MobileObjectInfo type; ///< This is 32 bits but could be 8 bits if stored in uint8_t
};

/// A static thing's height above the land as it was put down, kept once the land under it has been changed (it doesn't
/// move with the land): its height then, and where it lay, for as long as it still lies there
struct PlacedHeight
{
	float height {0.0f};
	glm::vec3 position {0.0f};
};

/// A mobile static that is a rock: a thing of its own to the scripts
struct Rock
{
};

} // namespace openblack::ecs::components
