/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightShine.h"

#include <cstdint>

#include "Common/X87.h"

namespace openblack::graphics::light_shine
{

namespace
{
// The turn per millisecond, a tenth of a thousandth of pi the other way, as a single-precision number
constexpr float k_TurnPerMillisecond = -0.00031415926f;
} // namespace

double TextureTurn(std::chrono::milliseconds realTime)
{
	// The game keeps the time as a 32-bit count. The angle rounds to a float, and the game takes no remainder of a turn
	const auto milliseconds = static_cast<int32_t>(realTime.count());
	return static_cast<double>(x87::Mul(static_cast<double>(milliseconds), k_TurnPerMillisecond));
}

glm::vec2 TurnedUv(glm::vec2 uv, float cosTurn, float sinTurn)
{
	const float across = uv.x - 0.5f;
	const float down = uv.y - 0.5f;
	return {across * cosTurn + down * sinTurn + 0.5f, across * sinTurn - down * cosTurn + 0.5f};
}

float VertexAlpha(int vertexBone, int solidBone)
{
	return vertexBone == solidBone ? 1.0f : 0.0f;
}

} // namespace openblack::graphics::light_shine
