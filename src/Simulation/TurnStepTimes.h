/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <string>

#include "GameTurnOrder.h"

namespace openblack::simulation
{

/// The time of each step of one turn, for a developer report of the turns that take a long time. It keeps no game state.
class TurnStepTimes
{
public:
	/// Adds the time of one step to the time of that step
	void Add(game_turn::Step step, float milliseconds);

	/// The time of all steps
	[[nodiscard]] float Total() const;

	/// The steps that took time, and at least `least` milliseconds, the longest first, as "step=ms" items. A step is its number
	/// in game_turn::Step.
	[[nodiscard]] std::string Report(float least) const;

private:
	std::array<float, static_cast<size_t>(game_turn::Step::_Count)> _milliseconds {};
};

} // namespace openblack::simulation
