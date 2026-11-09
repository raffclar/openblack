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

namespace openblack::ecs::components
{

/// A street light that a land script makes. It is not on the map and it has no model, light or sound. Only its place
/// is kept.
struct StreetLight
{
	glm::vec3 position;
};

} // namespace openblack::ecs::components
