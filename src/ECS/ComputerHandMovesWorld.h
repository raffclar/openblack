/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

namespace openblack::computer_hand
{

/// What the moves of the hand read in the game: the places and the heights of things and of the land, and the influence
class MovesWorld
{
public:
	virtual ~MovesWorld() = default;
	/// The map position of the thing, none when it is gone
	[[nodiscard]] virtual std::optional<map_coords::MapCoords> CoordsOf(entt::entity thing) const = 0;
	/// The height of the thing
	[[nodiscard]] virtual float HeightOf(entt::entity thing) const = 0;
	/// The height of the land under a map position, in metres
	[[nodiscard]] virtual float LandHeight(const map_coords::MapCoords& coords) const = 0;
	/// The influence of the god at a map position, with the influence of its allies. 0 is no influence.
	[[nodiscard]] virtual float InfluenceAt(const map_coords::MapCoords& coords) const = 0;
};

} // namespace openblack::computer_hand
