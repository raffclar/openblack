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

#include <functional>

#include <glm/vec3.hpp>

#include "Sound/AnimEffectKeys.h"

/// The rules for the dust that a creature's footsteps throw up. Each footstep of the creature's animation makes a number
/// of brown puffs at the creature's place on the ground. The puffs move out in random directions and fade in one
/// second. On the land and in shallow water, the puffs are the same. Where snow lies, the puffs show the colour of the
/// land's light.
namespace openblack::creature_footstep_puffs
{
/// The colour of each puff before the snow changes it (0xAARRGGBB)
constexpr uint32_t k_Colour = 0x40806040u;
/// The size of each puff for each unit of the shown size
constexpr float k_SizePerSize = 4.0f;
/// The speed on each axis is (r - 100) times this for each unit of the shown size, r a random number from 0 to 200
constexpr float k_SpeedPerSize = 0.05f;
/// The random number for each axis of the speed is less than this
constexpr int32_t k_SpeedRandomRange = 201;
constexpr float k_SpeedRandomMiddle = 100.0f;

/// Gives a random whole number from 0 to less than a limit, from the shared random numbers of the game
using LocalRandom = std::function<uint32_t(int32_t)>;

/// The number of puffs for an animation sound: 3 for a light footstep, 6 for a normal footstep, 12 for a stamp and 0 for
/// all other sounds
[[nodiscard]] constexpr int32_t CountOf(audio::SoundAction action)
{
	switch (action)
	{
	case audio::SoundAction::FootstepLight:
		return 3;
	case audio::SoundAction::FootstepNormal:
		return 6;
	case audio::SoundAction::FootStamp:
		return 12;
	default:
		return 0;
	}
}
/// The size of each puff, from the shown size of the creature
[[nodiscard]] constexpr float SizeOf(float shownSize)
{
	return shownSize * k_SizePerSize;
}
/// Gives the velocity of one puff. The random numbers come first on z, then on y, then on x
[[nodiscard]] glm::vec3 VelocityOf(float shownSize, const LocalRandom& random);

} // namespace openblack::creature_footstep_puffs
