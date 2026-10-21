/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

#include <algorithm>
#include <optional>

/// The words of the future room, "The future is still uncertain...", which fade in once the camera has come in
namespace openblack::temple::future_room
{

/// The highest alpha of the words, out of 255
constexpr float k_MostAlpha = 128.0f;
/// How much the alpha goes up each second
constexpr float k_AlphaPerSecond = 64.0f;
/// The seconds in a millisecond
constexpr float k_SecondsPerMillisecond = 0.001f;

/// The words of one visit to the room. Make a new one each time the player goes into the room.
struct Words
{
	/// The words show from the first frame the camera has come in with no dialog up, until the player leaves the
	/// room. A dialog that opens after that does not hide them.
	bool shown {false};
	/// How long the words have faded in, which starts again whenever the camera is not in the room's control
	float seconds {0.0f};
};

/// Steps the words by one frame and gives their alpha out of 255, or nothing while they do not show
/// @param arrived The camera has come into the room and the player has it
/// @param dialogOpen A dialog is up over the temple
/// @param frameMilliseconds How long the frame took
[[nodiscard]] inline std::optional<float> Step(Words& words, bool arrived, bool dialogOpen, float frameMilliseconds)
{
	if (!arrived)
	{
		words.seconds = 0.0f;
	}
	else if (!dialogOpen)
	{
		words.shown = true;
	}
	if (!words.shown)
	{
		return std::nullopt;
	}
	words.seconds += frameMilliseconds * k_SecondsPerMillisecond;
	if (!arrived)
	{
		return std::nullopt;
	}
	// The alpha goes up in whole steps
	return std::min(std::trunc(words.seconds * k_AlphaPerSecond), k_MostAlpha);
}

} // namespace openblack::temple::future_room
