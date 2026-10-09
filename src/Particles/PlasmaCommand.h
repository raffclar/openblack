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

#include <glm/vec3.hpp>

namespace openblack::particles
{

/// One plasma beam an effect is asked to fire: from its start to its end, leaving and arriving along its tangents, for
/// its life, its wiggle drifting and its texture sliding by its speed, faded in and out to its alpha
struct PlasmaCommand
{
	glm::vec3 start {0.0f};
	glm::vec3 end {0.0f};
	glm::vec3 startTangent {0.0f};
	glm::vec3 endTangent {0.0f};
	/// Seconds
	float life {0.0f};
	float speed {0.0f};
	uint8_t alpha {0};
};

/// A temple heart's beams: two seconds long, at one and a half times the speed, faded to 70 of 255 at most, leaving the
/// heart straight up and arriving from straight above
inline constexpr float k_HeartPlasmaLife = 2.0f;
inline constexpr float k_HeartPlasmaSpeed = 1.5f;
inline constexpr uint8_t k_HeartPlasmaAlpha = 70;
inline constexpr glm::vec3 k_HeartPlasmaStartTangent {0.0f, 1.0f, 0.0f};
inline constexpr glm::vec3 k_HeartPlasmaEndTangent {0.0f, -1.0f, 0.0f};

} // namespace openblack::particles
