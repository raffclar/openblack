/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightningMaths.h"

#include <cmath>
#include <cstdint>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack::particles;

namespace
{
/// The cells across the square that a search covers: the radius over the cell size, rounded up and cut to a whole
/// number. A radius of zero or a little less than zero gives no cells.
int32_t CellsAcross(float radius)
{
	return static_cast<int32_t>(std::ceil(static_cast<double>(radius / maths::k_StrikeCellSize)));
}

/// The length of a vector, with the squares added z first, then y, then x, each step rounded to a float
float Length(glm::vec3 v)
{
	return std::sqrt(((v.z * v.z) + (v.y * v.y)) + (v.x * v.x));
}

/// Make a vector unit length in place, by its length's inverse, and give its length. A zero vector stays as it is.
float Normalise(glm::vec3& v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return 0.0f;
	}
	const float length = Length(v);
	const float inverse = 1.0f / length;
	v = {v.x * inverse, v.y * inverse, v.z * inverse};
	return length;
}
} // namespace

size_t maths::StrikeSearchCells(float radius)
{
	const auto across = CellsAcross(radius);
	return static_cast<size_t>(4 * across * across);
}

size_t maths::CloudSearchCells(float radius)
{
	const auto across = CellsAcross(radius);
	return static_cast<size_t>(across * across);
}

size_t maths::ForkCount(size_t targets)
{
	return 2 * targets;
}

std::vector<size_t> maths::PreferCreatures(const std::vector<bool>& isCreature)
{
	std::vector<size_t> creatures;
	for (size_t i = 0; i < isCreature.size(); ++i)
	{
		if (isCreature[i])
		{
			creatures.push_back(i);
		}
	}
	std::vector<size_t> result(isCreature.size());
	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] = creatures.empty() ? i : creatures[i % creatures.size()];
	}
	return result;
}

std::optional<glm::vec3> maths::ClashPoint(const BoltPose& newer, const BoltPose& older, float reach)
{
	const glm::vec3 heading(std::cos(newer.heading), 0.0f, std::sin(newer.heading));
	// They must point the same way across the land
	if (!(std::cos(older.heading) * heading.x + std::sin(older.heading) * heading.z > 0.0f))
	{
		return std::nullopt;
	}
	auto olderWay = older.centroid - older.origin;
	auto newerWay = older.centroid - newer.origin;
	const float olderLength = Normalise(olderWay);
	const float newerLength = Normalise(newerWay);
	const auto pull = (heading + olderWay) * 0.5f * k_ClashShare * std::min(olderLength, newerLength);
	const auto meeting = older.centroid - pull;
	auto fromNewer = meeting - newer.origin;
	// The meeting is within the newer bolt's reach
	const float distance = Normalise(fromNewer);
	if (!(distance < reach))
	{
		return std::nullopt;
	}
	// The meeting is in front of the newer bolt, at a true angle off its heading. The cosine of the widest angle keeps
	// its full precision.
	const float along = (fromNewer.z * heading.z) + (fromNewer.x * heading.x);
	if (!(std::cos(static_cast<double>(k_ClashWidestAngle)) < static_cast<double>(along)))
	{
		return std::nullopt;
	}
	return meeting;
}

glm::vec3 maths::ArcTangent(glm::vec3 normal, glm::vec3 nudge, float randomTangents, float length, float scale)
{
	return (normal + nudge * randomTangents) * (length * scale);
}

float maths::ArcLength(const ArcEnds& ends)
{
	return Length(ends.from - ends.to);
}

std::vector<glm::vec3> maths::ArcJoints(const ArcEnds& ends, glm::vec3 fromTangent, glm::vec3 toTangent,
                                        std::span<const glm::vec3> jitters, float randomFrac)
{
	const float length = ArcLength(ends);
	// The cubic's coefficients from its ends and their tangents
	const auto a = 2.0f * ends.from - 2.0f * ends.to + fromTangent + toTangent;
	const auto b = -3.0f * ends.from + 3.0f * ends.to - 2.0f * fromTangent - toTangent;
	const auto c = fromTangent;
	const auto d = ends.from;
	std::vector<glm::vec3> joints;
	joints.reserve(jitters.size());
	const auto count = jitters.size();
	// The step along the curve is the inverse of the gaps between the joints
	const float step = count > 1 ? 1.0f / (static_cast<float>(count) - 1.0f) : 0.0f;
	for (size_t k = 0; k < count; ++k)
	{
		const float t = static_cast<float>(k) * step;
		const float t2 = t * t;
		const float t3 = t2 * t;
		// Each term on its own, added from the highest power down, with the start last
		const auto point = d + (c * t + (b * t2 + a * t3));
		joints.push_back(point + jitters[k] * length * randomFrac);
	}
	return joints;
}

int maths::SkyStrikeThunderSize(float random)
{
	constexpr float k_SmallBelow = 0.3f;
	constexpr float k_MediumBelow = 0.75f;
	if (random < k_SmallBelow)
	{
		return 3;
	}
	return random < k_MediumBelow ? 2 : 1;
}
