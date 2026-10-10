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

#include <memory>

#include "3D/CameraTrack.h"

namespace openblack::ecs::components
{

/// A thing walking one of the camera editor's tracks, as a script told it to, until it has gone its share of the way
struct WalkPath
{
	/// The track's number in the file
	int32_t number {0};
	std::shared_ptr<const edt::EDTTrack> track;
	camera_track::Walk walk;
	/// A living walks it in its own state at its own speed, rather than being moved along it at the start of the turn
	bool living {false};
	/// The walking speed its step was worked out for, in whole map units a turn
	int32_t speed {0};
};

} // namespace openblack::ecs::components
