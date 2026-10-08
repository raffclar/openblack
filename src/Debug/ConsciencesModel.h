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

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Help/HelpSystem.h"
#include "Help/Spirits.h"

// What the Consciences debug window works out before it draws or acts: the names of the spirits' states, the message
// sets that match a search, the clips each spirit has, where a clip is played on the screen, and the speaker
// override's names. Pure functions and small value types, tested with hand-made data.

namespace openblack::debug::consciences
{

/// How the controller's state of a spirit reads in the window
[[nodiscard]] std::string_view ControlStateName(help::spirits::ControlState state);

/// How a spirit's own state reads in the window
[[nodiscard]] std::string DudeStateName(uint32_t state);

/// How a message set's mode reads in the window
[[nodiscard]] std::string_view SetModeName(uint32_t mode);

/// One message set as the window lists it
struct SetRow
{
	uint32_t set {0};
	uint32_t first {0};
	uint32_t last {0};
	uint32_t mode {0};
	uint32_t category {0};
	std::string_view script;
	/// How often it was sent since the last reset
	uint32_t sent {0};
};

/// Whether a set matches the window's search: every set for an empty search, the set of that number for a number,
/// else the sets whose script name holds the search, ignoring case
[[nodiscard]] bool MatchesSearch(const SetRow& row, std::string_view search);

/// One clip of a spirit: its slot, which a script plays it by, and its name
struct Clip
{
	uint32_t slot {0};
	std::string_view name;
};

/// Playing slot 0 stops the clip a spirit plays instead, as for a script, so it is not one of the clips to play
inline constexpr uint32_t k_StopClipSlot = 0;

/// The clips a spirit's file names, in slot order, from slot 1; an empty name has no clip
[[nodiscard]] std::vector<Clip> ClipsOf(std::span<const std::string> animNames);

/// Where the window plays a spirit's clip, in pixels: the good spirit at a quarter of the screen's width, the evil one
/// at three quarters, both half way down, as they appear
[[nodiscard]] glm::ivec2 ClipSpot(int dude, glm::ivec2 screen);

/// How a speaker override reads in the window
[[nodiscard]] std::string_view OverrideName(help::SpeakerOverride override);

} // namespace openblack::debug::consciences
