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

/// A building (or other large fixed thing) that is filed in every map cell its shape covers, rather than only in the
/// cell it stands in. Holds its mesh's box, unscaled and unturned, which its shape on the ground is made from.
struct MapFootprint
{
	glm::vec3 meshCentre;
	/// Half the mesh box's size along each of its axes
	glm::vec3 meshHalfSize;
};

} // namespace openblack::ecs::components
