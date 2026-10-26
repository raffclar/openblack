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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// A see-through copy of something, blended over what is behind it by its alpha, as the second totem shows the other
/// share. It is only a picture, a thing of the drawn registry: the cursor passes through it.
struct SeeThrough
{
	/// Its alpha, 0 to 255
	uint8_t alpha {255};
	/// The thing of the land that it copies
	entt::entity of {entt::null};
};

} // namespace openblack::ecs::components
