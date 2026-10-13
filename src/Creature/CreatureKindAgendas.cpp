/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureKindAgendas.h"

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;

namespace
{
Step PickUp(uint32_t thing)
{
	return {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing}};
}
} // namespace

std::vector<Step> creature_mind::GiveWoodToStore(const std::optional<TreeToTake>& take, uint32_t store, float radius,
                                                 float height)
{
	std::vector<Step> agenda;
	if (take.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::ToPoint,
		                               .point = take->point,
		                               .maxDistance = (take->radius + radius) * k_TreeReach}});
		agenda.push_back(PickUp(take->tree));
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = store, .maxDistance = height},
	                  .face = creature_face::Cue::Compassion});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_FaceStoreForWoodSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = store},
	                  .effect = Effect::Completed,
	                  .face = creature_face::Cue::Compassion});
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::ThrowInStore, .object = store}});
	return agenda;
}

std::vector<Step> creature_mind::PracticeThrow(std::optional<uint32_t> pickUp, glm::vec2 from, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .point = from + glm::vec2(k_PracticeThrowHeights * height)}});
	return agenda;
}

std::vector<Step> creature_mind::ThrowToyAt(uint32_t toy, uint32_t target, glm::vec2 targetPoint, float height,
                                            const Random& random)
{
	const bool happyBefore = random(k_HappyAroundToyThrowLots) == 0;
	const bool happyAfter = random(k_HappyAroundToyThrowLots) == 0;
	std::vector<Step> agenda;
	if (happyBefore)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Happy});
	}
	agenda.push_back(PickUp(toy));
	const Gaze watch {.object = target};
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = target, .maxDistance = height},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .object = target, .point = targetPoint},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	if (happyAfter)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Happy});
	}
	return agenda;
}

std::vector<Step> creature_mind::PlantTreeBy(std::optional<uint32_t> tree, uint32_t by)
{
	std::vector<Step> agenda;
	if (tree.has_value())
	{
		agenda.push_back(PickUp(*tree));
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::GoNearObject, .object = by, .maxDistance = 0.0f}});
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = by}});
	agenda.push_back(
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}});
	return agenda;
}
