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
#include <optional>

#include <glm/vec3.hpp>

/// The hand's tap after it knocks on a house. The hand stays upright at the point where it knocked while the tap
/// plays through one time. Holding something puts the tap off, and the tap starts again when the hand lets go.
namespace openblack::hand_knock
{

class Tap
{
public:
	/// A knock at this point. A knock while the tap plays does not start the tap again: it only moves the point.
	void Knock(glm::vec3 point);

	/// Sets whether the hand holds something in this frame. Holding puts the tap back to its start.
	/// Gives true when the hand plays the tap in this frame.
	bool Hold(bool holds);

	/// The time into the tap clip, in milliseconds, for this frame. Then the time increases by the frame's time, and
	/// the tap stops when that time reaches the clip's length.
	uint32_t Play(std::chrono::microseconds frame, uint32_t clipMs);

	/// Stops the tap, as when the hand has no tap clip
	void Stop() { _point.reset(); }

	/// The point where the hand knocked, while a tap waits or plays
	[[nodiscard]] const std::optional<glm::vec3>& Point() const { return _point; }
	/// The time into the tap
	[[nodiscard]] std::chrono::microseconds Time() const { return _time; }

private:
	std::optional<glm::vec3> _point;
	std::chrono::microseconds _time {0};
};

} // namespace openblack::hand_knock
