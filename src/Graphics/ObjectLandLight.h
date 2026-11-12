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

/// How a model in the object shader takes the land's light. Most models take the light of the point where they stand
/// and then the distance haze. The phantom building of a scaffold (on its site and in the hand) and the copy of a
/// scaffold at its yard place take the light of their place on the map, as the trees do: the four cells around the
/// place, with the game's weights of whole cells (land_light.sh LandLightMapAt). They take no distance haze, and the
/// land's colour of the place is added to them as it is.
namespace openblack::graphics::object_land_light
{

enum class Rule : uint8_t
{
	/// Blended by the part of the cell where the model stands, then the distance haze
	AtPoint,
	/// The cells around the model's place on the map with the weights of whole cells, and no distance haze
	AtMapPlace,
};

/// The rule of a model: `scaffoldGhost` is true for the phantom building of a scaffold and for the copy of a scaffold
/// at its yard place
[[nodiscard]] constexpr Rule Of(bool scaffoldGhost) noexcept
{
	return scaffoldGhost ? Rule::AtMapPlace : Rule::AtPoint;
}

/// Whether the model takes the distance haze
[[nodiscard]] constexpr bool Hazed(Rule rule) noexcept
{
	return rule == Rule::AtPoint;
}

/// The y of the object shader's u_lightRule for the rule: 1 for the light at the place on the map
[[nodiscard]] constexpr float UniformValue(Rule rule) noexcept
{
	return rule == Rule::AtMapPlace ? 1.0f : 0.0f;
}

} // namespace openblack::graphics::object_land_light
