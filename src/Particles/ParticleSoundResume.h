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

/// When the looping sound of a particle plays again after the audio stopped it far away (deviation 227). The audio stops
/// a placed sound when the listener is at its maximum distance or more, and does not start one that far away. The game
/// does not start that sound again while its particle lives (vanilla-bugs 122). openblack plays it again when the
/// listener comes back nearer than its maximum distance, for example when the camera comes back near a vortex, or when
/// the player leaves the temple.
namespace openblack::particles::sound_resume
{

/// What a looping particle sound does in a turn, and whether the listener was too far away for it
struct Step
{
	/// The listener is at the sound's maximum distance or more, or was when the sound stopped
	bool far {false};
	/// The sound plays again from its start in this turn
	bool replay {false};
};

/// The step of a particle sound in a turn. `wasFar` is the far value of the last turn, `playing` tells if the audio
/// plays the sound now, and `distance` is how far the listener is from the sound (none without a listener).
/// A sound that is not looping, or that has no maximum distance, does not play again. A sound that stopped while the
/// listener was near (for example, a sound with a higher priority took its voice) does not play again either.
[[nodiscard]] constexpr Step Next(bool wasFar, bool looping, bool playing, std::optional<float> distance, float maxDistance)
{
	if (!looping || maxDistance <= 0.0f || !distance.has_value())
	{
		return {.far = wasFar, .replay = false};
	}
	const bool far = *distance >= maxDistance;
	if (playing || far)
	{
		return {.far = far, .replay = false};
	}
	return {.far = false, .replay = wasFar};
}

} // namespace openblack::particles::sound_resume
