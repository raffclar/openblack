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

/// Drawn among the things that blend, each part of its model as its material says: its solid parts solid and its
/// see-through parts blended by their texture's alpha, as the light shining out of an opened reward chest
struct BlendedByMaterial
{
};

} // namespace openblack::ecs::components
