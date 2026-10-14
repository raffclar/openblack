/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightShine.h"

#include <cmath>

#include <numbers>

namespace openblack::graphics::light_shine
{

namespace
{
// The turn per millisecond, a tenth of a thousandth of pi the other way, as a single-precision number
constexpr float k_TurnPerMillisecond = -0.00031415926f;
} // namespace

double TextureTurn(std::chrono::milliseconds realTime)
{
	const double angle = static_cast<double>(realTime.count()) * static_cast<double>(k_TurnPerMillisecond);
	return std::fmod(angle, 2.0 * std::numbers::pi);
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
