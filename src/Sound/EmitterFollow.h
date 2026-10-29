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

#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "HearingRange.h"

namespace openblack::audio
{

/// What a placed sound does in each audio turn of the game
enum class Follows : uint8_t
{
	/// It stays where it started and plays to its end, however far the camera goes
	Nothing,
	/// It moves to the point of its owner. It stops when its owner is gone, is not available or has no point.
	Owner,
	/// It moves to the listener, as a sound that has no owner but follows
	Listener,
	/// It stays at its place, or its caller moves it. It stops when the camera is too far, as a followed sound does.
	Place,
};

/// What the audio turn does to a placed sound
struct FollowStep
{
	/// The sound stops at once
	bool stop {false};
	/// The sound moves to this point
	std::optional<glm::vec3> moveTo;
};

/// The audio turn's step for a placed sound that plays. ownerPoint is the point of the owner, or empty when the owner
/// is gone, is not available or has no point. The sound stops when the listener is at or beyond its maximum distance
/// from the new point.
[[nodiscard]] inline FollowStep StepFollowingSound(Follows follows, const std::optional<glm::vec3>& ownerPoint,
                                                   const glm::vec3& position, const glm::vec3& listener, float voiceMaxDistance)
{
	glm::vec3 target = position;
	switch (follows)
	{
	case Follows::Nothing:
		return {};
	case Follows::Owner:
		if (!ownerPoint.has_value())
		{
			return {.stop = true};
		}
		target = *ownerPoint;
		break;
	case Follows::Listener:
		target = listener;
		break;
	case Follows::Place:
		break;
	}
	if (!IsWithinPlayRange(glm::distance(listener, target), voiceMaxDistance))
	{
		return {.stop = true};
	}
	if (target == position)
	{
		return {};
	}
	return {.moveTo = target};
}

} // namespace openblack::audio
