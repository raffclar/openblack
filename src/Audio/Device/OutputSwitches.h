/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <atomic>

// Two switches of the debug GUI's Audio Player window, for testing: the game's sounds (the 16 sample channels) and its music
// (the 6 music channels). Both are on by default. Off, a channel's gain is 0 at the output and everything else runs
// as before: the channels keep playing, the music keeps decoding, and turning a switch on brings back each channel's
// own gain. The fields are atomic: the music thread reads them.

namespace openblack::audio
{

struct OutputSwitches
{
	std::atomic<bool> sounds {true};
	std::atomic<bool> music {true};
};

/// The switches of the running audio (one)
OutputSwitches& GetOutputSwitches();

/// The gain a channel gets: its own, or 0 when its switch is off
[[nodiscard]] constexpr float SwitchedGain(bool on, float gain) noexcept
{
	return on ? gain : 0.0f;
}

} // namespace openblack::audio
