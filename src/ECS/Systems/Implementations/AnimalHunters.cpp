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
#include <limits>
#include <vector>

#include "AnimalSystem.h"
#include "AnimalSystemDetail.h"
#include "Animals/HunterRules.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::animal_detail;
namespace grazers = openblack::animals::grazers;
namespace hunters = openblack::animals::hunters;

namespace
{
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

/// The hour of the day as the sky shows it
float VisualHours()
{
	return Locator::skyClockSystem::has_value() ? Locator::skyClockSystem::value().GetClock().GetVisualTime() : 0.0f;
}

/// Whether a sleeping cell is one: the cell at the corner of the map counts as none
bool HasSleepingCell(const Hunter& hunter)
{
	return hunter.sleepingCell.has_value() && *hunter.sleepingCell != glm::ivec2(0);
}
} // namespace

entt::entity AnimalSystem::CreateHunter(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flockEntity)
{
	auto& registry = EntityRegistry();
	if (!hunters::IsWildHunter(type) || !OnMap(position))
	{
		return entt::null;
	}
	const auto& info = InfoOf(type);
	const bool joins = registry.Valid(flockEntity) && registry.AllOf<Flock>(flockEntity);
	// At no age it takes a random one, younger joining a flock
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
	animal.move.speed = SpeedStateOf(info, hunters::k_UsualSpeed);
	SetTopState(animal, AnimalState::DecideWhatToDo);
	SyncWorld(animal);
	animal.previousPosition = animal.position;
	animal.previousHeading = animal.heading;
	// Lit by the land where it stands, as other things are
	registry.Assign<AnimalPose>(entity, AnimalPose {.light = AnimalLight::Land});
	animal.birthTurn = villager_age::BirthTurnFor(GameTurn(), bornAt);
	registry.Assign<Hunter>(entity);
	if (joins)
	{
		// It goes first in the line of its flock. No town keeps hunters, so the flock no longer belongs to a town
		animal.flock = flockEntity;
		auto& flock = registry.Get<Flock>(flockEntity);
		flock.members.insert(flock.members.begin(), entity);
		flock.town = entt::null;
		flock.most = std::max(flock.most, static_cast<uint32_t>(flock.members.size()));
	}
	else
	{
		// Alone, it is a flock of its own about where it is, as far as its kind wanders, and it chooses its lair
		flockEntity = CreateFlock(position, static_cast<float>(info.domainRadius),
		                          static_cast<float>(static_cast<int32_t>(info.flockDistance)));
		animal.flock = flockEntity;
		auto& flock = registry.Get<Flock>(flockEntity);
		flock.members.insert(flock.members.begin(), entity);
		ChooseLair(entity, animal);
		auto& own = registry.Get<Flock>(flockEntity);
		own.most = std::max(own.most, static_cast<uint32_t>(own.members.size()));
	}
	// It sleeps in the cell of its flock's home, and belongs to no town
	registry.Get<Hunter>(entity).sleepingCell = map_coords::Cell(registry.Get<const Flock>(flockEntity).place);
	animal.town = entt::null;
	return entity;
}

void AnimalSystem::ChooseLair(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	if (!registry.AllOf<Flock>(animal.flock))
	{
		return;
	}
	// The flock's home moves by the rule of its kind
	const bool leader = LeaderOf(animal.flock) == entity;
	const auto position = registry.Get<const Transform>(entity).position;
	switch (physics::living::LairOnLanding(animal.type, leader))
	{
	case physics::living::LandedLair::WhereItLanded:
		SetFlockCentre(animal.flock, Xz(position));
		break;
	case physics::living::LandedLair::ForestOfItsKind:
	{
		// Tigers and wolves choose a forest. A wolf chooses only when it leads its flock, and it keeps to where it is while
		// a script holds it. (A flag of the wolf's flock that stops the choice is not known in openblack.)
		if (animal.type == AnimalInfo::Wolf && !leader)
		{
			break;
		}
		std::optional<glm::vec3> lair;
		if (animal.type == AnimalInfo::Wolf && registry.AllOf<InScript>(entity))
		{
			lair = position;
		}
		else if (Locator::forestSystem::has_value())
		{
			lair = Locator::forestSystem::value().ForestLair(animal.type, position);
		}
		if (lair.has_value())
		{
			SetFlockCentre(animal.flock, Xz(*lair));
		}
		break;
	}
	case physics::living::LandedLair::Unchanged:
		break;
	}
}

HuntState* AnimalSystem::HuntOf(entt::entity hunter)
{
	auto& registry = EntityRegistry();
	if (auto* wolf = registry.TryGet<SpellWolf>(hunter))
	{
		return &wolf->hunt;
	}
	if (auto* wild = registry.TryGet<Hunter>(hunter))
	{
		return &wild->hunt;
	}
	return nullptr;
}

void AnimalSystem::Fed(entt::entity hunter, Animal& animal)
{
	animal.hunger = 0;
	if (auto* wild = EntityRegistry().TryGet<Hunter>(hunter))
	{
		wild->needs.hunger = 0;
	}
}

bool AnimalSystem::IsHungry(entt::entity hunter, const Animal& animal) const
{
	const auto limit = static_cast<int32_t>(InfoOf(animal.type).hunger);
	if (const auto* wild = EntityRegistry().TryGet<const Hunter>(hunter))
	{
		return limit != 0 && limit <= wild->needs.hunger;
	}
	return animal.hunger >= limit;
}

void AnimalSystem::SetHunterSpeed(entt::entity hunter, Animal& animal, size_t speed)
{
	// A spell wolf keeps the speed of its run
	if (EntityRegistry().AllOf<SpellWolf>(hunter))
	{
		return;
	}
	SetGrazerSpeed(animal, SpeedStateOf(InfoOf(animal.type), speed));
}

bool AnimalSystem::ReactToHunterNeeds(entt::entity entity, Animal& animal, Hunter& hunter)
{
	const auto& info = InfoOf(animal.type);
	auto& registry = EntityRegistry();
	if (animal.type == AnimalInfo::Wolf)
	{
		// A wolf sees to its first need only
		switch (hunters::FirstNeed(hunter.needs, LimitsOf(info), HerdSizeOf(animal)))
		{
		case hunters::Need::Hunger:
			return ReactToFoodNeeds(entity, animal);
		case hunters::Need::Sleep:
		{
			// It sleeps at the home of its pack
			const auto* flock = registry.TryGet<const Flock>(animal.flock);
			if (flock == nullptr)
			{
				return false;
			}
			SetupMoveToFixed(animal, glm::ivec2(flock->place.x, flock->place.z), 0.0f, AnimalState::Sleeps);
			SetHunterSpeed(entity, animal, hunters::k_UsualSpeed);
			return true;
		}
		case hunters::Need::Breed:
			SetTopState(animal, AnimalState::GivesBirth);
			return true;
		case hunters::Need::None:
		default:
			return false;
		}
	}
	// A big cat hunts once hungry. If it starts nothing, it sleeps once sleepy, where its flock sleeps
	if (info.hunger != 0 && static_cast<int32_t>(info.hunger) <= hunter.needs.hunger && ReactToFoodNeeds(entity, animal))
	{
		return true;
	}
	if (info.sleep != 0 && static_cast<int32_t>(info.sleep) <= hunter.needs.sleep && HasSleepingCell(hunter))
	{
		SetTopState(animal, AnimalState::SeekSleep);
		return true;
	}
	return false;
}

void AnimalSystem::HunterDecide(entt::entity entity, Animal& animal, Hunter& hunter)
{
	auto& registry = EntityRegistry();
	const auto& info = InfoOf(animal.type);
	if (animal.type == AnimalInfo::Wolf)
	{
		// A wolf with no pack does nothing. One that sees to no need goes to its pack's lair to hide there
		const auto* flock = registry.TryGet<const Flock>(animal.flock);
		if (flock == nullptr || ReactToHunterNeeds(entity, animal, hunter))
		{
			return;
		}
		const auto goal = hunters::RandomPointInSquare(glm::ivec2(flock->place.x, flock->place.z),
		                                               static_cast<float>(flock->members.size()), Random);
		SetupMoveToFixed(animal, goal, 0.0f, AnimalState::HideInLair);
		SetHunterSpeed(entity, animal, hunters::k_UsualSpeed);
		return;
	}
	// A big cat gives birth when its need to breed is full and its flock has lost members. It keeps the need.
	if (hunters::FirstNeed(hunter.needs, LimitsOf(info), HerdSizeOf(animal)) == hunters::Need::Breed)
	{
		SetTopState(animal, AnimalState::GivesBirth);
		return;
	}
	// Late at night it sleeps for as long as its kind can sleep
	if (hunters::CatsSleepAt(VisualHours()))
	{
		SetTopState(animal, AnimalState::SeekSleep);
		hunter.needs.sleep = static_cast<int16_t>(info.sleep);
		return;
	}
	if (info.flocksCanMerge != 0)
	{
		MergeNearbyHerd(entity, animal);
	}
	if (KeepLeaderWithinDomain(entity, animal) || KeepMemberWithinFlockArea(entity, animal))
	{
		return;
	}
	if (!ReactToHunterNeeds(entity, animal, hunter))
	{
		SetTopState(animal, AnimalState::StartWander);
	}
}

void AnimalSystem::HideInLair(entt::entity entity, Animal& animal, Hunter& hunter)
{
	auto& registry = EntityRegistry();
	// It rests while it is sleepy
	if (hunter.needs.sleep > 0)
	{
		hunter.needs.sleep = static_cast<int16_t>(hunter.needs.sleep - 2);
		return;
	}
	// The leader of a pack that a script holds hunts from the lair
	if (registry.AllOf<Flock>(animal.flock) && LeaderOf(animal.flock) == entity && registry.AllOf<InScript>(entity) &&
	    ReactToFoodNeeds(entity, animal))
	{
		return;
	}
	// Hungry, late at night, the pack goes out
	const auto& info = InfoOf(animal.type);
	if (hunters::FirstNeed(hunter.needs, LimitsOf(info), HerdSizeOf(animal)) != hunters::Need::Hunger ||
	    !hunters::WolvesHuntAt(VisualHours()))
	{
		return;
	}
	const auto* own = registry.TryGet<const Flock>(animal.flock);
	if (own == nullptr)
	{
		return;
	}
	// It goes to a flock with a leader no stronger than itself, if one is near enough. In the order the flocks were made
	std::vector<std::pair<uint32_t, entt::entity>> order;
	registry.Each<const Flock>([&order](entt::entity flock, const Flock& data) { order.emplace_back(data.made, flock); });
	std::ranges::sort(order);
	std::vector<hunters::FlockSeen> seen;
	std::vector<entt::entity> flocks;
	const map_coords::MapCoords here {.x = animal.move.position.x, .z = animal.move.position.y};
	for (const auto& [made, flock] : order)
	{
		const auto leader = LeaderOf(flock);
		const auto* leaderAnimal = registry.Valid(leader) ? registry.TryGet<const Animal>(leader) : nullptr;
		if (leaderAnimal == nullptr)
		{
			continue;
		}
		const auto& data = registry.Get<const Flock>(flock);
		seen.push_back({.leaderStrength = InfoOf(leaderAnimal->type).strength,
		                .height = data.place.altitude,
		                .distance = gutils::GetDistance(here, data.place)});
		flocks.push_back(flock);
	}
	glm::ivec2 centre;
	if (const auto goal = hunters::PackGoal(seen, info.strength, own->domainRadius))
	{
		const auto& target = registry.Get<const Flock>(flocks.at(*goal)).place;
		centre = {target.x, target.z};
	}
	else
	{
		// Else it goes to the nearest town of all, by the map distance in whole map units. The towns go in the order of
		// the game's list of all the towns, and the first of equals stays.
		entt::entity town = entt::null;
		int32_t nearest = 0;
		for (const auto each : ecs::player_towns::AllMadeNewestFirst(registry))
		{
			const auto* at = registry.TryGet<const Transform>(each);
			if (at == nullptr)
			{
				continue;
			}
			const auto distance = gutils::GetDistance(here, map_coords::FromMetres({at->position.x, at->position.z}));
			if (town == entt::null || distance < nearest)
			{
				town = each;
				nearest = distance;
			}
		}
		const auto* transform = registry.Valid(town) ? registry.TryGet<const Transform>(town) : nullptr;
		if (transform == nullptr)
		{
			return;
		}
		centre = Fixed(Xz(transform->position));
	}
	SetHunterSpeed(entity, animal, hunters::k_UsualSpeed);
	const auto point = hunters::RandomPointInSquare(centre, static_cast<float>(own->members.size()), Random);
	SetupMoveToFixed(animal, point, 0.0f, AnimalState::StartWander);
}

void AnimalSystem::SeekSleep(Animal& animal, std::optional<glm::ivec2> sleepingCell)
{
	// With no sleeping place it wanders off. Else it lies down about its flock's sleeping cell
	if (!sleepingCell.has_value() || *sleepingCell == glm::ivec2(0))
	{
		SetTopState(animal, AnimalState::StartWander);
		return;
	}
	const auto* flock = EntityRegistry().TryGet<const Flock>(animal.flock);
	const auto spot =
	    grazers::SleepSpot(*sleepingCell, flock != nullptr ? static_cast<uint32_t>(flock->members.size()) : 0, Random);
	SetupMoveTo(animal, Metres(spot), 0.0f, AnimalState::Sleeps);
}

void AnimalSystem::HunterTurn(entt::entity entity, Animal& animal, Hunter& hunter)
{
	// Its reaction comes before its own ways
	if (Flee(entity, animal) || LookOrGoToFood(entity, animal))
	{
		return;
	}
	switch (animal.state)
	{
	case AnimalState::DecideWhatToDo:
		HunterDecide(entity, animal, hunter);
		break;
	case AnimalState::StartWander:
		GrazerStartWander(entity, animal);
		break;
	case AnimalState::MoveInFlock:
		GrazerMoveInFlock(entity, animal);
		break;
	case AnimalState::Wander:
		if (!ReactToHunterNeeds(entity, animal, hunter))
		{
			WanderStep(entity, animal);
		}
		break;
	case AnimalState::MoveToPos:
		if (GroundMoveTo(entity, animal))
		{
			SetTopState(animal, animal.finalState);
		}
		break;
	case AnimalState::Chase:
		Chase(entity, animal);
		break;
	case AnimalState::Pounce:
		Pounce(entity, animal);
		break;
	case AnimalState::StartToEat:
		// So many mouthfuls at its usual speed, once its settling down has played
		animal.turnsUntilStateChange = static_cast<int16_t>(RandomWhole(hunters::k_MouthfulsRange) + hunters::k_MouthfulsMin);
		SetHunterSpeed(entity, animal, hunters::k_UsualSpeed);
		WaitForClip(animal, AnimalState::Eat);
		break;
	case AnimalState::Eat:
		Eat(entity, animal);
		break;
	case AnimalState::FinishEating:
		WaitForClip(animal, AnimalState::DecideWhatToDo);
		break;
	case AnimalState::SeekSleep:
		SeekSleep(animal, hunter.sleepingCell);
		break;
	case AnimalState::Sleeps:
		if (grazers::SleepTurn(hunter.needs))
		{
			SetTopState(animal, AnimalState::StartWander);
		}
		break;
	case AnimalState::HideInLair:
		if (animal.type == AnimalInfo::Wolf)
		{
			HideInLair(entity, animal, hunter);
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
		// Up again, it joins a flock nearby if it can, then wanders off
		if (EntityRegistry().AllOf<Flock>(animal.flock))
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
