/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

/// The scripts' switches that change as a script takes or gives back the camera or the dialogue
namespace openblack::script::switches
{

/// The switches a change of control sets; a switch left empty is left as it is
struct SwitchChanges
{
	/// The leashes are drawn
	std::optional<bool> leashesDrawn;
	/// The challenge scrolls are drawn (the signs always are)
	std::optional<bool> highlightsDrawn;
	/// The creatures of the other players are heard in their own voices
	std::optional<bool> otherCreatureVoices;

	bool operator==(const SwitchChanges&) const = default;
};

/// A script takes the camera: out in the world its shots show neither the leashes nor the scrolls; inside the temple
/// nothing changes
[[nodiscard]] constexpr SwitchChanges CameraTaken(bool insideTemple)
{
	if (insideTemple)
	{
		return {};
	}
	return {.leashesDrawn = false, .highlightsDrawn = false};
}

/// The script gives the camera back, or stops while it has it: the leashes and scrolls are drawn again, and the other
/// players' creatures are heard again whatever the script asked for
[[nodiscard]] constexpr SwitchChanges CameraReleased()
{
	return {.leashesDrawn = true, .highlightsDrawn = true, .otherCreatureVoices = true};
}

/// The script with the dialogue gives it back at its end: the other players' creatures are heard again. A script that
/// stops while it has the dialogue leaves the switch as it is.
[[nodiscard]] constexpr SwitchChanges DialogueEnded()
{
	return {.otherCreatureVoices = true};
}

} // namespace openblack::script::switches
