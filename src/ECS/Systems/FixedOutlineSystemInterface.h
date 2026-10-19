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

#include "3D/MapCoords.h"

namespace openblack::ecs::systems
{

/// The outlines on the ground of the things that stay put (see FixedOutlines.h), and the test of whether a place is
/// free of them. The map's cells give a thing its outline each time the thing goes into them. A thing that is out of the
/// cells is in no search of a cell, so its old outline has no effect.
class FixedOutlineSystemInterface
{
public:
	virtual ~FixedOutlineSystemInterface() = default;

	/// Gives a thing that goes into the map's cells its outline, when the game gives that kind of thing one
	virtual void Insert(entt::entity entity) = 0;

	/// Whether a circle of half a unit at a place touches the outline of a thing in the place's cell. A place off the
	/// map counts as one that touches.
	[[nodiscard]] virtual bool CollidesWithFixed(const map_coords::MapCoords& place) const = 0;
	/// Whether the land at a place is water. A place off the map, or on no land block, counts as water.
	[[nodiscard]] virtual bool IsWater(const map_coords::MapCoords& place) const = 0;
	/// Whether the game makes a tree, a pot or an object at a place: the place is free of the fixed things, or it is
	/// water
	[[nodiscard]] virtual bool IsOkToCreateAt(const map_coords::MapCoords& place) const = 0;
};

} // namespace openblack::ecs::systems
