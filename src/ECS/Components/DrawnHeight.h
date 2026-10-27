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

/// The height at which the frames draw a thing that rises and sinks by the computer's clock, as a town's totem and its
/// icon do. The turns go by the thing's Transform, which the frames do not change. Only the draw, the cursor, the hand
/// and the sounds read this height. It is not a part of the synced world.
struct DrawnHeight
{
	float y {0.0f};
};

} // namespace openblack::ecs::components
