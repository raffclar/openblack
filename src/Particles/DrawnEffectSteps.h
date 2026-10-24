/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <chrono>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

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

/// The order in which the frame draws some things, such as the stones or the dispensers. With a camera and a land the
/// frame draws the things of the land's list in the list's order, the nearest blocks first, then the things that are
/// not on the list's blocks. Without a camera or a land it draws every thing, in the given order.
/// `onBlocks(thing)` tells whether a thing is on the land's blocks, so that the list decides whether the frame draws it.
template <typename OnBlocks>
[[nodiscard]] std::vector<entt::entity> DrawOrder(std::span<const entt::entity> list, std::span<const entt::entity> things,
                                                  bool listDecides, OnBlocks&& onBlocks)
{
	if (!listDecides)
	{
		return {things.begin(), things.end()};
	}
	std::vector<entt::entity> order;
	order.reserve(things.size());
	std::ranges::copy_if(list, std::back_inserter(order),
	                     [things](entt::entity listed) { return std::ranges::find(things, listed) != things.end(); });
	std::ranges::copy_if(things, std::back_inserter(order), [&onBlocks](entt::entity thing) { return !onBlocks(thing); });
	return order;
}

} // namespace openblack::particles::drawn_steps
