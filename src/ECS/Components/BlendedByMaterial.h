/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Drawn among the things that blend: its solid parts as their materials say, and its alpha-textured parts added over
/// what is behind them by their texture's alpha, without writing depth. The light that shines out of an opened reward
/// chest is drawn so.
struct BlendedByMaterial
{
	/// The colour, 0 to 255, that its alpha-textured parts shine in, in place of the land's light (see
	/// graphics::light_shine). None to take the land's light.
	std::optional<glm::vec3> shineColour;
};

} // namespace openblack::ecs::components
