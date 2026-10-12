/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerShepherd.h"

#include <algorithm>
#include <array>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/DrinkingWater.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DrinkingWater.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ShepherdRules.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerHome.h"
#include "VillagerMoveToObject.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_shepherd = openblack::ecs::villager_shepherd;
namespace rules = openblack::ecs::shepherd_rules;
using map_coords::MapCoords;

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

entt::entity TownOf(entt::entity villager)
{
	const auto& registry = Entities();
	const auto town = registry.Get<const Villager>(villager).town;
	return town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town) ? town : entt::null;
}

MapCoords PlaceOf(entt::entity thing)
{
	return script_flocks::PositionOf(Entities(), thing);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

/// Walks the villager to a place round what is in the way, then on into a state
void MoveTo(LivingAction& action, const MapCoords& goal, VillagerStates final)
{
	if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, final))
	{
		villager_home::SetupMobileMoveTo(action, map_coords::ToMetres(goal), final);
	}
}

ShepherdWork& WorkOf(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* work = registry.TryGet<ShepherdWork>(villager))
	{
		return *work;
	}
	return registry.Assign<ShepherdWork>(villager);
}

/// The flock the villager herds, if it is still there
entt::entity FlockOf(entt::entity villager)
{
	return script_flocks::FlockOf(Entities(), villager);
}

/// The villager points at a flock, leaving any flock it was in
void SetFlock(entt::entity villager, entt::entity flock)
{
	auto& registry = Entities();
	script_flocks::Remove(registry, villager, true);
	if (auto* membership = registry.TryGet<FlockMember>(villager))
	{
		membership->flock = flock;
		return;
	}
	registry.Assign<FlockMember>(villager).flock = flock;
}

/// Whether the shepherd's season of the jobs table has it work now
bool JobActive()
{
	const auto& job = Locator::infoConstants::value().job.at(static_cast<size_t>(rules::k_ShepherdJob));
	uint32_t season = 0;
	if (Locator::weatherSystem::has_value() && Locator::time::has_value())
	{
		season = Locator::weatherSystem::value().GetSeason(Locator::time::value().GetTurn());
	}
	const std::array activities {job.spring, job.summer, job.autumn, job.winter};
	return rules::JobActive(activities.at(std::min<std::size_t>(season, activities.size() - 1)));
}

/// The town's flocks, the latest made first
std::vector<entt::entity> FlocksOf(entt::entity town)
{
	std::vector<std::pair<uint32_t, entt::entity>> found;
	Entities().Each<const Flock>([&](entt::entity flock, const Flock& data) {
		if (data.town == town)
		{
			found.emplace_back(data.made, flock);
		}
	});
	std::ranges::sort(found, [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<entt::entity> flocks;
	flocks.reserve(found.size());
	std::ranges::transform(found, std::back_inserter(flocks), [](const auto& pair) { return pair.second; });
	return flocks;
}

/// The first of the town's flocks no one herds that has a leader
entt::entity FirstFreeFlock(entt::entity town)
{
	const auto& registry = Entities();
	for (const auto flock : FlocksOf(town))
	{
		if (registry.Get<const Flock>(flock).shepherd != entt::null)
		{
			continue;
		}
		const auto leader = script_flocks::Leader(registry, flock);
		if (leader != entt::null && registry.Valid(leader))
		{
			return flock;
		}
	}
	return entt::null;
}

/// The speed of a member of the flock, in whole map units a turn
uint16_t SpeedOf(entt::entity member)
{
	auto& registry = Entities();
	if (registry.AllOf<Animal>(member) && Locator::animalSystem::has_value())
	{
		return Locator::animalSystem::value().SpeedOf(member);
	}
	if (const auto* walker = registry.TryGet<const WallHug>(member))
	{
		return static_cast<uint16_t>(wall_hug::WholeSpeed(walker->speed));
	}
	return 0;
}

/// The villager walks at a speed, in whole map units a turn, exactly
void SetSpeed(entt::entity villager, uint16_t speed)
{
	if (auto* walker = Entities().TryGet<WallHug>(villager))
	{
		walker->speed = GetSpeedStateSpeed(static_cast<SpeedState>(villager_speed::FinalSpeed(speed, 1.0f)));
	}
}

/// The newest member of the flock: the shepherd itself once it has joined
entt::entity HeadOf(entt::entity flock)
{
	const auto& members = Entities().Get<const Flock>(flock).members;
	return members.empty() ? entt::null : members.front();
}

/// The nearest animal of the villager's flock within reach of it
entt::entity NearestAnimal(entt::entity villager, entt::entity flock, float reach)
{
	const auto& registry = Entities();
	const auto& members = registry.Get<const Flock>(flock).members;
	std::vector<rules::MemberView> views;
	std::vector<entt::entity> livings;
	views.reserve(members.size());
	for (const auto member : members)
	{
		if (!registry.Valid(member))
		{
			continue;
		}
		views.push_back(
		    {.position = PlaceOf(member), .self = member == villager, .villager = registry.AllOf<Villager>(member)});
		livings.push_back(member);
	}
	const auto nearest = rules::NearestAnimal(views, PlaceOf(villager), reach);
	return nearest.has_value() ? livings.at(*nearest) : entt::null;
}

/// The villager's town's store, when it works
std::optional<entt::entity> WorkingStoreOf(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	const auto pit = construction::StoragePitOf(town);
	if (!pit.has_value() || !villager_home::IsFunctional(*pit))
	{
		return std::nullopt;
	}
	return pit;
}

std::optional<entt::entity> AbodeOf(entt::entity villager)
{
	const auto abode = Entities().Get<const Villager>(villager).abode;
	return abode != entt::null && Entities().Valid(abode) ? std::optional(abode) : std::nullopt;
}

/// Where the flock goes to drink: its home's water, looked for further out when the home has none
std::optional<MapCoords> WaterFor(entt::entity abode)
{
	auto& registry = Entities();
	if (const auto* water = registry.TryGet<const DrinkingWater>(abode); water != nullptr && water->place.has_value())
	{
		return water->place;
	}
	drinking_water::FindFor(abode, rules::k_ShepherdWaterReach);
	const auto* water = registry.TryGet<const DrinkingWater>(abode);
	return water != nullptr ? water->place : std::nullopt;
}

void MoveFlockPlace(entt::entity flock, const MapCoords& place)
{
	if (Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().MoveFlockPlace(flock, glm::ivec2(place.x, place.z));
	}
	else
	{
		Entities().Get<Flock>(flock).place = place;
	}
}

bool IsShepherdingState(VillagerStates state)
{
	switch (state)
	{
	case VillagerStates::ShepherdLookForFlock:
	case VillagerStates::ShepherdMoveFlockToWater:
	case VillagerStates::ShepherdMoveFlockToFood:
	case VillagerStates::ShepherdMoveFlockBack:
	case VillagerStates::ShepherdDecideWhatToDoWithFlock:
	case VillagerStates::ShepherdWaitForFlock:
	case VillagerStates::ShepherdSlaughterAnimal:
	case VillagerStates::ShepherdFetchStray:
	case VillagerStates::ShepherdGotoFlock:
	case VillagerStates::ShepherdTakeAnimalForSlaughter:
	case VillagerStates::ShepherdTakesControlOfFlock:
	case VillagerStates::ShepherdReleasesControlOfFlock:
	case VillagerStates::ShepherdCheckAnimalForSlaughter:
		return true;
	default:
		return false;
	}
}
} // namespace

std::optional<villager_shepherd::FlockChoice> villager_shepherd::FindBestFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	const auto& registry = Entities();
	const auto flocks = FlocksOf(town);
	const auto here = PlaceOf(villager);
	std::vector<rules::FlockCandidate> candidates;
	candidates.reserve(flocks.size());
	std::ranges::transform(flocks, std::back_inserter(candidates), [&](entt::entity flock) {
		const auto& data = registry.Get<const Flock>(flock);
		return rules::FlockCandidate {.herded = data.shepherd != entt::null,
		                              .distance = gutils::GetDistanceInMetres(data.place, here)};
	});
	const auto choice = rules::ChooseBestFlock(candidates);
	if (!choice.has_value())
	{
		return std::nullopt;
	}
	return FlockChoice {.flock = flocks.at(choice->index), .score = choice->score};
}

uint32_t villager_shepherd::VillagerBecomesShepherd(LivingAction& action, entt::entity flock)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	if (!script_flocks::IsFlock(registry, flock))
	{
		const auto town = TownOf(villager);
		if (town == entt::null)
		{
			return 0;
		}
		flock = FirstFreeFlock(town);
		if (flock == entt::null)
		{
			return 0;
		}
	}
	// Someone else herds it already
	if (const auto shepherd = registry.Get<const Flock>(flock).shepherd; shepherd != entt::null && shepherd != villager)
	{
		return 0;
	}
	SetFlock(villager, flock);
	registry.Get<Flock>(flock).shepherd = villager;
	return ShepherdGotoFlock(action);
}

uint32_t villager_shepherd::ShepherdLookForFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	const auto flock = FirstFreeFlock(town);
	if (flock == entt::null)
	{
		return 0;
	}
	// It leaves any flock it was in, which keeps it as its shepherd
	SetFlock(villager, flock);
	Entities().Get<Flock>(flock).shepherd = villager;
	return 1;
}

uint32_t villager_shepherd::ShepherdGotoFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto flock = FlockOf(villager);
	const auto leader = flock != entt::null ? script_flocks::Leader(registry, flock) : entt::null;
	if (leader == entt::null || !registry.Valid(leader))
	{
		ShepherdLookForFlock(action);
		return 0;
	}
	if (!JobActive())
	{
		return 0;
	}
	MoveTo(action, script_flocks::Position(registry, flock), VillagerStates::ShepherdTakesControlOfFlock);
	return 1;
}

uint32_t villager_shepherd::ShepherdTakesControlOfFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto flock = FlockOf(villager);
	if (!JobActive() || flock == entt::null)
	{
		return 0;
	}
	// It walks up to the leader the flock had before it joined
	const auto leader = script_flocks::Leader(registry, flock);
	script_flocks::AddLiving(registry, flock, villager);
	WorkOf(villager).countdown = rules::k_LookOverTurns;
	villager_move_to_object::SetupMoveToObject(action, leader, VillagerStates::ShepherdDecideWhatToDoWithFlock);
	return 1;
}

uint32_t villager_shepherd::ShepherdDecideWhatToDoWithFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& work = WorkOf(villager);
	--work.countdown;
	if (work.countdown != 0)
	{
		return 1;
	}
	const auto flock = FlockOf(villager);
	const auto members = flock != entt::null ? Entities().Get<const Flock>(flock).members.size() : 0;
	float wantsFood = 0.0f;
	if (const auto town = TownOf(villager); town != entt::null && Locator::townDesireSystem::has_value())
	{
		wantsFood = Locator::townDesireSystem::value().GetDesire(town, TownDesireInfo::ForFood);
	}
	const auto& trigger = Locator::infoConstants::value()
	                          .townDesire.at(static_cast<size_t>(TownDesireInfo::ForFood))
	                          .desireTriggersVillagerAction;
	switch (rules::DecideWithFlock(members, wantsFood, trigger))
	{
	case rules::FlockDecision::Release:
		SetTopState(action, VillagerStates::ShepherdReleasesControlOfFlock);
		return 0;
	case rules::FlockDecision::Slaughter:
		SetTopState(action, VillagerStates::ShepherdTakeAnimalForSlaughter);
		return 1;
	case rules::FlockDecision::MoveToWater:
		SetTopState(action, VillagerStates::ShepherdMoveFlockToWater);
		return 1;
	}
	return 1;
}

uint32_t villager_shepherd::ShepherdMoveFlockToWater(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto flock = FlockOf(villager);
	const auto abode = AbodeOf(villager);
	const auto water = abode.has_value() && flock != entt::null ? WaterFor(*abode) : std::nullopt;
	if (!water.has_value())
	{
		SetTopState(action, VillagerStates::ShepherdReleasesControlOfFlock);
		return 0;
	}
	// The flock's place is kept to come back to, and the leader takes it to the water
	auto& data = registry.Get<Flock>(flock);
	data.savedPlace = data.place;
	MoveFlockPlace(flock, *water);
	auto& work = WorkOf(villager);
	work.countdown = rules::k_TurnsAtWater;
	work.pending = VillagerStates::ShepherdMoveFlockBack;
	MoveTo(action, *water, VillagerStates::ShepherdWaitForFlock);
	SetSpeed(villager, SpeedOf(HeadOf(flock)));
	return 1;
}

uint32_t villager_shepherd::ShepherdMoveFlockToFood(LivingAction& /*action*/)
{
	return 1;
}

uint32_t villager_shepherd::ShepherdMoveFlockBack(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto flock = FlockOf(villager);
	if (flock == entt::null)
	{
		return 0;
	}
	const auto back = Entities().Get<const Flock>(flock).savedPlace;
	MoveFlockPlace(flock, back);
	MoveTo(action, back, VillagerStates::ShepherdReleasesControlOfFlock);
	SetSpeed(villager, SpeedOf(HeadOf(flock)));
	return 1;
}

uint32_t villager_shepherd::ShepherdWaitForFlock(LivingAction& action)
{
	auto& work = WorkOf(EntityOf(action));
	--work.countdown;
	if (work.countdown != 0)
	{
		return 1;
	}
	if (work.pending != VillagerStates::InvalidState)
	{
		const auto next = work.pending;
		SetTopState(action, next);
		work.pending = VillagerStates::InvalidState;
		return 1;
	}
	work.countdown = rules::k_ShortWait;
	SetTopState(action, VillagerStates::ShepherdDecideWhatToDoWithFlock);
	return 1;
}

uint32_t villager_shepherd::ShepherdReleasesControlOfFlock(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (FlockOf(villager) == entt::null)
	{
		return 0;
	}
	auto& work = WorkOf(villager);
	work.countdown = rules::k_ShortWait;
	work.pending = VillagerStates::GoHome;
	SetTopState(action, VillagerStates::ShepherdWaitForFlock);
	return 1;
}

uint32_t villager_shepherd::ShepherdTakeAnimalForSlaughter(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	if (const auto flock = FlockOf(villager); flock != entt::null)
	{
		const auto animal = NearestAnimal(villager, flock, rules::k_PickAnimalReach);
		if (animal != entt::null && WorkingStoreOf(villager).has_value() && Locator::animalSystem::has_value())
		{
			// It leaves the flock to another shepherd and takes the animal away in a flock of the two of them
			script_flocks::Remove(registry, villager, true);
			if (script_flocks::IsFlock(registry, flock))
			{
				registry.Get<Flock>(flock).shepherd = entt::null;
			}
			const auto led = Locator::animalSystem::value().SeparateIntoOwnFlock(animal);
			script_flocks::AddLiving(registry, led, villager);
			auto& ledData = registry.Get<Flock>(led);
			ledData.shepherd = villager;
			ledData.flockDistance = rules::k_LedFlockDistance;
			ledData.keepsTogether = true;
			WorkOf(villager).countdown = rules::k_ShortWait;
			SetTopState(action, VillagerStates::ShepherdCheckAnimalForSlaughter);
			return 1;
		}
	}
	WorkOf(villager).countdown = rules::k_ShortWait;
	SetTopState(action, VillagerStates::ShepherdDecideWhatToDoWithFlock);
	return 0;
}

uint32_t villager_shepherd::ShepherdCheckAnimalForSlaughter(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& work = WorkOf(villager);
	--work.countdown;
	if (work.countdown >= 1)
	{
		return 0;
	}
	const auto flock = FlockOf(villager);
	if (flock == entt::null || Entities().Get<const Flock>(flock).members.size() < 2)
	{
		work.countdown = rules::k_ShortWait;
		SetTopState(action, VillagerStates::ShepherdDecideWhatToDoWithFlock);
		return 0;
	}
	const auto animal = NearestAnimal(villager, flock, rules::k_LeadReach);
	const auto here = PlaceOf(villager);
	if (animal == entt::null)
	{
		// It turns to its flock's newest member, itself, and waits for the animal to catch up
		Locator::livingActionSystem::value().VillagerSetYAngle(
		    villager, gutils::ConvertGameAngleTo3D(gutils::GetAngleFromXZ(here, PlaceOf(HeadOf(flock)))));
		work.countdown = rules::k_ShortWait;
		return 0;
	}
	const auto store = WorkingStoreOf(villager);
	if (!store.has_value())
	{
		return 0;
	}
	const auto door = map_coords::FromMetres(villager_home::ArrivePosition(*store));
	const auto goal = rules::StoreLegGoal(here, door);
	auto final = VillagerStates::ShepherdCheckAnimalForSlaughter;
	if (rules::StoreLegFor(gutils::GetDistanceInMetres(here, door)) == rules::StoreLeg::Door)
	{
		work.countdown = rules::k_WaitAtDoor;
		final = VillagerStates::ShepherdSlaughterAnimal;
	}
	MoveTo(action, goal, final);
	// It walks at the animal's pace
	SetSpeed(villager, SpeedOf(animal));
	return 1;
}

uint32_t villager_shepherd::ShepherdSlaughterAnimal(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	auto& work = WorkOf(villager);
	--work.countdown;
	const auto flock = FlockOf(villager);
	if (work.countdown >= 1 || flock == entt::null)
	{
		return 1;
	}
	if (const auto animal = NearestAnimal(villager, flock, rules::k_TakeReach); animal != entt::null)
	{
		// The animal leaves the flock, the shepherd takes its food, and the animal walks to it and is gone
		script_flocks::Remove(registry, animal, true);
		const auto& info =
		    Locator::infoConstants::value().animal.at(static_cast<size_t>(registry.Get<const Animal>(animal).type));
		auto& person = registry.Get<Villager>(villager);
		const auto picked =
		    villager_carry::PickUp(person, ResourceType::Food, rules::SlaughterFood(info.foodValue, info.foodType), 0);
		if (auto* tally = registry.TryGet<TownResourceTally>(TownOf(villager)))
		{
			tally->foodCarried += static_cast<float>(picked);
		}
		if (Locator::animalSystem::has_value())
		{
			Locator::animalSystem::value().SendToSlaughter(animal, map_coords::ToMetres(PlaceOf(villager)));
		}
		WorkOf(villager).pending = VillagerStates::ArrivesAtStoragePitForDropOff;
		SetTopState(action, VillagerStates::ShepherdWaitForFlock);
	}
	WorkOf(villager).countdown = rules::k_ShortWait;
	return 1;
}

uint32_t villager_shepherd::ShepherdFetchStray(LivingAction& /*action*/)
{
	return 1;
}

bool villager_shepherd::ExitShepherding(LivingAction& action, VillagerStates next)
{
	if (IsShepherdingState(next))
	{
		return false;
	}
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	if (const auto flock = FlockOf(villager); flock != entt::null)
	{
		// The flock's place goes back, the shepherd leaves it and no one herds it
		MoveFlockPlace(flock, registry.Get<const Flock>(flock).savedPlace);
		script_flocks::Remove(registry, villager, true);
		if (script_flocks::IsFlock(registry, flock))
		{
			registry.Get<Flock>(flock).shepherd = entt::null;
		}
	}
	if (auto* membership = registry.TryGet<FlockMember>(villager))
	{
		membership->flock = entt::null;
	}
	registry.Remove<ShepherdWork>(villager);
	// Leaving is never held up
	return false;
}
