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

#include <chrono>

namespace openblack::testbed_scenarios
{

/// When a testbed scenario moves on. Its steps change the world, so it moves on with the turns: the turns of the world,
/// and the turns of the temple while the player is in the temple. While the game's menu holds the game paused, no turn
/// plays. Then a scenario that works the menu moves on by the time of the frames, in steps of the length of one turn,
/// so that it gives its commands at the same times at each frame rate.
class ScenarioClock
{
public:
	/// The time of one step of a scenario: the length of a turn
	static constexpr auto k_Step = std::chrono::milliseconds(100);
	static constexpr float k_StepSeconds = std::chrono::duration<float>(k_Step).count();

	/// A turn of the temple moves a scenario on only while the game is not paused. A tool that pauses the game to look
	/// at a frame stops the scenario too.
	[[nodiscard]] static constexpr bool TempleTurnMoves(bool paused) noexcept { return !paused; }

	/// The steps that the time of one frame gives while the menu holds the paused game. At other times it gives none,
	/// and it does not keep the time.
	[[nodiscard]] uint32_t StepsOfFrame(std::chrono::microseconds frame, bool pausedMenu) noexcept
	{
		if (!pausedMenu)
		{
			_carried = {};
			return 0;
		}
		_carried += frame;
		const auto steps = _carried / k_Step;
		_carried -= steps * k_Step;
		return static_cast<uint32_t>(steps);
	}

private:
	/// The time of the frames that is not yet a full step
	std::chrono::microseconds _carried {0};
};

} // namespace openblack::testbed_scenarios
