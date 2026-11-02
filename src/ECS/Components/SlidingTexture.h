/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// The texture of a thing that only the frames make slides across its model, as the water of the third land's
/// waterfall does. It slides only on the primitives whose materials let it slide. All the things of one model slide
/// together.
struct SlidingTexture
{
	/// How far the texture has slid, across and down
	glm::vec2 offset {0.0f};
};

} // namespace openblack::ecs::components
