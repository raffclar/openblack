/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

namespace openblack::frame_pace
{

/// When an effect the game moved on once a frame moves on. The game drew its frames at the screen's refresh (it waits
/// for the vertical sync by default, at the display's default rate), so such an effect stepped at most 60 times a second.
/// Stepped every frame at hundreds of frames a second it would look different, so it steps no more often than that
/// refresh, each step taking all the game time since the last; slower frames step once each, as the game's did.
struct Pace
{
	/// The time between steps at the screen's refresh
	static constexpr std::chrono::duration<float, std::milli> k_StepInterval {1000.0f / 60.0f};

	/// The real time since the last step that counts towards the next
	std::chrono::duration<float, std::milli> sinceStep {0.0f};
	/// The game time since the last step, which the next step moves the effect on by
	std::chrono::milliseconds gameSinceStep {0};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.sinceStep, self.gameSinceStep);
	}
};

/// The game time an effect moves on by after a frame of this real and game time; nothing on a frame it doesn't step
[[nodiscard]] std::chrono::milliseconds Step(Pace& pace, std::chrono::milliseconds frameRealTime,
                                             std::chrono::milliseconds frameGameTime);
/// Whether an effect that the game made once a frame steps after a frame of this real time. Use it for an effect that
/// steps while the game is paused too, such as the splashes that the rain makes.
[[nodiscard]] bool Due(Pace& pace, std::chrono::milliseconds frameRealTime);

} // namespace openblack::frame_pace
