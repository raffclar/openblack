/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindScaffolds.h"

#include <array>
#include <optional>

#include <LNDFile.h>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ComputerMindCommon.h"
#include "ComputerMindConquest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindScaffoldRules.h"
#include "ECS/Registry.h"
#include "ECS/ScaffoldOfferRules.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/TownAreaRules.h"
#include "ECS/WorkshopRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerHome.h"

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

constexpr auto k_Worship = MindDomain::Worship;
/// The cells along each side of the map
constexpr int32_t k_MapCells = 512;

/// The thing exists and has a component
template <typename Component>
bool IsA(entt::entity thing)
{
	auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Component>(thing);
}

/// The object of the node, or of the first node above it that has one
entt::entity OwnObject(const computer_mind::Mind& mind, std::optional<uint16_t> node)
{
	for (auto at = node; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return mind.nodes[*at].object;
		}
	}
	return entt::null;
}

/// The object of the first node above the node that has one
entt::entity ObjectAbove(const computer_mind::Mind& mind, uint16_t node)
{
	return OwnObject(mind, mind.nodes[node].parent);
}

/// The object of the leaf that the node chose, or of the first node above that leaf with one
entt::entity LeafThing(const computer_mind::Mind& mind, uint16_t node)
{
	auto leaf = node;
	while (mind.nodes[leaf].bestLeaf.has_value() && *mind.nodes[leaf].bestLeaf != leaf)
	{
		leaf = *mind.nodes[leaf].bestLeaf;
	}
	return OwnObject(mind, leaf);
}

/// The node that the node chose, none when it chose itself or nothing
std::optional<uint16_t> ChosenNode(const computer_mind::Mind& mind, uint16_t node)
{
	const auto chosen = mind.nodes[node].bestLeaf;
	return chosen.has_value() && *chosen != node ? chosen : std::nullopt;
}

/// The hand refuses to pick up a thing that a hand holds
bool HandCanPickUp(entt::entity thing)
{
	return !Entities().AllOf<InHand>(thing);
}

/// How much a town wants houses and civic buildings, and its places for adults
computer_mind_rules::ScaffoldTownFacts FactsOfTown(entt::entity town)
{
	computer_mind_rules::ScaffoldTownFacts facts;
	if (Locator::townDesireSystem::has_value())
	{
		const auto& desires = Locator::townDesireSystem::value();
		facts.wantsAbodes = desires.GetRawDesire(town, TownDesireInfo::ForAbodes);
		facts.wantsCivic = desires.GetRawDesire(town, TownDesireInfo::ForCivicBuilding);
	}
	// The game compares the places as a signed number
	if (const auto* stats = Entities().TryGet<const TownStats>(town))
	{
		facts.adultPlaces = static_cast<int32_t>(stats->adultPlaces);
	}
	return facts;
}

/// The worth of a scaffold in scaffolds
uint32_t ValueOf(entt::entity scaffold)
{
	return ecs::workshop::ValueOfScale(Entities().Get<const Transform>(scaffold).scale.x);
}

/// The scaffold of the god's workshops that is best for the god to use, other than `excluded`. The god looks at each
/// workshop of each of its towns, and at each scaffold of the workshop, the newest first. A scaffold on a building site,
/// in a hand or in the physics does not count. A scaffold must be strictly better than the best so far.
entt::entity BestScaffoldToUse(const RuleContext& context, entt::entity excluded)
{
	auto& registry = Entities();
	const float power =
	    Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().InfluencePower(context.player) : 0.0f;
	const float reach = power + computer_mind_rules::k_ScaffoldReachMore;
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : TownsOf(context.player))
	{
		const auto facts = FactsOfTown(town);
		for (const auto abode : registry.Get<const Town>(town).abodes)
		{
			const auto* workshop = registry.Valid(abode) ? registry.TryGet<const Workshop>(abode) : nullptr;
			if (workshop == nullptr)
			{
				continue;
			}
			for (const auto scaffold : workshop->scaffolds)
			{
				if (scaffold == excluded || !IsA<Scaffold>(scaffold) ||
				    registry.Valid(registry.Get<const Scaffold>(scaffold).site) || registry.AnyOf<InHand, InPhysics>(scaffold))
				{
					continue;
				}
				const auto coords = CoordsOfThing(scaffold);
				if (!coords.has_value())
				{
					continue;
				}
				const float nearness =
				    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(context.state.hand.coords, *coords), reach);
				const float value = computer_mind_rules::ScaffoldUseValue(ValueOf(scaffold), facts, nearness);
				const float score = PlacePull(context, *coords) * value;
				if (score > best)
				{
					best = score;
					chosen = scaffold;
				}
			}
		}
	}
	return chosen;
}

// --- Join two scaffolds ---

/// The god wants to join two scaffolds when its towns want civic buildings more than houses. The best scaffold to use is
/// the node's object.
float CombineScaffolds(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	float wantsAbodes = 0.0f;
	float wantsCivic = 0.0f;
	for (const auto town : TownsOf(context.player))
	{
		const auto facts = FactsOfTown(town);
		wantsAbodes = facts.wantsAbodes + wantsAbodes;
		wantsCivic = facts.wantsCivic + wantsCivic;
	}
	if (!computer_mind_rules::WantsToCombine(wantsAbodes, wantsCivic))
	{
		return 0.0f;
	}
	const auto scaffold = BestScaffoldToUse(context, entt::null);
	if (scaffold == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = scaffold;
	return 1.0f;
}

/// The best scaffold to use other than the scaffold above is the node's object
float ScaffoldToCombineWith(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto scaffold = ObjectAbove(mind, node);
	if (!IsA<Scaffold>(scaffold))
	{
		return 0.0f;
	}
	const auto other = BestScaffoldToUse(context, scaffold);
	if (other == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = other;
	return 1.0f;
}

/// The hand picks up the node's scaffold and gives it to the scaffold that the chosen leaf found, which joins them
bool JoinScaffolds(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto held = OwnObject(mind, node);
	const auto target = LeafThing(mind, node);
	if (!IsA<Scaffold>(held) || !IsA<Scaffold>(target) || !HandCanPickUp(held))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = held}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = target}});
	return true;
}

// --- Use a scaffold for a building ---

/// The best scaffold to use is the node's object
float PlaceToUseScaffold(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto scaffold = BestScaffoldToUse(context, entt::null);
	if (scaffold == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = scaffold;
	return 1.0f;
}

/// Whether the cell under a map position is too steep for a scaffold. A cell off the map is not.
bool TooSteepAt(const map_coords::MapCoords& coords)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	// The four corners of a cell are the cell and the cells after it in its block of land
	const auto* cell = Locator::terrainSystem::value().FindCell({map_coords::CellOf(coords.x), map_coords::CellOf(coords.z)});
	if (cell == nullptr)
	{
		return false;
	}
	return computer_mind_rules::TooSteepForScaffold({cell[0].altitude, cell[1].altitude, cell[17].altitude, cell[18].altitude});
}

/// The town that most wants the building of the scaffold above is the node's object, and the place for the scaffold is
/// the node's place. For each town the god tries four places at a random distance from its centre. A place counts when
/// the town offers a building there and the land is not too steep. How much the town wants the building.
float LocationToUseScaffold(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto scaffold = ObjectAbove(mind, node);
	if (!IsA<Scaffold>(scaffold) || !Locator::workshopSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return 0.0f;
	}
	auto& registry = Entities();
	auto& random = Locator::gameRandom::value();
	auto& workshops = Locator::workshopSystem::value();
	const auto value = ValueOf(scaffold);
	float best = 0.0f;
	entt::entity chosen = entt::null;
	map_coords::MapCoords place {};
	for (const auto town : TownsOf(context.player))
	{
		const float desire = computer_mind_rules::ScaffoldTownDesire(value, FactsOfTown(town));
		const auto& area = registry.Get<const Town>(town).area;
		const float radius = ecs::town_area::Radius(area);
		const float spread = random.GameFloatRand(radius * computer_mind_rules::k_ScaffoldPlaceSpread);
		const float distance = computer_mind_rules::ScaffoldPlaceDistance(spread, radius);
		float angle = random.GameFloatRand(ecs::scaffold_offer::k_FullTurn);
		for (int32_t i = 0; i < computer_mind_rules::k_ScaffoldPlaceTries; ++i)
		{
			auto at = ecs::town_area::Centre(area) + gutils::GetPosFromAngle(angle, distance);
			at.x = computer_mind_rules::KeepCellOnMap(at.x, k_MapCells);
			at.z = computer_mind_rules::KeepCellOnMap(at.z, k_MapCells);
			const auto offer =
			    workshops.OfferAt(town, glm::vec3(map_coords::ToMetres(at.x), 0.0f, map_coords::ToMetres(at.z)), value);
			if (offer.has_value() && !TooSteepAt(at) && desire > best)
			{
				best = desire;
				chosen = town;
				place = at;
			}
			angle = angle + computer_mind_rules::k_ScaffoldPlaceStep;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	mind.nodes[node].place = {map_coords::ToMetres(place.x), context.hand.moves.LandHeight(place) + place.altitude,
	                          map_coords::ToMetres(place.z)};
	return best;
}

/// The hand picks up the node's scaffold and puts it down at the place that the chosen leaf found
bool UseScaffold(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto scaffold = OwnObject(mind, node);
	const auto chosen = ChosenNode(mind, node);
	if (scaffold == entt::null || !Entities().Valid(scaffold) || !chosen.has_value() || !HandCanPickUp(scaffold))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = scaffold}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandAtPos,
	                   {Argument {.place = mind.nodes[*chosen].place}, Argument {.value = context.state.speed}});
	return true;
}

// --- Fill a workshop ---

/// The first workshop of the god's towns that works, has room for a scaffold and has less wood than a scaffold needs is
/// the node's object. How much it wants wood: its room as a share of the most scaffolds that it makes.
float FillWorkshop(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	if (!Locator::workshopSystem::has_value() || !Locator::resourceStoreSystem::has_value() ||
	    !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	auto& registry = Entities();
	const auto& workshops = Locator::workshopSystem::value();
	const auto& stores = Locator::resourceStoreSystem::value();
	const auto woodForScaffold = Locator::infoConstants::value().scaffold.woodValue;
	for (const auto town : TownsOf(context.player))
	{
		for (const auto abode : registry.Get<const Town>(town).abodes)
		{
			if (!IsA<Workshop>(abode) || !ecs::villager_home::IsFunctional(abode) || workshops.RoomOf(abode) == 0 ||
			    !(stores.GetResource(abode, ResourceType::Wood) < woodForScaffold))
			{
				continue;
			}
			const auto* info = ecs::world_objects::AbodeInfoOf(abode);
			if (info == nullptr)
			{
				continue;
			}
			mind.nodes[node].object = abode;
			return computer_mind_rules::WorkshopFillValue(workshops.RoomOf(abode), info->maxNumMobileObjectsToProduce);
		}
	}
	return 0.0f;
}

/// The hand picks up the wood that the chosen leaf found and gives it to the node's workshop. The action counts as
/// started also when the hand cannot pick up the wood.
bool CarryWoodToWorkshop(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto workshop = OwnObject(mind, node);
	const auto thing = LeafThing(mind, node);
	if (workshop == entt::null || thing == entt::null || !Entities().Valid(workshop) || !Entities().Valid(thing))
	{
		return false;
	}
	if (HandCanPickUp(thing))
	{
		computer_hand::Add(context.state.hand, StepKind::PickUpObject,
		                   {Argument {.object = thing}, Argument {.value = context.state.speed}});
		computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = workshop}});
	}
	return true;
}

} // namespace

void computer_world::RegisterScaffoldRules(RuleTable& table)
{
	AddRule(table, k_Worship, NodeKind::ActionCombineScaffolds, {.intensity = CombineScaffolds, .perform = JoinScaffolds});
	AddRule(table, k_Worship, NodeKind::ObjectFindScaffoldToCombineWith, {.intensity = ScaffoldToCombineWith});
	AddRule(table, k_Worship, NodeKind::ActionFindPlaceToUseScaffoldForBuilding,
	        {.intensity = PlaceToUseScaffold, .perform = UseScaffold});
	AddRule(table, k_Worship, NodeKind::ObjectFindLocationToUseScaffold, {.intensity = LocationToUseScaffold});
	AddRule(table, k_Worship, NodeKind::ActionFillWorkshop, {.intensity = FillWorkshop, .perform = CarryWoodToWorkshop});
	// The site above the node is a building site, not a town, thus the god never wants its creature to help build. The
	// action of the node can never start, and it has no rule.
	AddRule(table, k_Worship, NodeKind::ObjectGetCreaturetoHelpBuild, {.intensity = CreatureToTown});
}
