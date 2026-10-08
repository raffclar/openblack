/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{
/// Where lines meet the land, as the game tests them: the camera, the hand, the gestures and the debug tools ask it for
/// the land or sea under a pixel instead of casting rays through a physics world
class LandPickSystemInterface
{
public:
	virtual ~LandPickSystemInterface() = default;

	/// The land or sea under a pixel: along the line from the camera through the pixel's point on the near plane
	/// (land_pick::UnderPixel); without the sea, only the land
	[[nodiscard]] virtual std::optional<glm::vec3> LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint,
	                                                              bool withSea) const = 0;
	/// The x and z where the line from `from` through `to`, carried on to the map's edge, first meets the land
	[[nodiscard]] virtual std::optional<glm::vec2> LandAlong(glm::vec3 from, glm::vec3 to) const = 0;
};
} // namespace openblack::ecs::systems
