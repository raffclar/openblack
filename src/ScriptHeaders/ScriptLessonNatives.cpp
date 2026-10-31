/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptLessonNatives.h"

#include <bit>

#include "Common/X87.h"
#include "Creature/CreaturePlayerAttention.h"

namespace openblack::script::lesson_natives
{

namespace
{
/// Half of the angle, and degrees to radians (0x3C8EFA35), as the game keeps them
constexpr float k_Half = 0.5f;
constexpr float k_RadiansPerDegree = std::bit_cast<float>(0x3C8EFA35u);
/// A full turn in degrees, which the game divides by the angle of a creature's look
constexpr float k_FullTurnDegrees = std::bit_cast<float>(0x43B40000u);

/// The side of the point from an edge of the cone at an angle: more than 0 on the side of the cone's inside for the
/// first edge, and on the side of the outside for the second edge. The game rounds the sine to a float and keeps the
/// cosine wide.
float Side(float angle, float dx, float dz)
{
	const double cosine = x87::Cos(angle);
	const float sine = x87::RoundToFloat(x87::Sin(angle));
	return x87::Sub(x87::Mul(dz, cosine), x87::Mul(dx, sine));
}
} // namespace

float HalfViewAngle(float degrees)
{
	return (degrees * k_Half) * k_RadiansPerDegree;
}

bool InViewCone(glm::vec3 position, float yAngle, float halfAngle, glm::vec3 point)
{
	const float dx = point.x - position.x;
	const float dz = point.z - position.z;
	// Outside the first edge, or not a number: out
	if (!(Side(yAngle - halfAngle, dx, dz) >= 0.0f))
	{
		return false;
	}
	// Inside the second edge or on it, or not a number: in
	const float second = Side(yAngle + halfAngle, dx, dz);
	return !(second > 0.0f);
}

int32_t CreatureLookParts(float degrees)
{
	return map_coords::FtoL(x87::Div(k_FullTurnDegrees, degrees));
}

bool CreatureSeesCamera(const map_coords::MapCoords& creature, const map_coords::MapCoords& camera, uint16_t lookAngle,
                        float degrees)
{
	// The game divides the half turn by the parts as numbers with no sign
	const auto parts = static_cast<uint32_t>(CreatureLookParts(degrees));
	if (parts == 0)
	{
		return false;
	}
	return creature_player_attention::IsDirectionClose(creature, camera, lookAngle, parts);
}

} // namespace openblack::script::lesson_natives
