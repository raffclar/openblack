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

/// Per-entity opacity of a mesh (0 = invisible, 1 = opaque), e.g. the roots and roots piles that fade out.
/// Entities with this component are drawn in a separate alpha-blended pass after the opaque objects.
struct Alpha
{
	float value = 1.0f;
};

} // namespace openblack::ecs::components
