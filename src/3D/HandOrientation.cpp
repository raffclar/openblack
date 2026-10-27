/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandOrientation.h"

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Common/PortableRotations.h"
#include "Common/X87.h"

namespace openblack::hand_orientation
{

namespace
{
constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
/// A lean across the land smaller than this, squared, gives no heading
constexpr float k_MinHeadingLeanSquared = 1.0e-6f;
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr double k_HalfPiWide = static_cast<double>(std::numbers::pi_v<float> * 0.5f);
constexpr double k_PiWide = static_cast<double>(std::numbers::pi_v<float>);

/// The angle of the point (along, across) from the along axis, worked by eighths of a turn. The ratio rounds to a float
/// and the arc tangent keeps its full precision until the sum, which rounds to a float.
float AngleByEighths(float along, float across)
{
	if (along >= across && !(-across > along))
	{
		return x87::RoundToFloat(x87::Atan(across / along));
	}
	if (across >= along && !(-along > across))
	{
		return x87::Sub(k_HalfPiWide, x87::Atan(along / across));
	}
	if (-across >= along && !(along >= across))
	{
		const double arc = x87::Atan(across / along);
		return across >= 0.0f ? x87::Add(arc, k_PiWide) : x87::Sub(arc, k_PiWide);
	}
	return x87::Sub(-k_HalfPiWide, x87::Atan(along / across));
}

/// A frame of a front and an up square to it, with the side across them
glm::mat3 Frame(glm::vec3 front, glm::vec3 up)
{
	return {glm::normalize(glm::cross(front, up)), up, front};
}
} // namespace

bool LeansAcrossLand(glm::vec3 rayDirection)
{
	return !(std::abs(rayDirection.x) <= k_MinHeadingLean && std::abs(rayDirection.z) <= k_MinHeadingLean);
}

glm::vec3 HeadingAlongRay(glm::vec3 rayDirection, glm::vec3 previousHeading)
{
	if (!LeansAcrossLand(rayDirection))
	{
		return previousHeading;
	}
	return glm::normalize(glm::vec3(rayDirection.x, 0.0f, rayDirection.z));
}

float HeadingAngle(glm::vec3 rayDirection)
{
	const float lean = rayDirection.x * rayDirection.x + rayDirection.z * rayDirection.z;
	if (!(lean > k_MinHeadingLeanSquared))
	{
		return 0.0f;
	}
	return AngleByEighths(-rayDirection.z, rayDirection.x);
}

std::array<glm::vec3, 3> TippedRows(float headingAngle, float tilt)
{
	// The sines and cosines keep their full precision until they are stored. The sine of the turn is also used at full
	// precision in two products, which round to a float.
	const float tip = -tilt;
	const float tipCos = x87::RoundToFloat(x87::Cos(tip));
	const float tipSin = x87::RoundToFloat(x87::Sin(tip));
	const float turn = headingAngle + k_Pi;
	const float turnCos = x87::RoundToFloat(x87::Cos(turn));
	const double turnSinWide = x87::Sin(turn);
	const float turnSin = x87::RoundToFloat(turnSinWide);
	const float negativeTipSin = -tipSin;
	constexpr float k_Zero = 0.0f;
	return {{
	    {turnCos - k_Zero, k_Zero, k_Zero + turnSin},
	    {k_Zero - x87::Mul(negativeTipSin, turnSinWide), tipCos, negativeTipSin * turnCos + k_Zero},
	    {k_Zero - x87::Mul(tipCos, turnSinWide), tipSin, tipCos * turnCos + k_Zero},
	}};
}

glm::mat3 TippedHand(const std::array<glm::vec3, 3>& rows)
{
	return {rows[0], rows[2], -rows[1]};
}

glm::mat3 TurnToHeading(const glm::mat3& facingCamera, glm::vec3 cameraHeading, glm::vec3 heading)
{
	const auto angle = portable_math::Atan2(glm::cross(cameraHeading, heading).y, glm::dot(cameraHeading, heading));
	return glm::mat3(portable_math::Rotate(glm::mat4(1.0f), angle, k_Up)) * facingCamera;
}

glm::mat3 StandOnSlope(const glm::mat3& onLevelLand, glm::vec3 heading, glm::vec3 up)
{
	const auto upright = glm::normalize(up);
	auto front = heading - glm::dot(heading, upright) * upright;
	if (glm::length(front) <= 0.0f)
	{
		// The slope faces along the heading itself: there is no front to lay along it
		return onLevelLand;
	}
	front = glm::normalize(front);
	return Frame(front, upright) * glm::transpose(Frame(heading, k_Up)) * onLevelLand;
}

glm::mat3 TipForwards(const glm::mat3& onLevelLand, glm::vec3 heading, float angle)
{
	const auto across = glm::cross(k_Up, heading);
	if (glm::length(across) <= 0.0f)
	{
		return onLevelLand;
	}
	return glm::mat3(portable_math::Rotate(glm::mat4(1.0f), angle, glm::normalize(across))) * onLevelLand;
}

} // namespace openblack::hand_orientation
