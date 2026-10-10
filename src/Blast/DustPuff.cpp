/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DustPuff.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::dust_puff;

namespace
{
/// Each sprite of dust flies off at this many times the puff's size a second; each of smoke at a random share of it
/// between these
constexpr float k_SpeedScale = 1.5f;
constexpr float k_SmokeSlowest = 0.3f;
constexpr float k_SmokeFastest = 1.0f;
/// Dust is wholly opaque until its life falls below this share, then fades
constexpr float k_OpaqueLife = 0.7f;
/// Smoke is never more than this faint: its alpha is its life times this, in whole steps
constexpr float k_SmokeAlpha = 100.0f;
/// The sprites' starting roll, drawn though each frame sets it afresh
constexpr float k_FullTurn = 6.2831855f;
/// Its spin by its life
constexpr float k_Spin = 5.0f;
/// Its cell runs back through so many of the sheet as its life runs out
constexpr float k_Cells = 15.0f;
constexpr float k_SmallestHalfWidth = 0.0001f;
} // namespace

Puff dust_puff::Make(const glm::vec3& centre, float size, const std::function<float(float, float)>& random, Kind kind,
                     std::optional<uint32_t> colour)
{
	Puff puff;
	puff.kind = kind;
	puff.size = size;
	puff.colour = colour.value_or(kind == Kind::Smoke ? k_SmokeColour : k_Colour);
	for (size_t i = 0; i < k_Sprites; ++i)
	{
		const float z = random(-size, size);
		const float y = random(-size, size);
		const float x = random(-size, size);
		puff.positions.at(i) = centre + glm::vec3(x, y, z);
		[[maybe_unused]] const float roll = random(0.0f, k_FullTurn);
		// Up and out, smoke at a speed of its own
		const float outZ = random(-size, size);
		const float outX = random(-size, size);
		const glm::vec3 velocity(outX, size, outZ);
		const float speed = kind == Kind::Smoke ? random(k_SmokeSlowest, k_SmokeFastest) * size : size * k_SpeedScale;
		const float length = glm::length(velocity);
		puff.velocities.at(i) = length > 0.0f ? velocity * (speed / length) : glm::vec3(0.0f);
	}
	return puff;
}

bool dust_puff::Advance(Puff& puff, float seconds)
{
	puff.life -= seconds * (puff.kind == Kind::Smoke ? k_SmokeLifeRate : k_LifeRate);
	if (puff.life <= 0.0f)
	{
		return false;
	}
	for (size_t i = 0; i < k_Sprites; ++i)
	{
		puff.positions.at(i) += puff.velocities.at(i) * seconds;
	}
	return true;
}

std::array<SpriteLook, k_Sprites> dust_puff::Look(const Puff& puff)
{
	std::array<SpriteLook, k_Sprites> looks {};
	float alpha = puff.life < k_OpaqueLife ? puff.life * (1.0f / k_OpaqueLife) * 255.0f : 255.0f;
	if (puff.kind == Kind::Smoke)
	{
		alpha = std::trunc(puff.life * k_SmokeAlpha);
	}
	const float halfWidth = std::max(((1.0f - puff.life) * 2.0f + 1.0f) * puff.size * 0.5f, k_SmallestHalfWidth);
	for (size_t i = 0; i < k_Sprites; ++i)
	{
		const auto& velocity = puff.velocities.at(i);
		const float turn = velocity.x <= velocity.z ? 1.0f : -1.0f;
		looks.at(i) = {
		    .position = puff.positions.at(i),
		    .halfWidth = halfWidth,
		    .angle = turn * puff.life * k_Spin + velocity.x,
		    .alpha = alpha,
		    .cell = static_cast<int>(puff.life * k_Cells) & 0x3F,
		};
	}
	return looks;
}
