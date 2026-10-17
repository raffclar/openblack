/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <span>

#include <glm/vec3.hpp>

#include "Common/GUtilsDistance.h"

/// Where a street lantern can be made
namespace openblack::ecs::street_lanterns
{

/// How near a thing of the statics' kind can stand to a new lantern, in metres
inline constexpr float k_Spacing = 0.5f;

/// No lantern is made where a thing of the statics' kind in the lantern's map cell stands less than the spacing away,
/// measured across the land as the game measures
[[nodiscard]] inline bool Refused(glm::vec3 at, std::span<const glm::vec3> staticsInCell)
{
	return std::ranges::any_of(staticsInCell,
	                           [at](glm::vec3 other) { return gutils::GetDistanceInMetres(at, other) < k_Spacing; });
}

} // namespace openblack::ecs::street_lanterns
