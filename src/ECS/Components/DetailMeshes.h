/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/core/fwd.hpp>

namespace openblack::ecs::components
{

/// A model drawn in less detail the further into the view it stands (see graphics::mesh_detail): its high, standard and
/// low meshes, whose skeletons are the same, so that one pose drives them all. Its Mesh is the one the rest of the game
/// knows it by. The standard mesh's bounding box decides the bands.
struct DetailMeshes
{
	std::array<entt::id_type, 3> meshes;
	/// How important it is to keep in detail, which widens its bands
	float importance {0.0f};
};

} // namespace openblack::ecs::components
