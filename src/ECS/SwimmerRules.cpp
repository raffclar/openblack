/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SwimmerRules.h"

#include <bit>

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The ring lies a little above the sea's level
constexpr float k_RingHeight = std::bit_cast<float>(0x3D4CCCCDu);
constexpr float k_RingGrowth = 1.0f;
constexpr float k_RingAspect = 1.0f;
constexpr float k_RingRate = 1.0f;
constexpr uint8_t k_RingCell = 0x30;
constexpr uint32_t k_RingColour = 0xFFFFFFFFu;
} // namespace

bool swimmer_rules::Swims(bool highDetail, AnimId clip)
{
	return highDetail && clip == k_SwimClip;
}

std::optional<water_rings::Ring> swimmer_rules::Ring(int32_t& timer, int32_t frameMilliseconds, const glm::vec3& bonePlace,
                                                     const std::function<float()>& angle)
{
	timer += frameMilliseconds;
	if (timer <= k_RingInterval)
	{
		return std::nullopt;
	}
	timer = 0;
	return water_rings::Ring {.position = glm::vec3(bonePlace.x, k_RingHeight, bonePlace.z),
	                          .growth = k_RingGrowth,
	                          .angle = angle(),
	                          .aspect = k_RingAspect,
	                          .rate = k_RingRate,
	                          .cell = k_RingCell,
	                          .argb = k_RingColour};
}
