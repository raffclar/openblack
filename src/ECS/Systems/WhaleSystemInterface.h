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

namespace openblack::ecs::systems
{

/// The whales: the opening's sharks
class WhaleSystemInterface
{
public:
	virtual ~WhaleSystemInterface() = default;

	/// At the start of each turn, before anything moves them: each whale's turn starts where it is
	virtual void ProcessTurn() = 0;
	/// Each frame: they swim, drawn between where they were at the start of the turn and where they are, facing the way
	/// they move, and their wake spreads on the water
	virtual void Update(std::chrono::duration<float, std::milli> gameTime, float turnFraction) = 0;
	/// The milliseconds counted towards the next wake ring, shared by every whale
	[[nodiscard]] virtual int32_t GetWakeTimer() const = 0;
};

} // namespace openblack::ecs::systems
