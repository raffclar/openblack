/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
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

} // namespace

void computer_world::RegisterWorshipRules(RuleTable& table)
{
	constexpr auto k_Worship = MindDomain::Worship;
	AddRule(table, k_Worship, NodeKind::ActionWorshipSiteThatNeedsVillagers, {.intensity = SiteNeedingVillagers});
	AddRule(table, k_Worship, NodeKind::ObjectTakeVillagerFromTownToWorshipSite,
	        {.intensity = VillagerForSite, .perform = CarryToSite});
}
