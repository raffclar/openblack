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

#include <algorithm>
#include <numbers>

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// The largest pan. A larger pan plays as this pan.
constexpr uint32_t k_MaxPan = 180;

/// The direction of a sound that is not in the world, from its pan, in degrees clockwise from the front of the listener.
/// The pan 90 is the front, 180 is the right and 0 is the left.
[[nodiscard]] constexpr uint32_t PanAzimuth(uint32_t pan)
{
	constexpr uint32_t k_FrontPan = 90;
	constexpr uint32_t k_FullTurn = 360;
	const auto kept = std::min(pan, k_MaxPan);
	return kept >= k_FrontPan ? kept - k_FrontPan : kept + k_FullTurn - k_FrontPan;
}

/// Where the sound library plays a sound that is not in the world, from its pan: 1 m from the listener in the direction
/// of the pan, at the height of the listener. The place is in the listener frame: x right, y forward, z up.
[[nodiscard]] inline glm::vec3 PanPlace(uint32_t pan)
{
	const double radians = static_cast<double>(PanAzimuth(pan)) * std::numbers::pi / 180.0;
	return {static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)), 0.0f};
}

} // namespace openblack::audio
