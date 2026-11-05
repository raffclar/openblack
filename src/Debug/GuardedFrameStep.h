/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>
#include <cstdint>

#include <tuple>

#include "Debug/FrameWriteGuard.h"
#include "GameFrameOrder.h"
#include "Temple/TempleFrameWrites.h"

namespace openblack::frame_write_guard
{

/// What the guard of a frame step reads from the game
template <typename T>
concept StepWorld = requires(T& world) {
	{ world.Turn() } -> std::convertible_to<uint32_t>;
	{ world.TakeFrameWrites() } -> std::convertible_to<temple::FrameWrites>;
	{ world.LandLoads() } -> std::convertible_to<uint32_t>;
	{ world.IsNetworkGame() } -> std::convertible_to<bool>;
};

/// Plays one frame step under the guard of a tool's run (see Guard). With no guard, the step only plays. The guard does
/// not look at the turns, which alone may change the synced world, or at the steps before the turn guardFrom.
///
/// The guard does not tell the changes of the debug windows. The timeline of the testbed runner acts in them by the game
/// time of the frame. It is a tool for the debug builds and is not part of the game. The guard tells only the turns of
/// these changes, because the next steps can file the runner's new things in the map.
///
/// The temple of a game alone writes the synced world in some frame steps, as the original game does. The guard knows
/// these writers by name. All other writes in a frame step trip the guard. The temple can also write after the draw,
/// between two frames. The guard gives these changes and their writers to the next step.
template <typename GuardT, StepWorld World, std::invocable Play>
bool PlayGuardedStep(GuardT* guard, game_frame::Step step, uint32_t guardFrom, World& world, Play&& play)
{
	if (guard == nullptr || step == game_frame::Step::Turns || world.Turn() < guardFrom)
	{
		if (guard != nullptr)
		{
			guard->Forget();
		}
		const bool played = play();
		// The guard looks only at the frame writes of the steps that it looks at
		if (guard != nullptr)
		{
			std::ignore = world.TakeFrameWrites();
		}
		return played;
	}
	const auto loadsBefore = world.LandLoads();
	guard->Before();
	const bool played = play();
	const auto writers = world.TakeFrameWrites();
	if (!LooksAtStep(loadsBefore, world.LandLoads()))
	{
		// The step loaded a land, which makes a new synced world: the next step takes a new sample
		guard->Forget();
		return played;
	}
	if (step == game_frame::Step::DebugGui)
	{
		guard->AfterTool(game_frame::Name(step), world.Turn());
	}
	else
	{
		guard->After(game_frame::Name(step), world.Turn(), writers, world.IsNetworkGame());
	}
	return played;
}

} // namespace openblack::frame_write_guard
