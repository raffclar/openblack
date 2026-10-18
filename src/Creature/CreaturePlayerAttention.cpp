/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlayerAttention.h"

#include <cmath>

#include <glm/geometric.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::creature_player_attention;

namespace
{
/// A half turn in game angle steps
constexpr uint32_t k_HalfTurn = 0x800;

/// The map position of a point: its x and z truncated to map units
map_coords::MapCoords MapPositionOf(glm::vec3 point)
{
	return {.x = map_coords::ToFixed(point.x), .z = map_coords::ToFixed(point.z)};
}
} // namespace

bool creature_player_attention::IsCross(float attitudeToPlayer)
{
	return attitudeToPlayer < k_CrossBelow;
}

bool creature_player_attention::AwayFromHand(glm::vec3 creature, glm::vec3 hand, glm::vec3 cameraOrigin, glm::vec3 cameraFocus)
{
	const float limit = ViewWidthAtFocus(cameraOrigin, cameraFocus) * k_AwayFromHandShare;
	const auto distance = gutils::GetDistanceInMetres(MapPositionOf(creature), MapPositionOf(hand));
	return distance > limit;
}

float creature_player_attention::ViewWidthAtFocus(glm::vec3 cameraOrigin, glm::vec3 cameraFocus)
{
	// The distance adds the squares from z to x. The tangent goes wide into the multiply
	const auto offset = cameraOrigin - cameraFocus;
	const float focusDistance = std::sqrt(((offset.z * offset.z) + (offset.y * offset.y)) + (offset.x * offset.x));
	return x87::Mul(x87::Tan(k_ViewHalfAngle), focusDistance);
}

void creature_player_attention::UpdateHandSpeed(HandSpeed& speed, glm::vec3 movement, float turnsPerSecond)
{
	const auto now = movement * turnsPerSecond;
	speed.velocity += (now - speed.velocity) * k_HandSpeedFollow;
}

bool creature_player_attention::IsHandMoving(const HandSpeed& speed)
{
	return glm::length(speed.velocity) > 0.0f;
}

bool creature_player_attention::IsDirectionClose(const map_coords::MapCoords& creature, const map_coords::MapCoords& point,
                                                 uint16_t heading)
{
	const auto toPoint = gutils::GetAngleFromXZ(creature, point);
	// The difference does not go round the circle. Thus two angles near the zero angle, one on each side, are far apart
	const auto difference = toPoint > heading ? toPoint - heading : heading - toPoint;
	if (static_cast<uint32_t>(difference) < k_HalfTurn / k_CloseDirectionParts)
	{
		return true;
	}
	return map_coords::CellX(creature) == map_coords::CellX(point) && map_coords::CellZ(creature) == map_coords::CellZ(point);
}

uint32_t creature_player_attention::FollowHandTurns(uint32_t turnsPerSecond)
{
	return static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(turnsPerSecond) * k_FollowHandSeconds));
}

float creature_player_attention::FollowHandReach(float radius)
{
	return radius + k_FollowHandReach;
}

FollowHandAction creature_player_attention::FollowHandTurn(const FollowHandView& view, uint32_t& turnsLeft)
{
	if (!view.hasPlayer || turnsLeft == 0)
	{
		return FollowHandAction::Done;
	}
	--turnsLeft;
	if (!view.handInMap)
	{
		return FollowHandAction::Fail;
	}
	if (view.bodyPlaying)
	{
		return view.directionClose ? FollowHandAction::Continue : FollowHandAction::WalkToHand;
	}
	if (view.distance < view.reach)
	{
		return FollowHandAction::Done;
	}
	return FollowHandAction::WalkToHand;
}

WatchPlayerAction creature_player_attention::WatchPlayerTurn(const WatchPlayerView& view)
{
	if (!view.waitingForOrders || !view.hasPlayer)
	{
		return {.done = true};
	}
	if (view.bodyPlaying)
	{
		return {};
	}
	const auto& target = view.handMovement > k_WatchHandMovement ? view.hand : view.camera;
	constexpr float k_QuarterTurn = 1.5707963705062866f;
	if (target.has_value() && target->distance > k_WatchTurnDistance && std::abs(target->turn) > k_QuarterTurn)
	{
		return {.turnTo = target->point};
	}
	return {};
}
