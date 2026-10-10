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

#include <optional>

#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// What the villagers' states share about their clips and the ground under them
namespace openblack::ecs::villager_clips
{

/// A clip's length in milliseconds (its play time), none for one not loaded
[[nodiscard]] std::optional<float> ClipMilliseconds(AnimId clip);
/// Whether a clip lasting so many milliseconds has played through so many times in the turns since the villager went
/// into its state, the turns counted in whole milliseconds as the game counts them
[[nodiscard]] constexpr bool HasPlayed(uint32_t turnsSinceStateChange, uint32_t turnMilliseconds, uint32_t playTime,
                                       uint32_t times)
{
	return turnsSinceStateChange * turnMilliseconds >= playTime * times;
}
/// What setting a clip on a villager does to its place in the clip
enum class ClipPlace : uint8_t
{
	/// It plays on from where it was, in whatever clip it now has
	Keep,
	/// It starts the clip from its beginning
	Restart,
};
/// A clip set on a villager: the same clip again plays on. A different one takes over from the same place, and starts
/// from its beginning only when that is asked for and the villager isn't dancing: a dancer keeps in time with the dance.
[[nodiscard]] constexpr ClipPlace PlaceOnSetClip(bool sameClip, bool restartAsked, bool dancing)
{
	return !sameClip && restartAsked && !dancing ? ClipPlace::Restart : ClipPlace::Keep;
}
/// What a dancer's clip does when its group starts a move: whether it starts over, and the turns its state counts since
/// then. It starts over only once it has played through since the dancer's state changed or the clip last started over,
/// so the move's clips start together without one being cut short, however many turns the move is started on.
struct DanceMoveClip
{
	bool restart;
	uint32_t turnsSinceStateChange;
};
[[nodiscard]] constexpr DanceMoveClip OnDanceMove(uint32_t turnsSinceStateChange, uint32_t turnMilliseconds, uint32_t playTime)
{
	if (HasPlayed(turnsSinceStateChange, turnMilliseconds, playTime, 1))
	{
		return {.restart = true, .turnsSinceStateChange = 0};
	}
	return {.restart = false, .turnsSinceStateChange = turnsSinceStateChange};
}
/// Whether a clip has played through so many times since the villager went into its state
[[nodiscard]] bool ClipPlayed(const components::LivingAction& action, AnimId clip, uint32_t times = 1);
/// What a villager's clip does when its dance group starts a move, by the clip it has and the turns since its state changed
[[nodiscard]] DanceMoveClip OnDanceMove(const components::LivingAction& action, AnimId clip);
/// Whether the land under a point is water, the shallow shore included
[[nodiscard]] bool IsOnWater(glm::vec3 point);

} // namespace openblack::ecs::villager_clips
