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

namespace openblack::clouds
{
struct Layout;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::archetypes
{
class CloudArchetype
{
public:
	/// One of the sky's clouds, as the clouds of a land are laid out
	static entt::entity Create(const clouds::Layout& layout);
	/// Removes the clouds of the last land and makes the sky's clouds of a new land. The layout comes from the numbers
	/// of the C runtime, which every computer draws at the start of the land. The clouds are things of the drawn
	/// registry, so they take no entity numbers from the land. Only the frames of a computer that draws move them.
	static void CreateSky();
	/// Removes each thing of the drawn registry that is not a cloud of the sky. A load of a saved game keeps the clouds
	/// that the land laid out as it loaded, because a save does not keep the clouds.
	static void RemoveAllButTheSky(Registry& drawn);
	CloudArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
