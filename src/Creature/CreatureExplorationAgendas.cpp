/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureExplorationAgendas.h"

#include "Common/GUtilsAngle.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_exploration;
using creature_face::Cue;
using creature_mind::Effect;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// A game angle is a full turn in this many steps
constexpr uint32_t k_FullTurn = 0x800;

Step GoTo(glm::vec2 place, float within)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = place, .maxDistance = within},
	        .gaze = creature_mind::GoingLook()};
}

/// The creature turns to a place on the land. It looks at the place at the level of the sea.
Step TurnTo(glm::vec2 place)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::TurnToFace, .point = place},
	        .gaze = Gaze {.point = glm::vec3 {place.x, 0.0f, place.y}}};
}

Step PointAt(glm::vec2 place)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .point = place, .pointHeight = 0.0f, .seconds = k_PointSeconds},
	        .gaze = Gaze {.point = glm::vec3 {place.x, 0.0f, place.y}}};
}
} // namespace

std::array<glm::vec2, k_RidgePoints> creature_exploration::RidgePoints(glm::vec2 hill)
{
	std::array<glm::vec2, k_RidgePoints> points {};
	for (uint32_t i = 0; i < k_RidgePoints; ++i)
	{
		const auto angle = static_cast<uint16_t>((i * k_FullTurn) / k_RidgePoints);
		points.at(i) = hill + glm::vec2 {gutils::GetXByAngle(angle, k_RidgeRadius), gutils::GetZByAngle(angle, k_RidgeRadius)};
	}
	return points;
}

std::vector<Step> creature_exploration::GoToHillAndLook(glm::vec2 hill, float height)
{
	return {GoTo(hill, k_LookFromHeights * height)};
}

std::vector<Step> creature_exploration::GoToHillAndSit(glm::vec2 hill, float height)
{
	return {GoTo(hill, k_ExploreHeights * height)};
}

std::vector<Step> creature_exploration::GoToHillAndWalkAlongRidge(glm::vec2 hill, float height)
{
	std::vector<Step> agenda {GoTo(hill, k_ExploreHeights * height)};
	agenda.back().effect = Effect::Completed;
	for (const auto point : RidgePoints(hill))
	{
		agenda.push_back(GoTo(point, k_LookFromHeights * height));
	}
	return agenda;
}

std::vector<Step> creature_exploration::Explore(glm::vec2 place, float height)
{
	return {GoTo(place, k_ExploreHeights * height)};
}

std::vector<Step> creature_exploration::SitDownOnBeach(glm::vec2 place, glm::vec2 sea, float height, const Random& random)
{
	auto walk = GoTo(place, height);
	walk.face = Cue::Smile;
	const float sit = k_BeachSitSeconds + creature_emote::FloatRandom(random, k_BeachSitExtraSeconds);
	return {
	    walk,
	    TurnTo(sea),
	    {.kind = Step::Kind::Static,
	     .seconds = sit,
	     .animation = animations::k_Sit,
	     .sequence = {animations::k_StartSit, animations::k_Sit, animations::k_EndSit}},
	};
}

std::vector<Step> creature_exploration::LookAtMountains(glm::vec2 hill, const Random& random)
{
	std::vector<Step> agenda {TurnTo(hill)};
	if (random(k_PointAtMountainsLots) == 0)
	{
		// The action is complete once the creature has pointed, before it turns to the mountains again
		agenda.push_back(PointAt(hill));
		agenda.back().effect = Effect::Completed;
	}
	agenda.push_back(TurnTo(hill));
	return agenda;
}

std::vector<Step> creature_exploration::LookOutToSea(glm::vec2 coast, glm::vec2 sea, float height, const Random& random)
{
	std::vector<Step> agenda {GoTo(coast, k_LookFromHeights * height), TurnTo(sea)};
	if (random(k_PointOutToSeaLots) == 0)
	{
		agenda.push_back(PointAt(sea));
	}
	return agenda;
}

std::vector<Step> creature_exploration::ThrowInTheSea(std::optional<uint32_t> pickUp, glm::vec2 coast, glm::vec2 sea,
                                                      float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = *pickUp}});
	}
	auto walk = GoTo(coast, k_LookFromHeights * height);
	walk.face = Cue::Playfulness;
	agenda.push_back(walk);
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Throw, .point = sea}});
	return agenda;
}
