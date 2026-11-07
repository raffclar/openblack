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

#include <glm/vec2.hpp>

#include "Common/ModelInverseSquareRoot.h"

/// The ambient wind: the direction of the background weather's wind along the ground. Each turn sets it again as the
/// storms move on. The sea's texture drifts with it.
namespace openblack::ecs::ambient_wind
{

/// Each unit of a background wind byte is an eighth of a unit of wind
inline constexpr float k_ByteScale = 0.125f;

/// The ambient wind before the first turn: the direction of 1 along x and 2 along z
inline constexpr glm::vec2 k_StartWind {1.0f, 2.0f};

/// A wind along the ground made one unit long by the 3D engine's inverse square root. A calm wind stays calm.
[[nodiscard]] inline glm::vec2 Normalise(glm::vec2 wind)
{
	// The wind has no height, which adds 0 to the sum
	const float lengthSquared = ((wind.x * wind.x) + 0.0f) + (wind.y * wind.y);
	const float inverse = gutils::ModelInverseSquareRoot(lengthSquared);
	return {inverse * wind.x, inverse * wind.y};
}

/// The ambient wind of a turn from the background weather's wind bytes
[[nodiscard]] inline glm::vec2 FromBackground(int8_t windX, int8_t windZ)
{
	return Normalise({static_cast<float>(windX) * k_ByteScale, static_cast<float>(windZ) * k_ByteScale});
}

/// The ambient wind before the first turn
[[nodiscard]] inline glm::vec2 Start()
{
	return Normalise(k_StartWind);
}

} // namespace openblack::ecs::ambient_wind
