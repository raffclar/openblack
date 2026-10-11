/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerBuild.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/FieldCrop.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/BuildingSiteRules.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/VillagerCarry.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerAnimate.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_build = openblack::ecs::villager_build;

namespace
{
/// A builder within this of its place walks to it round what is in the way; farther, along the paths
constexpr float k_HugDistance = 40.0f;
/// A builder this near its place has arrived at it
constexpr float k_AtPlace = 0.2f;
/// Within this of the building a builder is touching it
constexpr float k_Touching = 0.001f;
/// A builder turns to face the building at this speed
constexpr int32_t k_FaceBuildingStep = 0x80;
/// What a state function answers while its villager is still on its way to a store
constexpr uint32_t k_StillBusy = 36;

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

VillagerStates TopState(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top);
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

entt::entity TownOf(const Villager& villager)
{
	const auto& registry = Entities();
	return villager.town != entt::null && registry.Valid(villager.town) && registry.AllOf<Town>(villager.town) ? villager.town
	                                                                                                           : entt::null;
}

glm::vec3 PositionOf(entt::entity entity)
{
	return Entities().Get<const Transform>(entity).position;
}

glm::vec2 GroundOf(entt::entity entity)
{
	return glm::xz(PositionOf(entity));
}

/// A builder disciple is offered sites even when no builder is wanted
bool IsBuilderDisciple(const Villager&)
{
	// TODO(temple-builders): disciples aren't kept on villagers yet
	return false;
}

/// Walks the villager to a point round what is in the way, then on into a state, as long as the states let it
void MoveTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, final))
	{
		villager_home::SetupMobileMoveTo(action, goal, final);
	}
}

/// The same along the town's paths
void MoveOnPathsTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	// TODO(temple-builders): along the town's footpaths where there are any; villagers don't walk footpaths yet
	MoveTo(action, goal, final);
}

/// Turns the villager towards a point by at most a step of the game's angles; whether it now faces it
bool LookAtPos(entt::entity villager, glm::vec2 point, int32_t step)
{
	auto& transform = Entities().Get<Transform>(villager);
	const auto x = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
	const float facingRadians = -std::atan2(-x.z, x.x) - glm::radians(90.0f);
	const auto target = static_cast<int32_t>(gutils::GetAngleFromXZ(glm::xz(transform.position), point));
	const auto facing = static_cast<int32_t>(gutils::ConvertAngle3DToGame(facingRadians));
	const int32_t difference = target - facing;
	int32_t angle = target;
	bool facesIt = true;
	if (std::abs(difference) >= step)
	{
		const bool forwards = difference < 1 ? std::abs(difference) > 0x3FF : difference < 0x400;
		angle = (facing + (forwards ? step : -step)) & gutils::k_GameAngleMask;
		facesIt = false;
	}
	transform.rotation = glm::eulerAngleY(-gutils::ConvertGameAngleTo3D(angle) - glm::radians(90.0f));
	Entities().SetDirty();
	return facesIt;
}

/// The villager's storage pit: its town's, else its home when that is a storage pit
std::optional<entt::entity> StoragePitOf(const Villager& villager)
{
	return construction::StoragePitOf(TownOf(villager));
}

bool IsFunctional(std::optional<entt::entity> pit)
{
	return pit.has_value() && villager_home::IsFunctional(*pit);
}

/// Where the villager leaves wood: its working storage pit
std::optional<glm::vec2> DropOffPosition(const Villager& villager)
{
	const auto pit = StoragePitOf(villager);
	if (IsFunctional(pit))
	{
		return villager_home::ArrivePosition(*pit);
	}
	// TODO(temple-builders): without a working storage pit a town keeps a temporary pot of wood; not yet
	return std::nullopt;
}

/// How good or evil the land is where the villager stands: each player's influence there times their alignment
float LandAlignmentAt(glm::vec3 position)
{
	if (!Locator::influenceSystem::has_value() || !Locator::alignmentSystem::has_value())
	{
		return 0.0f;
	}
	const auto& influence = Locator::influenceSystem::value();
	const auto& alignment = Locator::alignmentSystem::value();
	float sum = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		sum += alignment.GetPlayerAlignment(player) * influence.PlayerInfluence(player, position);
	}
	return field_crop::ClampAlignment(sum);
}

bool IsWorker(entt::entity building, entt::entity villager)
{
	const auto& workers = Entities().Get<const BuildingSite>(building).workers;
	return std::ranges::find(workers, villager) != workers.end();
}

/// Whether the villager should fetch wood before building at a site
bool ShouldIGetWood(entt::entity villager, entt::entity building)
{
	auto& registry = Entities();
	const auto& person = registry.Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	std::vector<int16_t> workersWood;
	for (const auto worker : registry.Get<const BuildingSite>(building).workers)
	{
		const auto* other = registry.Valid(worker) ? registry.TryGet<const Villager>(worker) : nullptr;
		workersWood.push_back(other != nullptr ? other->woodHeld : int16_t {0});
	}
	const auto here = PositionOf(villager);
	const auto dropOff = DropOffPosition(person).value_or(glm::xz(here));
	return building_site::ShouldFetchWood({
	    .woodAtSite = construction::WoodAtSite(building),
	    .workersWood = workersWood,
	    .woodNeeded = construction::WoodNeededToBuild(building),
	    .distanceToBuilding = gutils::GetDistanceInMetres(here, PositionOf(building)),
	    .distanceToDropOff = gutils::GetDistanceInMetres(dropOff, glm::xz(here)),
	    .woodHeld = person.woodHeld,
	    .maxWoodCarried = static_cast<int32_t>(info.maxWoodCarried),
	    .woodPerBuilderWanted = static_cast<int32_t>(info.amountOfWoodPerBuilderWanted),
	});
}

int32_t WoodRoom(const Villager& villager)
{
	const auto& info = InfoOf(villager);
	// The room left is kept in 16 bits
	return static_cast<int16_t>(villager_carry::Room(villager, ResourceType::Wood, static_cast<int32_t>(info.maxFoodCarried),
	                                                 static_cast<int32_t>(info.maxWoodCarried)));
}

/// Where a site's place is on the ground
glm::vec2 PlacePosition(entt::entity building, uint32_t place)
{
	return glm::xz(Entities().Get<const BuildingSite>(building).places.at(place));
}

uint32_t GotoBuildingSite(LivingAction& action, entt::entity building)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto town = TownOf(person);
	if (town == entt::null || !construction::IsSiteValid(town, building))
	{
		return 0;
	}
	if (!IsWorker(building, villager) && construction::BuildersNeeded(building) <= 0 && !IsBuilderDisciple(person))
	{
		return 0;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	auto& builder = VillagerOf(action);
	builder.buildingSite = building;
	// It makes for the place round the building on its own side, give or take an eighth of a turn
	const auto random = [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); };
	const float angle = gutils::Get3DAngleFromXZ(GroundOf(building), GroundOf(villager));
	builder.buildPlace = static_cast<int32_t>(building_site::PlaceAt(building_site::FirstPlaceAngle(angle, random)));
	const auto goal = PlacePosition(building, static_cast<uint32_t>(builder.buildPlace));
	if (gutils::GetDistanceInMetres(GroundOf(villager), goal) <= k_HugDistance)
	{
		MoveTo(action, goal, VillagerStates::ArrivesAtBuildingSite);
	}
	else
	{
		MoveOnPathsTo(action, goal, VillagerStates::ArrivesAtBuildingSite);
	}
	return 1;
}

uint32_t GotoStoragePitForBuildingMaterials(LivingAction& action, entt::entity building)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto town = TownOf(person);
	if (town == entt::null || !construction::IsSiteValid(town, building))
	{
		return 0;
	}
	if (WoodRoom(person) < 1)
	{
		return GotoBuildingSite(action, building);
	}
	const auto pit = StoragePitOf(person);
	std::optional<glm::vec2> target;
	if (IsFunctional(pit))
	{
		// A storage pit's wood is taken at its door
		target = villager_home::ArrivePosition(*pit);
	}
	else
	{
		target = DropOffPosition(person);
	}
	if (!target.has_value())
	{
		return 0;
	}
	if (!IsWorker(building, villager))
	{
		if (construction::BuildersNeeded(building) <= 0 && !IsBuilderDisciple(person))
		{
			return 0;
		}
		if (!villager_build::IsBuildingState(TopState(action)))
		{
			person.buildingSite = building;
		}
	}
	if (pit.has_value())
	{
		MoveOnPathsTo(action, *target, VillagerStates::ArrivesAtStoragePitForBuildingMaterials);
	}
	else
	{
		MoveTo(action, *target, VillagerStates::ArrivesAtStoragePitForBuildingMaterials);
	}
	return 1;
}

uint32_t SetupGetBuildingSupplies(LivingAction& action, entt::entity building)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto town = TownOf(person);
	if (town == entt::null || !construction::IsSiteValid(town, building))
	{
		return 0;
	}
	if (!ShouldIGetWood(villager, building))
	{
		return GotoBuildingSite(action, building);
	}
	const auto pit = StoragePitOf(person);
	const auto& info = InfoOf(person);
	const auto here = GroundOf(villager);
	const auto store = DropOffPosition(person);
	const auto source = building_site::DecideWoodSource({
	    .forBuilding = true,
	    .distanceToStore = store.has_value() ? gutils::GetDistanceInMetres(*store, here) : 0.0f,
	    .storeWood = IsFunctional(pit) ? Locator::resourceStoreSystem::value().GetResource(*pit, ResourceType::Wood) : 0u,
	    .room = WoodRoom(person),
	    .maxWoodCarried = static_cast<int32_t>(info.maxWoodCarried),
	    // TODO(temple-builders): the nearest of the town's forests, which builders fell for wood (the foresters' job)
	    .distanceToForest = std::nullopt,
	    .forestHasBigForest = false,
	    .maxDistance = static_cast<float>(Locator::infoConstants::value().town.maxDistanceForTownForest),
	});
	switch (source)
	{
	case building_site::WoodSource::Store:
		return GotoStoragePitForBuildingMaterials(action, building);
	case building_site::WoodSource::BigForest:
	case building_site::WoodSource::Forest:
		// TODO(temple-builders): fetching wood from a forest comes with the foresters' job
		return 0;
	case building_site::WoodSource::None:
		break;
	}
	return 0;
}

uint32_t SetupBuildingObject(LivingAction& action, entt::entity building)
{
	if (TownOf(VillagerOf(action)) == entt::null)
	{
		return 0;
	}
	if (construction::IsBuilt(building) && world_objects::LifeOf(building) >= 1.0f)
	{
		return 0;
	}
	// Nothing in the game can be pushed aside, so the ground about a site is always clear
	return SetupGetBuildingSupplies(action, building);
}

/// The best site of the villager's town for it
std::optional<entt::entity> BestSite(entt::entity villager, entt::entity town, bool disciple)
{
	const auto sites = construction::SitesOfTown(town);
	std::vector<building_site::SiteCandidate> candidates;
	const auto here = GroundOf(villager);
	for (const auto building : sites)
	{
		const auto edge = construction::NearestEdgeToPos(building, here);
		candidates.push_back({.wantsBuilders = construction::BuildersNeeded(building) > 0,
		                      .distanceToEdge = gutils::GetDistanceInMetres(here, edge),
		                      .desireForVillagers = construction::DesireForVillagers(building)});
	}
	const auto best = building_site::BestSite(candidates, disciple);
	return best.has_value() ? std::optional(sites[*best]) : std::nullopt;
}

/// The villager takes what it can of a store's resource there, walking to the store first. The amount it took, or
/// still busy while it walks or took less than it asked for.
uint32_t AtStoreRemoveResource(LivingAction& action, entt::entity store, ResourceType type, uint32_t amount)
{
	const auto villager = EntityOf(action);
	const auto edge = villager_home::ArrivePosition(store);
	const float radius = systems::object_measures::TwoDRadius(Entities(), villager);
	if (!(gutils::GetDistanceInMetres(GroundOf(villager), edge) <= radius))
	{
		MoveTo(action, edge, TopState(action));
		return k_StillBusy;
	}
	auto& person = VillagerOf(action);
	const auto taken =
	    static_cast<int16_t>(Locator::resourceStoreSystem::value().RemoveResource(store, type, static_cast<int16_t>(amount)));
	if (taken == 0)
	{
		return 0;
	}
	villager_carry::PickUp(person, type, taken, 0);
	if (auto* tally = Entities().TryGet<TownResourceTally>(TownOf(person)))
	{
		(type == ResourceType::Food ? tally->foodCarried : tally->woodCarried) += static_cast<float>(taken);
	}
	// TODO(temple-builders): a sped-up or poisoned store speeds up or poisons who takes from it
	return static_cast<uint32_t>(static_cast<int32_t>(taken)) < amount ? k_StillBusy : 1u;
}

uint32_t ArrivesAtStoragePitForResource(LivingAction& action, ResourceType type, uint32_t amount, VillagerStates arrive,
                                        VillagerStates fail)
{
	auto& person = VillagerOf(action);
	if (amount > 0)
	{
		const auto pit = StoragePitOf(person);
		if (IsFunctional(pit))
		{
			const auto held = Locator::resourceStoreSystem::value().GetResource(*pit, type);
			amount = std::min(amount, held);
			if (amount > 0)
			{
				const auto result = AtStoreRemoveResource(action, *pit, type, amount);
				if (result == k_StillBusy)
				{
					return k_StillBusy;
				}
				if (result == 1 && arrive != VillagerStates::InvalidState)
				{
					MoveOnPathsTo(action, villager_home::ArrivePosition(*pit), arrive);
					return 1;
				}
			}
			SetTopState(action, fail);
			return 0;
		}
		// TODO(temple-builders): without a working storage pit the town's temporary pot is used; not yet
	}
	SetTopState(action, fail);
	return 0;
}
} // namespace

bool villager_build::IsBuildingState(VillagerStates state)
{
	switch (state)
	{
	case VillagerStates::ArrivesAtStoragePitForBuildingMaterials:
	case VillagerStates::ArrivesAtBuildingSite:
	case VillagerStates::Building:
	case VillagerStates::ForesterChopsTreeForBuilding:
	case VillagerStates::ArrivesAtBigForestForBuilding:
	case VillagerStates::ReenterBuildingState:
	case VillagerStates::TakeWoodFromTreeForBuilding:
	case VillagerStates::TakeWoodFromPotForBuilding:
		return true;
	default:
		return false;
	}
}

uint32_t villager_build::CheckSatisfyToBuild(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& person = VillagerOf(action);
	const auto town = TownOf(person);
	if (town == entt::null)
	{
		return 0;
	}
	const auto site = BestSite(villager, town, IsBuilderDisciple(person));
	return site.has_value() && SetupBuildingObject(action, *site) == 1 ? 1 : 0;
}

uint32_t villager_build::CheckNeededForBuilding(LivingAction& action)
{
	const auto town = TownOf(VillagerOf(action));
	if (town == entt::null || construction::SitesOfTown(town).empty())
	{
		return 0;
	}
	return CheckSatisfyToBuild(action);
}

uint32_t villager_build::ArrivesAtStoragePitForBuildingMaterials(LivingAction& action)
{
	auto& person = VillagerOf(action);
	const auto building = person.buildingSite;
	if (building != entt::null && construction::IsSiteValid(TownOf(person), building))
	{
		const auto room = WoodRoom(person);
		if (room == 0)
		{
			if (GotoBuildingSite(action, building) == 1)
			{
				return 1;
			}
		}
		else
		{
			return ArrivesAtStoragePitForResource(action, ResourceType::Wood, static_cast<uint32_t>(room),
			                                      VillagerStates::ReenterBuildingState, VillagerStates::DecideWhatToDo);
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_build::ArrivesAtBuildingSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto building = person.buildingSite;
	if (building != entt::null && construction::IsSiteValid(TownOf(person), building) && person.buildPlace >= 0 &&
	    person.buildPlace < static_cast<int32_t>(building_site::k_Places))
	{
		const auto place = PlacePosition(building, static_cast<uint32_t>(person.buildPlace));
		if (gutils::GetDistanceInMetres(GroundOf(villager), place) > k_AtPlace)
		{
			MoveTo(action, place, VillagerStates::ArrivesAtBuildingSite);
			return 1;
		}
		if (LookAtPos(villager, GroundOf(building), k_FaceBuildingStep))
		{
			// It puts all its wood down at the site; what the site doesn't take is lost
			if (person.woodHeld != 0)
			{
				Locator::resourceStoreSystem::value().AddResource(building, ResourceType::Wood,
				                                                  static_cast<uint32_t>(static_cast<int32_t>(person.woodHeld)),
				                                                  PositionOf(villager));
				const auto dropped = villager_carry::PutDown(person, ResourceType::Wood, 0);
				if (auto* tally = Entities().TryGet<TownResourceTally>(TownOf(person)))
				{
					tally->woodCarried -= static_cast<float>(dropped);
				}
				villager_animate::SetStateCarriedObject(villager);
			}
			const auto turns = action.turnsSinceStateChange;
			Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::Building);
			action.turnsSinceStateChange = turns;
		}
		return 1;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_build::Building(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto building = person.buildingSite;
	const auto town = TownOf(person);
	if (town == entt::null)
	{
		return 0;
	}
	if (!Locator::livingActionSystem::value().VillagerIsReadyForNewAnimation(action, 1))
	{
		return 1;
	}
	action.turnsSinceStateChange = 0;
	if (building != entt::null && construction::IsSiteValid(town, building))
	{
		// A stroke uses the site's wood, a little more on evil land, and raises the building by its share of its worth
		const auto& info = InfoOf(person);
		const auto want = building_site::WoodPerStroke(info.woodUsedPerBuildCycle, LandAlignmentAt(PositionOf(villager)));
		auto& stores = Locator::resourceStoreSystem::value();
		const auto atSite = construction::WoodAtSite(building);
		const auto use = atSite < static_cast<uint32_t>(want) ? atSite : static_cast<uint32_t>(want);
		if (use != 0)
		{
			const float progress = static_cast<float>(use) / construction::SiteWoodValue(building);
			stores.RemoveResource(building, ResourceType::Wood, use);
			construction::BuildBy(building, progress);
			if (auto* tally = Entities().TryGet<TownResourceTally>(town))
			{
				tally->woodUsed += static_cast<float>(use);
			}
			// TODO(temple-builders): the wood its player's town has built with counts in the player's statistics
		}
		// Building on while there is something to build and wood to build it with
		if (construction::IsSiteValid(town, building))
		{
			if (construction::WoodAtSite(building) == 0)
			{
				return SetupGetBuildingSupplies(action, building);
			}
			const auto randomFloat = [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); };
			const auto randomInt = [](uint32_t limit) { return Locator::gameRandom::value().GameRand(limit); };
			person.buildPlace = static_cast<int32_t>(building_site::NextPlace(
			    static_cast<uint32_t>(person.buildPlace), construction::BuildingRadius(building), randomFloat, randomInt));
			MoveTo(action, PlacePosition(building, static_cast<uint32_t>(person.buildPlace)),
			       VillagerStates::ArrivesAtBuildingSite);
			return 1;
		}
	}
	VillagerOf(action).buildingSite = entt::null;
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_build::ReenterBuildingState(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto building = person.buildingSite;
	const auto town = TownOf(person);
	if (town != entt::null && building != entt::null && construction::IsSiteValid(town, building))
	{
		if (!ShouldIGetWood(villager, building))
		{
			const auto random = [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); };
			const float angle = gutils::Get3DAngleFromXZ(GroundOf(building), GroundOf(villager));
			person.buildPlace = static_cast<int32_t>(building_site::PlaceAt(building_site::FirstPlaceAngle(angle, random)));
			const auto& registry = Entities();
			const float gap =
			    gutils::GetDistanceInMetres(GroundOf(villager), GroundOf(building)) -
			    (systems::object_measures::TwoDRadius(registry, villager) + construction::BuildingRadius(building));
			if (gap <= k_Touching)
			{
				SetTopState(action, VillagerStates::ArrivesAtBuildingSite);
				return 1;
			}
			const auto goal = PlacePosition(building, static_cast<uint32_t>(person.buildPlace));
			if (gutils::GetDistanceInMetres(GroundOf(villager), goal) <= k_HugDistance)
			{
				MoveTo(action, goal, VillagerStates::ArrivesAtBuildingSite);
			}
			else
			{
				MoveOnPathsTo(action, goal, VillagerStates::ArrivesAtBuildingSite);
			}
			return 1;
		}
		if (SetupGetBuildingSupplies(action, building) == 1)
		{
			return 1;
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_build::EnterBuilding(LivingAction& action, VillagerStates previous, VillagerStates)
{
	const auto villager = EntityOf(action);
	const auto& person = VillagerOf(action);
	if (!construction::IsSiteValid(TownOf(person), person.buildingSite))
	{
		return false;
	}
	if (!IsBuildingState(previous))
	{
		construction::AddWorker(person.buildingSite, villager);
	}
	return true;
}

bool villager_build::ExitBuilding(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	if (!IsBuildingState(next))
	{
		const auto town = TownOf(person);
		if (town != entt::null && construction::IsSiteValid(town, person.buildingSite))
		{
			construction::RemoveWorker(person.buildingSite, villager);
		}
		person.buildingSite = entt::null;
	}
	// Leaving is never held up
	return false;
}

uint32_t villager_build::ScriptInCrowd(LivingAction& action)
{
	if (Locator::livingActionSystem::value().VillagerIsReadyForNewAnimation(action, 1))
	{
		villager_animate::SetStateAnim(EntityOf(action));
		action.turnsSinceStateChange = 0;
	}
	return 1;
}

uint32_t villager_build::CheckTakeResourcesToStoragePit(LivingAction& action)
{
	const auto& person = VillagerOf(action);
	const auto& info = InfoOf(person);
	if (person.woodHeld > static_cast<int32_t>(info.minWoodToShowGraphic) ||
	    person.foodHeld > static_cast<int32_t>(info.minFoodToShowGraphic))
	{
		SetTopState(action, VillagerStates::GotoStoragePitForDropOff);
		return 1;
	}
	return 0;
}

uint32_t villager_build::GotoStoragePitForDropOff(LivingAction& action)
{
	const auto& person = VillagerOf(action);
	const auto pit = StoragePitOf(person);
	if (IsFunctional(pit))
	{
		MoveOnPathsTo(action, villager_home::ArrivePosition(*pit), VillagerStates::ArrivesAtStoragePitForDropOff);
		return 1;
	}
	const auto held = villager_carry::HeldResource(person);
	if (held == ResourceType::Food || held == ResourceType::Wood)
	{
		// TODO(temple-builders): without a working storage pit the load goes to the town's temporary pot; not yet
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 0;
}

uint32_t villager_build::ArrivesAtStoragePitForDropOff(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(action);
	const auto type = villager_carry::HeldResource(person);
	const auto held = type == ResourceType::Food ? person.foodHeld : person.woodHeld;
	if (type != ResourceType::None && held != 0)
	{
		const auto pit = StoragePitOf(person);
		if (IsFunctional(pit))
		{
			// It walks to the pit's door, then gives the larger of its loads; the pit counts what it took
			const auto door = villager_home::ArrivePosition(*pit);
			const auto* wallHug = Entities().TryGet<const WallHug>(villager);
			const float step =
			    gutils::ConvertWholeDistanceToMeters(wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f));
			if (!(gutils::GetDistanceInMetres(GroundOf(villager), door) <= step))
			{
				MoveTo(action, door, TopState(action));
				return 1;
			}
			const auto added = Locator::resourceStoreSystem::value().AddResource(
			    *pit, type, static_cast<uint32_t>(static_cast<uint16_t>(held)), std::nullopt);
			if (added != 0)
			{
				const auto dropped = villager_carry::PutDown(person, type, static_cast<uint16_t>(added));
				if (auto* tally = Entities().TryGet<TownResourceTally>(TownOf(person)))
				{
					(type == ResourceType::Food ? tally->foodCarried : tally->woodCarried) -= static_cast<float>(dropped);
				}
			}
			MoveOnPathsTo(action, door, VillagerStates::DecideWhatToDo);
			return 1;
		}
		// TODO(temple-builders): without a working storage pit the load goes to the town's temporary pot; not yet
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}
