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
#include <chrono>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "AnimalSystem.h"
#include "AnimalSystemDetail.h"
#include "Animals/AnimalRules.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Tree.h"
#include "ECS/Map.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::animal_detail;
namespace grazers = openblack::animals::grazers;

namespace
{
/// A grazer's speeds: its usual one is the first of its kind's
constexpr size_t k_UsualSpeed = 0;
/// Herds look for others to merge with over twice their kind's reach
constexpr uint32_t k_MergeReachTimes = 2;
constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());

uint32_t GameTurn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

grazers::NeedLimits LimitsOf(const GAnimalInfo& info)
{
	return {.hunger = info.hunger, .sleep = info.sleep, .breed = info.needToBreed, .grownUpAge = info.grownUpAge};
}

/// Whether a cell holds a thing that stays put: a building, field, feature, pile and the like, or a tree of a forest
bool HasFixed(glm::vec2 point)
{
	if (!Locator::entitiesMap::has_value())
	{
		return false;
	}
	const auto& registry = EntityRegistry();
	const auto cell = map_coords::CellOf(point);
	const auto fixed = Locator::entitiesMap::value().GetFixedInGridCell(glm::u16vec2(cell));
	// A tree on its own is no fixed thing to the game; a tree of a forest is
	return std::ranges::any_of(fixed, [&registry](entt::entity entity) {
		return registry.Valid(entity) && (!registry.AllOf<Tree>(entity) || registry.AllOf<ForestMember>(entity));
	});
}

glm::ivec2 CellOf(glm::ivec2 position)
{
	return {position.x >> 16, position.y >> 16};
}
} // namespace

entt::entity AnimalSystem::CreateGrazer(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flockEntity,
                                        entt::entity town)
{
	auto& registry = EntityRegistry();
	if (!grazers::IsGrazer(type) || !OnMap(position))
	{
		return entt::null;
	}
	const auto& info = InfoOf(type);
	const bool joins = registry.Valid(flockEntity) && registry.AllOf<Flock>(flockEntity);
	// At no age it takes a random one, younger joining a herd
	const auto bornAt = animals::birds::ScriptBirdAge(age, joins, RandomWhole);
	// The day it was born a random part of its starting age back, then its size for its age
	(void)RandomWhole(info.startAge / 2);
	const float scale = animals::BirthScale(bornAt, info.grownUpAge, info.ageToScale.values, Random);
	// It stands on the land, facing along +x, at its usual speed, deciding what to do
	const glm::vec3 point(position.x, Ground(position), position.y);
	const auto entity =
	    ecs::archetypes::AnimalArchetype::Create(type, point, gutils::ConvertGameAngleTo3D(0), scale, PlayerNames::NEUTRAL);
	auto& animal = registry.Get<Animal>(entity);
	animal.move.position = Fixed(position);
	animal.move.goal = animal.move.position;
	animal.move.angle = 0;
	animal.move.speed = SpeedStateOf(info, k_UsualSpeed);
	SetTopState(animal, AnimalState::DecideWhatToDo);
	SyncWorld(animal);
	animal.previousPosition = animal.position;
	animal.previousHeading = animal.heading;
	// Lit by the land where it stands, as other things are
	registry.Assign<AnimalPose>(entity, AnimalPose {.light = AnimalLight::Land});
	auto& grazer = registry.Assign<Grazer>(entity);
	grazer.birthTurn = villager_age::BirthTurnFor(GameTurn(), bornAt);
	// Alone, it is a herd of its own about where it is, as far as its kind wanders
	if (!joins)
	{
		flockEntity = CreateFlock(position, static_cast<float>(info.domainRadius),
		                          static_cast<float>(static_cast<int32_t>(info.flockDistance)));
		registry.Get<Flock>(flockEntity).town = town;
	}
	animal.flock = flockEntity;
	auto& herd = registry.Get<Flock>(flockEntity);
	herd.members.insert(herd.members.begin(), entity);
	herd.most = std::max(herd.most, static_cast<uint32_t>(herd.members.size()));
	// It sleeps where its herd's home is, and belongs to the town it was made for
	grazer.sleepingCell = map_coords::Cell(herd.place);
	animal.town = town;
	return entity;
}

void AnimalSystem::SetFlockTown(entt::entity flock, entt::entity town)
{
	if (auto* data = EntityRegistry().TryGet<Flock>(flock))
	{
		data->town = town;
	}
}

glm::ivec2 AnimalSystem::HerdPosOf(entt::entity flock) const
{
	const auto& registry = EntityRegistry();
	const auto leader = LeaderOf(flock);
	if (const auto* data = registry.Valid(leader) ? registry.TryGet<const Animal>(leader) : nullptr)
	{
		return data->move.position;
	}
	const auto* herd = registry.TryGet<const Flock>(flock);
	return herd != nullptr ? glm::ivec2(herd->place.x, herd->place.z) : glm::ivec2(0);
}

bool AnimalSystem::WithinDomain(const Animal& animal, glm::ivec2 point) const
{
	const auto* herd = EntityRegistry().TryGet<const Flock>(animal.flock);
	if (herd == nullptr)
	{
		return false;
	}
	return !(gutils::GetDistanceInMetres(glm::ivec2(herd->place.x, herd->place.z), point) >
	         static_cast<float>(herd->domainRadius));
}

void AnimalSystem::SetGrazerSpeed(Animal& animal, uint16_t speed)
{
	animal.move.speed = speed;
	// Its clip is the one of its state at its new speed, a new one starting from its beginning
	const auto clip = ClipFor(animal, animal.state);
	if (clip != animal.animation)
	{
		animal.animation = clip;
		animal.clipPlace = 0;
	}
}

bool AnimalSystem::GroundMoveTo(Animal& animal)
{
	const auto result = animals::StepMove(animal.move, TurnAngleOf(animal));
	animal.height = 0.0f;
	SyncWorld(animal);
	return result.arrived;
}

std::optional<grazers::HerdSize> AnimalSystem::HerdSizeOf(const Animal& animal) const
{
	const auto* herd = EntityRegistry().TryGet<const Flock>(animal.flock);
	if (herd == nullptr)
	{
		return std::nullopt;
	}
	return grazers::HerdSize {.members = static_cast<uint32_t>(herd->members.size()), .most = herd->most};
}

uint32_t AnimalSystem::AgeOf(const Grazer& grazer) const
{
	return villager_age::AgeOf(GameTurn(), grazer.birthTurn);
}

void AnimalSystem::SetNewWander(entt::entity entity, Animal& animal, std::optional<glm::ivec2> centre, int32_t inner,
                                int32_t outer)
{
	const auto& registry = EntityRegistry();
	std::vector<grazers::HerdMate> mates;
	uint16_t flockDistance = 0;
	if (const auto* herd = registry.TryGet<const Flock>(animal.flock))
	{
		flockDistance = herd->flockDistance;
		// The newest member first
		for (const auto member : herd->members)
		{
			const auto* mate = member != entity && registry.Valid(member) ? registry.TryGet<const Animal>(member) : nullptr;
			if (mate != nullptr)
			{
				mates.push_back({.position = mate->move.position, .step = mate->move.step});
			}
		}
	}
	animal.move.step = grazers::NewWanderStep({.position = animal.move.position,
	                                           .angle = animal.move.angle,
	                                           .speed = animal.move.speed,
	                                           .turnAngle = TurnAngleOf(animal),
	                                           .centre = centre,
	                                           .inner = inner,
	                                           .outer = outer,
	                                           .flockDistance = flockDistance},
	                                          mates, RandomWhole);
	// It faces the way it steps
	animal.move.angle = gutils::GetAngleFromDXDZ(animal.move.step.x, animal.move.step.y);
	SyncWorld(animal);
}

bool AnimalSystem::KeepLeaderWithinDomain(entt::entity entity, Animal& animal)
{
	auto* herd = EntityRegistry().TryGet<Flock>(animal.flock);
	if (herd == nullptr || LeaderOf(animal.flock) != entity)
	{
		return false;
	}
	// Out of its herd's reach, or there its kind's stay time, the leader takes the herd somewhere new about its home
	const auto& info = InfoOf(animal.type);
	if (WithinDomain(animal, HerdPosOf(animal.flock)) && herd->turnsOnLeg < info.stayTime)
	{
		return false;
	}
	const auto goal = RandomPos(animal, map_coords::ToMetres(herd->place), static_cast<float>(info.domainInnerRadius),
	                            static_cast<float>(herd->domainRadius));
	SetupMoveTo(animal, goal, 0.0f, AnimalState::DecideWhatToDo);
	herd->turnsOnLeg = 0;
	return true;
}

bool AnimalSystem::KeepMemberWithinFlockArea(entt::entity /*entity*/, Animal& animal)
{
	const auto* herd = EntityRegistry().TryGet<const Flock>(animal.flock);
	if (herd == nullptr)
	{
		return false;
	}
	const auto leader = HerdPosOf(animal.flock);
	const auto distance = static_cast<float>(herd->flockDistance);
	if (WithinDomain(animal, animal.move.position) && !(gutils::GetDistanceInMetres(leader, animal.move.position) > distance))
	{
		return false;
	}
	// Too far from its leader, it goes back to somewhere near it, unless that is out of the herd's reach while the
	// leader is within it; either way it is busy this turn
	const auto goal = RandomPos(animal, Metres(leader), 0.0f, distance);
	if (WithinDomain(animal, Fixed(goal)) || !WithinDomain(animal, leader))
	{
		SetupMoveTo(animal, goal, 0.0f, AnimalState::DecideWhatToDo);
	}
	return true;
}

std::optional<glm::ivec2> AnimalSystem::LookForGrazeSpot(entt::entity entity, const Animal& animal) const
{
	const auto& registry = EntityRegistry();
	const auto* herd = registry.TryGet<const Flock>(animal.flock);
	const auto& info = InfoOf(animal.type);
	const auto here = animal.move.position;
	const auto ownCell = CellOf(here);
	const auto suits = [&](glm::ivec2 point) {
		const auto cell = CellOf(point);
		const auto metres = Metres(point);
		if (!WithinDomain(animal, point) || !OnMap(metres) || cell == ownCell)
		{
			return false;
		}
		// Ahead of it, within half its view
		if (gutils::GetAngleDifference(animal.move.angle, gutils::GetAngleFromXZ(here, point)) >
		    static_cast<uint32_t>(static_cast<int32_t>(info.viewAngle) / 2))
		{
			return false;
		}
		// Where no other of its herd stands or is going
		if (herd != nullptr)
		{
			for (const auto member : herd->members)
			{
				const auto* mate = member != entity && registry.Valid(member) ? registry.TryGet<const Animal>(member) : nullptr;
				if (mate != nullptr && (CellOf(mate->move.position) == cell || CellOf(mate->move.goal) == cell))
				{
					return false;
				}
			}
		}
		return !CellCollides(metres, static_cast<uint32_t>(info.collideType)) && !HasFixed(metres);
	};
	return grazers::FindGrazeSpot(here, herd != nullptr ? herd->domainRadius : 0, suits);
}

bool AnimalSystem::ReactToGrazerNeeds(entt::entity entity, Animal& animal, Grazer& grazer)
{
	const auto& info = InfoOf(animal.type);
	switch (
	    grazers::NeedToSee(grazer.needs, LimitsOf(info), AgeOf(grazer), HerdSizeOf(animal), grazer.sleepingCell.has_value()))
	{
	case grazers::Need::Breed:
		SetTopState(animal, AnimalState::GivesBirth);
		return true;
	case grazers::Need::Graze:
	{
		const auto spot = LookForGrazeSpot(entity, animal);
		if (!spot.has_value())
		{
			return false;
		}
		SetGrazerSpeed(animal, SpeedStateOf(info, k_UsualSpeed));
		SetupMoveTo(animal, Metres(*spot), 0.0f, AnimalState::StartToEat);
		return true;
	}
	case grazers::Need::Sleep:
		SetTopState(animal, AnimalState::SeekSleep);
		return true;
	case grazers::Need::None:
	default:
		return false;
	}
}

void AnimalSystem::GrazerStartWander(entt::entity entity, Animal& animal)
{
	// Off along the way it faces, at its usual speed, steered as it sets off
	animal.move.step = animals::StepAlong(animal.move.angle, animal.move.speed);
	SetGrazerSpeed(animal, SpeedStateOf(InfoOf(animal.type), k_UsualSpeed));
	SetTopState(animal, AnimalState::Wander);
	const auto* herd = EntityRegistry().TryGet<const Flock>(animal.flock);
	std::optional<glm::ivec2> centre;
	if (herd != nullptr)
	{
		centre = HerdPosOf(animal.flock);
	}
	SetNewWander(entity, animal, centre, static_cast<int32_t>(InfoOf(animal.type).domainInnerRadius),
	             herd != nullptr ? herd->domainRadius : 0);
}

void AnimalSystem::GrazerWander(entt::entity entity, Animal& animal, Grazer& grazer)
{
	if (ReactToGrazerNeeds(entity, animal, grazer))
	{
		return;
	}
	// Out of its herd's reach it decides again
	if (!WithinDomain(animal, animal.move.position))
	{
		SetTopState(animal, AnimalState::DecideWhatToDo);
		return;
	}
	// A straight step, steered afresh as it crosses into a new cell
	const auto before = CellOf(animal.move.position);
	animal.move.position += animal.move.step;
	SyncWorld(animal);
	if (CellOf(animal.move.position) != before)
	{
		const auto* herd = EntityRegistry().TryGet<const Flock>(animal.flock);
		SetNewWander(entity, animal, herd != nullptr ? std::optional(HerdPosOf(animal.flock)) : std::nullopt, 0,
		             herd != nullptr ? herd->flockDistance : 0);
		if (InfoOf(animal.type).flocksCanMerge != 0)
		{
			MergeNearbyHerd(entity, animal);
		}
	}
}

void AnimalSystem::GrazerDecide(entt::entity entity, Animal& animal, Grazer& grazer)
{
	const auto& info = InfoOf(animal.type);
	if (grazers::ReadyToBreed(grazer.needs, LimitsOf(info), HerdSizeOf(animal)))
	{
		grazer.needs.breed = 0;
		SetTopState(animal, AnimalState::GivesBirth);
		return;
	}
	if (info.flocksCanMerge != 0)
	{
		MergeNearbyHerd(entity, animal);
	}
	if (EntityRegistry().AllOf<Flock>(animal.flock) &&
	    (KeepLeaderWithinDomain(entity, animal) || KeepMemberWithinFlockArea(entity, animal)))
	{
		return;
	}
	if (!ReactToGrazerNeeds(entity, animal, grazer))
	{
		SetTopState(animal, AnimalState::StartWander);
	}
}

void AnimalSystem::GivesBirth(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	// A young one a year old joins the herd and decides what to do at once; then the mother wanders off
	const auto young = CreateGrazer(animal.type, Metres(animal.move.position), 1, animal.flock, animal.town);
	if (registry.Valid(young))
	{
		GrazerDecide(young, registry.Get<Animal>(young), registry.Get<Grazer>(young));
	}
	SetTopState(registry.Get<Animal>(entity), AnimalState::StartWander);
}

void AnimalSystem::MergeNearbyHerd(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	auto* mine = registry.TryGet<Flock>(animal.flock);
	// A herd of a town never merges into another
	if (mine == nullptr || registry.Valid(mine->town) || !Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto& info = InfoOf(animal.type);
	const auto mineEntity = animal.flock;
	const bool mineScripted = registry.AllOf<ScriptControlled>(mineEntity);
	const float reach = static_cast<float>(info.domainRadius * k_MergeReachTimes) / 10.0f;
	auto cells = map_coords::FtoL(std::max(reach * reach, 1.0f));
	map_coords::Spiral spiral;
	auto point = animal.move.position;
	for (; cells > 0; --cells)
	{
		if (OnMap(Metres(point)))
		{
			for (const auto other : Locator::entitiesMap::value().GetMobileInGridCell(glm::u16vec2(CellOf(point))))
			{
				const auto* them = other != entity && registry.Valid(other) ? registry.TryGet<const Animal>(other) : nullptr;
				if (them == nullptr || them->Dead() || them->type != animal.type || them->flock == mineEntity ||
				    !registry.Valid(them->flock))
				{
					continue;
				}
				auto& theirs = registry.Get<Flock>(them->flock);
				const bool theirsScripted = registry.AllOf<ScriptControlled>(them->flock);
				if ((theirsScripted && mineScripted) || mine->members.size() + theirs.members.size() > info.maxFlockSize)
				{
					continue;
				}
				// The bigger keeps them all, unless the other is a script's
				const bool keepMine = grazers::MergeKeepsLooker(static_cast<uint32_t>(mine->members.size()),
				                                                static_cast<uint32_t>(theirs.members.size()), theirsScripted);
				const auto keeperEntity = keepMine ? mineEntity : them->flock;
				auto& keeper = keepMine ? *mine : theirs;
				auto& gone = keepMine ? theirs : *mine;
				const auto joining = gone.members;
				keeper.most =
				    grazers::MergedMost(keeper.most, gone.most, static_cast<uint32_t>(joining.size()), info.maxFlockSize);
				for (const auto member : joining)
				{
					JoinFlock(member, keeperEntity);
				}
				return;
			}
		}
		const auto& next = spiral.Next();
		point += glm::ivec2(next.x, next.z) * map_coords::k_FixedPerCell;
	}
}

void AnimalSystem::GrazerTurn(entt::entity entity, Animal& animal, Grazer& grazer)
{
	auto& registry = EntityRegistry();
	const auto& info = InfoOf(animal.type);
	// Seeing to its needs, and growing a quarter of a year at a time while young
	if (SeesToNeedsIn(animal.state))
	{
		const auto age = AgeOf(grazer);
		grazers::GrowNeeds(grazer.needs, LimitsOf(info), age, HerdSizeOf(animal));
		if (auto* herd = registry.TryGet<Flock>(animal.flock); herd != nullptr && LeaderOf(animal.flock) == entity)
		{
			++herd->turnsOnLeg;
		}
		if (age < info.grownUpAge && GameTurn() % villager_age::k_GrowthTurns == 0)
		{
			auto& transform = registry.Get<Transform>(entity);
			SetScale(entity, grazers::GrownScale(transform.scale.x, age, info.ageToScale.values, Random));
		}
	}
	// Fleeing what it reacts to comes before its own ways
	if (Flee(entity, animal))
	{
		return;
	}
	switch (animal.state)
	{
	case AnimalState::DecideWhatToDo:
		GrazerDecide(entity, animal, grazer);
		break;
	case AnimalState::StartWander:
	case AnimalState::MoveInFlock:
		GrazerStartWander(entity, animal);
		break;
	case AnimalState::Wander:
		GrazerWander(entity, animal, grazer);
		break;
	case AnimalState::MoveToPos:
		if (GroundMoveTo(animal))
		{
			SetTopState(animal, animal.finalState);
		}
		break;
	case AnimalState::StartToEat:
		// The common animal's meal is drawn and then its own, so many grazing clips once its head is down
		(void)RandomWhole(grazers::k_CommonMealsRange);
		SetGrazerSpeed(animal, SpeedStateOf(info, k_UsualSpeed));
		grazer.meals = static_cast<int16_t>(RandomWhole(grazers::k_MealsRange) + grazers::k_MealsMin);
		WaitForClip(animal, AnimalState::Eat);
		break;
	case AnimalState::Eat:
		--grazer.meals;
		if (grazer.meals == 0)
		{
			WaitForClip(animal, AnimalState::FinishEating);
			grazer.needs.hunger = 0;
		}
		else
		{
			WaitForClip(animal, AnimalState::Eat);
		}
		break;
	case AnimalState::FinishEating:
		WaitForClip(animal, AnimalState::DecideWhatToDo);
		break;
	case AnimalState::SeekSleep:
	{
		if (!grazer.sleepingCell.has_value())
		{
			SetTopState(animal, AnimalState::StartWander);
			break;
		}
		const auto* herd = registry.TryGet<const Flock>(animal.flock);
		const auto spot =
		    grazers::SleepSpot(*grazer.sleepingCell, herd != nullptr ? static_cast<uint32_t>(herd->members.size()) : 0, Random);
		SetupMoveTo(animal, Metres(spot), 0.0f, AnimalState::Sleeps);
		break;
	}
	case AnimalState::Sleeps:
		if (grazers::SleepTurn(grazer.needs))
		{
			SetTopState(animal, AnimalState::StartWander);
		}
		break;
	case AnimalState::GivesBirth:
		GivesBirth(entity, animal);
		break;
	case AnimalState::WaitForClip:
		if (animal.turnsInState * k_TurnMilliseconds >= PlayTimeOf(animal.animation))
		{
			SetTopState(animal, animal.afterClip);
		}
		break;
	case AnimalState::InteractDecideWhatToDo:
		// Up again, it joins a herd nearby if it can, then wanders off
		if (registry.AllOf<Flock>(animal.flock))
		{
			MergeNearbyHerd(entity, animal);
		}
		GrazerStartWander(entity, animal);
		break;
	default:
		// Held by a script, or watching what it fled: it keeps still
		break;
	}
}
