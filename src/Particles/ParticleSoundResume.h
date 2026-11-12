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

/// When the looping sound of a particle plays again after the audio stopped it because the listener left it (deviation
/// 235). The audio stops a placed sound when the listener is at its maximum distance or more, and does not start one
/// that far away. The temple stops every sound effect when the player goes in. The game does not start that sound again
/// while its particle lives (vanilla-bugs 122). openblack plays it again when the listener comes back nearer than its
/// maximum distance, for example when the camera comes back near a vortex, or when the player leaves the temple.
namespace openblack::particles::sound_resume
{

/// What the rule knows about a particle sound in a turn
struct Turn
{
	/// The sound loops until its particle lets go of it
	bool looping {false};
	/// The audio plays the sound now
	bool playing {false};
	/// The audio stopped the sound since the last turn because the listener left it: the listener went to the maximum
	/// distance of the sound or more, or into the temple
	bool listenerLeft {false};
	/// How far the listener is from the sound, none without a listener
	std::optional<float> distance;
	/// The maximum distance of the sample, 0 when no sample is loaded
	float maxDistance {0.0f};
};

/// The result of the rule in a turn
struct Step
{
	/// The listener left the sound, or was too far for the sound to start, and the sound did not play again yet
	bool far {false};
	/// The sound plays again from its start in this turn
	bool replay {false};
};

/// The step of a particle sound in a turn. `wasFar` is the far value of the last turn. A far sound plays again in the
/// first turn that the listener is nearer than its maximum distance. These sounds do not play again:
/// - a sound that does not loop, or that has no maximum distance;
/// - a sound that stopped for a different reason, for example when a sound with a higher priority took its voice.
[[nodiscard]] constexpr Step Next(bool wasFar, const Turn& turn)
{
	const bool far = wasFar || turn.listenerLeft;
	if (!turn.looping || turn.maxDistance <= 0.0f || !turn.distance.has_value())
	{
		return {.far = far, .replay = false};
	}
	if (turn.playing || !far)
	{
		return {.far = false, .replay = false};
	}
	if (*turn.distance >= turn.maxDistance)
	{
		return {.far = true, .replay = false};
	}
	return {.far = false, .replay = true};
}

} // namespace openblack::particles::sound_resume
