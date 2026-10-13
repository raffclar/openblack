/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureKindAgendas.h"

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
