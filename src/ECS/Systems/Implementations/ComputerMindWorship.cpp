/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ComputerMindCommon.h"
#include "ComputerMindMiracles.h"
#include "ComputerMindRuleTable.h"
#include "ComputerMindScaffolds.h"
#include "ComputerMindTowns.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/ComputerMindWorshipRules.h"
#include "ECS/Dances.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerBreeding.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "ECS/WorshipSites.h"
#include "Hand/HandGrabRules.h"
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
namespace object_measures = openblack::ecs::systems::object_measures;

namespace
{

/// The centre of the town, none when it has none
entt::entity TownCentreOf(entt::entity town)
{
	auto& registry = Entities();
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		const auto* info = registry.Valid(abode) ? ecs::world_objects::AbodeInfoOf(abode) : nullptr;
		if (info != nullptr && info->abodeType == AbodeType::TownCentre)
		{
			return abode;
		}
	}
	return entt::null;
}

/// How much a town needs more worshippers: none with no people, in an emergency, or with no centre that works
float TownWorshipNeedOf(entt::entity town)
{
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return 0.0f;
	}
	const auto count = ecs::villager_worship::HeadcountOf(town);
	if (count.population == 0)
	{
		return 0.0f;
	}
	if (Locator::townSystem::has_value() && Locator::townSystem::value().IsInStateOfEmergency(town))
	{
		return 0.0f;
	}
	const auto centre = TownCentreOf(town);
	if (centre == entt::null || !ecs::villager_home::IsFunctional(centre))
	{
		return 0.0f;
	}
	return computer_mind_rules::TownWorshipNeed(count.population, count.worshipping, count.onTheWay);
}

/// A hand can take the villager: it is not at home, not in a hand and not on its way to hide
bool Reachable(entt::entity villager)
{
	auto& registry = Entities();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	const bool hiding = action != nullptr && action->states[static_cast<size_t>(LivingAction::Index::Top)] ==
	                                             static_cast<uint8_t>(VillagerStates::GoAndHideInNearbyBuilding);
	return hand_grab::ValidForPlaceInHand({.kind = hand_grab::GrabKind::Villager,
	                                       .available = registry.Valid(villager),
	                                       .atHome = registry.AllOf<AtHome>(villager),
	                                       .inHand = registry.AllOf<InHand>(villager),
	                                       .hiding = hiding});
}

/// The worship site of the god that needs dancers most
float SiteNeedingVillagers(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto site : SitesOf(context.player))
	{
		if (site == entt::null)
		{
			continue;
		}
		// A built site with icons needs dancers; the need of its neediest town counts
		const auto icons = static_cast<uint32_t>(IconsOf(site).size());
		const bool built = Locator::worshipSiteSystem::has_value() && Locator::worshipSiteSystem::value().IsBuilt(site);
		const float dancerNeed =
		    built ? computer_mind_rules::SiteDancerNeed(Locator::worshipSiteSystem::value().Dancers(site), icons) : 0.0f;
		float need = 0.0f;
		if (dancerNeed != 0.0f)
		{
			float townNeed = 0.0f;
			for (const auto town : registry.Get<const WorshipSite>(site).towns)
			{
				const float each = TownWorshipNeedOf(town);
				if (townNeed < each)
				{
					townNeed = each;
				}
			}
			need = townNeed * dancerNeed;
		}
		if (need > best)
		{
			best = need;
			chosen = site;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return best;
}

/// The villager that the god carries to the worship site of the node above
float VillagerForSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	entt::entity site = entt::null;
	for (auto at = mind.nodes[node].parent; site == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		site = mind.nodes[*at].object;
	}
	if (site == entt::null || !registry.Valid(site) || !registry.AllOf<WorshipSite>(site))
	{
		return 0.0f;
	}
	std::optional<glm::vec2> gatePlace;
	if (Locator::worshipSiteSystem::has_value())
	{
		gatePlace = Locator::worshipSiteSystem::value().PlaceAt(site, WorshipSiteSystemInterface::SitePlace::Gate);
	}
	const auto gate = gatePlace.has_value() ? map_coords::FromMetres(*gatePlace) : map_coords::MapCoords {};
	// A villager far enough from the gate that the god may take
	const auto farFromGate = [&gate](entt::entity villager) {
		const auto coords = CoordsOfThing(villager);
		return coords.has_value() && gutils::GetDistanceInMetres(gate, *coords) > computer_mind_rules::k_CarryToWorshipDistance;
	};
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : registry.Get<const WorshipSite>(site).towns)
	{
		const float need = TownWorshipNeedOf(town);
		if (!(need > best))
		{
			continue;
		}
		// First the people of its buildings, newest building first, then its homeless
		entt::entity found = entt::null;
		for (const auto abode : registry.Get<const Town>(town).abodes)
		{
			const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
			if (building == nullptr || found != entt::null)
			{
				continue;
			}
			for (const auto villager : building->inhabitants)
			{
				if (registry.Valid(villager) && registry.AllOf<Villager>(villager) &&
				    ecs::villager_worship::IsAvailableForWorshipSite(villager, false) && Reachable(villager) &&
				    farFromGate(villager) && PlacePull(context, *CoordsOfThing(villager)) > 0.0f)
				{
					found = villager;
					break;
				}
			}
		}
		if (found == entt::null)
		{
			for (const auto villager : registry.Get<const Town>(town).homelessVillagers)
			{
				if (registry.Valid(villager) && registry.AllOf<Villager>(villager) &&
				    ecs::villager_worship::IsAvailableForWorshipSite(villager, false) && farFromGate(villager))
				{
					found = villager;
					break;
				}
			}
		}
		if (found != entt::null)
		{
			best = need;
			chosen = found;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return best;
}

/// The hand carries the villager of the node to the gate of the worship site above and drops it there
bool CarryToSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	entt::entity villager = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; villager == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		villager = mind.nodes[*at].object;
	}
	entt::entity site = entt::null;
	for (auto at = mind.nodes[node].parent; site == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		site = mind.nodes[*at].object;
	}
	if (villager == entt::null || !registry.Valid(villager) || site == entt::null || !registry.Valid(site))
	{
		return false;
	}
	// With no gate the action counts as done, but the hand does nothing
	const auto gatePlace = Locator::worshipSiteSystem::has_value()
	                           ? Locator::worshipSiteSystem::value().PlaceAt(site, WorshipSiteSystemInterface::SitePlace::Gate)
	                           : std::nullopt;
	if (!gatePlace.has_value())
	{
		return true;
	}
	const auto gate = map_coords::FromMetres(*gatePlace);
	const glm::vec3 place {map_coords::ToMetres(gate.x), context.hand.moves.LandHeight(gate) + gate.altitude,
	                       map_coords::ToMetres(gate.z)};
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = villager}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, StepKind::MoveHandToPos, {Argument {.place = place}, Argument {.value = 900.0f}});
	computer_hand::Add(context.state.hand, StepKind::Wait, {Argument {.value = 1.0f}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandAtPos,
	                   {Argument {.place = place}, Argument {.value = context.state.speed}});
	return true;
}

/// The object of the first node from a node up the tree that has one, none when no node has one
entt::entity FirstObjectFrom(const computer_mind::Mind& mind, std::optional<uint16_t> from)
{
	for (auto at = from; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return mind.nodes[*at].object;
		}
	}
	return entt::null;
}

/// The node itself or the first node above it with an object, none when no node has one
std::optional<uint16_t> NodeWithObject(const computer_mind::Mind& mind, std::optional<uint16_t> from)
{
	for (auto at = from; at.has_value(); at = mind.nodes[*at].parent)
	{
		if (mind.nodes[*at].object != entt::null)
		{
			return at;
		}
	}
	return std::nullopt;
}

/// The object of the leaf that the node chose, or of the first node above that leaf with one
entt::entity BestLeafThing(const computer_mind::Mind& mind, uint16_t node)
{
	auto leaf = node;
	while (mind.nodes[leaf].bestLeaf.has_value() && *mind.nodes[leaf].bestLeaf != leaf)
	{
		leaf = *mind.nodes[leaf].bestLeaf;
	}
	return FirstObjectFrom(mind, leaf);
}

/// The thing exists and has a component
template <typename Component>
bool IsA(entt::entity thing)
{
	auto& registry = Entities();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Component>(thing);
}

/// A place on the land for a step of the hand: a map position at its altitude above the land
glm::vec3 PlaceOnLand(const RuleContext& context, const map_coords::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), context.hand.moves.LandHeight(coords) + coords.altitude,
	        map_coords::ToMetres(coords.z)};
}

/// The hand picks up the thing and drops it at a place, at the speed of the god
void PickUpAndDropAt(RuleContext& context, entt::entity thing, const map_coords::MapCoords& place)
{
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = thing}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandAtPos,
	                   {Argument {.place = PlaceOnLand(context, place)}, Argument {.value = context.state.speed}});
}

/// The town of a thing: a town is its own, a villager's is the town it lives in and a building's the town it is of
entt::entity TownOfThing(entt::entity thing)
{
	auto& registry = Entities();
	if (IsA<Town>(thing))
	{
		return thing;
	}
	if (IsA<Villager>(thing))
	{
		const auto town = registry.Get<const Villager>(thing).town;
		return IsA<Town>(town) ? town : entt::null;
	}
	if (IsA<Abode>(thing))
	{
		return ecs::construction::TownOfSite(thing);
	}
	return entt::null;
}

/// How much the town wants a thing, with its two boosts, in the order that the game adds them
std::array<float, ecs::components::k_TownDesireCount> DesiresOf(entt::entity town)
{
	std::array<float, ecs::components::k_TownDesireCount> desires {};
	if (const auto* wants = Entities().TryGet<const TownDesire>(town))
	{
		for (size_t i = 0; i < desires.size(); ++i)
		{
			desires.at(i) = wants->desire.at(i) + wants->boost.at(i) + wants->boostA.at(i);
		}
	}
	return desires;
}

/// The town's people as the god counts them for its disciples: its adults and its disciples of each work, as they are
/// now
struct DiscipleCount
{
	uint32_t adults {0};
	std::array<uint8_t, 13> disciples {};
};
DiscipleCount DisciplesOf(entt::entity town)
{
	DiscipleCount count;
	auto& registry = Entities();
	registry.Each<const Villager>([&](entt::entity villager, const Villager& who) {
		if (who.town != town)
		{
			return;
		}
		if (who.lifeStage != Villager::LifeStage::Child)
		{
			++count.adults;
		}
		const auto work = ecs::villager_disciple::TypeOf(villager);
		if (work != VillagerDisciple::None)
		{
			auto& disciples = count.disciples.at(static_cast<size_t>(work));
			disciples = static_cast<uint8_t>(disciples + 1);
		}
	});
	return count;
}

/// The share of the town wanted at worship, as its totem gives it; none when the town has no totem
float WorshipShareOf(entt::entity town)
{
	auto& registry = Entities();
	const auto centre = TownCentreOf(town);
	if (centre == entt::null)
	{
		return 0.0f;
	}
	bool hasTotem = false;
	registry.Each<const VillageTotem>([&](const VillageTotem& totem) { hasTotem = hasTotem || totem.townCentre == centre; });
	return hasTotem ? registry.Get<const Town>(town).worshipShare : 0.0f;
}

/// The life of a living thing, from dead at 0 to full at 1
float LifeOf(entt::entity villager)
{
	return Entities().Get<const Villager>(villager).life;
}

/// Whether the villager is a child
bool IsChild(entt::entity villager)
{
	return Entities().Get<const Villager>(villager).lifeStage == Villager::LifeStage::Child;
}

/// Whether the villager is counted at worship
bool AtWorship(entt::entity villager)
{
	const auto* worshipper = Entities().TryGet<const Worshipper>(villager);
	return worshipper != nullptr && worshipper->atWorship;
}

/// Whether the villager is a disciple
bool IsDisciple(entt::entity villager)
{
	return ecs::villager_disciple::TypeOf(villager) != VillagerDisciple::None;
}

/// Whether the state that the villager ends up in lets it start new work
bool StateTakesNewWork(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto state = Locator::livingActionSystem::value().VillagerGetFinalState(*action);
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto index = static_cast<size_t>(state);
	return index < table.size() && table.at(index).field0x4 == -1;
}

/// How good the villager is as a free builder for the god: none when it is not free. A better villager than the best
/// so far becomes the best, but the best keeps only how near the hand the villager is.
bool BetterFreeBuilder(const RuleContext& context, entt::entity villager, float& best)
{
	auto& registry = Entities();
	if (!IsA<Villager>(villager) || !ecs::systems::villager_reactions::FreeForNewTask(villager) || IsChild(villager) ||
	    !Reachable(villager) || !StateTakesNewWork(villager) || AtWorship(villager) || IsDisciple(villager) ||
	    registry.Get<const Villager>(villager).buildingSite != entt::null || registry.AllOf<ScriptControlled>(villager))
	{
		return false;
	}
	const auto coords = CoordsOfThing(villager);
	if (!coords.has_value())
	{
		return false;
	}
	const float power =
	    Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().InfluencePower(context.player) : 0.0f;
	const float nearness = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(context.state.hand.coords, *coords), power);
	const float value = computer_mind_rules::FreeBuilderValue(PlacePull(context, *coords), LifeOf(villager), nearness);
	if (!(value > best))
	{
		return false;
	}
	best = nearness;
	return true;
}

/// The worship site of the god whose worshippers want food most. The god wants the food that the site needs from its
/// towns.
float SiteWantingFood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return 0.0f;
	}
	const auto& sites = Locator::worshipSiteSystem::value();
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto site : SitesOf(context.player))
	{
		if (site == entt::null)
		{
			continue;
		}
		if (const float desire = sites.DesireForFood(site); desire > best)
		{
			best = desire;
			chosen = site;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	mind.nodes[node].amount = sites.FoodNeededFromTown(chosen);
	return best;
}

/// The hand takes the food that the chosen leaf found and drops it on the worship site
bool CarryFoodToSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto thing = BestLeafThing(mind, node);
	const auto site = FirstObjectFrom(mind, node);
	if (thing == entt::null || !Entities().Valid(thing) || !IsA<WorshipSite>(site))
	{
		return false;
	}
	computer_hand::Add(context.state.hand, StepKind::PickUpObject,
	                   {Argument {.object = thing}, Argument {.value = context.state.speed}});
	computer_hand::Add(context.state.hand, StepKind::DropObjectInHandOnObject, {Argument {.object = site}});
	return true;
}

/// How much the dancers of a site want rest
float RestDesireAt(entt::entity site)
{
	auto& registry = Entities();
	const auto dance = registry.Get<const WorshipSite>(site).dance;
	if (dance == entt::null || !registry.Valid(dance))
	{
		return 0.0f;
	}
	std::vector<float> desires;
	for (const auto dancer : ecs::dances::Dancers(registry, dance))
	{
		desires.push_back(IsA<Villager>(dancer) ? computer_mind_rules::LifeDesire(LifeOf(dancer)) : 0.0f);
	}
	return computer_mind_rules::DancersRestDesire(desires);
}

/// The worship site of the god whose dancers want rest most
float SiteWantingRest(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto site : SitesOf(context.player))
	{
		if (site == entt::null || Locator::worshipSiteSystem::value().Dancers(site) == 0)
		{
			continue;
		}
		if (const float desire = computer_mind_rules::RestDesireCounted(RestDesireAt(site)); desire > best)
		{
			best = desire;
			chosen = site;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return computer_mind_rules::WorshippersRestUrge(best);
}

/// The most tired worshipper with a town at the worship site above, when its dancers want rest
float TiredWorshipper(RuleContext& /*context*/, computer_mind::Mind& mind, uint16_t node)
{
	const auto site = FirstObjectFrom(mind, mind.nodes[node].parent);
	if (!IsA<WorshipSite>(site) || !(RestDesireAt(site) > computer_mind_rules::k_RestDesireToTakeHome))
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto worshipper : Entities().Get<const WorshipSite>(site).worshippers)
	{
		if (!IsA<Villager>(worshipper))
		{
			continue;
		}
		if (const float want = computer_mind_rules::LifeDesire(LifeOf(worshipper));
		    want > best && TownOfThing(worshipper) != entt::null)
		{
			best = want;
			chosen = worshipper;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return computer_mind_rules::TiredWorshipperUrge(best);
}

/// The hand takes the tired worshipper home: it puts it down outside its home on the side of the door, else by its
/// town's centre, else at its town
bool TakeWorshipperHome(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto site = FirstObjectFrom(mind, mind.nodes[node].parent);
	const auto villager = FirstObjectFrom(mind, node);
	if (!IsA<Villager>(villager) || !IsA<WorshipSite>(site))
	{
		return false;
	}
	const auto town = TownOfThing(villager);
	if (town == entt::null)
	{
		return false;
	}
	auto home = registry.Get<const Villager>(villager).abode;
	if (!IsA<Abode>(home))
	{
		home = TownCentreOf(town);
	}
	map_coords::MapCoords place {};
	if (home != entt::null)
	{
		const auto homeCoords = CoordsOfThing(home).value_or(map_coords::MapCoords {});
		const auto door = map_coords::FromMetres(ecs::villager_home::ArrivePosition(home));
		const float angle = gutils::Get3DAngleFromXZ(homeCoords, door);
		const float distance = computer_mind_rules::HomeDropDistance(object_measures::TwoDRadius(registry, home),
		                                                             object_measures::TwoDRadius(registry, villager));
		place = homeCoords + gutils::GetPosFromAngle(angle, distance);
	}
	else
	{
		place = CoordsOfThing(town).value_or(map_coords::MapCoords {});
	}
	PickUpAndDropAt(context, villager, place);
	return true;
}

/// A town of the god with fewer than two breeders, looked for from a town picked at random
float TownWantingBreeder(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto towns = TownsOf(context.player);
	if (towns.empty())
	{
		return 0.0f;
	}
	const auto start = Locator::gameRandom::value().GameRand(static_cast<uint32_t>(towns.size()));
	for (size_t i = 0; i < towns.size(); ++i)
	{
		const auto town = towns.at((start + i) % towns.size());
		const auto breeders = DisciplesOf(town).disciples.at(static_cast<size_t>(VillagerDisciple::Breeder));
		if (static_cast<float>(breeders) < computer_mind_rules::k_FewestBreeders)
		{
			mind.nodes[node].object = town;
			return 1.0f;
		}
	}
	return 0.0f;
}

/// A villager of the town above to make a breeder: an adult of an age to make love, in its home or out of the hand's
/// reach, of a sex that has no breeder in the homes of the town yet. Men are looked at first.
float VillagerToMakeBreeder(computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto town = FirstObjectFrom(mind, mind.nodes[node].parent);
	if (!IsA<Town>(town))
	{
		return 0.0f;
	}
	std::array<bool, 2> hasBreeder {};
	std::array<entt::entity, 2> candidate {entt::null, entt::null};
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		if (!IsA<Abode>(abode))
		{
			continue;
		}
		for (const auto villager : registry.Get<const Abode>(abode).inhabitants)
		{
			if (!IsA<Villager>(villager) || !ecs::systems::villager_reactions::FreeForNewTask(villager) ||
			    AtWorship(villager) || Reachable(villager) || IsChild(villager) ||
			    !ecs::villager_breeding::IsSexuallyActive(villager) || registry.AllOf<ScriptControlled>(villager))
			{
				continue;
			}
			const auto sex = static_cast<size_t>(registry.Get<const Villager>(villager).sex) & 1;
			if (IsDisciple(villager))
			{
				if (ecs::villager_disciple::TypeOf(villager) == VillagerDisciple::Breeder)
				{
					hasBreeder.at(sex) = true;
				}
			}
			else if (candidate.at(sex) == entt::null)
			{
				candidate.at(sex) = villager;
			}
		}
	}
	for (size_t sex = 0; sex < candidate.size(); ++sex)
	{
		if (!hasBreeder.at(sex) && candidate.at(sex) != entt::null)
		{
			mind.nodes[node].object = candidate.at(sex);
			return 1.0f;
		}
	}
	return 0.0f;
}

/// The town of the node above for a free builder: its own town, or the town whose building site it is
entt::entity TownForBuilder(const computer_mind::Mind& mind, uint16_t node)
{
	const auto above = NodeWithObject(mind, mind.nodes[node].parent);
	if (!above.has_value())
	{
		return entt::null;
	}
	const auto& holder = mind.nodes[*above];
	if (holder.siteTown != entt::null)
	{
		if (IsA<Town>(holder.siteTown))
		{
			return holder.siteTown;
		}
		// A site of a town that has gone: a worship site's first town
		if (IsA<WorshipSite>(holder.object))
		{
			const auto& towns = Entities().Get<const WorshipSite>(holder.object).towns;
			return !towns.empty() && IsA<Town>(towns.front()) ? towns.front() : entt::null;
		}
		return entt::null;
	}
	return TownOfThing(holder.object);
}

/// The best free villager in the homes of the town above as a builder. The god wants it as much as the best keeps.
float FreeBuilderInTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	const auto town = TownForBuilder(mind, node);
	if (town == entt::null)
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		if (!IsA<Abode>(abode))
		{
			continue;
		}
		for (const auto villager : registry.Get<const Abode>(abode).inhabitants)
		{
			if (BetterFreeBuilder(context, villager, best))
			{
				chosen = villager;
			}
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	mind.nodes[node].object = chosen;
	return best;
}

/// The best free villager near the hand as a builder: the god looks at the cells in a spiral out from its hand, and
/// stops at the first cell with one
float FreeVillagerNearHand(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	if (!Locator::entitiesMap::has_value())
	{
		return 0.0f;
	}
	float best = 0.0f;
	auto coords = context.state.hand.coords;
	map_coords::Spiral spiral;
	for (int32_t cells = computer_mind_rules::k_FreeVillagerCells; cells != 0; --cells)
	{
		entt::entity chosen = entt::null;
		for (const auto thing : Locator::entitiesMap::value().GetAllInCell(map_coords::Cell(coords)))
		{
			if (IsA<Villager>(thing) && BetterFreeBuilder(context, thing, best))
			{
				chosen = thing;
			}
		}
		if (chosen != entt::null)
		{
			mind.nodes[node].object = chosen;
			return best;
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	return 0.0f;
}

/// How much the god wants to make a disciple for the town above, and the work it gives it
float DiscipleForTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto town = FirstObjectFrom(mind, mind.nodes[node].parent);
	if (!IsA<Town>(town))
	{
		return 0.0f;
	}
	const auto count = DisciplesOf(town);
	const auto desires = DesiresOf(town);
	const auto choice = computer_mind_rules::TownDiscipleWant(
	    count.disciples, count.adults, WorshipShareOf(town), desires, [&] { return VillagerToMakeBreeder(mind, node); },
	    [&] { return FreeBuilderInTown(context, mind, node); });
	if (!choice.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].amount = static_cast<int32_t>(choice->work);
	return choice->value;
}

/// The god makes the villager a disciple of the node's work at once, then picks it up and puts it down where it stands
bool MakeDisciple(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto villager = FirstObjectFrom(mind, node);
	const auto town = FirstObjectFrom(mind, mind.nodes[node].parent);
	if (!IsA<Villager>(villager) || !IsA<Town>(town))
	{
		return false;
	}
	ecs::villager_disciple::SetType(villager, static_cast<VillagerDisciple>(mind.nodes[node].amount));
	const auto coords = CoordsOfThing(villager);
	if (!coords.has_value())
	{
		return false;
	}
	PickUpAndDropAt(context, villager, *coords);
	return true;
}

/// The building sites of a town, the newest first
std::vector<entt::entity> NewestSitesOf(entt::entity town)
{
	auto sites = ecs::construction::SitesOfTown(town);
	std::ranges::stable_sort(sites, std::greater {},
	                         [town](entt::entity building) { return ecs::construction::SiteOf(town, building)->number; });
	return sites;
}

/// What the god knows of a town's site at a building
computer_mind_rules::SiteFacts FactsOfSite(entt::entity town, entt::entity building, bool wood)
{
	const bool built = ecs::construction::IsBuilt(building);
	return {.maxBuilders = ecs::construction::MaxBuilders(building),
	        .buildersNeeded = wood ? 0 : ecs::construction::BuildersNeeded(town, building),
	        .built = built,
	        .repairDesire = built ? ecs::construction::SiteDesireToBeRepaired(town, building) : 0.0f,
	        .woodNeeded = wood ? ecs::construction::WoodNeededToBuild(town, building) : 0.0f};
}

/// The building site of the god's towns that wants workers or wood most: it looks at the towns in turn, and stops after
/// the first town with a site that wants any
float SiteWanting(RuleContext& context, computer_mind::Mind& mind, uint16_t node, bool wood)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	entt::entity chosenTown = entt::null;
	for (const auto town : TownsOf(context.player))
	{
		const auto builders = DisciplesOf(town).disciples.at(static_cast<size_t>(VillagerDisciple::Builder));
		for (const auto building : NewestSitesOf(town))
		{
			const auto facts = FactsOfSite(town, building, wood);
			if (!(wood ? computer_mind_rules::TakesWood(facts) : computer_mind_rules::TakesWorkers(facts)))
			{
				continue;
			}
			const float pull = PlacePull(context, CoordsOfThing(building).value_or(map_coords::MapCoords {}));
			const float value =
			    wood ? computer_mind_rules::SiteWoodValue(pull, ecs::construction::WoodAtSite(town, building), facts.woodNeeded)
			         : computer_mind_rules::SiteWorkersValue(pull, ecs::construction::DesireForVillagers(town, building),
			                                                 builders, facts.maxBuilders);
			if (value > best)
			{
				best = value;
				chosen = building;
				chosenTown = town;
			}
		}
		if (chosen != entt::null)
		{
			mind.nodes[node].object = chosen;
			mind.nodes[node].siteTown = chosenTown;
			return best;
		}
	}
	return 0.0f;
}

float SiteWantingWorkers(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return SiteWanting(context, mind, node, false);
}

float SiteWantingWood(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return SiteWanting(context, mind, node, true);
}

/// The node itself or the first node above it that acts on a building site that is still open
std::optional<uint16_t> SiteNode(const computer_mind::Mind& mind, uint16_t node)
{
	const auto holder = NodeWithObject(mind, node);
	if (!holder.has_value())
	{
		return std::nullopt;
	}
	const auto& site = mind.nodes[*holder];
	if (site.siteTown == entt::null || ecs::construction::SiteOf(site.siteTown, site.object) == nullptr)
	{
		return std::nullopt;
	}
	return holder;
}

/// The map position of a point in the world, from its place on the land and its height above the sea. The altitude is
/// the height above the land under the point.
map_coords::MapCoords CoordsOfPoint(const RuleContext& context, glm::vec2 point, float height)
{
	auto coords = map_coords::FromMetres(point);
	// The land under the point is found a little differently from the point itself
	const map_coords::MapCoords under {.x = map_coords::FtoL(point.x * 65536.0f * 0.1f),
	                                   .z = map_coords::FtoL(point.y * 65536.0f * 0.1f)};
	coords.altitude = height - context.hand.moves.LandHeight(under);
	return coords;
}

/// Where the game holds a building to stand, as a map position: a worship site at the place of its first miracle icon
map_coords::MapCoords StandingPlaceOf(const RuleContext& context, entt::entity building)
{
	if (IsA<WorshipSite>(building) && Locator::worshipSiteSystem::has_value())
	{
		const auto& sites = Locator::worshipSiteSystem::value();
		const auto point = sites.PlaceAt(building, WorshipSiteSystemInterface::SitePlace::Standing);
		const auto height = sites.StandingHeight(building);
		if (point.has_value() && height.has_value())
		{
			return CoordsOfPoint(context, *point, *height);
		}
	}
	return CoordsOfThing(building).value_or(map_coords::MapCoords {});
}

/// Whether a thing with wood stands in the cell of a place
bool WoodInCell(const map_coords::MapCoords& place)
{
	if (!map_coords::InBounds(place) || !Locator::entitiesMap::has_value() || !Locator::resourceStoreSystem::has_value())
	{
		return false;
	}
	const auto& store = Locator::resourceStoreSystem::value();
	const auto things = Locator::entitiesMap::value().GetAllInCell(map_coords::Cell(place));
	return std::ranges::any_of(things, [&store](entt::entity thing) {
		return Entities().Valid(thing) && store.GetResource(thing, ResourceType::Wood) != 0;
	});
}

/// The hand takes the worker that the chosen leaf found and puts it down by the building of the site
bool CarryWorkerToSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto worker = BestLeafThing(mind, node);
	const auto site = SiteNode(mind, node);
	if (worker == entt::null || !Entities().Valid(worker) || !site.has_value())
	{
		return false;
	}
	const auto building = mind.nodes[*site].object;
	const float distance = computer_mind_rules::SiteWorkerDropRadius(object_measures::TwoDRadius(Entities(), building));
	const auto place = computer_mind_rules::SiteWorkerDropPlace(
	    StandingPlaceOf(context, building), map_coords::MapCoords {},
	    [distance](float angle) { return gutils::GetPosFromAngle(angle, distance); },
	    [](const map_coords::MapCoords& point) { return map_coords::InBounds(point); }, WoodInCell);
	PickUpAndDropAt(context, worker, place);
	return true;
}

/// Where the god puts wood for a building: at a worship site's door, at any other building where it stands
map_coords::MapCoords WoodPlaceOf(entt::entity building)
{
	auto place = CoordsOfThing(building).value_or(map_coords::MapCoords {});
	if (IsA<WorshipSite>(building) && Locator::worshipSiteSystem::has_value())
	{
		if (const auto door =
		        Locator::worshipSiteSystem::value().PlaceAt(building, WorshipSiteSystemInterface::SitePlace::Door))
		{
			place = map_coords::FromMetres(*door);
		}
	}
	return place;
}

/// The hand takes the wood that the chosen leaf found and drops it at the building of the site
bool CarryWoodToSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto wood = BestLeafThing(mind, node);
	const auto site = SiteNode(mind, node);
	if (wood == entt::null || !Entities().Valid(wood) || !site.has_value())
	{
		return false;
	}
	PickUpAndDropAt(context, wood, WoodPlaceOf(mind.nodes[*site].object));
	return true;
}

/// How much the god wants to cast the food miracle at the worship site above
float FoodSpellForSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return ShallCastAt(context, mind, node, MagicType::Food, CoordsOfThing(FirstObjectFrom(mind, mind.nodes[node].parent)));
}

/// The place where a worship site keeps its food, as a map position
std::optional<map_coords::MapCoords> FoodPlaceOf(const RuleContext& context, entt::entity site)
{
	const auto* transform = Entities().TryGet<const Transform>(site);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto point =
	    ecs::worship_site::TurnedPoint({transform->position.x, transform->position.z},
	                                   Entities().Get<const WorshipSite>(site).facing, ecs::worship_site::k_FoodPotPoint);
	return CoordsOfPoint(context, point, transform->position.y);
}

/// The god charges the food miracle of the node's icon and casts it where the worship site above keeps its food
bool CastFoodAtSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = FirstObjectFrom(mind, node);
	const auto site = FirstObjectFrom(mind, mind.nodes[node].parent);
	if (!IsA<WorshipIcon>(icon) || !IsA<WorshipSite>(site))
	{
		return false;
	}
	const auto place = FoodPlaceOf(context, site);
	if (!place.has_value())
	{
		return false;
	}
	ReachAndChant(context, icon);
	CastAtPlace(context, *place, icon, 0.0f);
	return true;
}

/// How much the god wants to cast the heal miracle, at no place
float HealSpellForSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return ShallCastAt(context, mind, node, MagicType::Heal, std::nullopt);
}

/// Whether the villager ends up hidden at its worship site
bool HidesAtSite(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	return action != nullptr && Locator::livingActionSystem::has_value() &&
	       Locator::livingActionSystem::value().VillagerGetFinalState(*action) == VillagerStates::HidingAtWorshipSite;
}

/// The god charges the heal miracle of the node's icon and casts it on the worshipper of the site above with the least
/// life. A worshipper that hides at the site does not count, and a worshipper with full life does not get the miracle.
bool HealWorshipper(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = FirstObjectFrom(mind, node);
	const auto site = FirstObjectFrom(mind, mind.nodes[node].parent);
	if (!IsA<WorshipIcon>(icon) || !IsA<WorshipSite>(site))
	{
		return false;
	}
	std::vector<entt::entity> worshippers;
	std::vector<float> lives;
	for (const auto worshipper : Entities().Get<const WorshipSite>(site).worshippers)
	{
		if (IsA<Villager>(worshipper) && !HidesAtSite(worshipper))
		{
			worshippers.push_back(worshipper);
			lives.push_back(LifeOf(worshipper));
		}
	}
	const auto weakest = computer_mind_rules::LeastLife(lives);
	if (!weakest.has_value())
	{
		return false;
	}
	const auto place = CoordsOfThing(worshippers[*weakest]);
	if (!place.has_value())
	{
		return false;
	}
	ReachAndChant(context, icon);
	CastAtPlace(context, *place, icon, 0.0f);
	return true;
}

/// How much the god wants to cast the wood miracle at the building of the site above
float WoodSpellForSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	return ShallCastAt(context, mind, node, MagicType::Wood, CoordsOfThing(FirstObjectFrom(mind, mind.nodes[node].parent)));
}

/// The god charges the wood miracle of the node's icon and casts it where it puts wood for the building of the site above
bool CastWoodAtSite(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	const auto icon = FirstObjectFrom(mind, node);
	const auto parent = mind.nodes[node].parent;
	const auto site = parent.has_value() ? SiteNode(mind, *parent) : std::nullopt;
	if (!IsA<WorshipIcon>(icon) || !site.has_value())
	{
		return false;
	}
	ReachAndChant(context, icon);
	CastAtPlace(context, WoodPlaceOf(mind.nodes[*site].object), icon, 0.0f);
	return true;
}

} // namespace

void computer_world::RegisterWorshipRules(RuleTable& table)
{
	constexpr auto k_Worship = MindDomain::Worship;
	AddRule(table, k_Worship, NodeKind::ActionWorshipSiteThatNeedsVillagers, {.intensity = SiteNeedingVillagers});
	AddRule(table, k_Worship, NodeKind::ObjectTakeVillagerFromTownToWorshipSite,
	        {.intensity = VillagerForSite, .perform = CarryToSite});
	AddRule(table, k_Worship, NodeKind::ActionGiveWorshippersFood, {.intensity = SiteWantingFood, .perform = CarryFoodToSite});
	AddRule(table, k_Worship, NodeKind::ActionGiveWorshippersRest, {.intensity = SiteWantingRest});
	AddRule(table, k_Worship, NodeKind::ObjectTakeVillagerFromWorshipSiteToTown,
	        {.intensity = TiredWorshipper, .perform = TakeWorshipperHome});
	AddRule(table, k_Worship, NodeKind::ActionMakeDiscipleBreader, {.intensity = TownWantingBreeder});
	AddRule(table, k_Worship, NodeKind::ActionGiveMyTownPopulation, {.intensity = TownWantingBreeder});
	AddRule(table, k_Worship, NodeKind::ObjectCreateDiscipleToFulfillTownDesire,
	        {.intensity = DiscipleForTown, .perform = MakeDisciple});
	AddRule(table, k_Worship, NodeKind::ObjectFindFreeVillagerBuilderInTown, {.intensity = FreeBuilderInTown});
	AddRule(table, k_Worship, NodeKind::ObjectFindFreeVillagerInArea, {.intensity = FreeVillagerNearHand});
	AddRule(table, k_Worship, NodeKind::ActionGiveBuildingSiteWorkers,
	        {.intensity = SiteWantingWorkers, .perform = CarryWorkerToSite});
	AddRule(table, k_Worship, NodeKind::ActionGiveBuildingSiteWood, {.intensity = SiteWantingWood, .perform = CarryWoodToSite});
	AddRule(table, k_Worship, NodeKind::ObjectCastFoodSpellOnWorshipSite,
	        {.intensity = FoodSpellForSite, .perform = CastFoodAtSite});
	AddRule(table, k_Worship, NodeKind::ObjectCastHealSpellAtWorshipSite,
	        {.intensity = HealSpellForSite, .perform = HealWorshipper});
	AddRule(table, k_Worship, NodeKind::ObjectShallCastWoodSpellAgain,
	        {.intensity = WoodSpellForSite, .perform = CastWoodAtSite});
	RegisterScaffoldRules(table);
}
