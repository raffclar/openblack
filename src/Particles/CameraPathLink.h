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

namespace openblack::particles
{

/// What a miracle's camera atom shares with the camera that follows its path. The atom keeps it for as long as it lives;
/// the camera keeps only a weak hold, so that the path ends when the atom goes.
struct CameraPathLink
{
	/// The cycle time of the animation that the path goes with, in whole milliseconds
	int32_t cycleMilliseconds {0};
	/// The frames of one play of the animation, and whether it loops
	int32_t frames {1000};
	bool loop {false};

	// Written at the end of each step of the atom
	/// The atom's first step took the camera onto the path
	bool started {false};
	/// The animation's frame at the end of the last two steps
	float previousFrame {0.0f};
	float currentFrame {0.0f};
	/// The atom is drawn from this step on
	bool shown {false};
	/// The pause before the path has gone by: the camera follows the path
	bool playing {false};

	// Written by each draw
	/// The time on the path of the frame that the last draw showed
	int32_t drawnMilliseconds {0};
	/// A draw showed the last frame: the path lets go of the camera at the end of the next game turn
	bool ended {false};
};

} // namespace openblack::particles
