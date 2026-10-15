/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRestAndHome.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// The dazed look about after a rest, with sleepy eyes and a grimace
Step GetUpDazed()
{
	return {.kind = Step::Kind::Action,
	        .animation = animations::k_Confused,
	        .sleepyEyes = true,
	        .face = creature_face::Cue::Grimace};
}

Step WalkTo(glm::vec2 point, float within, bool run = false)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = point, .run = run, .maxDistance = within},
	        .gaze = GoingLook()};
}

Step FaceDownSlope()
{
	return {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}};
}

/// Casting a one-shot miracle at itself from the casting pose, with a grimace
Step CastAtItself(uint32_t magicType, uint32_t self)
{
	return {.kind = Step::Kind::Cast,
	        .seconds = k_CastHoldSeconds,
	        .animation = k_CastPose[1],
	        .sequence = k_CastPose,
	        .cast = {.magicType = magicType, .object = self},
	        .face = creature_face::Cue::Grimace};
}
} // namespace

bool creature_mind::NothingFrighteningNear(float height, std::span<const float> otherCreatures)
{
	const auto reach = height * k_FrighteningHeights;
	return std::ranges::none_of(otherCreatures, [reach](float distance) { return distance < reach; });
}

bool creature_mind::FarFromHome(float homeDistance)
{
	return homeDistance > k_NearHome;
}

bool creature_mind::NotTooFarFromHome(bool hasTemple, float homeDistance)
{
	return hasTemple && homeDistance < k_FurthestFromHome;
}

bool creature_mind::IsBetter(float life, float exhaustion)
{
	return !(life < 1.0f) && !(exhaustion > 0.0f);
}

Step creature_mind::RestStep()
{
	return {.kind = Step::Kind::Static,
	        .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	        .untilBetter = true,
	        .mirrorAtRandom = true,
	        .closedEyes = true,
	        .effect = Effect::Completed,
	        .main = true};
}

std::vector<Step> creature_mind::RestToGetBetter()
{
	return {FaceDownSlope(), RestStep(), GetUpDazed()};
}

std::vector<Step> creature_mind::GoHomeToRecover(glm::vec2 home, float height)
{
	return {WalkTo(home, std::min(height, k_FurthestFromHomeToRecover)), FaceDownSlope(), RestStep(), GetUpDazed()};
}

std::vector<Step> creature_mind::HealItself(uint32_t healMagicType, glm::vec3 place)
{
	return {
	    {.kind = Step::Kind::Cast,
	     .seconds = k_CastHoldSeconds,
	     .animation = k_CastPose[1],
	     .sequence = k_CastPose,
	     .cast = {.magicType = healMagicType, .place = place},
	     .face = creature_face::Cue::Grimace},
	    {.kind = Step::Kind::HealItself, .face = creature_face::Cue::Grimace},
	};
}

std::vector<Step> creature_mind::RestOnTheSpot(uint32_t self, float seconds)
{
	return {{.kind = Step::Kind::Wait,
	         .seconds = seconds,
	         .face = creature_face::Cue::Exhausted,
	         .gaze = Gaze {.object = self, .bottom = true}}};
}

std::vector<Step> creature_mind::CastRestoreHealthInHand(uint32_t magicType, uint32_t self)
{
	return {{.kind = Step::Kind::UseUpHeld}, CastAtItself(magicType, self)};
}

std::vector<Step> creature_mind::PickUpAndCastRestoreHealth(uint32_t magicType, uint32_t self, uint32_t globe, bool handEmpty,
                                                            float waitSeconds)
{
	std::vector<Step> agenda;
	if (handEmpty)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = globe},
		                  .gaze = Gaze {.object = globe, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = waitSeconds});
	agenda.push_back({.kind = Step::Kind::UseUpHeld});
	agenda.push_back(CastAtItself(magicType, self));
	agenda.push_back({.kind = Step::Kind::UseUp, .object = globe});
	return agenda;
}

std::vector<Step> creature_mind::GoHome(glm::vec2 home, float height)
{
	return {WalkTo(home, std::min(height, k_FurthestFromHomeToStop))};
}

std::vector<Step> creature_mind::RunHome(glm::vec2 home, float height, bool frightenedFirst)
{
	std::vector<Step> agenda;
	if (frightenedFirst)
	{
		agenda.push_back({.kind = Step::Kind::Action,
		                  .animation = animations::k_Frightened,
		                  .face = creature_face::Cue::Frightened,
		                  .heldEyes = Eyes::Wide});
	}
	agenda.push_back(WalkTo(home, k_RunHomeHeights * height, true));
	return agenda;
}

glm::vec2 creature_mind::TempleStop(glm::vec3 standing, glm::vec3 heart, float heartRadius)
{
	const auto offset = heart - standing;
	const auto length = glm::length(offset);
	if (!(length > 0.0f))
	{
		return {standing.x, standing.z};
	}
	const auto distance = length - (k_TempleStopRoutes * heartRadius * k_HeartRouteShare);
	const auto stop = standing + ((offset / length) * distance);
	return {stop.x, stop.z};
}

std::vector<Step> creature_mind::EnterTemple(glm::vec2 home, glm::vec3 heart, glm::vec2 stop, float height)
{
	return {
	    WalkTo(home, height),
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::TurnToFace, .point = {heart.x, heart.z}},
	     .face = creature_face::Cue::Smile,
	     .gaze = Gaze {.point = heart}},
	    WalkTo(stop, height),
	};
}

std::vector<Step> creature_mind::PrayAtTemple(glm::vec2 home, float height)
{
	auto walk = WalkTo(home, height);
	walk.effect = Effect::Completed;
	walk.main = true;
	return {walk, {.kind = Step::Kind::Action, .animation = k_PrayAnimation}};
}

std::vector<Step> creature_mind::LookAtTemple(uint32_t part, bool pointsAtIt)
{
	const Gaze top {.object = part};
	std::vector<Step> agenda {
	    {.kind = Step::Kind::Move,
	     .seconds = k_FaceTempleSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = part},
	     .gaze = top},
	    {.kind = Step::Kind::Wait, .seconds = k_WatchTempleTopSeconds, .gaze = top},
	};
	if (pointsAtIt)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = part, .seconds = k_PointAtTempleSeconds},
		                  .face = creature_face::Cue::Amazed,
		                  .gaze = Gaze {.object = part, .itself = true}});
	}
	agenda.push_back(
	    {.kind = Step::Kind::Wait, .seconds = k_WatchTempleFootSeconds, .gaze = Gaze {.object = part, .bottom = true}});
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = k_WatchTempleTopAgainSeconds, .gaze = top});
	return agenda;
}
