/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <optional>
#include <span>

#include <glm/vec2.hpp>

/// The rules of a building going up: how much of it stands built, how a script or its builders add to it, and when it is
/// finished. Free of the game's state so they can be tested on their own.
namespace openblack::building_construction
{

/// How much of a building stands built, 0 to 1, and whether it has just been finished
struct Progress
{
	float built;
	bool finished;
};

/// A script sets how much of a building is built: never below none, and finished, all of it built, at all of it or more
[[nodiscard]] Progress SetBuilt(float built);

/// Builders add to (or take from) how much of an unfinished building is built: never below none, and finished, all of it
/// built, once it reaches all of it
[[nodiscard]] Progress BuildBy(float built, float amount);

/// A script asks a town to build what it planned at a place with a desire, and the building site takes five times it
inline constexpr float k_ScriptDesireScale = 5.0f;
[[nodiscard]] constexpr float SiteDesire(float scriptDesire)
{
	return scriptDesire * k_ScriptDesireScale;
}

/// How near a town's planned building must be to a place a script names for the script to start it: its distance across
/// the land, less its model's reach across the ground, less this, must be no more than this
inline constexpr float k_PlannedSearchRadius = 1.0f;

/// A planned building that a script may start: where it is across the land and its model's reach across the ground
struct PlannedCandidate
{
	glm::vec2 position;
	float reach;
};

/// Which of the planned buildings a script naming a place starts: of those whose distance less their reach and the
/// search radius is within the search radius, the nearest by that measure, the later one on a tie. None if there are
/// none.
[[nodiscard]] std::optional<std::size_t> PlannedAt(std::span<const PlannedCandidate> planned, glm::vec2 place,
                                                   float radius = k_PlannedSearchRadius);

/// The reach across the ground of a building's model: the larger of its half widths, scaled
[[nodiscard]] float ModelReach(glm::vec2 halfWidths, float scale);

/// How far a temple's inner walls stand in from its outer ones while it goes up, in its model's own units, whatever its
/// materials (other buildings set them in less)
inline constexpr float k_TempleInnerWallInset = 1.0f;

} // namespace openblack::building_construction
