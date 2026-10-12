/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerDiscipleWork.h"

#include <optional>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DiscipleWorkRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerWorkRules.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBuild.h"
#include "VillagerDisciples.h"
#include "VillagerFootpath.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_disciple_work = openblack::ecs::villager_disciple_work;

namespace
{
ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

Villager& VillagerOf(LivingAction& action)
{
	return Entities().Get<Villager>(EntityOf(action));
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

glm::vec2 GroundOf(entt::entity entity)
{
	return glm::xz(Entities().Get<const Transform>(entity).position);
}

std::optional<entt::entity> ValidTown(entt::entity town)
{
	const auto& registry = Entities();
	return town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town) ? std::optional(town) : std::nullopt;
}

const GVillagerInfo* VillagerInfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	const auto row = static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number));
	return row < infos.size() ? &infos.at(row) : nullptr;
}

bool IsFunctional(std::optional<entt::entity> building)
{
	return building.has_value() && Entities().Valid(*building) && villager_home::IsFunctional(*building);
}

/// Whether the villager has got to a point, as near as its step in whole units
bool AreWeThere(entt::entity villager, glm::vec2 point)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	const auto speed = static_cast<float>(wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f));
	return villager_work::AreWeThere(map_coords::FromMetres(GroundOf(villager)), map_coords::FromMetres(point), speed, 0.0f);
}

/// The villager's larger load (wood when as much as food) and how much of it there is, none when it carries nothing
std::pair<ResourceType, uint32_t> LargerLoad(const Villager& person)
{
	if (person.woodHeld < person.foodHeld)
	{
		return {ResourceType::Food, person.foodHeld};
	}
	if (person.woodHeld != 0)
	{
		return {ResourceType::Wood, person.woodHeld};
	}
	return {ResourceType::None, 0};
}

/// Where a town takes in what is brought to it: its working store's door
glm::vec2 DropOffPosition(entt::entity villager, std::optional<entt::entity> town)
{
	if (town.has_value())
	{
		if (const auto pit = construction::StoragePitOf(*town); IsFunctional(pit))
		{
			return villager_home::ArrivePosition(*pit);
		}
	}
	// TODO(disciples): a town without a working store takes things in at its temporary pot, which openblack lacks
	return GroundOf(villager);
}

/// With nothing to carry, the trader waits by the centre of the town it trades with
uint32_t SetTraderNothingToDo(LivingAction& action, entt::entity tradeTown)
{
	return villager_disciple::SetDiscipleNothingToDo(action, tradeTown) ? 1 : 0;
}

/// A town's workshops in its order, the newest first
std::vector<entt::entity> WorkshopsOf(entt::entity town)
{
	std::vector<entt::entity> workshops;
	auto& registry = Entities();
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (building != nullptr && building->type == AbodeNumber::Workshop)
		{
			workshops.push_back(abode);
		}
	}
	return workshops;
}

/// The wood a workshop needs to make a scaffold
uint32_t ScaffoldWood()
{
	return Locator::infoConstants::value().scaffold.woodValue;
}

/// The workshop of the villager's town a craftsman supplies, none when none wants wood or the villager has no town
std::optional<entt::entity> BestWorkshop(entt::entity villager)
{
	const auto town = ValidTown(Entities().Get<const Villager>(villager).town);
	if (!town.has_value())
	{
		return std::nullopt;
	}
	const auto workshops = WorkshopsOf(*town);
	std::vector<disciple_work::WorkshopFacts> facts;
	facts.reserve(workshops.size());
	const auto here = GroundOf(villager);
	for (const auto workshop : workshops)
	{
		// TODO(disciples): a workshop's scaffolds and their making belong to the workshop, which openblack lacks; until it
		// has them a workshop holds none and makes none, so always has room
		const auto& abode = Entities().Get<const Abode>(workshop);
		facts.push_back({.metresAway = gutils::GetDistanceInMetres(here, GroundOf(workshop)),
		                 .wantsWood = disciple_work::WorkshopWantsWood(disciple_work::WorkshopRoom(false, 0), abode.woodAmount,
		                                                               ScaffoldWood()),
		                 .functional = villager_home::IsFunctional(workshop)});
	}
	const auto best = disciple_work::BestWorkshop(facts);
	return best.has_value() ? std::optional(workshops.at(*best)) : std::nullopt;
}

bool IsAvailable(entt::entity villager)
{
	auto& registry = Entities();
	if (!registry.Valid(villager) || !registry.AllOf<Villager, LivingAction>(villager) ||
	    registry.AnyOf<InHand, InPhysics, CarriedByTornado, VillagerDeath>(villager))
	{
		return false;
	}
	return Locator::livingActionSystem::value().VillagerGetFinalState(registry.Get<const LivingAction>(villager)) !=
	       VillagerStates::Dying;
}

/// A missionary stops preaching: freed if still one, its reaction ended
void StopPreaching(entt::entity control)
{
	auto& registry = Entities();
	const auto preaching = registry.Get<const MissionaryControl>(control);
	if (registry.Valid(preaching.villager) && villager_disciple::TypeOf(preaching.villager) == VillagerDisciple::Missionary)
	{
		villager_disciple::SetType(preaching.villager, VillagerDisciple::None);
	}
	if (Locator::reactionSystem::has_value() && Locator::reactionSystem::value().IsActive(preaching.reaction))
	{
		Locator::reactionSystem::value().Remove(preaching.reaction);
	}
	registry.Destroy(control);
}

/// A missionary starts preaching in a town for a player: its reaction follows it about, impressing the villagers near it
void StartPreaching(entt::entity villager, entt::entity town, PlayerNames player)
{
	auto& registry = Entities();
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	const auto* info = VillagerInfoOf(registry.Get<const Villager>(villager));
	const auto position = registry.Get<const Transform>(villager).position;
	const auto control = registry.Create();
	registry.Assign<Transform>(control, position, glm::mat3(1.0f), glm::vec3(1.0f));
	const auto reaction = Locator::reactionSystem::value().Create({
	    .initiator = control,
	    .type = Reaction::ReactToMissionary,
	    .player = player,
	    .position = position,
	    .impressiveValue = info != nullptr ? info->missionaryImpressiveValue : 0.0f,
	    .balance = systems::ReactionSystemInterface::Balance::Missionaries,
	});
	registry.Assign<MissionaryControl>(
	    control, MissionaryControl {.villager = villager, .town = town, .player = player, .reaction = reaction});
}
} // namespace

uint32_t villager_disciple_work::CheckTrader(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto* disciple = Entities().TryGet<Disciple>(villager);
	const auto tradeTown = disciple != nullptr ? ValidTown(disciple->tradeTown) : std::nullopt;
	if (!tradeTown.has_value() || !Locator::townDesireSystem::has_value())
	{
		return 0;
	}
	const auto& desires = Locator::townDesireSystem::value();
	const auto resource = disciple_work::TradeResource(desires.GetRawDesire(*tradeTown, TownDesireInfo::ForFood),
	                                                   desires.GetRawDesire(*tradeTown, TownDesireInfo::ForWood));
	if (!resource.has_value())
	{
		return SetTraderNothingToDo(action, *tradeTown);
	}
	disciple->tradeResource = *resource;
	const auto& person = Entities().Get<const Villager>(villager);
	const auto* info = VillagerInfoOf(person);
	if (info == nullptr)
	{
		return 0;
	}
	// Its fullness is that of its larger load, whatever it is to carry; the food limit stands for both
	const auto here = GroundOf(villager);
	const auto leg =
	    disciple_work::ChooseTradeLeg(LargerLoad(person).second, info->maxTraderFoodCarried,
	                                  gutils::GetDistanceInMetres(here, DropOffPosition(villager, ValidTown(person.town))),
	                                  gutils::GetDistanceInMetres(here, DropOffPosition(villager, tradeTown)));
	SetTopState(action, leg == disciple_work::TradeLeg::Fetch ? VillagerStates::ArrivesAtStoragePitForTraderPickUp
	                                                          : VillagerStates::ArrivesAtStoragePitForTraderDropOff);
	return 1;
}

uint32_t villager_disciple_work::TraderFetch(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto* disciple = Entities().TryGet<const Disciple>(villager);
	if (disciple == nullptr)
	{
		return 0;
	}
	const auto& person = Entities().Get<const Villager>(villager);
	const auto* info = VillagerInfoOf(person);
	if (info == nullptr || (disciple->tradeResource != ResourceType::Food && disciple->tradeResource != ResourceType::Wood))
	{
		return 0;
	}
	const auto limit = disciple->tradeResource == ResourceType::Food ? info->maxTraderFoodCarried : info->maxTraderWoodCarried;
	const auto [carried, held] = LargerLoad(person);
	const auto amount = disciple_work::TradeFetchAmount(limit, held);
	const auto tradeTown = disciple->tradeTown;
	// Deviation #30: vanilla asked for the resource already carried
	const auto result = villager_build::ArrivesAtStoragePitForResource(
	    action, disciple_work::TradeFetchResource(carried, disciple->tradeResource), amount, VillagerStates::DecideWhatToDo,
	    VillagerStates::DecideWhatToDo);
	if (result == 0)
	{
		return SetTraderNothingToDo(action, tradeTown);
	}
	return result;
}

uint32_t villager_disciple_work::TraderDeliver(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto* disciple = Entities().TryGet<const Disciple>(villager);
	const auto tradeTown = disciple != nullptr ? ValidTown(disciple->tradeTown) : std::nullopt;
	// What it delivers leaves its own town's count of what its people carry; the other town's store counts it in
	return villager_build::DropOffAtStoragePit(action,
	                                           tradeTown.has_value() ? construction::StoragePitOf(*tradeTown) : std::nullopt);
}

uint32_t villager_disciple_work::CheckSupplyWorkshop(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto workshop = BestWorkshop(villager);
	if (!workshop.has_value())
	{
		return 0;
	}
	const auto& person = Entities().Get<const Villager>(villager);
	if (person.woodHeld != 0)
	{
		villager_footpath::SetupMoveToOnFootpath(action, *workshop, villager_home::ArrivePosition(*workshop),
		                                         VillagerStates::ArrivesAtWorkshopForDropOff);
		return 1;
	}
	const auto pit = construction::StoragePitOf(person.town);
	if (pit.has_value() && Entities().Valid(*pit) &&
	    Locator::resourceStoreSystem::value().GetResource(*pit, ResourceType::Wood) > 0)
	{
		villager_footpath::SetupMoveToOnFootpath(action, *pit, villager_home::ArrivePosition(*pit),
		                                         VillagerStates::ArrivesAtStoragePitForWorkshopMaterials);
		return 1;
	}
	return 0;
}

uint32_t villager_disciple_work::ArrivesAtWorkshopForDropOff(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	if (!ValidTown(person.town).has_value())
	{
		return 0;
	}
	if (const auto workshop = BestWorkshop(villager))
	{
		const auto door = villager_home::ArrivePosition(*workshop);
		if (!AreWeThere(villager, door))
		{
			villager_footpath::SetupMoveToOnFootpath(action, *workshop, door, VillagerStates::ArrivesAtWorkshopForDropOff);
			return 1;
		}
		// It puts down all its wood and the workshop takes it
		const auto dropped = villager_carry::PutDown(person, ResourceType::Wood, person.woodHeld);
		if (auto* tally = Entities().TryGet<TownResourceTally>(person.town))
		{
			tally->woodCarried -= static_cast<float>(dropped);
		}
		Locator::resourceStoreSystem::value().AddResource(*workshop, ResourceType::Wood, dropped, std::nullopt);
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_disciple_work::ArrivesAtStoragePitForWorkshopMaterials(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& person = VillagerOf(action);
	if (!ValidTown(person.town).has_value())
	{
		return 0;
	}
	const auto pit = construction::StoragePitOf(person.town);
	if (IsFunctional(pit))
	{
		const auto door = villager_home::ArrivePosition(*pit);
		if (!AreWeThere(villager, door))
		{
			villager_footpath::SetupMoveToOnFootpath(action, *pit, door,
			                                         VillagerStates::ArrivesAtStoragePitForWorkshopMaterials);
			return 1;
		}
		const auto* info = VillagerInfoOf(person);
		const auto room = info != nullptr ? disciple_work::CarryRoom(info->maxWoodCarried, person.woodHeld) : 0;
		if (BestWorkshop(villager).has_value() && room != 0)
		{
			villager_build::ArrivesAtStoragePitForResource(action, ResourceType::Wood, static_cast<uint32_t>(room),
			                                               VillagerStates::ArrivesAtWorkshopForDropOff,
			                                               VillagerStates::DecideWhatToDo);
			return 1;
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_disciple_work::MissionaryDisciple(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	auto& towns = Locator::townSystem::value();
	// The nearest building of any town, any player's or none: not necessarily the one it was put down by
	const auto abode = towns.NearestBuilding(GroundOf(villager));
	if (abode == entt::null || !registry.Valid(abode))
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// It preaches for the player of the town it came from, in the town it moves into
	const auto from = ValidTown(registry.Get<const Villager>(villager).town);
	towns.MoveInAnyway(abode, villager);
	const auto to = ValidTown(registry.Get<const Villager>(villager).town);
	if (from.has_value() && to.has_value())
	{
		StartPreaching(villager, *to, registry.Get<const Town>(*from).owner);
	}
	SetTopState(action, VillagerStates::GoHomeAndChange);
	return 1;
}

void villager_disciple_work::ProcessMissionaries()
{
	auto& registry = Entities();
	std::vector<entt::entity> stopping;
	registry.Each<const MissionaryControl, Transform>(
	    [&](entt::entity control, const MissionaryControl& preaching, Transform& transform) {
		    const bool reactionLives =
		        Locator::reactionSystem::has_value() && Locator::reactionSystem::value().IsActive(preaching.reaction);
		    if (!reactionLives || !ValidTown(preaching.town).has_value() || !IsAvailable(preaching.villager) ||
		        villager_disciple::TypeOf(preaching.villager) != VillagerDisciple::Missionary)
		    {
			    stopping.push_back(control);
			    return;
		    }
		    // Its preaching follows it about
		    transform.position = registry.Get<const Transform>(preaching.villager).position;
		    Locator::reactionSystem::value().Move(preaching.reaction, transform.position, glm::vec3(0.0f), 1.0f);
	    });
	for (const auto control : stopping)
	{
		StopPreaching(control);
	}
}

void villager_disciple_work::StopMissionariesIn(entt::entity town)
{
	auto& registry = Entities();
	std::vector<entt::entity> stopping;
	registry.Each<const MissionaryControl>([&stopping, town](entt::entity control, const MissionaryControl& preaching) {
		if (preaching.town == town)
		{
			stopping.push_back(control);
		}
	});
	for (const auto control : stopping)
	{
		StopPreaching(control);
	}
}
