/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include "3D/TempleLight.h"
#include "Common/X87.h"

/// The water of the pool in the temple's main room: two layers of one mesh. When the alpha of one layer increases, the
/// alpha of the other layer decreases.
namespace openblack::temple_pool
{

/// The layers take this many sixteenths of each byte of the temple's light
constexpr int32_t k_LightSixteenths = 13;
/// The alpha of the first layer moves this distance to each side of its middle
constexpr double k_AlphaReach = 32.0;
constexpr double k_AlphaMiddle = 128.0;

/// The alpha of the first layer at a time in seconds, of 255. The sine keeps its full precision until the multiply.
/// The multiply and the add each round to a float, and the cast removes the fraction.
[[nodiscard]] inline int32_t FirstLayerAlpha(float seconds)
{
	return static_cast<int32_t>(x87::Add(x87::Mul(x87::Sin(seconds), k_AlphaReach), k_AlphaMiddle));
}

/// The alpha of the second layer, of 255: the part of 255 that the first layer does not have
[[nodiscard]] constexpr int32_t SecondLayerAlpha(int32_t firstLayerAlpha)
{
	return (255 - firstLayerAlpha) & 0xFF;
}

/// The colour that multiplies the layers: each byte of the temple's light times 13, divided by 16 and rounded down.
/// The layers get all of the colour that the light adds.
[[nodiscard]] inline glm::vec3 LayerLight(const TempleLight& light)
{
	const auto bytes = glm::floor(light.multiply * 255.0f + 0.5f);
	return glm::floor(bytes * static_cast<float>(k_LightSixteenths) / 16.0f) / 255.0f;
}

} // namespace openblack::temple_pool
