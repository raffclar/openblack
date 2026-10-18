/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandPointLight.h"

#include <bit>

#include "3D/MapCoords.h"

namespace openblack::land_point_light
{
namespace
{
/// The cells are 10 metres wide. The map has 512 cells on each side.
constexpr float k_CellsPerMetre = std::bit_cast<float>(0x3DCCCCCDu);
constexpr int32_t k_LastCell = 511;
constexpr float k_FractionSteps = 256.0f;
constexpr uint32_t k_Opaque = 0xFF000000u;
} // namespace

uint32_t Blend(uint32_t from, uint32_t to, int32_t fraction)
{
	const auto channel = [from, to, fraction](uint32_t shift) {
		const auto a = static_cast<int32_t>((from >> shift) & 0xFFu);
		const auto b = static_cast<int32_t>((to >> shift) & 0xFFu);
		// An arithmetic shift rounds the step down, also when it is negative. A channel out of range wraps in its byte.
		return (static_cast<uint32_t>(a + (((b - a) * fraction) >> 8)) & 0xFFu) << shift;
	};
	return k_Opaque | channel(16) | channel(8) | channel(0);
}

int32_t CellFraction(float cells, int32_t cell)
{
	return map_coords::FtoL((cells - static_cast<float>(cell)) * k_FractionSteps);
}

uint32_t LightAt(glm::vec3 point, LightTable table, const CornersAt& corners)
{
	const float cellsX = point.x * k_CellsPerMetre;
	const float cellsZ = point.z * k_CellsPerMetre;
	const auto x = map_coords::FtoL(cellsX);
	const auto z = map_coords::FtoL(cellsZ);
	const auto brightest = table[table.size() - 1];
	if (x < 0 || x > k_LastCell || z < 0 || z > k_LastCell)
	{
		return brightest;
	}
	const auto found = corners(x, z);
	if (!found.has_value())
	{
		return brightest;
	}
	const auto& luminosity = *found;
	const auto alongX = CellFraction(cellsX, x);
	const auto alongZ = CellFraction(cellsZ, z);
	const auto near = Blend(table[luminosity[0]], table[luminosity[1]], alongZ);
	const auto far = Blend(table[luminosity[2]], table[luminosity[3]], alongZ);
	return Blend(near, far, alongX);
}

float DepthOf(glm::vec3 point, const glm::mat4& worldToClip)
{
	return (((point.z * worldToClip[2][3]) + (point.y * worldToClip[1][3])) + (point.x * worldToClip[0][3])) +
	       worldToClip[3][3];
}

uint32_t Hazed(uint32_t colour, float depth, const Haze& haze)
{
	if (!haze.on || depth < haze.nearDepth)
	{
		return colour;
	}
	const float clamped = depth > haze.farDepth ? haze.farDepth : depth;
	const float range = haze.farDepth - haze.nearDepth;
	const float amount = (clamped - haze.nearDepth) / range;
	const int32_t kept = 256 - map_coords::FtoL(static_cast<float>(256 - haze.k) * amount);
	if (kept >= 256)
	{
		return colour;
	}
	const auto scaled = [colour, kept](uint32_t mask) { return (((colour & mask) * static_cast<uint32_t>(kept)) >> 8) & mask; };
	return (colour & k_Opaque) | scaled(0xFF0000u) | scaled(0xFF00u) | scaled(0xFFu);
}

} // namespace openblack::land_point_light
