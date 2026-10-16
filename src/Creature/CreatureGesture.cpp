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

glm::vec3 creature_gesture::HandTarget(glm::vec3 head, glm::vec2 facing, float radius, glm::vec3 shapePoint)
{
	const auto ahead = facing * (k_AheadRadii * radius);
	const glm::vec2 side {facing.y, -facing.x};
	const auto size = radius * k_SizePerRadius;
	const auto across = side * (size * shapePoint.x);
	return {head.x + ahead.x + across.x, head.y + (size * (1.0f - shapePoint.z)), head.z + ahead.y + across.y};
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
