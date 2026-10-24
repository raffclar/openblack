/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>
#include <utility>

#include "Common/FramePace.h"

/// When the effects that the land's draw moves on take a step. The game moves some effects on in the draw of the thing
/// that has them, by the game time of the frame: the pool of a teleport stone and the swirl of a miracle dispenser.
/// Such an effect takes a step only in a frame that draws its thing. Its random numbers are only this computer's, so its
/// steps do not change the world.
namespace openblack::particles::drawn_steps
{

/// The seconds by which the effects that the land draws move on after a frame, none in a frame where they do not step.
/// They step no more often than 60 times a second, and each step takes all the game time since the last. In a frame
/// that does not draw the land, a step that is due does not occur and its game time is lost, as in the game.
[[nodiscard]] inline std::optional<float> StepSeconds(frame_pace::Pace& pace, std::chrono::milliseconds frameRealTime,
                                                      std::chrono::milliseconds frameGameTime, bool landDrawn)
{
	pace.gameSinceStep += frameGameTime;
	if (!frame_pace::Due(pace, frameRealTime))
	{
		return std::nullopt;
	}
	const auto step = std::exchange(pace.gameSinceStep, std::chrono::milliseconds {0});
	if (!landDrawn)
	{
		return std::nullopt;
	}
	return std::chrono::duration<float>(step).count();
}

} // namespace openblack::particles::drawn_steps
