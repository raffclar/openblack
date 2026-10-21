/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleExitGlow.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <glm/vec2.hpp>

#include "Common/X87.h"

namespace openblack::graphics::temple_exit_glow
{

namespace
{
/// The glow grows at this rate after its start, and at the same rate again past a strength of 2, up to 4
constexpr double k_GrowthRate = 1.5;
constexpr float k_SecondGrowthFrom = 2.0f;
constexpr float k_SecondGrowthRate = 1.5f;
constexpr float k_Strongest = 4.0f;
/// The grey and the alpha of the near end, for each unit of strength, of 255
constexpr float k_GreyPerStrength = 127.0f;
constexpr float k_AlphaPerStrength = 75.0f;
/// The near end widens from a strength of 2, this much for each unit of strength more
constexpr float k_NearWidthFrom = 2.0f;
constexpr float k_NearWidthPerStrength = 7.0f;
/// The far end is this much wider and higher for each unit of strength; its height starts at the near end's
constexpr float k_FarWidthPerStrength = 15.0f;
constexpr float k_FarHeightPerStrength = 10.0f;
constexpr float k_Height = 20.0f;
/// The two ends stand on this circle each, this high over the floor, in line with the door
constexpr float k_NearRadius = 80.0f;
constexpr float k_FarRadius = 60.0f;
constexpr float k_FloorHeight = 5.0f;
/// A door's angle round the room is its number of eighths of a turn
constexpr float k_EighthTurn = 0.785398185f;

/// The sheet has eight strips across it, from one edge to most of the way to the other
constexpr int k_Strips = 8;
constexpr float k_StripCentre = 4.0f;
constexpr float k_StripStep = 0.25f;

/// Each corner's place on the atmosphere's texture wanders round its own point
constexpr float k_Wander = 0.1f;
constexpr float k_CentreU = 0.625f;
constexpr float k_CentreV = 0.375f;
constexpr float k_SecondStart = 52.0f;
constexpr float k_SecondRate = 1.01f;
constexpr float k_ThirdStart = 23.0f;
constexpr float k_FourthRate = 0.99f;
constexpr float k_FourthStart = 63.0f;

uint8_t ToByte(float value)
{
	// The game takes the whole part, and at most 255
	const auto whole = static_cast<int32_t>(value);
	return static_cast<uint8_t>(std::min(whole, 255));
}

float Wander(float angle)
{
	return x87::Mul(x87::Sin(angle), k_Wander);
}
} // namespace

std::optional<Sheet> At(float stateSeconds)
{
	float strength = x87::Mul(x87::Sub(stateSeconds, k_StartSeconds), k_GrowthRate);
	if (strength < 0.0f)
	{
		return std::nullopt;
	}
	if (strength > k_SecondGrowthFrom)
	{
		strength = ((strength - k_SecondGrowthFrom) * k_SecondGrowthRate) + k_SecondGrowthFrom;
	}
	if (strength > k_Strongest)
	{
		strength = k_Strongest;
	}
	else if (strength <= 0.0f)
	{
		return std::nullopt;
	}
	const auto grey = ToByte(strength * k_GreyPerStrength);
	const auto alpha = ToByte(strength * k_AlphaPerStrength);
	const float nearWidth = std::max((strength - k_NearWidthFrom) * k_NearWidthPerStrength, 0.0f);

	const float angle = static_cast<float>(k_ExitDoor) * k_EighthTurn;
	const double cosine = x87::Cos(angle);
	const double sine = x87::Sin(angle);
	return Sheet {
	    .near = {x87::Mul(cosine, k_NearRadius), k_FloorHeight, x87::Mul(sine, k_NearRadius)},
	    .far = {x87::Mul(cosine, k_FarRadius), k_FloorHeight, x87::Mul(sine, k_FarRadius)},
	    .nearHeight = k_Height,
	    .farHeight = (strength * k_FarHeightPerStrength) + k_Height,
	    .nearWidth = nearWidth,
	    .farWidth = strength * k_FarWidthPerStrength,
	    .colour = {grey, grey, grey, alpha},
	};
}

void Append(const Sheet& sheet, float phase, BeamMesh& mesh)
{
	// The sheet spreads across the line between its ends, on the floor
	const float alongY = sheet.far.y - sheet.near.y;
	const float alongZ = sheet.far.z - sheet.near.z;
	float acrossX = alongZ;
	float acrossY = alongY;
	float acrossZ = -(sheet.far.x - sheet.near.x);
	if (acrossX != 0.0f || acrossY != 0.0f || acrossZ != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(((acrossY * acrossY) + (acrossZ * acrossZ)) + (acrossX * acrossX));
		acrossX = acrossX * inverse;
		acrossY = acrossY * inverse;
		acrossZ = acrossZ * inverse;
	}
	const glm::vec3 nearAcross {acrossX * sheet.nearWidth, acrossY * sheet.nearWidth, acrossZ * sheet.nearWidth};
	const glm::vec3 farAcross {acrossX * sheet.farWidth, acrossY * sheet.farWidth, acrossZ * sheet.farWidth};
	// The far end sinks by half of how much higher it rises than the near end
	const float drop = (sheet.farHeight - sheet.nearHeight) * 0.5f;

	const auto first = static_cast<uint16_t>(mesh.vertices.size());
	const auto clear = glm::u8vec4(0);
	float wander = phase;
	const auto corners = [&wander]() {
		const glm::vec2 a {Wander(wander) + k_CentreU, Wander(k_SecondStart - (wander * k_SecondRate)) + k_CentreV};
		const glm::vec2 b {Wander(k_ThirdStart - wander) + k_CentreU,
		                   Wander((wander * k_FourthRate) + k_FourthStart) + k_CentreV};
		wander = wander + k_EighthTurn;
		return std::array {a, b};
	};
	for (int strip = 0; strip < k_Strips; ++strip)
	{
		const float share = (static_cast<float>(strip) - k_StripCentre) * k_StripStep;
		const glm::vec3 nearBase {(nearAcross.x * share) + sheet.near.x, (nearAcross.y * share) + sheet.near.y,
		                          (nearAcross.z * share) + sheet.near.z};
		const glm::vec3 nearTop {nearBase.x, sheet.nearHeight + nearBase.y, nearBase.z};
		const glm::vec3 farBase {(farAcross.x * share) + sheet.far.x, ((farAcross.y * share) + sheet.far.y) - drop,
		                         (farAcross.z * share) + sheet.far.z};
		const glm::vec3 farTop {farBase.x, sheet.farHeight + farBase.y, farBase.z};
		const auto nearUv = corners();
		const auto farUv = corners();
		mesh.vertices.push_back({.position = nearBase, .colour = sheet.colour, .uv = nearUv[0]});
		mesh.vertices.push_back({.position = nearTop, .colour = sheet.colour, .uv = nearUv[1]});
		mesh.vertices.push_back({.position = farBase, .colour = clear, .uv = farUv[0]});
		mesh.vertices.push_back({.position = farTop, .colour = clear, .uv = farUv[1]});
	}
	const auto at = [first](int index) { return static_cast<uint16_t>(first + index); };
	// Each strip from end to end
	for (int strip = 0; strip < k_Strips; ++strip)
	{
		const int k = strip * 4;
		mesh.indices.insert(mesh.indices.end(), {at(k), at(k + 1), at(k + 3), at(k + 3), at(k + 2), at(k)});
	}
	// Then each of the first three strips joined to its mirror on the far side: their bottoms, their tops and their near
	// ends
	for (int pair = 0; pair < 3; ++pair)
	{
		const int c = 1 + (pair * 4);
		const int e = 31 - (pair * 4);
		mesh.indices.insert(mesh.indices.end(), {at(c - 1), at(e - 3), at(e - 1), at(e - 1), at(c + 1), at(c - 1)});
		mesh.indices.insert(mesh.indices.end(), {at(c), at(e - 2), at(e), at(e), at(c + 2), at(c)});
		mesh.indices.insert(mesh.indices.end(), {at(c - 1), at(c), at(e - 2), at(e - 2), at(e - 3), at(c - 1)});
	}
}

} // namespace openblack::graphics::temple_exit_glow
