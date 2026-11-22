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

/// What the game does with the sound of a particle in each turn. Pure rules, so they are tested without audio.
///
/// - When the particle is gone, the game lets go of the sound.
/// - A looping sound starts again in each turn when the camera is nearer than k_ReplayReach. The play type of its
///   sample decides what happens to a voice that plays already: a sample that plays once keeps its voice.
/// - A sound that travels at the speed of sound (thunder) waits. It starts in the turn that its wait goes below zero. A
///   wait of zero or less at the start never ends, so the sound never plays.
/// - Thunder that waits does not go with its particle. It keeps its wait and starts when the wait ends. The game lets
///   go of it after that. This is a deviation, which the user asked for: in the game, thunder whose particle goes first
///   is never heard.
namespace openblack::particles::sound_turn
{

/// A sound of a particle starts in a turn only when the camera is nearer than this, in metres
inline constexpr float k_ReplayReach = 1200.0f;

/// What a particle sound does in a turn
enum class Step : uint8_t
{
	/// Nothing
	Nothing,
	/// The particle is gone: the game lets go of the sound, or fades it if it did that before
	LetGo,
	/// The game asks the audio to start the sound, if the camera is near enough
	Start,
};

/// Whether a point at this squared distance from the camera is near enough for a particle sound to start
[[nodiscard]] constexpr bool WithinReplayReach(float distanceSquared)
{
	return distanceSquared < k_ReplayReach * k_ReplayReach;
}

/// The step of a particle sound in a turn. `wait` is the time in seconds before a sound that travels at the speed of
/// sound is heard; the rule counts it down by `turnSeconds`. `travels` is true for a sound that travels at the speed of
/// sound.
[[nodiscard]] constexpr Step Next(bool particleLives, bool looping, bool travels, float& wait, float turnSeconds)
{
	const bool waits = wait > 0.0f;
	if (!particleLives && !(travels && waits))
	{
		return Step::LetGo;
	}
	if (particleLives && looping)
	{
		return Step::Start;
	}
	if (!waits)
	{
		return Step::Nothing;
	}
	wait -= turnSeconds;
	return wait < 0.0f ? Step::Start : Step::Nothing;
}

} // namespace openblack::particles::sound_turn
