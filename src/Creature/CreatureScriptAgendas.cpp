/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureScriptAgendas.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>

#include <glm/geometric.hpp>

using namespace openblack::creature_mind;

namespace
{
/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds)
{
	Step turn {.kind = Step::Kind::Move, .seconds = seconds};
	turn.movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing};
	return turn;
}

/// Pointing somewhere for a while, amazed
Step Point(ObjectOrder order, float seconds, Gaze gaze)
{
	order.kind = ObjectOrder::Kind::PointAt;
	order.seconds = std::max(seconds, k_ShortestPointSeconds);
	return {.kind = Step::Kind::Object, .order = order, .face = openblack::creature_face::Cue::Amazed, .gaze = gaze};
}

/// The eight directions about a point the ground is looked at in, in the order they are tried
constexpr std::array<glm::vec2, 8> k_SlopeDirections {
    glm::vec2 {1.0f, 0.0f},  glm::vec2 {1.0f, 1.0f},   glm::vec2 {0.0f, 1.0f},  glm::vec2 {-1.0f, 1.0f},
    glm::vec2 {-1.0f, 0.0f}, glm::vec2 {-1.0f, -1.0f}, glm::vec2 {0.0f, -1.0f}, glm::vec2 {1.0f, -1.0f},
};
} // namespace

std::vector<Step> openblack::creature_mind::LookForever(uint32_t thing)
{
	return {
	    FaceThing(thing, k_FaceThingSeconds),
	    {.kind = Step::Kind::Wait, .seconds = k_LookForeverSeconds, .gaze = Gaze {.object = thing}},
	};
}

std::vector<Step> openblack::creature_mind::LookButDontApproach(uint32_t thing, float chance)
{
	return {
	    FaceThing(thing, k_FaceThingSeconds),
	    {.kind = Step::Kind::Wait,
	     .seconds = k_LookButDontApproachSeconds + (chance * k_LookButDontApproachExtraSeconds),
	     .gaze = Gaze {.object = thing, .bottom = true}},
	};
}

std::vector<Step> openblack::creature_mind::LookAtCamera(bool hasPlayer)
{
	if (!hasPlayer)
	{
		// Without a player there is no camera of its own to look at: it is done at once
		return {{.kind = Step::Kind::Wait, .seconds = 0.0f}};
	}
	// TODO(opening-skip): the game also turns it to the camera when it is more than a quarter turn away, and stops
	// looking early once the camera is more than 100 away or has been still for a while; only the look is kept here
	return {{.kind = Step::Kind::Wait,
	         .seconds = k_LookAtCameraSeconds,
	         .face = creature_face::Cue::Curiosity,
	         .gaze = Gaze {.camera = true}}};
}

std::vector<Step> openblack::creature_mind::PointAtThing(uint32_t thing)
{
	return {Point({.object = thing}, k_PointAtThingSeconds, {.object = thing})};
}

std::vector<Step> openblack::creature_mind::PointAtCamera(bool hasPlayer, glm::vec3 camera)
{
	std::vector<Step> agenda;
	if (hasPlayer)
	{
		agenda.push_back(
		    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = {camera.x, camera.z}}});
	}
	agenda.push_back(
	    Point({.point = {camera.x, camera.z}, .pointHeight = camera.y}, k_PointAtCameraSeconds, {.point = camera}));
	return agenda;
}

std::vector<Step> openblack::creature_mind::SitDownAt(uint32_t place, glm::vec2 point, float radius, const Random& random)
{
	return {
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = radius}},
	    FaceThing(place, k_FacePlaceSeconds),
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}},
	    SitDown(random),
	};
}

std::vector<Step> openblack::creature_mind::FaceForScript(glm::vec3 point)
{
	return {{.kind = Step::Kind::Move,
	         .movement = {.kind = Movement::Kind::FacePoint, .point = {point.x, point.z}},
	         .gaze = Gaze {.point = point}}};
}

std::vector<Step> openblack::creature_mind::MoveForScript(glm::vec2 from, glm::vec3 point, float height, float distance)
{
	const glm::vec2 to {point.x, point.z};
	auto first = distance;
	auto second = distance;
	if (distance == 0.0f)
	{
		first = glm::distance(from, to) * k_FirstLegShare;
		first = height >= first ? height : std::min(first, (k_FirstLegHeightShare * height) + k_FirstLegMost);
		second = height;
	}
	const auto leg = [&to, &point](float within) {
		return Step {.kind = Step::Kind::Move,
		             .movement = {.kind = Movement::Kind::ToPoint, .point = to, .maxDistance = within},
		             .gaze = Gaze {.point = point}};
	};
	return {leg(first), leg(second)};
}

bool openblack::creature_mind::NeedsToTurnToFace(float distance, float turn)
{
	return distance >= k_OnTopOf && std::abs(turn) > k_FacingWithin;
}

glm::vec2 openblack::creature_mind::DownSlope(glm::vec2 from, const std::function<std::optional<float>(glm::vec2)>& heightAt)
{
	glm::vec2 lowest {0.0f};
	auto lowestHeight = std::numeric_limits<float>::max();
	for (const auto& direction : k_SlopeDirections)
	{
		const auto point = from + (direction * k_SlopeLookDistance);
		const auto height = heightAt(point);
		if (height.has_value() && *height < lowestHeight)
		{
			lowest = point;
			lowestHeight = *height;
		}
	}
	return lowest;
}
