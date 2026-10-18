/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureGesture.h"

#include <algorithm>

#include "Common/X87.h"

using namespace openblack;

glm::vec3 creature_gesture::SamplePath(std::span<const glm::vec3> points, float share)
{
	if (points.empty())
	{
		return glm::vec3(0.0f);
	}
	if (share >= 1.0f || points.size() == 1)
	{
		return share >= 1.0f ? points.back() : points.front();
	}
	share = std::max(share, 0.0f);
	const auto along = static_cast<float>(points.size() - 1) * share;
	// The whole part of the place gives the point before, cut towards zero
	const auto index = static_cast<size_t>(static_cast<int32_t>(along));
	const auto fraction = along - static_cast<float>(index);
	const auto& from = points[index];
	const auto& to = points[std::min(index + 1, points.size() - 1)];
	return from + ((to - from) * fraction);
}

glm::vec3 creature_gesture::HandTarget(glm::vec3 head, GameHeading heading, float radius, glm::vec3 shapePoint)
{
	// The point ahead: the FPU's sine and cosine of the heading times two radii, each product rounded
	const auto angle = static_cast<double>(heading.Radians());
	const auto aheadReach = radius + radius;
	const glm::vec3 ahead {x87::Mul(x87::Sin(angle), aheadReach) + head.x, head.y,
	                       -x87::Mul(x87::Cos(angle), aheadReach) + head.z};
	// The side is a quarter turn back from the heading, as long as the size
	const auto size = radius * k_SizePerRadius;
	const auto sideAngle = static_cast<double>(heading.Radians() - k_QuarterTurn);
	const auto sideX = x87::Mul(x87::Sin(sideAngle), size);
	const auto sideZ = -x87::Mul(x87::Cos(sideAngle), size);
	const auto up = size * (1.0f - shapePoint.z);
	// The game adds the side and then a lift with zero parts across
	return {((sideX * shapePoint.x) + ahead.x) + 0.0f, (0.0f + ahead.y) + up, ((sideZ * shapePoint.x) + ahead.z) + 0.0f};
}

float creature_gesture::SharePerTurn(float length, float referenceLength)
{
	return 1.0f / ((length / referenceLength) * k_ReferenceTurns);
}

creature_gesture::Outcome creature_gesture::Advance(Drawing& drawing, uint32_t gesture, bool armPoints, float sharePerTurn)
{
	++drawing.turns;
	if (drawing.drawn == 0.0f && (!armPoints || drawing.turns < k_HoldTurns))
	{
		return Outcome::Waiting;
	}
	if (gesture > k_LastGesture)
	{
		return Outcome::Failed;
	}
	drawing.drawn = std::min(drawing.drawn + sharePerTurn, 1.0f);
	if (drawing.drawn < 1.0f)
	{
		drawing.turns = 0;
	}
	if (drawing.drawn >= 1.0f && drawing.turns > k_HoldTurns)
	{
		return Outcome::Done;
	}
	return Outcome::Drawing;
}
