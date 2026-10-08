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

namespace openblack::ecs::systems
{
/// What a PrepareDraw does with the instances
enum class DrawUpdate : uint8_t
{
	None,    ///< nothing changed: the instances stay as they are
	Refill,  ///< the instances are written again into the ranges they have (a refill that no longer fits rebuilds)
	Rebuild, ///< the draw lists are made again, then the instances written
};

/// The PrepareDraw of a frame: `dirty` (Registry::SetDirty), `layoutDirty` (a component of the draw layout gained or
/// lost), `optionsChanged` (a debug view turned on or off), `drawBoundingBox` (the boxes' rows are only written by a
/// rebuild)
[[nodiscard]] constexpr DrawUpdate ChooseDrawUpdate(bool dirty, bool layoutDirty, bool optionsChanged, bool drawBoundingBox)
{
	if (layoutDirty || optionsChanged)
	{
		return DrawUpdate::Rebuild;
	}
	if (!dirty)
	{
		return DrawUpdate::None;
	}
	return drawBoundingBox ? DrawUpdate::Rebuild : DrawUpdate::Refill;
}

/// Whether the frame prepares the entities' draw: not while a full screen film hides the world (`worldHidden`), since
/// the scene is not drawn then and nothing else reads the draw lists. The flags stay set, so the first frame after the
/// film takes the same path from the world as it is then
[[nodiscard]] constexpr bool ShouldPrepareDraw(bool drawEntities, bool worldHidden)
{
	return drawEntities && !worldHidden;
}
} // namespace openblack::ecs::systems
