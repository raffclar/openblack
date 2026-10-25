/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OtherHeldDraw.h"

#include <algorithm>

namespace openblack::hand_grab::other_held
{

void Reported(ecs::components::HeldDrawTrack& track, glm::vec3 placeBefore)
{
	track.intervalMs = track.elapsedMs;
	track.elapsedMs = 0;
	track.from = placeBefore;
}

glm::vec3 Step(ecs::components::HeldDrawTrack& track, glm::vec3 reported, int32_t frameMs)
{
	track.elapsedMs += frameMs;
	if (track.intervalMs == 0)
	{
		return reported;
	}
	const float share = static_cast<float>(std::min(track.elapsedMs, track.intervalMs)) / static_cast<float>(track.intervalMs);
	const auto& from = track.from;
	return {
	    (reported.x - from.x) * share + from.x,
	    (reported.y - from.y) * share + from.y,
	    (reported.z - from.z) * share + from.z,
	};
}

} // namespace openblack::hand_grab::other_held
