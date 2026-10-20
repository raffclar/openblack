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
#include <cstdint>

#include <array>
#include <bit>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The camera's bookmarks: the places that the number keys keep, the order in which the Tab keys step through them, the
/// bookmark that the camera follows, and the throb of the markers on the land. Pure rules, tested without the game.
namespace openblack::camera_bookmarks
{

/// The number keys 1 to 7 keep a bookmark each
inline constexpr size_t k_KeySlots = 7;
/// The Tab keys step through this many slots. The slot after the last key slot has no key, so it is never set.
inline constexpr size_t k_SteppedSlots = 8;
/// The steps go round this many slots. The town key and the go-back key keep their places in the slots after the
/// stepped slots.
inline constexpr size_t k_AllSlots = 11;
/// The slot of the go-back key: the camera's view before the last jump to a bookmark
inline constexpr size_t k_BackSlot = 10;

/// A marker hangs this far above its place on the land, or above the thing that it is on
inline constexpr float k_MarkerLift = 2.0f;

/// The markers throb on a clock of milliseconds that goes round every two seconds. The height of a marker is a fifth
/// of a second behind its width.
inline constexpr int32_t k_PulsePeriodMs = 2000;
inline constexpr int32_t k_PulseLagMs = 200;
/// The turns of the throb in each millisecond, the angle of one turn, and the least half size of a marker, as the
/// game keeps them
inline constexpr float k_PulseTurnsPerMs = std::bit_cast<float>(0x3A03126Fu);
inline constexpr float k_PulseTurnAngle = std::bit_cast<float>(0x40C90FDBu);
inline constexpr double k_PulseLeast = std::bit_cast<double>(0x3FC99999A0000000ull);

/// The clock of the throb after a frame: it moves on by the frame's game time. When it goes past the full period, it
/// goes back by whole periods. A clock at exactly the full period stays there for one frame.
[[nodiscard]] int32_t StepPulseClock(int32_t clockMs, int32_t frameMs);
/// The half width and the half height of every marker at a time of the clock. Each swings from 0.2 to 1.2 once a
/// second.
[[nodiscard]] glm::vec2 MarkerHalfSize(int32_t clockMs);

/// The place of a marker: above its place on the land, or above the thing that it is on
[[nodiscard]] glm::vec3 MarkerPlace(glm::vec3 point);

/// The slot that Ctrl and Tab jumps to: the next set slot after the current slot, round all the slots. The search can
/// come back to the current slot. Only the stepped slots can be the result.
[[nodiscard]] std::optional<size_t> NextSet(size_t current, const std::array<bool, k_SteppedSlots>& set);
/// The slot that Shift and Tab jumps to: the last set slot before the current slot, round all the slots. The search
/// never comes back to the current slot.
[[nodiscard]] std::optional<size_t> PreviousSet(size_t current, const std::array<bool, k_SteppedSlots>& set);

/// What Shift and a number key does with a set bookmark
enum class FollowKey : uint8_t
{
	/// The bookmark is on the land: the camera jumps to it and stops any follow
	Jump,
	/// The bookmark is on a thing that the camera does not follow: the camera starts to follow it
	Follow,
	/// The camera follows the bookmark already: it stops
	Stop,
};
[[nodiscard]] FollowKey ShiftKey(bool onThing, std::optional<size_t> followed, size_t slot);

} // namespace openblack::camera_bookmarks
