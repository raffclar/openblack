/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ZSort.h"

using namespace openblack::graphics;

namespace
{
constexpr float k_RainTileScale = -0.0125f; ///< -1 / 80
} // namespace

float zsort::Key(const glm::vec3& point, const glm::vec3& camera, SumOrder order) noexcept
{
	// each difference, each square and the two sums in float, at the original's 24-bit precision, no fused
	// multiply-add
	const float x = point.x - camera.x;
	const float y = point.y - camera.y;
	const float z = point.z - camera.z;
	const float xx = x * x;
	const float yy = y * y;
	const float zz = z * z;
	if (order == SumOrder::XZY)
	{
		const float xz = xx + zz;
		return xz + yy;
	}
	const float xy = xx + yy;
	return xy + zz;
}

uint32_t zsort::PackRainUser(float x, float z, int32_t alpha) noexcept
{
	// truncated products, then shifts and subtractions in 32-bit integers
	const auto tileZ = static_cast<int32_t>(z * k_RainTileScale);
	const auto tileX = static_cast<int32_t>(x * k_RainTileScale);
	uint32_t user = static_cast<uint32_t>(alpha);
	user = (user << 8u) - static_cast<uint32_t>(tileZ);
	user = (user << 8u) - static_cast<uint32_t>(tileX);
	return user;
}

zsort::RainUser zsort::UnpackRainUser(uint32_t user) noexcept
{
	// the low byte, the second byte and the third byte
	return {static_cast<int32_t>(user & 0xFFu), static_cast<int32_t>((user >> 8u) & 0xFFu),
	        static_cast<int32_t>((user >> 16u) & 0xFFu)};
}
