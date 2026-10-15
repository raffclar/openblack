/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSacrifice.h"

#include <algorithm>
#include <iterator>

#include <glm/vec2.hpp>

#include "Creature/CreatureFace.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_sacrifice;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;

std::optional<size_t> creature_sacrifice::SitePlace(std::span<const bool> placeHasSite)
{
	const auto found = std::ranges::find(placeHasSite, true);
	if (found == placeHasSite.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(placeHasSite.begin(), found));
}

std::vector<Step> creature_sacrifice::Sacrifice(std::optional<uint32_t> pickUp, glm::vec3 altar, float height)
{
	const glm::vec2 ground {altar.x, altar.z};
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = *pickUp},
		                  .face = creature_face::Cue::Anger,
		                  .gaze = Gaze {.object = *pickUp, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = ground, .maxDistance = k_AltarHeights * height},
	                  .gaze = creature_mind::GoingLook(ground)});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::TurnToFace, .point = ground},
	                  .gaze = Gaze {.point = altar}});
	// The creature only puts the thing down: the altar does not take it, and the thing stays alive
	agenda.push_back(
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}});
	return agenda;
}
