/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{
/// The roots of a living tree (the hand's, HandSystem::UpdateRoots). The tree draws them from its own drawn matrix
/// (its rotation scaled by 0.15 * the mesh extent), so the drawing builds their model from the tree's
/// (ecs::DrawnModel: in the hand, between two physics turns ...) with the roots' scale; their Transform is the logic's,
/// set per turn. A falling root (DropRoots) has none
struct TreeRoots
{
	entt::entity tree {entt::null};
	/// 0.15 x the tree mesh's extent: the nine rotation elements of the tree's drawn matrix are multiplied by it, the
	/// translation is not
	float factor {1.0f};
};
} // namespace openblack::ecs::components
