/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandFeel.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/X87Vector.h"

using namespace openblack;

namespace
{
/// How much of the way from the face towards the camera the hand leans, and how much it stands upwards
constexpr float k_TowardsCamera = 0.25f;
constexpr float k_Upwards = 0.5f;
/// Holding something, the hand is kept out from the face by this share of the held thing's radius
constexpr float k_HeldShare = 0.5f;

/// The vector made one long with its squares added in the given order; a vector of no length stays as it is
template <x87::SumOrder Order>
[[nodiscard]] glm::vec3 Unit(glm::vec3 v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	return x87::Scaled<Order>(v);
}
} // namespace

hand_feel::Feel hand_feel::FeelOf(const Picked& picked)
{
	if (picked.ignored)
	{
		return Feel::Nothing;
	}
	if (picked.living)
	{
		return picked.creature ? Feel::OnModel : Feel::AtPosition;
	}
	return picked.posedModel && !picked.animatedStatic ? Feel::AtPosition : Feel::OnModel;
}

glm::vec3 hand_feel::PointAlongCursor(glm::vec3 camera, glm::vec3 nearPoint, float distance)
{
	auto way = nearPoint - camera;
	if (way.x != 0.0f || way.y != 0.0f || way.z != 0.0f)
	{
		way = x87::Scaled<x87::SumOrder::Zyx>(way, distance);
	}
	return camera + way;
}

glm::vec3 hand_feel::CursorDirection(glm::vec3 camera, glm::vec3 nearPoint, float furthest)
{
	return Unit<x87::SumOrder::Xzy>(PointAlongCursor(camera, nearPoint, furthest) - camera);
}

glm::vec3 hand_feel::FeelDirection(glm::vec3 camera, glm::vec3 cursorDirection, std::optional<RaisedGround> raised)
{
	if (!raised.has_value())
	{
		return cursorDirection;
	}
	// The lift is added to the rise from the camera, not to the ground
	const glm::vec3 way {raised->ground.x - camera.x, (raised->ground.y - camera.y) + raised->lift,
	                     raised->ground.z - camera.z};
	return Unit<x87::SumOrder::Yxz>(way);
}

hand_feel::Rest hand_feel::RestOnModel(glm::vec3 camera, glm::vec3 cursorDirection, glm::vec3 feltDirection, glm::vec3 hitPoint,
                                       glm::vec3 hitNormal, bool feelsModel, std::optional<Holding> holding)
{
	const auto normal = Unit<x87::SumOrder::Zyx>(hitNormal);
	auto point = hitPoint;
	auto towardsCamera = camera - point;
	// How far along the felt direction the face is
	const float along = -x87::Dot<x87::SumOrder::Zxy>(towardsCamera, feltDirection);
	if (holding.has_value())
	{
		// Holding something, the point is put on the line of sight at the same distance
		point = cursorDirection * along + camera;
		towardsCamera = camera - point;
	}
	if (1.0f <= along)
	{
		towardsCamera = Unit<x87::SumOrder::Zyx>(towardsCamera);
	}
	else
	{
		point = camera + cursorDirection;
		towardsCamera = -cursorDirection;
	}
	if (holding.has_value())
	{
		float deeper = 0.0f;
		if (holding->creature.has_value())
		{
			const auto offset = glm::vec2(point.x, point.z) - holding->creature->centre;
			const float across = std::sqrt((offset.y * offset.y) + (offset.x * offset.x));
			if (across < holding->creature->radius)
			{
				deeper = holding->creature->radius - across;
			}
		}
		point += towardsCamera * (holding->radius * k_HeldShare + deeper);
	}
	if (!feelsModel)
	{
		return {.point = point, .up = std::nullopt};
	}
	const glm::vec3 up {(towardsCamera.x * k_TowardsCamera) - normal.x,
	                    ((towardsCamera.y * k_TowardsCamera) - normal.y) + k_Upwards,
	                    (towardsCamera.z * k_TowardsCamera) - normal.z};
	return {.point = point, .up = up};
}

glm::vec3 hand_feel::RestAtPosition(glm::vec3 camera, glm::vec3 nearPoint, glm::vec3 feltDirection, glm::vec3 position,
                                    float radius)
{
	const float distance = x87::Distance<x87::SumOrder::Xzy>(position, camera) - radius;
	const auto first = PointAlongCursor(camera, nearPoint, distance);
	return PointAlongCursor(camera, nearPoint, ((first.y - position.y) * feltDirection.y) + distance);
}

bool hand_feel::TurnsToLand(bool restsOnFace, bool overThing, float aboveLand, float handHeight)
{
	return !restsOnFace || (overThing && aboveLand < handHeight * 0.5f);
}

float hand_feel::HoverLimit(const Hover& hover)
{
	auto limit = hover.currentDistance;
	if (hover.land.has_value())
	{
		limit = x87::Distance<x87::SumOrder::Zyx>(hover.camera, *hover.land);
		if (!hover.landIsSea && !hover.holding)
		{
			limit -= hover.handHeight;
		}
	}
	return limit <= hover.minDistance ? hover.minDistance : std::min(limit, hover.reach);
}

hand_feel::HoverDistances hand_feel::HoverDistancesOf(const Hover& hover)
{
	const float limit = HoverLimit(hover);
	// The hand eases to the distance of the point it rests at, or else of the cursor's point at the limit
	const auto at = hover.restOnThing.has_value() ? hover.restOnThing : hover.cursorPoint;
	const float target = at.has_value() ? x87::Distance<x87::SumOrder::Xzy>(hover.camera, *at) : limit;
	return {.target = std::max(target, 1.0f), .limit = limit};
}

glm::vec3 hand_feel::UnitUp(glm::vec3 up)
{
	return Unit<x87::SumOrder::Zyx>(up);
}

void hand_feel::EaseHover(Zoomer& hover, float target, float seconds, bool pulling)
{
	if (pulling)
	{
		return;
	}
	hover.SetDestination(target, hover.GetValue() <= target ? k_HoverEaseOutSeconds : k_HoverEaseInSeconds);
	hover.Update(seconds);
	if (hover.GetValue() < 1.0f)
	{
		hover.Reset(1.0f);
	}
}
