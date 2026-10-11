/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerWorship.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/VillagerCarry.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "ECS/WorshipperRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBuild.h"
#include "VillagerDance.h"
#include "VillagerDisciples.h"
#include "VillagerFire.h"
#include "VillagerFood.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_worship = openblack::ecs::villager_worship;
namespace rules = openblack::ecs::worshipper_rules;
using SitePlace = openblack::ecs::systems::WorshipSiteSystemInterface::SitePlace;

namespace
{
/// What ProcessInWorship says when the worshipper has gone off (home, to decide, to eat) or died
constexpr uint32_t k_LeftWorship = 0x23;

/// The states that share the way to the site, the site's dance and hiding place, its pot and the supplies' errands:
/// moving between states of one of these keeps the villager as it was
constexpr std::array k_WayToSiteStates {VillagerStates::GotoWorshipSiteForWorship,
                                        VillagerStates::ArrivesAtWorshipSiteForWorship};
constexpr std::array k_AtSiteStates {VillagerStates::WorshippingAtWorshipSite, VillagerStates::HidingAtWorshipSite};
constexpr std::array k_AtPotStates {VillagerStates::RestartWorshippingAtWorshipSite, VillagerStates::GetFoodAtWorshipSite};
constexpr std::array k_SupplyStates {
    VillagerStates::GotoStoragePitForWorshipSupplies, VillagerStates::ArrivesAtStoragePitForWorshipSupplies,
    VillagerStates::GotoWorshipSiteWithSupplies,      VillagerStates::MoveToWorshipSiteWithSupplies,
    VillagerStates::ArrivesAtWorshipSiteWithSupplies,
};
/// Going through a teleport stone on the way keeps a villager on its way to the site
constexpr std::array k_TeleportStates {VillagerStates::GoTowardsTeleportReaction, VillagerStates::TeleportReaction,
                                       VillagerStates::GoTowardsTeleportReactionQuickly};

template <size_t N>
bool Among(const std::array<VillagerStates, N>& states, VillagerStates state)
{
	return std::ranges::find(states, state) != states.end();
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

entt::entity TownOf(entt::entity villager)
{
	const auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || person->town == entt::null || !registry.Valid(person->town) || !registry.AllOf<Town>(person->town))
	{
		return entt::null;
	}
	return person->town;
}

entt::entity SiteOfTown(entt::entity town)
{
	const auto& registry = Entities();
	if (town == entt::null)
	{
		return entt::null;
	}
	const auto site = registry.Get<const Town>(town).worshipSite;
	return site != entt::null && registry.Valid(site) && registry.AllOf<WorshipSite>(site) ? site : entt::null;
}

systems::WorshipSiteSystemInterface& Sites()
{
	return Locator::worshipSiteSystem::value();
}

glm::vec2 PositionOf(entt::entity entity)
{
	return glm::xz(Entities().Get<const Transform>(entity).position);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

VillagerStates TopOf(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top);
}

VillagerStates FinalOf(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetFinalState(action);
}

/// Walks to a point, round what is in the way, and on into a state. Whether the walk was set up.
bool MoveTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	if (!Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, final))
	{
		return false;
	}
	villager_home::SetupMobileMoveTo(action, goal, final);
	return true;
}

/// Walks to a point of a site, on its footpaths where there are any
bool MoveOnPathsTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	// TODO(villagers): along the footpath from the town to the site; villagers don't walk footpaths yet
	return MoveTo(action, goal, final);
}

/// Whether the villager has got to a point, as near as its step and some slack in the map's whole units
bool AreWeThere(entt::entity villager, glm::vec2 point, float slack)
{
	const auto& registry = Entities();
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	const auto speed = static_cast<float>(wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f));
	const auto at = map_coords::FromMetres(PositionOf(villager));
	const auto goal = map_coords::FromMetres(point);
	const auto dx = static_cast<float>(at.x - goal.x);
	const auto dz = static_cast<float>(at.z - goal.z);
	const float reach = speed + slack;
	return dx * dx + dz * dz < reach * reach;
}

/// The town centre of a town, standing and built, if it has one
bool TownCentreStands(entt::entity town)
{
	const auto& registry = Entities();
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		const auto* data = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (data == nullptr || data->type != AbodeNumber::TownCentre)
		{
			continue;
		}
		const auto* progress = registry.TryGet<const BuildProgress>(abode);
		return progress == nullptr || progress->built >= 1.0f;
	}
	return false;
}

/// Whether a town's people can worship at a site: it has one, a standing town centre, and the site is its player's
bool TownCanWorshipAt(entt::entity town, entt::entity site)
{
	return site != entt::null && TownCentreStands(town) &&
	       Entities().Get<const WorshipSite>(site).player == Entities().Get<const Town>(town).owner;
}

Worshipper& WorshipperOf(entt::entity villager)
{
	auto& registry = Entities();
	return registry.AllOf<Worshipper>(villager) ? registry.Get<Worshipper>(villager) : registry.Assign<Worshipper>(villager);
}

bool IsCountedAtWorship(entt::entity villager)
{
	const auto* worshipper = Entities().TryGet<const Worshipper>(villager);
	return worshipper != nullptr && worshipper->atWorship;
}

/// A worshipper with nothing left to mark loses its mark
void TidyWorshipper(entt::entity villager)
{
	auto& registry = Entities();
	if (const auto* worshipper = registry.TryGet<const Worshipper>(villager);
	    worshipper != nullptr && !worshipper->atWorship && !worshipper->onTheWay)
	{
		registry.Remove<Worshipper>(villager);
	}
}

/// The site's dance, if it has one
Dance* DanceOfSite(entt::entity site)
{
	auto& registry = Entities();
	const auto dance = registry.Get<const WorshipSite>(site).dance;
	return dances::IsDance(registry, dance) ? &registry.Get<Dance>(dance) : nullptr;
}

/// The villager is counted at worship by its town, and among its site's worshippers
void AddToWorshipSite(entt::entity villager)
{
	auto& worshipper = WorshipperOf(villager);
	const auto town = TownOf(villager);
	if (!worshipper.atWorship)
	{
		if (town != entt::null)
		{
			auto& counted = Entities().Get<Town>(town).worshippers;
			if (std::ranges::find(counted, villager) == counted.end())
			{
				counted.push_back(villager);
			}
		}
		worshipper.atWorship = true;
	}
	if (const auto site = SiteOfTown(town); site != entt::null)
	{
		Sites().AddWorshipper(site, villager);
	}
}

/// The villager is no longer counted at worship, nor among its site's worshippers, and leaves its dance
void RemoveFromWorshipSite(entt::entity villager)
{
	auto& registry = Entities();
	if (const auto town = TownOf(villager); town != entt::null)
	{
		if (IsCountedAtWorship(villager))
		{
			std::erase(registry.Get<Town>(town).worshippers, villager);
		}
		if (const auto site = SiteOfTown(town); site != entt::null && Sites().IsWorshipper(site, villager))
		{
			Sites().RemoveWorshipper(site, villager);
		}
		dances::RemoveDancer(registry, villager);
	}
	if (auto* worshipper = registry.TryGet<Worshipper>(villager))
	{
		worshipper->atWorship = false;
	}
	TidyWorshipper(villager);
}

/// The villager stops being on its way to the site
void LeaveTheWay(entt::entity villager)
{
	auto& registry = Entities();
	if (const auto town = TownOf(villager); town != entt::null)
	{
		std::erase(registry.Get<Town>(town).onWayToWorship, villager);
	}
	if (auto* worshipper = registry.TryGet<Worshipper>(villager))
	{
		worshipper->onTheWay = false;
	}
	TidyWorshipper(villager);
}

/// Whether a villager is free to be sent to worship: not held by a script, in a state it may be taken from, and not a
/// disciple unless disciples may go, and not already worshipping or on its way
bool IsAvailableForWorshipSite(entt::entity villager, bool disciplesToo)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr || registry.AllOf<ScriptControlled>(villager))
	{
		return false;
	}
	if ((StateInfo(FinalOf(*action)).field0xa8 & 1) == 0)
	{
		return false;
	}
	if (!disciplesToo && villager_disciple::TypeOf(villager) != VillagerDisciple::None)
	{
		return false;
	}
	return !villager_worship::IsAtOrOnTheWayToWorshipSite(villager);
}

/// The town's people in the order the town goes through them: those in its buildings, then its homeless
std::vector<entt::entity> PeopleOf(entt::entity town)
{
	const auto& registry = Entities();
	const auto& data = registry.Get<const Town>(town);
	std::vector<entt::entity> people;
	for (const auto abode : data.abodes)
	{
		if (const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr)
		{
			people.insert(people.end(), building->inhabitants.begin(), building->inhabitants.end());
		}
	}
	people.insert(people.end(), data.homelessVillagers.begin(), data.homelessVillagers.end());
	std::erase_if(people, [&registry](entt::entity villager) {
		return !registry.Valid(villager) || !registry.AllOf<Villager, LivingAction, Transform>(villager);
	});
	return people;
}

/// How fit each villager is to go to worship, or to be called back first
std::vector<float> KeysOf(entt::entity town, const std::vector<entt::entity>& people)
{
	std::vector<float> keys;
	keys.reserve(people.size());
	const auto site = SiteOfTown(town);
	const auto centre = site != entt::null ? Sites().PlaceAt(site, SitePlace::Centre) : std::nullopt;
	for (const auto villager : people)
	{
		if (!centre.has_value())
		{
			keys.push_back(0.0f);
			continue;
		}
		const float villagerToSite =
		    gutils::GetDistanceInMetres(map_coords::FromMetres(PositionOf(villager)), map_coords::FromMetres(*centre));
		const float townToSite =
		    gutils::GetDistanceInMetres(map_coords::FromMetres(PositionOf(town)), map_coords::FromMetres(*centre));
		keys.push_back(rules::CandidateKey(villagerToSite, townToSite, world_objects::LifeOf(villager)));
	}
	return keys;
}

/// A teleport stone of its player's the villager may go through to reach a site too far to walk to, if any
std::optional<entt::entity> StoneToSite(entt::entity villager, entt::entity site, entt::entity town)
{
	if (!Locator::teleportSystem::has_value())
	{
		return std::nullopt;
	}
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceThatVillagersWillGoToWorship;
	const auto& registry = Entities();
	return Locator::teleportSystem::value().RouteStoneFor(registry.Get<const Town>(town).owner,
	                                                      registry.Get<const Transform>(villager).position,
	                                                      registry.Get<const Transform>(site).position, maxDistance);
}

/// Whether the villager can get to its site: walking, or through a stone it is to go through
bool CanGetToTheWorshipSite(entt::entity villager, std::optional<entt::entity>& stone)
{
	const auto town = TownOf(villager);
	const auto site = SiteOfTown(town);
	if (town == entt::null || site == entt::null)
	{
		return true;
	}
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceThatVillagersWillGoToWorship;
	const float distance =
	    gutils::GetDistanceInMetres(map_coords::FromMetres(PositionOf(villager)), map_coords::FromMetres(PositionOf(site)));
	if (distance > maxDistance)
	{
		stone = StoneToSite(villager, site, town);
	}
	// Every town has a player here, the neutral player among them
	return rules::CanReachSite(distance, maxDistance, true, stone.has_value());
}

/// A villager whose site is still going up helps build it. Whether it took that up.
bool CheckNeededForWorshipSiteBuilding(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	const auto site = SiteOfTown(town);
	if (site == entt::null)
	{
		return false;
	}
	const auto& requests = Entities().Get<const WorshipSite>(site).buildRequests;
	const bool asked = std::ranges::any_of(requests, [town](const WorshipSite::BuildRequest& r) { return r.town == town; });
	if (!asked || Sites().IsBuilt(site))
	{
		return false;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return villager_build::TakeUpSite(action, site) == 1;
}

/// Where a worshipper eats: the edge of the site's pot nearest it, or the pot's own place
glm::vec2 PotEdge(entt::entity site, entt::entity villager)
{
	const auto& registry = Entities();
	const auto pot = registry.Get<const WorshipSite>(site).foodPot;
	if (pot == entt::null || !registry.Valid(pot) || !registry.AllOf<Transform>(pot))
	{
		return PositionOf(site);
	}
	const auto middle = PositionOf(pot);
	const auto towards = PositionOf(villager) - middle;
	const float length = glm::length(towards);
	if (length <= 0.0f)
	{
		return middle;
	}
	const float reach = std::min(world_objects::SizeOf(pot).radius, length);
	return middle + towards / length * reach;
}

/// Whether a hungry worshipper eats at its site, walking to its pot when asked. Whether it eats there.
bool CheckAllowedToRestAtWorshipSite(LivingAction& action, bool moveToFood)
{
	const auto villager = EntityOf(action);
	const auto& person = Entities().Get<const Villager>(villager);
	const auto site = SiteOfTown(TownOf(villager));
	const auto siteFood = site != entt::null ? Sites().FoodInPot(site) : 0u;
	if (!rules::EatsAtSite(person.food, InfoOf(person).hungryForFood, siteFood, villager_food::FoodRequiredForMeal(villager)))
	{
		return false;
	}
	if (moveToFood && site != entt::null)
	{
		MoveTo(action, PotEdge(site, villager), VillagerStates::GetFoodAtWorshipSite);
	}
	return true;
}

/// Too many of its town worship, or it can't worship there any more: it goes. Whether it went.
bool CheckVillagerGoBackToTownFromWorship(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return false;
	}
	const auto site = SiteOfTown(town);
	// The site lets each go in turn as they ask; nothing asks in this version of the game, so any may go
	constexpr bool k_MayGoHome = true;
	switch (rules::CheckGoingBack(TownCanWorshipAt(town, site), villager_worship::WorshippersNeeded(town, false, false).count,
	                              k_MayGoHome))
	{
	case rules::GoingBack::Decide:
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return true;
	case rules::GoingBack::GoHome:
		SetTopState(action, VillagerStates::GoHomeFromWorship);
		return TopOf(action) == VillagerStates::GoHomeFromWorship;
	case rules::GoingBack::Stay:
		break;
	}
	return false;
}

/// Chanting costs the worshipper life; with none left it dies of it. Whether it died.
bool ReduceVillagerLifeByChant(entt::entity villager)
{
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null)
	{
		return false;
	}
	const auto& person = Entities().Get<const Villager>(villager);
	const float damage = rules::ChantDamage(Sites().ChantsPerDancer(site), InfoOf(person).chantLifeRate);
	world_objects::ReduceLife(villager, damage);
	if (world_objects::LifeOf(villager) > 0.0f)
	{
		return false;
	}
	const auto town = TownOf(villager);
	std::optional<PlayerNames> killer;
	if (town != entt::null)
	{
		killer = Entities().Get<const Town>(town).owner;
	}
	villager_fire::DieByEffect(villager,
	                           villager_fire::DeathCause {.reason = DeathReason::Chant, .killer = killer, .weight = damage});
	return true;
}

/// A worshipper's turn at the site: it may go home, it pays for its chanting, and it may go to eat. 1 when it worships
/// on.
uint32_t ProcessInWorship(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (CheckVillagerGoBackToTownFromWorship(action))
	{
		return k_LeftWorship;
	}
	if (ReduceVillagerLifeByChant(villager))
	{
		return k_LeftWorship;
	}
	return CheckAllowedToRestAtWorshipSite(action, true) ? k_LeftWorship : 1;
}

/// The villager joins its site's dance and walks to its place in it, counted at worship. Whether it did.
bool StartWorshippingAtWorshipSite(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null || DanceOfSite(site) == nullptr)
	{
		return false;
	}
	const auto dance = registry.Get<const WorshipSite>(site).dance;
	if (dances::DanceOf(registry, villager) != dance &&
	    !dances::AddDancer(registry, dance, villager, villager_dance::DanceSex(villager)))
	{
		return false;
	}
	const auto slot = villager_dance::SlotOf(villager);
	if (!slot.has_value() || !MoveTo(action, *slot, VillagerStates::WorshippingAtWorshipSite))
	{
		return false;
	}
	AddToWorshipSite(villager);
	return true;
}

/// The dance is full: the villager walks to the site's door to worship hidden there, counted at worship. Whether it did.
uint32_t StartHidingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null)
	{
		return villager_home::DecideWhatToDo(action);
	}
	const auto door = Sites().PlaceAt(site, SitePlace::Door);
	if (!door.has_value() || !MoveOnPathsTo(action, *door, VillagerStates::HidingAtWorshipSite))
	{
		return 0;
	}
	AddToWorshipSite(villager);
	return 1;
}
} // namespace

rules::Headcount villager_worship::HeadcountOf(entt::entity town)
{
	const auto& registry = Entities();
	const auto& data = registry.Get<const Town>(town);
	uint32_t population = 0;
	if (const auto* stats = registry.TryGet<const TownStats>(town))
	{
		population = stats->adults + stats->children;
	}
	// The site lets its worshippers go home in turn as they ask; nothing asks in this version of the game
	return {.population = population,
	        .worshipping = static_cast<uint32_t>(data.worshippers.size()),
	        .onTheWay = static_cast<uint32_t>(data.onWayToWorship.size()),
	        .askingToGoHome = 0,
	        .share = data.worshipShare};
}

rules::Needed villager_worship::WorshippersNeeded(entt::entity town, bool countOnTheWay, bool makeUpForThoseGoingHome)
{
	return rules::WorshippersNeeded(HeadcountOf(town), countOnTheWay, makeUpForThoseGoingHome);
}

void villager_worship::AdjustWorshippers(entt::entity town, int32_t count, bool ignoreLowLife, bool needReach)
{
	auto& registry = Entities();
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town) || !Locator::infoConstants::has_value())
	{
		return;
	}
	// Two goes: the first leaves disciples at their work, the second takes them too
	for (const bool disciplesToo : {false, true})
	{
		if (count > 0)
		{
			auto people = PeopleOf(town);
			std::erase_if(people,
			              [disciplesToo](entt::entity villager) { return !IsAvailableForWorshipSite(villager, disciplesToo); });
			const auto keys = KeysOf(town, people);
			for (const auto index : rules::CandidateOrder(keys, true))
			{
				if (count == 0)
				{
					break;
				}
				const auto villager = people.at(index);
				if (!registry.Valid(villager) || !registry.AllOf<LivingAction>(villager))
				{
					continue;
				}
				const auto& person = registry.Get<const Villager>(villager);
				if (rules::LifeAllowsWorship(ignoreLowLife, world_objects::LifeOf(villager),
				                             InfoOf(person).damageThresholdToGoHome) &&
				    CheckWorshipActivity(registry.Get<LivingAction>(villager), needReach))
				{
					--count;
				}
			}
		}
		else if (count < 0)
		{
			auto people = PeopleOf(town);
			std::erase_if(people, [](entt::entity villager) { return !IsAtOrOnTheWayToWorshipSite(villager); });
			const auto keys = KeysOf(town, people);
			for (const auto index : rules::CandidateOrder(keys, false))
			{
				if (count == 0)
				{
					break;
				}
				const auto villager = people.at(index);
				if (!registry.Valid(villager) || !registry.AllOf<LivingAction>(villager))
				{
					continue;
				}
				auto& action = registry.Get<LivingAction>(villager);
				SetTopState(action, VillagerStates::DecideWhatToDo);
				if (villager_home::DecideWhatToDo(action) == 1)
				{
					++count;
				}
			}
		}
	}
}

void villager_worship::ShareSet(entt::entity town)
{
	const auto needed = WorshippersNeeded(town, true, true).count;
	if (needed != 0)
	{
		AdjustWorshippers(town, needed, true, false);
	}
}

void villager_worship::TownTurn(entt::entity town, uint32_t turn)
{
	constexpr uint32_t k_TopUpEvery = 10;
	if (turn % k_TopUpEvery != 0)
	{
		return;
	}
	const auto needed = WorshippersNeeded(town, true, false).count;
	if (needed > 0)
	{
		AdjustWorshippers(town, needed, true, false);
	}
}

void villager_worship::Forget(entt::entity villager)
{
	auto& registry = Entities();
	registry.Each<Town>([villager](Town& town) {
		std::erase(town.worshippers, villager);
		std::erase(town.onWayToWorship, villager);
	});
	registry.Each<WorshipSite>([villager](WorshipSite& site) { std::erase(site.worshippers, villager); });
}

bool villager_worship::IsAtOrOnTheWayToWorshipSite(entt::entity villager)
{
	const auto& registry = Entities();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return false;
	}
	auto state = FinalOf(*action);
	// Through a teleport stone it is still on the way it was going
	if (Among(k_TeleportStates, state))
	{
		state = Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Previous);
	}
	return IsCountedAtWorship(villager) || state == VillagerStates::ArrivesAtWorshipSiteForWorship ||
	       state == VillagerStates::ArrivesAtWorshipSiteWithSupplies;
}

entt::entity villager_worship::WorshipSiteOf(entt::entity villager)
{
	// TODO(worship): a villager of no town worships at its player's temple's site of its tribe; openblack's townless
	// villagers have no player
	return SiteOfTown(TownOf(villager));
}

uint32_t villager_worship::CheckNeededForWorship(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (IsCountedAtWorship(villager))
	{
		// Counted at worship already, it goes back to the dance, or stops worshipping when it can't
		if (StartWorshippingAtWorshipSite(action))
		{
			return 1;
		}
		RemoveFromWorshipSite(villager);
		return 0;
	}
	const auto town = TownOf(villager);
	if (town == entt::null || Entities().Get<const Town>(town).worshipShare == 0.0f)
	{
		return 0;
	}
	const auto needed = WorshippersNeeded(town, true, true);
	if (needed.count > 0)
	{
		return CheckWorshipActivity(action, needed.alreadyEnough) ? 1 : 0;
	}
	return 0;
}

bool villager_worship::CheckWorshipActivity(LivingAction& action, bool needReach)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	const auto site = SiteOfTown(town);
	if (site == entt::null || town == entt::null || !TownCentreStands(town) ||
	    Entities().Get<const WorshipSite>(site).player != Entities().Get<const Town>(town).owner)
	{
		return false;
	}
	std::optional<entt::entity> stone;
	if (!CanGetToTheWorshipSite(villager, stone) && needReach)
	{
		return false;
	}
	bool sent = CheckNeededForWorshipSiteBuilding(action);
	if (!sent)
	{
		sent = GotoWorshipSiteForWorship(action) == 1;
	}
	if (sent && stone.has_value() && Locator::teleportSystem::has_value())
	{
		// Too far to walk, it goes through the stone on its way
		Locator::teleportSystem::value().SetupReact(*stone, villager);
	}
	return sent;
}

uint32_t villager_worship::GotoWorshipSiteForWorship(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	const auto site = SiteOfTown(town);
	uint32_t result = 0;
	if (site != entt::null && Sites().IsBuilt(site))
	{
		// The site's dance waits for those on their way
		if (auto* dance = DanceOfSite(site))
		{
			++dance->onTheirWay;
		}
		WorshipperOf(villager).onTheWay = true;
		const auto gate = Sites().PlaceAt(site, SitePlace::Gate).value_or(PositionOf(site));
		if (gutils::GetDistanceInMetres(map_coords::FromMetres(PositionOf(villager)), map_coords::FromMetres(gate)) > 0.0f)
		{
			MoveOnPathsTo(action, gate, VillagerStates::ArrivesAtWorshipSiteForWorship);
		}
		else
		{
			SetTopState(action, VillagerStates::ArrivesAtWorshipSiteForWorship);
		}
		if (town != entt::null)
		{
			auto& onTheWay = registry.Get<Town>(town).onWayToWorship;
			if (std::ranges::find(onTheWay, villager) == onTheWay.end())
			{
				onTheWay.push_back(villager);
			}
		}
		result = 1;
	}
	// A worship disciple has done its work once it sets off; the town's share grows to keep it worshipping
	if (villager_disciple::TypeOf(villager) == VillagerDisciple::Worship)
	{
		villager_disciple::SetType(villager, VillagerDisciple::None);
		if (town != entt::null && result == 1)
		{
			const auto headcount = HeadcountOf(town);
			const auto share = rules::ShareAfterDisciple(rules::WorshippersNeeded(headcount, true, true).count,
			                                             headcount.worshipping, headcount.onTheWay, headcount.population);
			if (share.has_value() && Locator::villageTotemSystem::has_value())
			{
				Locator::villageTotemSystem::value().SetTownShare(town, *share);
			}
		}
	}
	return result;
}

uint32_t villager_worship::ArrivesAtWorshipSiteForWorship(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null)
	{
		return villager_home::DecideWhatToDo(action);
	}
	const auto gate = Sites().PlaceAt(site, SitePlace::Gate).value_or(PositionOf(site));
	const float distance =
	    gutils::GetDistanceInMetres(map_coords::FromMetres(PositionOf(villager)), map_coords::FromMetres(gate));
	const auto* dance = DanceOfSite(site);
	const auto tribe = static_cast<size_t>(Entities().Get<const WorshipSite>(site).tribe);
	const auto maxVisible = Locator::infoConstants::value().worshipSite.at(tribe).maxDancersVisible;
	switch (rules::OnReachingGate(distance, dance != nullptr ? dance->dancers : 0u, maxVisible))
	{
	case rules::Arrival::Dance:
		StartWorshippingAtWorshipSite(action);
		break;
	case rules::Arrival::Hide:
		StartHidingAtWorshipSite(action);
		break;
	case rules::Arrival::WalkOn:
		MoveOnPathsTo(action, gate, VillagerStates::ArrivesAtWorshipSiteForWorship);
		break;
	}
	return 1;
}

uint32_t villager_worship::WorshippingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (auto* worshipper = Entities().TryGet<Worshipper>(villager); worshipper != nullptr && worshipper->onTheWay)
	{
		// On its first turn in its place it is no longer waited for
		worshipper->onTheWay = false;
		if (const auto site = SiteOfTown(TownOf(villager)); site != entt::null)
		{
			if (auto* dance = DanceOfSite(site); dance != nullptr && dance->onTheirWay > 0)
			{
				--dance->onTheirWay;
			}
		}
	}
	const auto result = ProcessInWorship(action);
	if (result != 1)
	{
		return result;
	}
	return villager_dance::PerformDance(action, VillagerStates::WorshippingAtWorshipSite);
}

uint32_t villager_worship::HidingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto door = Sites().PlaceAt(site, SitePlace::Door).value_or(PositionOf(site));
	if (AreWeThere(villager, door, rules::k_HidingReach))
	{
		return ProcessInWorship(action);
	}
	MoveTo(action, door, VillagerStates::HidingAtWorshipSite);
	return 1;
}

uint32_t villager_worship::RestartWorshippingAtWorshipSite(LivingAction& action)
{
	if (!StartWorshippingAtWorshipSite(action))
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t villager_worship::GetFoodAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto edge = PotEdge(site, villager);
	if (!AreWeThere(villager, edge, 0.0f))
	{
		MoveTo(action, edge, VillagerStates::GetFoodAtWorshipSite);
		return 1;
	}
	if (const auto meal = villager_food::FoodRequiredForMeal(villager); meal != 0)
	{
		villager_food::GetResourceFrom(villager, site, ResourceType::Food, static_cast<uint16_t>(meal));
	}
	villager_food::EatFoodHeld(villager);
	Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::RestartWorshippingAtWorshipSite);
	return 1;
}

uint32_t villager_worship::GotoAltarForRest(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto site = SiteOfTown(TownOf(villager));
	if (site == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	MoveOnPathsTo(action, Sites().PlaceAt(site, SitePlace::Rest).value_or(PositionOf(site)),
	              VillagerStates::ArrivesAtAltarForRest);
	return 1;
}

uint32_t villager_worship::ArrivesAtAltarForRest(LivingAction& action)
{
	if (!CheckAllowedToRestAtWorshipSite(action, false))
	{
		return AtAltarFinishedRest(action);
	}
	Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::AtAltarRest);
	return 1;
}

uint32_t villager_worship::AtAltarRest(LivingAction& action)
{
	return ArrivesAtAltarForRest(action);
}

uint32_t villager_worship::AtAltarFinishedRest(LivingAction& action)
{
	if (StartWorshippingAtWorshipSite(action))
	{
		return 1;
	}
	return villager_home::DecideWhatToDo(action);
}

uint32_t villager_worship::GoHomeFromWorship(LivingAction& action)
{
	return villager_home::DoGoingHome(action, VillagerStates::ArrivesHomeFromWorship, VillagerStates::SleepInTentFromWorship);
}

uint32_t villager_worship::CheckSatisfySupplyWorship(LivingAction& action)
{
	return GotoStoragePitForWorshipSupplies(action) == 1 ? 1 : 0;
}

namespace
{
/// The food a villager's town would send its site now, none when it can't spare it all
int32_t FoodForSite(entt::entity villager)
{
	const auto town = TownOf(villager);
	const auto site = SiteOfTown(town);
	if (site == entt::null)
	{
		return 0;
	}
	const auto pit = construction::StoragePitOf(town);
	std::optional<uint32_t> pitFood;
	if (pit.has_value() && Locator::resourceStoreSystem::has_value())
	{
		pitFood = Locator::resourceStoreSystem::value().GetResource(*pit, ResourceType::Food);
	}
	const auto* stats = Entities().TryGet<const TownStats>(town);
	const auto spare = rules::FoodSpareForWorship(pitFood, stats != nullptr ? stats->foodForDinner : 0.0f);
	return rules::FoodForSiteIfEnough(Sites().FoodNeededFromTown(site), spare);
}
} // namespace

uint32_t villager_worship::GotoStoragePitForWorshipSupplies(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	const auto pit = construction::StoragePitOf(town);
	if (!pit.has_value() || !villager_home::IsFunctional(*pit) || SiteOfTown(town) == entt::null)
	{
		return 0;
	}
	const auto& person = Entities().Get<const Villager>(villager);
	const auto need = FoodForSite(villager);
	if (need == 0)
	{
		return 0;
	}
	const auto held = static_cast<int32_t>(person.foodHeld);
	const auto most = static_cast<int32_t>(InfoOf(person).maxFoodCarried);
	if (static_cast<uint32_t>(held) > static_cast<uint32_t>(need) || held == most)
	{
		return GotoWorshipSiteWithSupplies(action);
	}
	if (Locator::resourceStoreSystem::value().GetResource(*pit, ResourceType::Food) == 0)
	{
		return 0;
	}
	// TODO(villagers): to the edge of the pit's food nearest it, along the footpaths; the pit's door for now
	MoveTo(action, villager_home::ArrivePosition(*pit), VillagerStates::ArrivesAtStoragePitForWorshipSupplies);
	return 1;
}

uint32_t villager_worship::ArrivesAtStoragePitForWorshipSupplies(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (SiteOfTown(TownOf(villager)) != entt::null)
	{
		auto need = FoodForSite(villager);
		if (need != 0)
		{
			const auto& person = Entities().Get<const Villager>(villager);
			if (person.foodHeld >= need)
			{
				return GotoWorshipSiteWithSupplies(action);
			}
			const auto room = static_cast<int32_t>(InfoOf(person).maxFoodCarried) - person.foodHeld;
			need = std::min(need, room);
			if (need > 0)
			{
				return villager_build::ArrivesAtStoragePitForResource(action, ResourceType::Food, static_cast<uint32_t>(need),
				                                                      VillagerStates::InvalidState,
				                                                      VillagerStates::DecideWhatToDo);
			}
		}
	}
	return villager_home::DecideWhatToDo(action);
}

uint32_t villager_worship::GotoWorshipSiteWithSupplies(LivingAction& /*action*/)
{
	// The game never sets off for the site with the food: whoever would gives up here
	return 0;
}

uint32_t villager_worship::MoveToWorshipSiteWithSupplies(LivingAction& /*action*/)
{
	return 1;
}

uint32_t villager_worship::ArrivesAtWorshipSiteWithSupplies(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (const auto site = SiteOfTown(TownOf(villager)); site != entt::null)
	{
		const auto held = Entities().Get<const Villager>(villager).foodHeld;
		if (held != 0 && Locator::resourceStoreSystem::has_value())
		{
			// It puts all it carries into the site's pot
			auto& stores = Locator::resourceStoreSystem::value();
			const auto added = stores.AddResource(site, ResourceType::Food, static_cast<uint32_t>(held), std::nullopt);
			villager_carry::PutDown(Entities().Get<Villager>(villager), ResourceType::Food, static_cast<uint16_t>(added));
			Sites().RemoveFoodOnTheWay(site, held);
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_worship::ExitMoveToWorshipSite(LivingAction& action, VillagerStates next)
{
	if (!Among(k_WayToSiteStates, next) && !Among(k_TeleportStates, next))
	{
		LeaveTheWay(EntityOf(action));
	}
	return false;
}

bool villager_worship::ExitAtWorshipSite(LivingAction& action, VillagerStates next)
{
	if (Among(k_AtSiteStates, next))
	{
		return false;
	}
	const auto villager = EntityOf(action);
	if (StateInfo(next).isReactionState == 0 && next != VillagerStates::GetFoodAtWorshipSite)
	{
		RemoveFromWorshipSite(villager);
		return false;
	}
	// Reacting to something or going to eat, it is still counted at worship, but out of the dance
	if (const auto site = SiteOfTown(TownOf(villager)); site != entt::null)
	{
		Sites().RemoveWorshipper(site, villager);
	}
	dances::RemoveDancer(Entities(), villager);
	return false;
}

bool villager_worship::ExitGetFoodAtWorship(LivingAction& action, VillagerStates next)
{
	// The states marked as keeping a villager at the site keep it counted at worship
	if (!Among(k_AtPotStates, next) && StateInfo(next).woundedStays == 0)
	{
		RemoveFromWorshipSite(EntityOf(action));
	}
	return false;
}

bool villager_worship::ExitMoveToWorshipSiteWithSupplies(LivingAction& action, VillagerStates next)
{
	if (!Among(k_SupplyStates, next))
	{
		const auto villager = EntityOf(action);
		if (const auto site = SiteOfTown(TownOf(villager)); site != entt::null)
		{
			Sites().RemoveFoodOnTheWay(site, Entities().Get<const Villager>(villager).foodHeld);
		}
	}
	return false;
}
