/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// How important it is to keep a thing of one mesh in view: the importance widens the distance bands of the thing. Each
/// band then reaches further, as for a villager of several meshes. A thing without this component has an importance of 0.
struct DrawImportance
{
	float importance {0.0f};
};

} // namespace openblack::ecs::components
