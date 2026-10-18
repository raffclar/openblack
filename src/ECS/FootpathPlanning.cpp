/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootpathPlanning.h"

#include <cmath>

#include <algorithm>
#include <iterator>

#include "Common/X87.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::footpath_planning;
using creature_route::Way;

namespace
{

/// Whole map units to metres and back
constexpr double k_MetresPerUnit = 1.52587890625e-4;
constexpr float k_UnitsPerMetre = 6553.60009765625f;
/// The most cost of a route is the distance and this, times the next
constexpr float k_CostExtra = 1.0f;
constexpr float k_CostScale = 5.0f;
/// An arc is stepped round by a tenth of a half turn
constexpr float k_StepCosine = 0.95107483863830566f;
constexpr float k_StepSine = 0.30896061658859253f;
/// The stepping stops within this share of the radius of where the arc leaves
constexpr double k_ArcEndShare = 0.35;
/// The circle an end is moved out of is solved as a quadratic
constexpr float k_MinusTwo = -2.0f;
constexpr float k_Four = 4.0f;

glm::vec2 Normalised(glm::vec2 v)
{
	if (v.x != 0.0f || v.y != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt((v.y * v.y) + (v.x * v.x));
		v.x = inverse * v.x;
		v.y = v.y * inverse;
	}
	return v;
}

/// The nodes of an arc: from where it begins round its circle, stepped while still far from where it leaves, then
/// where it leaves, which is as far from the centre as where it began
void ArcPlaces(const creature_route::Circle& circle, Way way, glm::vec2 from, glm::vec2 to, std::vector<glm::ivec2>& places)
{
	glm::vec2 at {from.x - circle.centre.x, from.y - circle.centre.y};
	auto leave = Normalised({to.x - circle.centre.x, to.y - circle.centre.y});
	const float distance = std::sqrt((at.y * at.y) + (at.x * at.x));
	leave.x = distance * leave.x;
	leave.y = leave.y * distance;
	const float near = x87::Mul(circle.radius, k_ArcEndShare);
	const float nearSquared = near * near;
	const auto farFromLeaving = [&]() {
		const float dx = at.x - leave.x;
		const float dy = at.y - leave.y;
		return (dy * dy) + (dx * dx) > nearSquared;
	};
	while (farFromLeaving())
	{
		places.push_back(footpath_planning::ToPlace({circle.centre.x + at.x, circle.centre.y + at.y}));
		if (way == Way::Clockwise)
		{
			const float x = (at.y * k_StepSine) + (at.x * k_StepCosine);
			at.y = (at.y * k_StepCosine) - (at.x * k_StepSine);
			at.x = x;
		}
		else
		{
			const float x = (at.x * k_StepCosine) - (at.y * k_StepSine);
			at.y = (at.y * k_StepCosine) + (at.x * k_StepSine);
			at.x = x;
		}
	}
	places.push_back(footpath_planning::ToPlace({leave.x + circle.centre.x, circle.centre.y + leave.y}));
}

} // namespace

glm::vec2 footpath_planning::ToPoint(glm::ivec2 place)
{
	return {x87::RoundToFloat(static_cast<double>(place.x) * k_MetresPerUnit),
	        x87::RoundToFloat(static_cast<double>(place.y) * k_MetresPerUnit)};
}

glm::ivec2 footpath_planning::ToPlace(glm::vec2 point)
{
	return {static_cast<int32_t>(point.x * k_UnitsPerMetre), static_cast<int32_t>(point.y * k_UnitsPerMetre)};
}

float footpath_planning::Range(glm::vec2 from, glm::vec2 to)
{
	const float dx = from.x - to.x;
	const float dy = from.y - to.y;
	return std::sqrt((dy * dy) + (dx * dx));
}

float footpath_planning::MostCost(glm::vec2 from, glm::vec2 to)
{
	return (Range(from, to) + k_CostExtra) * k_CostScale;
}

std::vector<glm::ivec2> footpath_planning::RoutePlaces(std::span<const creature_route::Leg> legs,
                                                       std::span<const creature_route::Obstacle> circles, glm::vec2 start,
                                                       glm::vec2 goal)
{
	std::vector<glm::ivec2> places {footpath_planning::ToPlace(start)};
	for (size_t i = 0; i < legs.size(); ++i)
	{
		const auto& leg = legs[i];
		const auto* before = i > 0 ? &legs[i - 1] : nullptr;
		if (leg.arc && before != nullptr && before->heading.has_value() && *before->heading < circles.size())
		{
			ArcPlaces(circles[*before->heading].circle, before->way, leg.from, leg.to, places);
		}
		else if (i + 1 < legs.size())
		{
			places.push_back(footpath_planning::ToPlace(leg.to));
		}
	}
	places.push_back(footpath_planning::ToPlace(goal));
	return places;
}

std::vector<PlanNode> footpath_planning::LayBetween(std::span<const PlanNode> nodes, size_t first, size_t last,
                                                    std::span<const glm::ivec2> route, std::optional<glm::ivec2> rejoin)
{
	std::vector<PlanNode> laid(nodes.begin(), nodes.begin() + static_cast<std::ptrdiff_t>(first + 1));
	std::ranges::transform(route, std::back_inserter(laid),
	                       [](glm::ivec2 place) { return PlanNode {.place = place, .planned = true}; });
	if (rejoin.has_value())
	{
		laid.push_back({.place = *rejoin, .planned = true, .rejoin = true});
	}
	std::ranges::copy_if(nodes.subspan(first + 1, last - first - 1), std::back_inserter(laid),
	                     [](const PlanNode& node) { return node.hidden || node.rejoin; });
	laid.insert(laid.end(), nodes.begin() + static_cast<std::ptrdiff_t>(last), nodes.end());
	return laid;
}

void footpath_planning::Settle(std::span<PlanNode> nodes)
{
	for (auto& node : nodes)
	{
		if (!node.hidden && !node.rejoin)
		{
			node.planned = false;
		}
	}
}

glm::vec2 footpath_planning::ClearOfBuilding(glm::vec2 end, glm::vec2 buildingPlace, glm::vec2 otherEnd,
                                             std::span<const creature_route::Circle> circles)
{
	if (end == buildingPlace)
	{
		auto towards = Normalised({otherEnd.x - end.x, otherEnd.y - end.y});
		end.x = (towards.x * k_OffMiddleStep) + end.x;
		end.y = (towards.y * k_OffMiddleStep) + end.y;
	}
	for (const auto& circle : circles)
	{
		const float radius = circle.radius + k_EndClearExtra;
		const glm::vec2 out {end.x - buildingPlace.x, end.y - buildingPlace.y};
		const glm::vec2 centre {circle.centre.x - buildingPlace.x, circle.centre.y - buildingPlace.y};
		const float b = ((centre.x * out.x) + (centre.y * out.y)) * k_MinusTwo;
		const float a = (out.x * out.x) + (out.y * out.y);
		const float c = ((centre.y * centre.y) + (centre.x * centre.x)) - (radius * radius);
		const float discriminant = (b * b) - ((c * a) * k_Four);
		// A line that misses the circle leaves the end where it is
		if (!(discriminant >= 0.0f))
		{
			continue;
		}
		const float along = (std::sqrt(discriminant) - b) / (a + a);
		// An end outside the circle stays where it is
		if (!(along >= 1.0f))
		{
			continue;
		}
		end.x = (out.x * along) + buildingPlace.x;
		end.y = (out.y * along) + buildingPlace.y;
	}
	return end;
}
