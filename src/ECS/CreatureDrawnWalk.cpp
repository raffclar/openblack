/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDrawnWalk.h"

#include <cmath>
#include <cstdint>

#include <algorithm>

#include "Common/PortableRotations.h"
#include "Creature/CreatureAnimation.h"

namespace openblack::ecs
{

components::CreatureAnimation::DrawnPlace creature_drawn_walk::PlaceAt(const components::CreatureLocomotion& walk,
                                                                       float breathPhase, float fraction)
{
	const auto heading = GameHeading(walk.fromHeading.Radians() + (walk.toHeading.TurnFrom(walk.fromHeading) * fraction));
	components::CreatureAnimation::DrawnPlace place {
	    .position = walk.fromPosition + ((walk.toPosition - walk.fromPosition) * fraction),
	    .rotation = glm::mat3(portable_math::EulerAngleY(heading.Yaw())),
	    .slots = {},
	};
	place.slots.reserve(walk.tracks.size());
	for (const auto& track : walk.tracks)
	{
		auto time = track.fromMs + (track.advanceMs * fraction);
		if (track.breathing)
		{
			time = static_cast<float>(
			    creature_animation::BreathTime(breathPhase, static_cast<uint32_t>(std::max(track.durationMs, 1.0f))));
		}
		else if (track.looping)
		{
			time = track.durationMs > 0.0f ? std::fmod(time, track.durationMs) : 0.0f;
		}
		else
		{
			time = std::clamp(time, 0.0f, std::max(track.durationMs - 1.0f, 0.0f));
		}
		place.slots.push_back({.animation = track.animation,
		                       .timeMs = time,
		                       .weight = track.weight,
		                       .mirrored = track.mirrored,
		                       .blendedWith = track.blendedWith,
		                       .blendWeight = track.blendWeight});
	}
	return place;
}

bool creature_drawn_walk::Changed(const std::optional<components::CreatureAnimation::DrawnPlace>& before,
                                  const components::CreatureAnimation::DrawnPlace& after)
{
	if (!before.has_value() || before->position != after.position || before->rotation != after.rotation ||
	    before->slots.size() != after.slots.size())
	{
		return true;
	}
	return !std::ranges::equal(before->slots, after.slots, [](const auto& left, const auto& right) {
		return left.animation == right.animation && left.timeMs == right.timeMs && left.weight == right.weight &&
		       left.mirrored == right.mirrored && left.blendedWith == right.blendedWith &&
		       left.blendWeight == right.blendWeight;
	});
}

} // namespace openblack::ecs
