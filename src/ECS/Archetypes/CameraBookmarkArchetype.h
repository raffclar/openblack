/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/fwd.hpp>

#include "Camera/CameraBookmarks.h"

namespace openblack::ecs::archetypes
{

class CameraBookmarkArchetype
{
public:
	/// The markers of the bookmarks of the number keys, none of them set
	static std::array<entt::entity, camera_bookmarks::k_KeySlots> CreateAll();
};

} // namespace openblack::ecs::archetypes
