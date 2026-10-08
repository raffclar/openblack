/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "3D/CameraTracks.h"

namespace openblack::ecs::components
{
/// The path data of a mobile object walking a camera track (CHL WALK_PATH), while it is in the walking list.
/// ECS/MobileWalkPaths.h.
struct MobileWalkPath
{
	std::shared_ptr<const CameraTrack> track; ///< The scripted camera track made from the path
	std::unique_ptr<CameraWayRunner> runner;  ///< The track's runner on the position way
	float to {1.0f};                          ///< Leaves the list once current / duration >= to
	bool forward {true};                      ///< False walks the samples from the end
	float current {0.0f};                     ///< from * duration, then + step each turn up to the duration
	float step {100.0f};                      ///< 100 ms of the track per game turn (a turn is 100 ms)
	float cachedSpeed {1.0f};                 ///< 1, not read by MoveAlongPath (a Living's: its speed)
};

/// The same path data for a Living walking a track (ECS/LivingWalkPath.h): the step from its speed, cachedSpeed that
/// speed (as a float). A type of its own, so that ProcessMobileWalkPaths does not move it
struct LivingWalkPath
{
	MobileWalkPath path;
	int32_t trackNumber {0};
};
} // namespace openblack::ecs::components
