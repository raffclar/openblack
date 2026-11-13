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

#include <glm/vec2.hpp>

/// The rules of the ghost that a thing leaves when it goes whole into a store or a building site, when an altar takes
/// it, or when a script deletes it. The ghost is a copy of the thing's model. It breaks up through a moving pattern for
/// half a second of game time.
namespace openblack::ecs::destruction_ghost
{

/// The time that a ghost shows, in milliseconds of game time
constexpr int32_t k_LifeMilliseconds = 500;
/// The alpha reference of the second pass. The thing draws in its own materials, and the materials with an alpha test
/// use this reference.
constexpr uint8_t k_OwnMaterialsAlphaRef = 10;
/// How fast the pattern moves along its second axis, and how far
constexpr double k_ScrollRate = 0.7;
constexpr double k_ScrollHeight = 1.7000000476837158;

/// The time left after a frame that a ghost shows in. The first frame that draws a ghost does not take time from it.
/// The ghost goes when no time is left.
[[nodiscard]] constexpr int32_t TimeLeftAfter(int32_t millisecondsLeft, int32_t frameMilliseconds, bool shown)
{
	return shown ? std::max(millisecondsLeft - frameMilliseconds, 0) : millisecondsLeft;
}

/// The alpha reference of the pattern, from 0 to 255. A texel of the pattern lays the ghost's depth and colour when its
/// alpha is at the reference or more. At the start the reference is 0, so all of the ghost shows. At the end it is
/// 255, so nearly none of it shows.
[[nodiscard]] inline uint8_t PatternAlphaRef(int32_t millisecondsLeft)
{
	const auto passed =
	    static_cast<int32_t>(static_cast<float>(millisecondsLeft) * 255.0f / static_cast<float>(k_LifeMilliseconds));
	return static_cast<uint8_t>(0xFF - std::clamp(passed, 0, 0xFF));
}

/// How far the pattern has moved across the ghost's model, by the share of the time that is left
[[nodiscard]] inline glm::vec2 PatternScroll(int32_t millisecondsLeft)
{
	const double share = static_cast<float>(millisecondsLeft) / static_cast<float>(k_LifeMilliseconds);
	return {static_cast<float>(std::cos(share) + std::cos(share)),
	        static_cast<float>(std::sin(share * k_ScrollRate) * k_ScrollHeight)};
}

} // namespace openblack::ecs::destruction_ghost
