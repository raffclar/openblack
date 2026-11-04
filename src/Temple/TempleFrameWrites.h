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
#include <bitset>
#include <string_view>

namespace openblack::temple
{

/// The writes of the synced world that the temple of a game alone does in a frame step, not in a turn. The original
/// game does these writes from the frame, in the same order.
enum class FrameWrite : uint8_t
{
	/// The keys or the mouse take the player into the temple: the help stops, the advisors go home, the temple keeps the
	/// dialogue
	Entry,
	/// The player leaves the temple: the dialogue, the scripts and the bars come back
	Exit,
	/// The multiplayer front end opens the temple at its multiplayer room
	MultiplayerFrontEnd,
	/// A key or a press takes the camera away from a scroll, for example in the creature cave, and the help of the
	/// scroll stops
	ScrollKey,
};

inline constexpr size_t k_FrameWriteCount = 4;

/// The name of each frame write, for the logs of the tools
inline constexpr std::array<std::string_view, k_FrameWriteCount> k_FrameWriteNames = {
    "temple entry by keys or mouse",
    "temple exit",
    "multiplayer front end",
    "scroll key",
};

[[nodiscard]] constexpr std::string_view Name(FrameWrite write)
{
	return k_FrameWriteNames.at(static_cast<size_t>(write));
}

/// The frame writes that the temple did since the last time that a tool took them
class FrameWrites
{
public:
	void Note(FrameWrite write) { _writes.set(static_cast<size_t>(write)); }
	[[nodiscard]] bool Contains(FrameWrite write) const { return _writes.test(static_cast<size_t>(write)); }
	[[nodiscard]] bool Empty() const { return _writes.none(); }

private:
	std::bitset<k_FrameWriteCount> _writes;
};

} // namespace openblack::temple
