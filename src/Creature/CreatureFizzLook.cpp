/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFizzLook.h"

#include <cmath>

#include <algorithm>

namespace openblack::creature_fizz_look
{

namespace
{
constexpr uint8_t k_Gone = 255;
constexpr uint8_t k_ThresholdBelowLevel = 5;
constexpr float k_ScrollAcrossPerSecond = 0.1f;
constexpr float k_ScrollDownPerSecond = 0.2f;
/// How far a creature fizzes out before its hair and its reflection are gone
constexpr float k_ExtrasGoneFizz = 0.2f;

float Slide(float at, float perSecond, float seconds)
{
	// Worked in double precision, as the game works it, before it is stored again
	return static_cast<float>(std::fmod(static_cast<double>(at) + static_cast<double>(perSecond) * seconds, 1.0));
}
} // namespace

uint8_t Level(float fizz)
{
	return static_cast<uint8_t>(static_cast<double>(std::clamp(fizz, 0.0f, 1.0f)) * 255.0);
}

bool Drawn(float fizz)
{
	return Level(fizz) != k_Gone;
}

bool Fizzing(float fizz)
{
	return fizz > 0.0f && Drawn(fizz);
}

uint8_t StaticThreshold(uint8_t level)
{
	return level > k_ThresholdBelowLevel ? static_cast<uint8_t>(level - k_ThresholdBelowLevel) : uint8_t {0};
}

float BodyAlpha(uint8_t level)
{
	return static_cast<float>(k_Gone - level) / 255.0f;
}

glm::vec2 Scroll(glm::vec2 scroll, float seconds)
{
	return {Slide(scroll.x, k_ScrollAcrossPerSecond, seconds), Slide(scroll.y, k_ScrollDownPerSecond, seconds)};
}

bool HairShown(float fizz)
{
	return fizz < k_ExtrasGoneFizz;
}

bool ReflectionShown(float fizz)
{
	return fizz < k_ExtrasGoneFizz;
}

} // namespace openblack::creature_fizz_look
