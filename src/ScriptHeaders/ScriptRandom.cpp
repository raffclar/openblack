/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptRandom.h"

#include "Common/GameRandom.h"

namespace openblack::script::random
{

uint32_t WholeNumberBetween(uint32_t low, uint32_t high, GameRandomInterface& random)
{
	const uint32_t width = high - low + 1u;
	return random.GameRand(width) + low;
}

float NumberFrom(float low, float high, GameRandomInterface& random)
{
	const float span = high - low;
	const float width = span + 1.0f;
	const float drawn = random.GameFloatRand(width) + low;
	// Cut towards zero, and kept to the low 32 bits
	return static_cast<float>(static_cast<int32_t>(static_cast<int64_t>(drawn)));
}

} // namespace openblack::script::random
