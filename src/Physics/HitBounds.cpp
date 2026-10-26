/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HitBounds.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

using namespace openblack::physics::hit_bounds;

namespace
{

constexpr float k_Infinity = std::numeric_limits<float>::infinity();
constexpr float k_Sqrt3 = 1.7320509f;

// The bounds below are true for each operation of float precision without overflow and underflow. Each constant is a
// bound with a margin of two to four times. The limits keep the values away from overflow and underflow.

/// Four times the largest error of the edge test of the search, for unit sizes of the edge, the offset and the normal
constexpr float k_EdgeError = 0x1p-18f;
/// More than the error of a part of a point in the world, for each unit of the sum of 19 times the largest part of the
/// point in the body's space and two times the shift
constexpr float k_CornerError = 0x1p-23f;
/// More than the error of a part of a normal in the world, for each unit of three times its largest part
constexpr float k_NormalError = 0x1p-21f;
/// More than the relative error of a length, and of the dot product of a normal and an edge
constexpr float k_DotError = 0x1p-20f;
/// More than the largest distance between a hit and the plane of its face, for a unit size of the parts of the
/// corners, the point and the direction
constexpr float k_PlaneError = 0x1p-19f;
/// More than the largest error of a hit and of the far end of the line, for a unit size of the point and the direction
constexpr float k_LineError = 0x1p-17f;
/// More than the relative error of the margin from the float operations that make it
constexpr float k_MarginError = 0x1p-10f;
/// A small distance that each margin gets in addition
constexpr float k_SmallestMargin = 1e-20f;
/// The limits of the size of a part of a point of a body, of an edge of a face and of the length of a normal
constexpr float k_LargestLocalPart = 0x1p20f;
constexpr float k_SmallestEdge = 0x1p-16f;
constexpr float k_SmallestNormal = 0.5f;
constexpr float k_LargestNormal = 2.0f;
/// The largest size of a part of a point or of a direction that a line box accepts
constexpr float k_LargestLinePart = 0x1p40f;

[[nodiscard]] float LargestPart(glm::vec3 v)
{
	return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

[[nodiscard]] bool IsFinite(glm::vec3 v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

float openblack::physics::hit_bounds::FaceMargin(const LocalFace& face, float largestPart, float shift)
{
	const auto& n = face.normal;
	// A zero normal stays zero in the world, so the face never faces a line
	if (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f)
	{
		return 0.0f;
	}
	if (!IsFinite(n) || !IsFinite(face.a) || !IsFinite(face.b) || !IsFinite(face.c) || !(largestPart <= k_LargestLocalPart) ||
	    !(shift <= k_LargestShift))
	{
		return k_Infinity;
	}
	const glm::vec3 ab = face.b - face.a;
	const glm::vec3 ac = face.c - face.a;
	const float edge = std::max({glm::length(ab), glm::length(ac), glm::length(face.c - face.b)}) * (1.0f + k_DotError);
	const float normalLength = glm::length(n);
	if (!(edge >= k_SmallestEdge) || !(normalLength >= k_SmallestNormal) || !(normalLength <= k_LargestNormal))
	{
		return k_Infinity;
	}

	// The largest turn of the axes, and the largest errors of a corner and of a normal in the world
	constexpr float k_Stretch = 1.0f + 2.0f * k_LargestAxisError;
	constexpr float k_Shrink = 1.0f - 2.0f * k_LargestAxisError;
	constexpr float k_SmallestDeterminant = 1.0f - 5.0f * k_LargestAxisError;
	const float cornerError = k_CornerError * (19.0f * largestPart + 2.0f * shift);
	const float normalError = k_NormalError * 3.0f * LargestPart(n);

	// The value that the three edge tests add to, in the body's space, at its lowest
	const glm::vec3 size = glm::max(glm::max(face.a, face.b), face.c) - glm::min(glm::min(face.a, face.b), face.c);
	const float localSize = LargestPart(size);
	const float localNormalSum = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
	const glm::vec3 across = glm::cross(ab, ac);
	const float localArea =
	    (across.x * n.x + across.y * n.y) + across.z * n.z - k_EdgeError * localSize * localSize * localNormalSum;
	if (!(localArea > 0.0f))
	{
		return k_Infinity;
	}

	// The same values for the face in the world
	const float worldSize = k_Stretch * edge + 2.0f * cornerError;
	const float normalLow = k_Shrink * normalLength * (1.0f - k_DotError) - k_Sqrt3 * normalError;
	const float normalHigh = k_Stretch * normalLength * (1.0f + k_DotError) + k_Sqrt3 * normalError;
	if (!(normalLow > 0.0f))
	{
		return k_Infinity;
	}
	const float worldNormalSum = k_Sqrt3 * normalHigh;
	const float areaChange =
	    (4.0f * k_Sqrt3 * k_Stretch * edge * cornerError + 12.0f * cornerError * cornerError) * normalHigh +
	    k_Stretch * k_Stretch * edge * edge * k_Sqrt3 * normalError;
	const float worldArea = k_SmallestDeterminant * localArea - areaChange;
	if (!(worldArea > 0.0f))
	{
		return k_Infinity;
	}

	// How far the corners can be from the plane through the first corner with the normal, in the world
	const float localOffPlane =
	    std::max(std::abs((ab.x * n.x + ab.y * n.y) + ab.z * n.z), std::abs((ac.x * n.x + ac.y * n.y) + ac.z * n.z)) +
	    k_DotError * localNormalSum * localSize;
	const float worldOffPlane =
	    (localOffPlane + 6.0f * k_LargestAxisError * normalHigh * edge + 2.0f * k_Sqrt3 * k_Stretch * normalHigh * cornerError +
	     k_Sqrt3 * normalError * k_Stretch * edge + 6.0f * normalError * cornerError) /
	    normalLow;

	// How much a passing hit can go out of the triangle, for each unit of its distance from the corners
	const float spill = k_EdgeError * worldSize * worldNormalSum / worldArea;
	if (!(2.0f * spill * worldSize + 4.0f * spill * worldOffPlane <= 0.25f))
	{
		return k_Infinity;
	}
	const float largestWorldPart = shift + 2.0f * largestPart;
	const float margin =
	    3.0f * spill * worldSize * worldSize + 3.0f * worldOffPlane + k_PlaneError * largestWorldPart + k_SmallestMargin;
	return margin * (1.0f + k_MarginError);
}

float openblack::physics::hit_bounds::ShapeMargin(std::span<const LocalFace> faces, float largestPart, float shift)
{
	float margin = 0.0f;
	for (const auto& face : faces)
	{
		margin = std::max(margin, FaceMargin(face, largestPart, shift));
	}
	return margin;
}

bool openblack::physics::hit_bounds::FitsMargin(const glm::mat3& axes, glm::vec3 centre, glm::vec3 ahead, float shift)
{
	if (!IsFinite(axes[0]) || !IsFinite(axes[1]) || !IsFinite(axes[2]) || !IsFinite(centre) || !IsFinite(ahead))
	{
		return false;
	}
	// The products of the axes, against those of axes that are square and of unit length. The test keeps half of the
	// limit for the errors of these sums.
	float axisError = 0.0f;
	for (int i = 0; i < 3; ++i)
	{
		for (int j = i; j < 3; ++j)
		{
			const float product = glm::dot(axes[i], axes[j]);
			axisError = std::max(axisError, std::abs(i == j ? product - 1.0f : product));
		}
	}
	return axisError <= 0.5f * k_LargestAxisError && glm::determinant(axes) > 0.5f &&
	       LargestPart(centre) + LargestPart(ahead) <= 0.99f * shift;
}

std::optional<Box> openblack::physics::hit_bounds::LineBox(glm::vec3 point, glm::vec3 direction)
{
	const float size = LargestPart(point) + LargestPart(direction);
	if (!(size <= k_LargestLinePart))
	{
		return std::nullopt;
	}
	const float margin = size * k_LineError;
	const glm::vec3 end = point - direction;
	return Box {.min = glm::min(point, end) - margin, .max = glm::max(point, end) + margin};
}

bool openblack::physics::hit_bounds::Touches(const Box& first, const Box& second)
{
	return first.min.x <= second.max.x && second.min.x <= first.max.x && first.min.y <= second.max.y &&
	       second.min.y <= first.max.y && first.min.z <= second.max.z && second.min.z <= first.max.z;
}
