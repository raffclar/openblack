/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::MindDomain;
using computer_mind::NodeKind;

namespace
{

/// The thing that the node acts on: its own thing, else the first thing of a node above
entt::entity ThingOf(const computer_mind::Mind& mind, uint16_t node)
{
	for (std::optional<uint16_t> at = node; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return mind.nodes[*at].object;
		}
	}
	return entt::null;
}

/// A land script tells the god to pick up a thing and drop it at a script marker. The action starts when the thing has
/// a place in the world and the marker is a script marker.
bool PickUpAndDrop(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto thing = ThingOf(mind, node);
	const auto marker = mind.nodes[node].argument;
	const bool thingHasPlace = thing != entt::null && registry.Valid(thing) && !registry.AllOf<ScriptMarker>(thing) &&
	                           CoordsOfThing(thing).has_value();
	if (!thingHasPlace || marker == entt::null || !registry.Valid(marker) || !registry.AllOf<ScriptMarker>(marker))
	{
		return false;
	}
	const auto coords = CoordsOfThing(marker);
	if (!coords.has_value())
	{
		return false;
	}
	const glm::vec3 place {map_coords::ToMetres(coords->x), context.hand.moves.LandHeight(*coords) + coords->altitude,
	                       map_coords::ToMetres(coords->z)};
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = thing}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandAtPos,
	                   {Argument {.place = place}, Argument {.value = context.state.speed}});
	return true;
}

} // namespace

void computer_world::RegisterCoreRules(RuleTable& table)
{
	// The rules of the root, the top nodes and the needs are not made yet
	AddRule(table, MindDomain::Core, NodeKind::ObjectDIRECTPickupAndDropObject, {.perform = PickUpAndDrop});
}
