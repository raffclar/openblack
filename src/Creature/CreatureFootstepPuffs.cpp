/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFootstepPuffs.h"

using namespace openblack;

glm::vec3 creature_footstep_puffs::VelocityOf(float shownSize, const LocalRandom& random)
{
	const float scale = shownSize * k_SpeedPerSize;
	const auto axis = [&random, scale]() {
		return (static_cast<float>(random(k_SpeedRandomRange)) - k_SpeedRandomMiddle) * scale;
	};
	const float z = axis();
	const float y = axis();
	const float x = axis();
	return {x, y, z};
}
