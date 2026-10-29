/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AnimalSystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "AnimalSystemDetail.h"
#include "Animals/AnimalAnimation.h"
#include "Animals/AnimalMove.h"
#include "Animals/AnimalRules.h"
#include "Animals/BirdRules.h"
#include "Animals/PredatorRules.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/BeingEatenRules.h"
#include "ECS/ClipSoundPlayer.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlockRules.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ReactionRules.h"
#include "ObjectMeasures.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace living = openblack::physics::living;
// Not "flock": on Linux that is also a function from <sys/file.h>
namespace flock_rules = openblack::magic::flock;
using namespace openblack::ecs::systems::animal_detail;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());
/// Game turns a second
constexpr float k_TurnsPerSecond = 1000.0f / static_cast<float>(k_TurnMilliseconds);
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A spell wolf eases into its tilt over a second, tilting by its speed times its turn, to at most half a radian
constexpr float k_WolfTimeToBank = 1.0f;
constexpr float k_WolfBankPerSpeedTurn = 0.5f / 20000.0f;
constexpr float k_WolfMostBank = 0.5f;
/// A wolf's run takes the fifth of its kind's speeds
constexpr size_t k_WolfRunSpeed = 4;
/// A bird on its special move within this many metres of its goal takes its usual speed
constexpr float k_SpecialMoveSlowing = 6.0f;
/// A wolf leaps at its prey from half its leap's stride (scaled) within its attack distance, else from this close
constexpr float k_PounceFromFar = 0.5f;
/// It leaps only at prey within this many game angles of the way it faces
constexpr uint32_t k_PounceAngle = 0x80;
/// A speed state times this is metres a second
constexpr float k_SpeedStateToMetresPerSecond = 100.0f / 65536.0f;
/// A wolf eats this many mouthfuls, and up to this many more
constexpr uint32_t k_MouthfulsMin = 15;
constexpr uint32_t k_MouthfulsRange = 10;
/// An animal a miracle makes is born up to this many years past this age
constexpr uint32_t k_BirthAgeRange = 20;
constexpr uint32_t k_BirthAgeMin = 5;
/// How a flock's followers follow: in formation
constexpr int k_FollowInFormation = 3;
/// A challenge script's animal on its own keeps within 2 m of where it was made, its flock's followers within 1 m
constexpr float k_ScriptAnimalFlockReach = 2.0f;
constexpr float k_ScriptAnimalFlockDistance = 1.0f;
/// It belongs to the nearest town within this many metres
constexpr float k_ScriptAnimalTownReach = 500.0f;

} // namespace

/// By the game's table of the animals' states
void AnimalSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _turn);
	reflection::Save(out, _flocksMade);
	reflection::Save(out, _soundPace);
}

bool AnimalSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _turn);
	reflection::Load(in, _flocksMade);
	reflection::Load(in, _soundPace);
	return !in.Failed();
}

bool AnimalSystem::SeesToNeedsIn(AnimalState state)
{
	return Locator::infoConstants::value().animalStateTable.at(GameStateOf(state)).field0xa4 != 0;
}

namespace
{
/// A newcomer goes first in a flock's line, so the first to join stays last, and leads
void JoinLine(Flock& flock, entt::entity animal)
{
	flock.members.insert(flock.members.begin(), animal);
}
} // namespace

entt::entity AnimalSystem::CreateFlock(glm::vec2 centre, float domainRadius, float flockDistance)
{
	auto& registry = EntityRegistry();
	const auto entity = registry.Create();
	registry.Assign<Flock>(entity, Flock {.place = map_coords::FromMetres(centre),
	                                      .domainRadius = static_cast<uint16_t>(domainRadius),
	                                      .flockDistance = static_cast<uint16_t>(flockDistance),
	                                      .made = _flocksMade++,
	                                      .savedPlace = map_coords::FromMetres(centre)});
	return entity;
}

entt::entity AnimalSystem::CreateScriptFlock(int32_t id, glm::vec2 position, glm::vec2 home, float reach, float flockDistance)
{
	// Made where the script says, then its home moved to where it says; with no reach given it takes the game's
	const auto entity = CreateFlock(position, reach != 0.0f ? reach : animals::birds::k_DefaultFlockReach, flockDistance);
	auto& flockData = EntityRegistry().Get<Flock>(entity);
	flockData.scriptId = id;
	flockData.place = map_coords::FromMetres(home);
	return entity;
}

entt::entity AnimalSystem::FindScriptFlock(int32_t id) const
{
	// The latest made with the number
	entt::entity found = entt::null;
	uint32_t latest = 0;
	EntityRegistry().Each<const Flock>([&](entt::entity entity, const Flock& flockData) {
		if (flockData.scriptId == id && (found == entt::null || flockData.made > latest))
		{
			found = entity;
			latest = flockData.made;
		}
	});
	return found;
}

entt::entity AnimalSystem::CreateBird(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flockEntity)
{
	auto& registry = EntityRegistry();
	if (!animals::birds::IsLandBird(type) || !OnMap(position))
	{
		return entt::null;
	}
	const auto& info = InfoOf(type);
	const bool joins = registry.Valid(flockEntity) && registry.AllOf<Flock>(flockEntity);
	const auto bornAt = animals::birds::ScriptBirdAge(age, joins, RandomWhole);
	// The day it was born a random part of its starting age back, then its size for its age
	(void)RandomWhole(info.startAge / 2);
	const float scale = animals::BirthScale(bornAt, info.grownUpAge, info.ageToScale.values, Random);
	// It flies at its kind's height over the land, facing along +x
	const float height = info.altitudeNormal;
	const glm::vec3 point(position.x, Ground(position) + height, position.y);
	const auto entity =
	    ecs::archetypes::AnimalArchetype::Create(type, point, gutils::ConvertGameAngleTo3D(0), scale, PlayerNames::NEUTRAL);
	auto& animal = registry.Get<Animal>(entity);
	// Born as long ago as its age, which also set its size
	animal.birthTurn = villager_age::BirthTurnFor(Locator::time::value().GetTurn(), bornAt);
	animal.height = height;
	animal.goalHeight = height;
	animal.move.position = Fixed(position);
	animal.move.goal = animal.move.position;
	animal.move.angle = 0;
	animal.move.speed = SpeedStateOf(info, 0);
	SetTopState(animal, AnimalState::DecideWhatToDo);
	SyncWorld(animal);
	animal.previousPosition = animal.position;
	animal.previousHeading = animal.heading;
	// Every bird is drawn in the brightest of the land's light, wherever it is
	registry.Assign<AnimalPose>(entity, AnimalPose {.light = AnimalLight::BrightestLand});
	// Alone, it is a flock of its own about where it is, as far as its kind wanders
	if (!joins)
	{
		flockEntity = CreateFlock(position, static_cast<float>(info.domainRadius),
		                          static_cast<float>(static_cast<int32_t>(info.flockDistance)));
	}
	animal.flock = flockEntity;
	// A flock a bird joins belongs to no town, and keeps the most it has had
	auto& joined = registry.Get<Flock>(flockEntity);
	joined.town = entt::null;
	JoinLine(joined, entity);
	joined.most = std::max(joined.most, static_cast<uint32_t>(joined.members.size()));
	return entity;
}

entt::entity AnimalSystem::CreateScriptAnimal(AnimalInfo type, glm::vec2 position)
{
	// Made as a land script makes an animal with no flock, at a random age
	auto entity = entt::entity {entt::null};
	if (animals::grazers::IsGrazer(type))
	{
		entity = CreateGrazer(type, position, 0, entt::null, entt::null);
	}
	else if (animals::hunters::IsWildHunter(type))
	{
		entity = CreateHunter(type, position, 0, entt::null);
	}
	else
	{
		entity = CreateBird(type, position, 0, entt::null);
	}
	if (entity == entt::null)
	{
		return entt::null;
	}
	// Its own flock keeps it close about where it was made
	auto& registry = EntityRegistry();
	auto& flockData = registry.Get<Flock>(registry.Get<const Animal>(entity).flock);
	flockData.domainRadius = static_cast<uint16_t>(k_ScriptAnimalFlockReach);
	flockData.flockDistance = static_cast<uint16_t>(k_ScriptAnimalFlockDistance);
	// It belongs to the nearest town within 500 m
	registry.Get<Animal>(entity).town = NearestTown(position, k_ScriptAnimalTownReach);
	// The script holds it still until it says otherwise
	SetScriptState(entity, LivingStates::LivingInScript);
	return entity;
}

entt::entity AnimalSystem::CreateStillAnimal(AnimalInfo type, glm::vec3 position, float heading, float scale)
{
	auto& registry = EntityRegistry();
	const auto entity = ecs::archetypes::AnimalArchetype::Create(type, position, heading, scale, PlayerNames::NEUTRAL);
	auto& animal = registry.Get<Animal>(entity);
	animal.move.position = Fixed(glm::vec2(position.x, position.z));
	animal.move.goal = animal.move.position;
	// It stands in the clip of an animal that a script holds still
	SetTopState(animal, AnimalState::InScript);
	// The miracle's wolf has a white light and every bird has the brightest light of the land.
	// The other kinds have the light of the land where they stand.
	auto light = AnimalLight::Land;
	if (type == AnimalInfo::SpellWolf)
	{
		light = AnimalLight::White;
	}
	else if (IsBird(type))
	{
		light = AnimalLight::BrightestLand;
	}
	registry.Assign<AnimalPose>(entity, AnimalPose {.light = light});
	return entity;
}

entt::entity AnimalSystem::CreateLoadedAnimal(const LoadedAnimal& loaded)
{
	auto& registry = EntityRegistry();
	const bool bird = animals::birds::IsLandBird(loaded.type);
	const bool grazer = animals::grazers::IsGrazer(loaded.type);
	const auto xz = Metres(loaded.position);
	if ((!bird && !grazer) || !OnMap(xz))
	{
		return entt::null;
	}
	const glm::vec3 point(xz.x, loaded.altitude, xz.y);
	const auto entity = ecs::archetypes::AnimalArchetype::Create(loaded.type, point, gutils::ConvertGameAngleTo3D(loaded.angle),
	                                                             loaded.scale, PlayerNames::NEUTRAL);
	auto& animal = registry.Get<Animal>(entity);
	animal.birthTurn = loaded.birthTurn;
	animal.life = loaded.life;
	// It keeps its height above the land under it, and heads for the same height at its goal
	animal.height = bird ? std::max(loaded.altitude - Ground(xz), 0.0f) : 0.0f;
	animal.goalHeight = animal.height;
	animal.move.position = loaded.position;
	animal.move.goal = loaded.goal;
	animal.move.angle = loaded.angle;
	animal.move.speed = loaded.speed;
	animal.move.step = loaded.step;
	// A move that goes round a thing is not kept yet: the animal steps straight through to its goal
	animal.move.stage = animals::MoveStageOfGame(loaded.moveState).value_or(animals::MoveStage::StepThrough);
	// A state that openblack does not have yet becomes a new decision
	SetTopState(animal, AnimalStateOfGame(loaded.state).value_or(AnimalState::DecideWhatToDo));
	animal.finalState = AnimalStateOfGame(loaded.finalState).value_or(AnimalState::DecideWhatToDo);
	SyncWorld(animal);
	animal.previousPosition = animal.position;
	animal.previousHeading = animal.heading;
	registry.Assign<AnimalPose>(entity, AnimalPose {.light = bird ? AnimalLight::BrightestLand : AnimalLight::Land});
	animal.town = loaded.town;
	auto* flockData = registry.Valid(loaded.flock) ? registry.TryGet<Flock>(loaded.flock) : nullptr;
	if (flockData != nullptr)
	{
		animal.flock = loaded.flock;
		flockData->members.push_back(entity);
		flockData->most = std::max(flockData->most, static_cast<uint32_t>(flockData->members.size()));
	}
	if (grazer)
	{
		// It sleeps where its herd's home is
		auto& needs = registry.Assign<Grazer>(entity);
		if (flockData != nullptr)
		{
			needs.sleepingCell = map_coords::Cell(flockData->place);
		}
	}
	return entity;
}

entt::entity AnimalSystem::TempleFlock(entt::entity templeEntity)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(templeEntity) || !registry.AllOf<Temple, Transform>(templeEntity))
	{
		return entt::null;
	}
	// Its flock, made the first time, about the temple
	auto* templeBirds = registry.TryGet<TempleBirds>(templeEntity);
	if (templeBirds == nullptr)
	{
		templeBirds = &registry.Assign<TempleBirds>(templeEntity);
	}
	if (!registry.Valid(templeBirds->flock) || !registry.AllOf<Flock>(templeBirds->flock))
	{
		const auto& transform = registry.Get<const Transform>(templeEntity);
		const glm::vec2 home {transform.position.x, transform.position.z};
		templeBirds->flock = CreateFlock(home, animals::birds::k_TempleFlockReach, animals::birds::k_TempleFlockDistance);
		registry.Get<Flock>(templeBirds->flock).temple = templeEntity;
	}
	return templeBirds->flock;
}

void AnimalSystem::JoinFlock(entt::entity entity, entt::entity flockEntity)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	auto* flockData = registry.Valid(flockEntity) ? registry.TryGet<Flock>(flockEntity) : nullptr;
	if (animal == nullptr || flockData == nullptr)
	{
		return;
	}
	// Out of its flock first, even when it is the same one, then in after the others: the first to join leads
	LeaveFlock(entity, *animal);
	if (!registry.Valid(flockEntity))
	{
		return;
	}
	animal->flock = flockEntity;
	JoinLine(registry.Get<Flock>(flockEntity), entity);
}

void AnimalSystem::ReleaseFromScript(entt::entity entity)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	if (animal == nullptr || registry.AnyOf<CarriedByTornado, InHand, InPhysics>(entity))
	{
		return;
	}
	// Alone, it is a flock of its own about where it is, as far as its kind wanders
	if (!registry.Valid(animal->flock) || !registry.AllOf<Flock>(animal->flock))
	{
		const auto& info = InfoOf(animal->type);
		const auto own =
		    CreateFlock(Metres(animal->move.position), static_cast<float>(static_cast<uint16_t>(info.domainRadius)),
		                static_cast<float>(static_cast<int32_t>(info.flockDistance)));
		animal->flock = own;
		JoinLine(registry.Get<Flock>(own), entity);
	}
	if (animal->life > 0.0f)
	{
		// As after a landing: it joins a flock near it if it can, then it starts on a leg of its own
		MergeNearbyHerd(entity, *animal);
		StartWander(entity, *animal);
		return;
	}
	// With no life it dies. One that a kill made dying before is dead at once.
	const bool wasDying = animal->dying;
	SetDying(entity);
	if (animals::LandsAs(false, wasDying) == animals::LandedAs::Dead)
	{
		SetTopState(*animal, AnimalState::Dead);
	}
}

void AnimalSystem::StartWandering(entt::entity animal)
{
	if (auto* living = Locator::entitiesRegistry::value().TryGet<Animal>(animal))
	{
		SetTopState(*living, AnimalState::StartWander);
	}
}

bool AnimalSystem::SetScriptState(entt::entity entity, LivingStates state)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return false;
	}
	// The state is a row of the table of the animals' states. The rows that have a start of their own (wandering, sleep,
	// a lair and a search for food) are not done, and no land's script gives one of them to an animal.
	const auto gameState = static_cast<size_t>(state);
	const auto next = AnimalStateOfGame(gameState);
	if (!next.has_value() || animals::StateHasOwnStart(gameState))
	{
		return false;
	}
	// Only an animal out in the world takes it: not one held in a hand, carried off or flying through the air
	if (EntityRegistry().AnyOf<CarriedByTornado, InHand, InPhysics>(entity))
	{
		return true;
	}
	// It remembers the state that it ends in, as before a reaction. An animal that reacts and goes into a state that is
	// not a reaction stops the reaction, and keeps the new state
	RememberStateBeforeReaction(entity);
	if (animals::IsReactionState(static_cast<uint8_t>(GameStateOf(animal->state))) &&
	    !animals::IsReactionState(static_cast<uint8_t>(gameState)))
	{
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().StopReacting(entity);
		}
		StopReaction(entity, false);
	}
	// The state starts afresh, a land bird choosing its flying clip again, and its count of turns starts at none
	SetTopState(*animal, *next);
	animal->turnsUntilStateChange = 0;
	return true;
}

void AnimalSystem::ScriptMoveTo(entt::entity entity, glm::vec2 point)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	// Only an animal out in the world is sent: not one held in a hand, carried off or flying through the air
	if (animal == nullptr || EntityRegistry().AnyOf<CarriedByTornado, InHand, InPhysics>(entity))
	{
		return;
	}
	// TODO(town-script-natives): the game also leaves one drowning where it is; openblack's animals don't drown yet
	const auto goal = Fixed(point);
	if (animals::AreWeThere(animal->move.position, goal, animal->move.speed))
	{
		SetTopState(*animal, AnimalState::InScript);
		return;
	}
	SetupMoveTo(*animal, point, 0.0f, AnimalState::InScript);
}

std::optional<uint8_t> AnimalSystem::FinalGameStateOf(entt::entity entity) const
{
	const auto* animal = EntityRegistry().TryGet<const Animal>(entity);
	if (animal == nullptr)
	{
		return std::nullopt;
	}
	const auto current = GameStateOf(animal->state);
	if (Locator::infoConstants::value().animalStateTable.at(current).isFinalState != 0)
	{
		return static_cast<uint8_t>(current);
	}
	// Waiting for a clip to end, it is on its way into the state after the clip; otherwise into where it is going's
	return static_cast<uint8_t>(
	    GameStateOf(animal->state == AnimalState::WaitForClip ? animal->afterClip : animal->finalState));
}

entt::entity AnimalSystem::CreateSpellAnimal(AnimalInfo type, glm::vec2 position, float heightAboveLand, uint16_t angle,
                                             PlayerNames owner, entt::entity flockEntity, entt::entity spell)
{
	auto& registry = EntityRegistry();
	const glm::vec3 point(position.x, Ground(position) + heightAboveLand, position.y);
	const auto& info = InfoOf(type);
	// Born at a random age (a spell wolf always grown), the day it was born a random part of its starting age back, at
	// its size for its age
	const auto age = RandomWhole(k_BirthAgeRange) + k_BirthAgeMin;
	(void)RandomWhole(info.startAge / 2);
	const auto bornAt = type == AnimalInfo::SpellWolf ? info.grownUpAge + 1 : age;
	const float scale = animals::BirthScale(bornAt, info.grownUpAge, info.ageToScale.values, Random);
	const auto entity =
	    ecs::archetypes::AnimalArchetype::Create(type, point, gutils::ConvertGameAngleTo3D(angle), scale, owner);
	auto& animal = registry.Get<Animal>(entity);
	// Born as long ago as its age, which also set its size
	animal.birthTurn = villager_age::BirthTurnFor(Locator::time::value().GetTurn(), bornAt);
	animal.flock = flockEntity;
	animal.height = heightAboveLand;
	animal.goalHeight = heightAboveLand;
	animal.move.position = Fixed(position);
	animal.move.goal = animal.move.position;
	animal.move.angle = angle;
	animal.move.speed = SpeedStateOf(info, 0);
	// A spell wolf is hungry from the start
	animal.hunger = static_cast<int32_t>(info.hunger);
	SyncWorld(animal);
	animal.previousPosition = animal.position;
	animal.previousHeading = animal.heading;
	registry.Assign<SpellAnimal>(entity, SpellAnimal {.spell = spell, .previous = point});
	// The miracle's doves and bats take the brightest of the land's light wherever they are, its wolves are white
	registry.Assign<AnimalPose>(
	    entity, AnimalPose {.light = type == AnimalInfo::SpellWolf ? AnimalLight::White : AnimalLight::BrightestLand});
	if (type == AnimalInfo::SpellWolf)
	{
		registry.Assign<SpellWolf>(entity);
	}
	if (auto* flockData = registry.TryGet<Flock>(flockEntity))
	{
		JoinLine(*flockData, entity);
	}
	return entity;
}

entt::entity AnimalSystem::LeaderOf(entt::entity flockEntity) const
{
	const auto& registry = EntityRegistry();
	const auto* flockData = registry.Valid(flockEntity) ? registry.TryGet<const Flock>(flockEntity) : nullptr;
	// The last in the line leads: the first to join
	return flockData == nullptr || flockData->members.empty() ? entt::null : flockData->members.back();
}

std::vector<entt::entity> AnimalSystem::MembersOf(entt::entity flockEntity) const
{
	const auto& registry = EntityRegistry();
	const auto* flockData = registry.Valid(flockEntity) ? registry.TryGet<const Flock>(flockEntity) : nullptr;
	// The leader first, then the others from the earliest to join
	return flockData != nullptr ? std::vector<entt::entity>(flockData->members.rbegin(), flockData->members.rend())
	                            : std::vector<entt::entity> {};
}

glm::vec2 AnimalSystem::GoalOf(entt::entity animal) const
{
	// Where it is heading now: a wolf hunting heads for its prey
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	return data != nullptr ? Metres(data->move.goal) : glm::vec2(0.0f);
}

float AnimalSystem::GoalHeightOf(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	return data != nullptr ? data->goalHeight : 0.0f;
}

void AnimalSystem::SetupMoveTo(Animal& animal, glm::vec2 goal, float goalHeight, AnimalState finalState)
{
	SetupMoveToFixed(animal, Fixed(goal), goalHeight, finalState);
}

void AnimalSystem::SetupMoveToFixed(Animal& animal, glm::ivec2 goal, float goalHeight, AnimalState finalState)
{
	animal.walksRound = false;
	animal.goalHeight = goalHeight;
	Banked(animal, animals::SetUpMove(animal.move, goal, TurnAngleOf(animal)));
	animal.finalState = finalState;
	// It takes its kind's move state, and that state's clip
	SetTopState(animal, AnimalState::MoveToPos);
	SyncWorld(animal);
}

void AnimalSystem::Banked(Animal& animal, const animals::Turn& turn)
{
	const bool wolf = animal.type == AnimalInfo::SpellWolf;
	if (!wolf && !IsBird(animal.type))
	{
		return;
	}
	const auto birdBank = animals::birds::BankOf(animal.type);
	const float timeToBank = wolf ? k_WolfTimeToBank : birdBank.seconds;
	if (turn.direction == 0)
	{
		animal.bank.SetTarget(0.0f, 0.0f, timeToBank);
		return;
	}
	float tilt = birdBank.angle;
	if (wolf)
	{
		tilt =
		    std::min(std::abs(static_cast<float>(animal.move.speed) * static_cast<float>(turn.step)) * k_WolfBankPerSpeedTurn,
		             k_WolfMostBank);
	}
	animal.bank.SetTarget(turn.direction < 0 ? -tilt : tilt, 0.0f, timeToBank);
}

void AnimalSystem::SetupMoveRoundTo(entt::entity entity, Animal& animal, glm::vec2 goal, AnimalState finalState)
{
	auto& registry = EntityRegistry();
	// A bird flies over what is in its way
	if (IsBird(animal.type) || !Locator::pathfindingSystem::has_value() || !registry.AllOf<Transform>(entity))
	{
		SetupMoveToFixed(animal, Fixed(goal), 0.0f, finalState);
		return;
	}
	animal.goalHeight = 0.0f;
	animal.move.goal = Fixed(goal);
	// The walk goes from where it is, facing as it faces, at its speed, and turns as its kind turns
	auto& walk = registry.AssignOrReplace<WallHug>(entity);
	walk.goalPlace = animal.move.goal;
	walk.position = animal.move.position;
	walk.placedAt = Metres(animal.move.position);
	walk.gameAngle = animal.move.angle;
	walk.yAngle = animal.heading;
	walk.speed = 0.0f;
	registry.AssignOrReplace<TurningWalker>(entity,
	                                        TurningWalker {.turnAngle = TurnAngleOf(animal), .speed = animal.move.speed});
	auto& transform = registry.Get<Transform>(entity);
	transform.position.x = walk.placedAt.x;
	transform.position.z = walk.placedAt.y;
	Locator::pathfindingSystem::value().SetupWalkRound(entity, animal.move.goal);
	const auto& set = registry.Get<const WallHug>(entity);
	animal.move.angle = set.gameAngle;
	animal.move.step = set.step;
	animal.move.stage =
	    registry.AllOf<MoveStateArrivedTag>(entity) ? animals::MoveStage::AtGoal : animals::MoveStage::StepThrough;
	animal.walksRound = true;
	animal.finalState = finalState;
	// It takes its kind's move state, and that state's clip
	SetTopState(animal, AnimalState::MoveToPos);
	SyncWorld(animal);
}

void AnimalSystem::EndWalkRound(entt::entity entity)
{
	EntityRegistry()
	    .Remove<WallHug, TurningWalker, WallHugObjectReference, WallHugLookahead, MoveStateLinearTag, MoveStateOrbitTag,
	            MoveStateExitCircleTag, MoveStateStepThroughTag, MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
}

bool AnimalSystem::MoveTo3D(Animal& animal)
{
	const auto before = Metres(animal.move.position);
	const float groundBefore = Ground(before);
	const float groundAtGoal = Ground(Metres(animal.move.goal));
	const auto result = animals::StepMove(animal.move, TurnAngleOf(animal));
	if (result.turned)
	{
		Banked(animal, result.turn);
	}
	animal.height = animals::FlyingHeight(groundBefore, animal.height, Ground(Metres(animal.move.position)), groundAtGoal,
	                                      animal.goalHeight, InfoOf(animal.type).altitudeMovementChange);
	SyncWorld(animal);
	return result.arrived;
}

glm::ivec2 AnimalSystem::RandomPos(const Animal& animal, glm::ivec2 centre, float inner, float outer) const
{
	const auto here = Metres(animal.move.position);
	// Every bird, seagulls too, keeps over the land's blocks
	const bool overLandOnly = IsBird(animal.type);
	const auto hasLand = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() &&
		       Locator::terrainSystem::value().FindCell(glm::u16vec2(map_coords::CellOf(point))) != nullptr;
	};
	// A point it may go to: on the map, where its kind can stand, one it can reach without circling, and for a bird, over
	// the land all the way. (The game also turns down a point the map's walls block; openblack's map has none of them.)
	const auto mask = static_cast<uint32_t>(InfoOf(animal.type).collideType);
	const auto reachable = [&](glm::ivec2 point) {
		return animals::OutsideTurningCircles(here, animal.move.angle, animal.move.speed, TurnAngleOf(animal), Metres(point));
	};
	const auto valid = [&](const map_coords::MapCoords& place) {
		const glm::ivec2 point {place.x, place.z};
		return !CellCollides(point, mask) && reachable(point) &&
		       (!overLandOnly || animals::OverLandAllTheWay(here, Metres(point), [&hasLand](glm::ivec2 cell) {
			       return hasLand(glm::vec2(cell) * 10.0f + 5.0f);
		       }));
	};
	// The angle then the distance, each a random number of the game's, then cells spiralling out from the point, all in
	// map units
	const auto found = script_flock_rules::CalcRandomPos(
	    {.x = centre.x, .z = centre.y}, inner, outer, [](float most) { return Random(most); }, valid);
	const glm::ivec2 point {found.x, found.z};
	// None: the middle if it can reach it, else where it is
	if (point == centre && !reachable(centre))
	{
		return animal.move.position;
	}
	return point;
}

void AnimalSystem::ProcessTurn()
{
	if (!BeginTurn())
	{
		return;
	}
	std::vector<entt::entity> animals;
	EntityRegistry().Each<const Animal>([&animals](entt::entity entity, const Animal&) { animals.push_back(entity); });
	for (const auto entity : animals)
	{
		AnimalTurn(entity);
	}
	EndTurn();
}

bool AnimalSystem::BeginTurn()
{
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	++_turn;
	return true;
}

void AnimalSystem::AnimalTurn(entt::entity entity)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(entity) || !registry.AllOf<Animal>(entity))
	{
		return;
	}
	// Carried off by a tornado, it is the tornado's until it lets go; flying, the physics'. In a hand it takes its turn
	auto& animal = registry.Get<Animal>(entity);
	if (!animals::TakesOwnTurn({.tornado = registry.AllOf<CarriedByTornado>(entity),
	                            .playersHand = registry.AllOf<InHand>(entity),
	                            .creature = registry.AllOf<HeldByCreature>(entity),
	                            .physics = registry.AllOf<InPhysics>(entity)}))
	{
		return;
	}
	animal.previousPosition = animal.position;
	animal.previousHeading = animal.heading;
	++animal.turnsInState;
	// The needs of every kind come first, and then the target. A target that it lets go of ends its turn
	NeedsTurn(entity, animal);
	if (DropTarget(entity, animal))
	{
		return;
	}
	// Every kind is brought down, eaten and lands in the same way
	if (CommonStateTurn(entity, animal))
	{
		return;
	}
	// In a hand it does nothing. A creature can make it die in its hand
	if (animal.state == AnimalState::InHand)
	{
		return;
	}
	if (animal.state == AnimalState::Dying || animal.state == AnimalState::Dead)
	{
		ProcessDeath(entity, animal);
		return;
	}
	if (auto* grazer = registry.TryGet<Grazer>(entity))
	{
		GrazerTurn(entity, animal, *grazer);
		return;
	}
	if (auto* hunter = registry.TryGet<Hunter>(entity))
	{
		HunterTurn(entity, animal, *hunter);
		return;
	}
	// Its reaction comes before its own ways
	if (Flee(entity, animal) || LookOrGoToFood(entity, animal))
	{
		return;
	}
	if (IsBird(animal.type))
	{
		Bird(entity, animal);
	}
	else if (registry.AllOf<SpellWolf>(entity))
	{
		Wolf(entity, animal);
	}
	else if (animal.state == AnimalState::WaitForClip &&
	         animal.turnsInState * k_TurnMilliseconds >= PlayTimeOf(animal.animation))
	{
		// Any other kind goes into its next state when its clip ends
		SetTopState(animal, animal.afterClip);
	}
}

void AnimalSystem::EndTurn()
{
	ProcessFading();
	ProcessTempleBirds();
	EntityRegistry().SetDirty();
}

// The birds

void AnimalSystem::SendBird(entt::entity bird, glm::vec2 goal, float height, bool leader)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(bird);
	if (animal == nullptr)
	{
		return;
	}
	// Off to its goal by its special move, then to choose its next leg
	SetupMoveTo(*animal, goal, height, AnimalState::StartWander);
	SetState(*animal, AnimalState::SpecialMoveToPos);
	// Once the leader is off, the others keep a formation behind it, and each special move ends in deciding again
	if (auto* flockData = leader ? registry.TryGet<Flock>(animal->flock) : nullptr)
	{
		flockData->followState = AnimalState::FollowFlock;
		flockData->followMode = k_FollowInFormation;
		flockData->afterMove = AnimalState::DecideWhatToDo;
	}
}

void AnimalSystem::Bird(entt::entity entity, Animal& animal)
{
	switch (animal.state)
	{
	case AnimalState::SpecialMoveToPos:
		SpecialMoveToPos(entity, animal);
		break;
	case AnimalState::DecideWhatToDo:
		DecideWhatToDo(entity, animal);
		break;
	case AnimalState::StartWander:
	case AnimalState::MoveInFlock:
		// Moving with its flock as a script set it, a bird starts on a leg of its own
		StartWander(entity, animal);
		break;
	case AnimalState::FollowFlock:
		FollowFlock(entity, animal);
		break;
	case AnimalState::WaitForClip:
		// Its clip played to the end, it goes on to what comes next, as every living thing does
		if (animal.turnsInState * k_TurnMilliseconds >= PlayTimeOf(animal.animation))
		{
			SetTopState(animal, animal.afterClip);
		}
		break;
	case AnimalState::InteractDecideWhatToDo:
		// Up again after a landing, it joins a flock near it if it can, then it starts on a leg of its own
		if (EntityRegistry().AllOf<Flock>(animal.flock))
		{
			MergeNearbyHerd(entity, animal);
		}
		StartWander(entity, animal);
		break;
	case AnimalState::MoveToPos:
		// Its kind's move: there, it takes its final state
		if (MoveTo3D(animal))
		{
			SetTopState(animal, animal.finalState);
		}
		break;
	default:
		break;
	}
}

void AnimalSystem::SpecialMoveToPos(entt::entity entity, Animal& animal)
{
	const auto* flockData = EntityRegistry().TryGet<const Flock>(animal.flock);
	// A land bird's leader on its way, its flock following, may give the leg up instead of flying on
	if (flockData != nullptr && flockData->followState == AnimalState::FollowFlock && LeaderOf(animal.flock) == entity &&
	    LeaderGivesUpLeg(entity, animal))
	{
		return;
	}
	if (MoveTo3D(animal))
	{
		// There, the flock's state after a special move takes over its final state
		SetTopState(animal, animal.finalState);
		if (flockData != nullptr)
		{
			SetState(animal, flockData->afterMove);
		}
		return;
	}
	// Close to its goal it takes its usual speed
	if (animal.move.stage == animals::MoveStage::StepThrough &&
	    glm::distance(Metres(animal.move.position), Metres(animal.move.goal)) <= k_SpecialMoveSlowing)
	{
		SetSpeed(animal, 0);
	}
}

bool AnimalSystem::LeaderGivesUpLeg(entt::entity /*entity*/, Animal& animal)
{
	auto* flockData = EntityRegistry().TryGet<Flock>(animal.flock);
	if (!animals::birds::IsLandBird(animal.type) || flockData == nullptr ||
	    !animals::birds::LeaderPicksNewLeg(flockData->turnsOnLeg, InfoOf(animal.type).stayTime))
	{
		return false;
	}
	// Its time up, it picks its next leg next turn
	flockData->turnsOnLeg = 0;
	SetTopState(animal, AnimalState::StartWander);
	return true;
}

void AnimalSystem::SetSpeed(Animal& animal, size_t speed)
{
	animal.move.speed = SpeedStateOf(InfoOf(animal.type), speed);
	// A land bird chooses its flying clip afresh, starting a new one from its beginning
	if (animals::birds::IsLandBird(animal.type))
	{
		const auto clip = ClipFor(animal, animal.state);
		if (clip != animal.animation)
		{
			animal.animation = clip;
			animal.clipPlace = 0;
		}
	}
}

void AnimalSystem::DecideWhatToDo(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	auto* flockData = registry.TryGet<Flock>(animal.flock);
	if (flockData == nullptr)
	{
		return;
	}
	const auto leaderEntity = LeaderOf(animal.flock);
	if (leaderEntity == entity)
	{
		// A land bird's leader whose stay is up picks its next leg next turn; otherwise it picks it at once
		if (LeaderGivesUpLeg(entity, animal))
		{
			return;
		}
		SetTopState(animal, AnimalState::StartWander);
		StartWander(entity, animal);
		return;
	}
	if (leaderEntity == entt::null)
	{
		return;
	}
	// A follower picks a point near the leader, at its height, and then follows the flock
	const auto& leader = registry.Get<const Animal>(leaderEntity);
	const auto point = RandomPos(animal, leader.move.position, 0.0f, static_cast<float>(flockData->flockDistance));
	SetupMoveToFixed(animal, point, leader.height, AnimalState::DecideWhatToDo);
	SetSpeed(animal, 0);
	SetState(animal, flockData->followState);
}

void AnimalSystem::StartWander(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	auto* flockData = registry.TryGet<Flock>(animal.flock);
	if (flockData == nullptr)
	{
		return;
	}
	const auto& info = InfoOf(animal.type);
	SetSpeed(animal, 0);
	// A point about where the flock was made, between its kind's inner radius and the flock's reach
	const auto point = RandomPos(animal, glm::ivec2(flockData->place.x, flockData->place.z),
	                             static_cast<float>(info.domainInnerRadius), static_cast<float>(flockData->domainRadius));
	// Higher or lower than its last goal by up to the variance; outside its kind's band about its normal height, back to
	// the normal height
	const float height = (animal.goalHeight + info.altitudeVariance) - Random(info.altitudeVariance + info.altitudeVariance);
	// About its flock's own height (a temple's), or its kind's
	const float base = animals::birds::BaseHeight(flockData->height, info.altitudeNormal);
	const float goalHeight = base + info.altitudeMin < height && height < base + info.altitudeMax ? height : base;
	SetupMoveToFixed(animal, point, goalHeight, AnimalState::DecideWhatToDo);
	SetState(animal, AnimalState::SpecialMoveToPos);
	flockData->followState = AnimalState::FollowFlock;
	flockData->followMode = k_FollowInFormation;
	flockData->afterMove = AnimalState::DecideWhatToDo;
	SpecialMoveToPos(entity, animal);
}

void AnimalSystem::FollowFlock(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	auto* flockData = registry.TryGet<Flock>(animal.flock);
	const auto leaderEntity = LeaderOf(animal.flock);
	if (flockData == nullptr || leaderEntity == entity || animal.state != flockData->followState)
	{
		DecideWhatToDo(entity, animal);
		return;
	}
	if (flockData->followMode != k_FollowInFormation)
	{
		return;
	}
	if (!MoveTo3D(animal))
	{
		// A land bird following chooses its flying clip afresh once it has been following as long as its clip plays
		if (animals::birds::IsLandBird(animal.type) && animal.turnsInState * k_TurnMilliseconds >= PlayTimeOf(animal.animation))
		{
			const auto clip = ClipFor(animal, animal.state);
			if (clip != animal.animation)
			{
				animal.animation = clip;
				animal.clipPlace = 0;
			}
		}
		return;
	}
	// There: on to its place in the formation, by its place in the flock, at the leader's height
	const auto found = std::ranges::find(flockData->members, entity);
	// Counted from the leader, the last in the line
	const int place = static_cast<int>(flockData->members.end() - found);
	const auto& leader = registry.Get<const Animal>(leaderEntity);
	const auto goal = animals::FormationGoal(leader.move.position, animal.move.position, animals::FormationSlotOf(place));
	SetupMoveToFixed(animal, goal, leader.height, AnimalState::DecideWhatToDo);
}

// The temples' birds

void AnimalSystem::ProcessTempleBirds()
{
	auto& registry = EntityRegistry();
	// A bird goes at once, out of its flock, which stays though it is empty
	const auto vanish = [&registry](entt::entity bird, Flock& flockData) {
		std::erase(flockData.members, bird);
		registry.Destroy(bird);
	};
	// A temple gone takes its flock with it, every bird at once
	std::vector<entt::entity> orphaned;
	registry.Each<const Flock>([&registry, &orphaned](entt::entity entity, const Flock& flockData) {
		if (flockData.temple != entt::null && !(registry.Valid(flockData.temple) && registry.AllOf<Temple>(flockData.temple)))
		{
			orphaned.push_back(entity);
		}
	});
	for (const auto entity : orphaned)
	{
		auto& flockData = registry.Get<Flock>(entity);
		for (const auto bird : std::vector(flockData.members))
		{
			vanish(bird, flockData);
		}
		registry.Destroy(entity);
	}

	// Each temple's flock is seen to every hundred turns of the game
	if (!Locator::time::has_value() || !Locator::alignmentSystem::has_value() ||
	    Locator::time::value().GetTurn() % animals::birds::k_TempleFlockEvery != 0)
	{
		return;
	}
	const auto most = Locator::infoConstants::value().citadelHeart.maxFlockCount;
	std::vector<entt::entity> temples;
	registry.Each<const Temple, const Transform>(
	    [&temples](entt::entity entity, const Temple&, const Transform&) { temples.push_back(entity); });
	for (const auto templeEntity : temples)
	{
		const auto& temple = registry.Get<const Temple>(templeEntity);
		const auto& transform = registry.Get<const Transform>(templeEntity);
		const glm::vec2 home {transform.position.x, transform.position.z};
		// Doves for a good player, bats for an evil one, as many as how far the player is from neutral. openblack's
		// temples stand built from the start.
		const float alignment = Locator::alignmentSystem::value().GetPlayerAlignment(temple.owner);
		const auto kind = animals::birds::TempleBirdKind(alignment);
		const auto count = animals::birds::TempleBirdCount(alignment, most, true);

		const auto flockEntity = TempleFlock(templeEntity);
		auto* templeBirds = registry.TryGet<TempleBirds>(templeEntity);
		bool changed = false;
		// Those of the other kind go at once, as the player crosses from good to evil or back
		for (const auto bird : std::vector(registry.Get<Flock>(flockEntity).members))
		{
			if (registry.Get<const Animal>(bird).type != kind)
			{
				vanish(bird, registry.Get<Flock>(flockEntity));
				changed = true;
			}
		}
		// Then one bird more or one fewer: born over the temple, or the newest gone
		auto& members = registry.Get<Flock>(flockEntity).members;
		switch (animals::birds::StepTowards(static_cast<uint32_t>(members.size()), count))
		{
		case animals::birds::TempleFlockStep::AddOne:
			changed = CreateBird(kind, home, 0, flockEntity) != entt::null || changed;
			break;
		case animals::birds::TempleFlockStep::RemoveOne:
			vanish(members.front(), registry.Get<Flock>(flockEntity));
			changed = true;
			break;
		case animals::birds::TempleFlockStep::None:
			break;
		}
		// They fly about the temple's height, taken again as the temple's model or the flock changes
		const auto* exterior = registry.TryGet<const TempleExterior>(templeEntity);
		const auto look = exterior != nullptr ? exterior->morphed : std::nullopt;
		const auto* mesh = registry.TryGet<const Mesh>(templeEntity);
		const auto& meshes = Locator::resources::value().GetMeshes();
		if ((changed || look != templeBirds->look) && mesh != nullptr && meshes.Contains(mesh->id))
		{
			templeBirds->look = look;
			registry.Get<Flock>(flockEntity).height =
			    animals::birds::TempleFlockHeight(transform.scale.y, meshes.Handle(mesh->id)->GetBoundingBox().Size().y);
		}
	}
}

// The wolves

void AnimalSystem::SendWolf(entt::entity wolf, glm::vec2 start, glm::vec2 destination, float halfWidth)
{
	auto& registry = EntityRegistry();
	auto* run = registry.TryGet<SpellWolf>(wolf);
	auto* animal = registry.TryGet<Animal>(wolf);
	if (run == nullptr || animal == nullptr)
	{
		return;
	}
	run->finalDestination = destination;
	run->corridor = flock_rules::MakeCorridor(start, destination, halfWidth);
	RunToFinalDestination(wolf, *animal);
}

void AnimalSystem::RunToFinalDestination(entt::entity wolf, Animal& animal)
{
	const auto& run = EntityRegistry().Get<const SpellWolf>(wolf);
	const float scale = EntityRegistry().Get<const Transform>(wolf).scale.x;
	// Its kind's run times its scale, a tenth faster, to a whole speed state: the only speed a spell wolf takes
	const auto state = static_cast<double>(scale) * static_cast<double>(SpeedStateOf(InfoOf(animal.type), k_WolfRunSpeed)) *
	                   static_cast<double>(flock_rules::k_WolfRunFactor);
	animal.move.speed = static_cast<uint16_t>(std::clamp(static_cast<int32_t>(state), 0, 0xFFFF));
	SetupMoveTo(animal, run.finalDestination, 0.0f, AnimalState::SetDying);
}

bool AnimalSystem::DropTarget(entt::entity entity, Animal& animal)
{
	// Only a hunter has a target: the prey that it brought down and eats
	auto& registry = EntityRegistry();
	auto* hunt = HuntOf(entity);
	if (hunt == nullptr || hunt->food == entt::null)
	{
		return false;
	}
	const bool gone = !registry.Valid(hunt->food);
	const auto drop = animals::DropOfTarget({.gone = gone,
	                                         .inPlayersHand = !gone && registry.AllOf<InHand>(hunt->food),
	                                         .inHand = animal.state == AnimalState::InHand});
	if (drop == animals::TargetDrop::Keeps)
	{
		return false;
	}
	hunt->food = entt::null;
	animal.turnsUntilStateChange = 0;
	if (drop == animals::TargetDrop::DropsAndDecides)
	{
		SetTopState(animal, AnimalState::DecideWhatToDo);
	}
	return true;
}

void AnimalSystem::Wolf(entt::entity entity, Animal& animal)
{
	switch (animal.state)
	{
	case AnimalState::MoveToPos:
		WolfMoveToPos(entity, animal);
		break;
	case AnimalState::SetDying:
		StartFading(entity);
		break;
	case AnimalState::DecideWhatToDo:
	case AnimalState::Wander:
		RunToFinalDestination(entity, animal);
		break;
	case AnimalState::StartWander:
		WolfStartWander(entity, animal);
		break;
	case AnimalState::Chase:
		Chase(entity, animal);
		break;
	case AnimalState::Pounce:
		Pounce(entity, animal);
		break;
	case AnimalState::StartToEat:
		// So many mouthfuls, once its settling down has played
		animal.turnsUntilStateChange = static_cast<int16_t>(RandomWhole(k_MouthfulsRange) + k_MouthfulsMin);
		WaitForClip(animal, AnimalState::Eat);
		break;
	case AnimalState::WaitForClip:
		if (animal.turnsInState * k_TurnMilliseconds >= PlayTimeOf(animal.animation))
		{
			SetTopState(animal, animal.afterClip);
		}
		break;
	case AnimalState::Eat:
		Eat(entity, animal);
		break;
	default:
		break;
	}
}

void AnimalSystem::WaitForClip(Animal& animal, AnimalState next)
{
	animal.afterClip = next;
	SetState(animal, AnimalState::WaitForClip);
}

void AnimalSystem::WolfMoveToPos(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	const auto& run = registry.Get<const SpellWolf>(entity);
	if (MoveTo3D(animal))
	{
		SetTopState(animal, animal.finalState);
	}
	// Hungry, it hunts what crosses its strip of land
	if (animal.state == AnimalState::MoveToPos && animal.hunger >= static_cast<int32_t>(InfoOf(animal.type).hunger))
	{
		ReactToFoodNeeds(entity, animal);
	}
	// Near where it was sent, it fades away
	if (flock_rules::WolfArrived(Metres(animal.move.position), run.finalDestination))
	{
		StartFading(entity);
	}
}

bool AnimalSystem::ReactToFoodNeeds(entt::entity entity, Animal& animal)
{
	auto& hunt = *HuntOf(entity);
	// The prey it found last turn, still there to hunt, is chased at its hunting speed; one brought down since is eaten
	if (PreyStillValid(entity, animal))
	{
		SetHunterSpeed(entity, animal, animals::hunters::k_HuntSpeed);
		hunt.huntStart = _turn;
		SetupMoveToTarget(entity, animal, hunt.prey);
		return true;
	}
	hunt.prey = FindPrey(entity, animal);
	if (hunt.prey == entt::null && hunt.remembered.has_value())
	{
		// None now: it goes at its hunting speed to where it last found some
		SetHunterSpeed(entity, animal, animals::hunters::k_HuntSpeed);
		SetupMoveTo(animal, *hunt.remembered, 0.0f, AnimalState::StartWander);
		hunt.remembered.reset();
		return true;
	}
	return false;
}

bool AnimalSystem::PreyStillValid(entt::entity entity, const Animal& animal)
{
	auto& registry = EntityRegistry();
	auto& hunt = *HuntOf(entity);
	if (hunt.prey == entt::null)
	{
		return false;
	}
	// The prey must be there, alive, low enough, within its hunting distance, and not under a script's control unless a
	// script also holds the hunter. Else the hunter forgets it
	bool valid = registry.Valid(hunt.prey);
	if (valid)
	{
		const auto& transform = registry.Get<const Transform>(hunt.prey);
		const auto* action = registry.TryGet<const LivingAction>(hunt.prey);
		const bool dying =
		    registry.AllOf<Villager>(hunt.prey) && action != nullptr &&
		    action->states[static_cast<size_t>(LivingAction::Index::Top)] == static_cast<uint8_t>(VillagerStates::Dying);
		valid = !dying && transform.position.y - Ground(Xz(transform.position)) <= flock_rules::k_PreyMaxHeight &&
		        ScriptAllowsHunt(entity, hunt.prey) &&
		        glm::distance(Metres(animal.move.position), Xz(transform.position)) < InfoOf(animal.type).huntingDistance;
	}
	if (!valid)
	{
		hunt.prey = entt::null;
	}
	return valid;
}

bool AnimalSystem::ScriptAllowsHunt(entt::entity hunter, entt::entity prey) const
{
	const auto& registry = EntityRegistry();
	return !registry.AllOf<ScriptControlled>(prey) || registry.AllOf<InScript>(hunter);
}

void AnimalSystem::WolfStartWander(entt::entity /*entity*/, Animal& animal)
{
	// It wanders off about its flock's leader, which a spell wolf takes up as its run next turn
	auto& registry = EntityRegistry();
	const auto* flockData = registry.TryGet<const Flock>(animal.flock);
	const auto leaderEntity = LeaderOf(animal.flock);
	SetTopState(animal, AnimalState::Wander);
	if (flockData != nullptr && leaderEntity != entt::null)
	{
		const auto centre = registry.Get<const Animal>(leaderEntity).move.position;
		const auto point = RandomPos(animal, centre, static_cast<float>(InfoOf(animal.type).domainInnerRadius),
		                             static_cast<float>(flockData->domainRadius));
		Banked(animal, animals::SetUpMove(animal.move, point, TurnAngleOf(animal)));
	}
}

void AnimalSystem::SetupMoveToTarget(entt::entity wolf, Animal& animal, entt::entity prey)
{
	auto& registry = EntityRegistry();
	HuntOf(wolf)->prey = prey;
	// It makes for where it can reach the prey
	const auto preyPoint = Xz(registry.Get<const Transform>(prey).position);
	const auto goal = animals::WorkingPosition(preyPoint, RadiusOf(prey), Metres(animal.move.position), RadiusOf(wolf));
	Banked(animal, animals::SetUpMove(animal.move, Fixed(goal), TurnAngleOf(animal)));
	animal.goalHeight = 0.0f;
	// The hunter eats prey that a hunter brought down before, and goes to it at its hunting speed. It chases all other
	// prey, in the clip that it has
	if (registry.AllOf<BroughtDown>(prey))
	{
		FinishPouncing(wolf, animal, prey);
		SetHunterSpeed(wolf, animal, animals::hunters::k_HuntSpeed);
		return;
	}
	SetState(animal, AnimalState::Chase);
}

void AnimalSystem::Abandon(entt::entity wolf, Animal& animal)
{
	auto& hunt = *HuntOf(wolf);
	hunt.prey = entt::null;
	hunt.remembered.reset();
	// A spell wolf runs on. A wild hunter wanders off at its usual speed
	if (EntityRegistry().AllOf<SpellWolf>(wolf))
	{
		RunToFinalDestination(wolf, animal);
		return;
	}
	SetHunterSpeed(wolf, animal, animals::hunters::k_UsualSpeed);
	SetTopState(animal, AnimalState::StartWander);
}

bool AnimalSystem::OutOfTheHunt(entt::entity prey) const
{
	const auto& registry = EntityRegistry();
	auto finalState = LivingStates::LivingInvalid;
	if (const auto* action = registry.TryGet<const LivingAction>(prey);
	    action != nullptr && registry.AllOf<Villager>(prey) && Locator::livingActionSystem::has_value())
	{
		finalState = static_cast<LivingStates>(Locator::livingActionSystem::value().VillagerGetFinalState(*action));
	}
	else if (const auto state = FinalGameStateOf(prey))
	{
		// An animal is judged by the state that it goes to
		finalState = static_cast<LivingStates>(*state);
	}
	return flock_rules::OutOfTheHunt(finalState, registry.AllOf<BroughtDown>(prey));
}

bool AnimalSystem::IsHuntingTargetValid(entt::entity wolf, const Animal& animal, entt::entity prey) const
{
	const auto& registry = EntityRegistry();
	const auto* run = registry.TryGet<const SpellWolf>(wolf);
	if (run == nullptr)
	{
		// A wild hunter takes any prey that is there
		return registry.Valid(prey);
	}
	const auto* spellAnimal = registry.TryGet<const SpellAnimal>(wolf);
	if ((spellAnimal != nullptr && spellAnimal->Fading()) || !registry.Valid(prey) || OutOfTheHunt(prey))
	{
		return false;
	}
	const auto point = Xz(registry.Get<const Transform>(prey).position);
	return flock_rules::IsOnCorridor(run->corridor, point, Metres(animal.move.position)) &&
	       animals::OutsideTurningCircles(Metres(animal.move.position), animal.move.angle, animal.move.speed,
	                                      TurnAngleOf(animal), point);
}

namespace
{
/// Whether a hunter can get at its prey: a villager not indoors at home, not in a hand, not hidden in a building and not
/// dying
bool PreyReachable(entt::entity prey)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Villager>(prey))
	{
		return true;
	}
	if (registry.AnyOf<AtHome, InHand>(prey))
	{
		return false;
	}
	const auto* action = registry.TryGet<const LivingAction>(prey);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return true;
	}
	const auto& states = Locator::livingActionSystem::value();
	return states.VillagerGetState(*action, LivingAction::Index::Final) != VillagerStates::Dying &&
	       states.VillagerGetState(*action, LivingAction::Index::Top) != VillagerStates::GoAndHideInNearbyBuilding;
}
} // namespace

void AnimalSystem::Chase(entt::entity wolf, Animal& animal)
{
	auto& registry = EntityRegistry();
	auto& hunt = *HuntOf(wolf);
	if (hunt.prey == entt::null || !registry.Valid(hunt.prey) || !PreyReachable(hunt.prey) ||
	    !ScriptAllowsHunt(wolf, hunt.prey))
	{
		Abandon(wolf, animal);
		return;
	}
	const auto& info = InfoOf(animal.type);
	// It heads straight for the prey, unless within a step of it already
	const auto prey = hunt.prey;
	const auto preyPoint = Xz(registry.Get<const Transform>(prey).position);
	if (!(glm::distance(Metres(animal.move.position), preyPoint) < map_coords::ToMetres(animal.move.speed)))
	{
		animal.move.goal = Fixed(preyPoint);
		MoveTo3D(animal);
	}
	if (_turn - hunt.huntStart >= info.chaseTime || !IsHuntingTargetValid(wolf, animal, prey))
	{
		Abandon(wolf, animal);
		return;
	}
	// Close enough and facing it, it leaps. Not facing it, it gives up the prey
	const float distance = glm::distance(Metres(animal.move.position), preyPoint);
	const float scale = registry.Get<const Transform>(wolf).scale.x;
	const auto pounceClip = animals::hunters::ClipsOf(animal.type).transform([](const auto& clips) { return clips.pounce; });
	const float reach =
	    distance < info.attackDistance ? scale * StrideOf(pounceClip.value_or(AnimId::AWolfPounce)) * 0.5f : k_PounceFromFar;
	if (distance <= reach)
	{
		const auto toPrey = gutils::GetAngleFromXZ(Metres(animal.move.position), preyPoint);
		if (gutils::GetAngleDifference(animal.move.angle, toPrey) <= k_PounceAngle)
		{
			SetTopState(animal, AnimalState::Pounce);
			return;
		}
		Abandon(wolf, animal);
	}
	// Its speed comes from its distance from the prey. Too far away, it gives up the prey
	if (const auto speed =
	        animals::hunters::ChaseSpeed(distance, info.attackDistance, info.stalkingDistance, info.huntingDistance, _turn))
	{
		SetHunterSpeed(wolf, animal, *speed);
	}
	if (!(distance < info.huntingDistance))
	{
		Abandon(wolf, animal);
	}
}

void AnimalSystem::Pounce(entt::entity wolf, Animal& animal)
{
	auto& registry = EntityRegistry();
	auto& hunt = *HuntOf(wolf);
	const float scale = registry.Get<const Transform>(wolf).scale.x;
	// The ground leapt so far, and the whole leap: its clip's stride, scaled
	const float leapt = static_cast<float>(animal.move.speed) * k_SpeedStateToMetresPerSecond * k_TurnSeconds *
	                    static_cast<float>(animal.turnsInState);
	const float leap = scale * StrideOf(animal.animation);
	if (hunt.prey != entt::null && !registry.Valid(hunt.prey))
	{
		hunt.prey = entt::null;
		hunt.remembered.reset();
	}
	// The leap continues along its last step
	animal.move.position += animal.move.step;
	SyncWorld(animal);
	if (hunt.prey == entt::null)
	{
		// With no prey it wanders off
		if (registry.AllOf<SpellWolf>(wolf))
		{
			WolfStartWander(wolf, animal);
		}
		else
		{
			GrazerStartWander(wolf, animal);
		}
		return;
	}
	const auto prey = hunt.prey;
	if (glm::distance(Metres(animal.move.position), Xz(registry.Get<const Transform>(prey).position)) <=
	    flock_rules::k_PounceReach)
	{
		BringDown(wolf, prey);
	}
	if (leap <= leapt)
	{
		if (registry.AllOf<BroughtDown>(prey))
		{
			FinishPouncing(wolf, animal, prey);
		}
		else
		{
			SetupMoveToTarget(wolf, animal, prey);
		}
	}
}

void AnimalSystem::IntoHand(entt::entity animal)
{
	auto* data = EntityRegistry().TryGet<Animal>(animal);
	if (data == nullptr)
	{
		return;
	}
	// It stops what it did, also when it dies or lies dead. It plays its kind's clip for the hand. Its mark of a kill
	// stays, so that it lies dead when it comes down
	SetTopState(*data, AnimalState::InHand);
	data->clipPlace = 0;
}

void AnimalSystem::BringDown([[maybe_unused]] entt::entity wolf, entt::entity prey)
{
	auto& registry = EntityRegistry();
	// The wolf brings the prey down again each turn that it is on the prey. The mark stays for all of the life of the prey
	registry.AssignOrReplace<BroughtDown>(prey);
	ecs::world_objects::SetLife(prey, ecs::being_eaten_rules::k_BroughtDownLife);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Animals: #{} brought down by wolf #{}", static_cast<uint32_t>(prey),
	                    static_cast<uint32_t>(wolf));
	if (registry.AllOf<Villager>(prey))
	{
		// The villager falls, then the wolf eats it. The states of the villager count the turns, but only while it stays
		// in these states
		if (auto* wallHug = registry.TryGet<WallHug>(prey))
		{
			wallHug->goalPlace = wall_hug::ToWhole(Xz(registry.Get<const Transform>(prey).position));
		}
		if (auto* action = registry.TryGet<LivingAction>(prey); action != nullptr && Locator::livingActionSystem::has_value())
		{
			Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::Downed,
			                                                      false);
		}
		return;
	}
	if (auto* other = registry.TryGet<Animal>(prey))
	{
		// The animal falls, then the wolf eats it. Its states count the turns, but only while it stays in these states
		SetTopState(*other, AnimalState::Downed);
	}
}

bool AnimalSystem::CommonStateTurn(entt::entity entity, Animal& animal)
{
	switch (animal.state)
	{
	case AnimalState::Downed:
		Downed(animal);
		return true;
	case AnimalState::BeingEaten:
		BeingEatenTurn(entity, animal);
		return true;
	case AnimalState::Landed:
		Landed(entity, animal);
		return true;
	case AnimalState::SetDying:
		// An animal that a script tells to start to die dies in its turn. A miracle's animal fades in its own way.
		if (EntityRegistry().AllOf<SpellAnimal>(entity))
		{
			return false;
		}
		SetDying(entity);
		return true;
	default:
		return false;
	}
}

void AnimalSystem::WaitForClipOfState(Animal& animal, AnimalState next)
{
	animal.afterClip = next;
	animal.state = AnimalState::WaitForClip;
}

void AnimalSystem::Downed(Animal& animal)
{
	// The fall started when the hunter brought it down. After the fall, the hunter eats it for a number of turns
	WaitForClipOfState(animal, AnimalState::BeingEaten);
	animal.turnsUntilStateChange = static_cast<int16_t>(being_eaten_rules::k_BeingEatenTurns);
}

void AnimalSystem::BeingEatenTurn(entt::entity entity, Animal& animal)
{
	const auto step = being_eaten_rules::BeingEatenTurn(static_cast<uint16_t>(animal.turnsUntilStateChange),
	                                                    EntityRegistry().AllOf<Indestructible>(entity));
	animal.turnsUntilStateChange = static_cast<int16_t>(step.turnsLeft);
	switch (step.result)
	{
	case being_eaten_rules::EatenTurn::Eaten:
		break;
	case being_eaten_rules::EatenTurn::GetsUp:
		// Nothing can kill it: it gets up as after a landing
		WaitForClip(animal, AnimalState::Landed);
		break;
	case being_eaten_rules::EatenTurn::Dies:
		// It dies, and its body lies dead for the turns that are left
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Animals: #{} eaten", static_cast<uint32_t>(entity));
		ecs::world_objects::SetLife(entity, 0.0f);
		WaitForClip(animal, AnimalState::Dead);
		break;
	}
}

void AnimalSystem::Landed(entt::entity entity, Animal& animal)
{
	// Its flock's home moves by its kind's rule
	ChooseLair(entity, animal);
	// It plays its landing clip to the end. Then it joins a flock near it if it can, and it wanders off
	WaitForClipOfState(animal, physics::living::JoinsAFlockAfterLanding(animal.type) ? AnimalState::InteractDecideWhatToDo
	                                                                                 : AnimalState::DecideWhatToDo);
}

void AnimalSystem::FinishPouncing(entt::entity wolf, Animal& animal, entt::entity prey)
{
	auto& registry = EntityRegistry();
	auto& hunt = *HuntOf(wolf);
	Fed(wolf, animal);
	hunt.prey = entt::null;
	hunt.remembered.reset();
	// It goes its scale short of the prey to eat it
	const auto& preyTransform = registry.Get<const Transform>(prey);
	const auto preyPoint = Xz(preyTransform.position);
	const glm::vec3 prey3(preyPoint.x, preyTransform.position.y - Ground(preyPoint), preyPoint.y);
	const auto wolfPoint = Metres(animal.move.position);
	const glm::vec3 wolf3(wolfPoint.x, animal.height, wolfPoint.y);
	const auto goal = flock_rules::EatingPosition(prey3, wolf3, registry.Get<const Transform>(wolf).scale.x);
	SetupMoveTo(animal, glm::vec2(goal.x, goal.z), 0.0f, AnimalState::StartToEat);
	hunt.food = prey;
}

void AnimalSystem::Eat(entt::entity wolf, Animal& animal)
{
	auto& hunt = *HuntOf(wolf);
	if (--animal.turnsUntilStateChange != 0)
	{
		// Another mouthful, for as long as its eating plays
		WaitForClip(animal, AnimalState::Eat);
		return;
	}
	// Fed, it decides again, which for a spell wolf is running on
	Fed(wolf, animal);
	hunt.food = entt::null;
	SetTopState(animal, AnimalState::DecideWhatToDo);
}

entt::entity AnimalSystem::FindPrey(entt::entity wolf, const Animal& animal)
{
	auto& registry = EntityRegistry();
	auto& hunt = *HuntOf(wolf);
	// What lives in each map cell
	std::map<std::pair<int32_t, int32_t>, std::vector<std::pair<entt::entity, flock_rules::PreyFacts>>> cells;
	const auto add = [&](entt::entity entity, const Transform& transform, flock_rules::PreyFacts facts) {
		const auto cell = map_coords::CellOf(Xz(transform.position));
		facts.heightAboveLand = transform.position.y - Ground(Xz(transform.position));
		cells[{cell.x, cell.y}].emplace_back(entity, facts);
	};
	// What the hunter itself is
	const bool hunterInScript = registry.AllOf<InScript>(wolf);
	const bool hunterInFlock = registry.AllOf<Flock>(animal.flock);
	const bool hunterYoung = AgeOf(animal) < InfoOf(animal.type).grownUpAge;
	// The facts that every prey has: whether a script protects or controls it, whether the hunter can get to it, and
	// whether a hunter brought it down before
	const auto withCommonFacts = [&](entt::entity entity, flock_rules::PreyFacts prey) {
		prey.indestructible = registry.AllOf<Indestructible>(entity);
		prey.reachable = PreyReachable(entity) && !registry.AnyOf<InHand, HeldByCreature, InPhysics, CarriedByTornado>(entity);
		prey.scriptControlled = registry.AllOf<ScriptControlled>(entity);
		prey.hunterInScript = hunterInScript;
		prey.hunterInFlock = hunterInFlock;
		prey.hunterYoung = hunterYoung;
		prey.broughtDown = registry.AllOf<BroughtDown>(entity);
		return prey;
	};
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager& /*villager*/, const Transform& t) {
		// A walking skeleton has no meat for it
		add(entity, t,
		    withCommonFacts(
		        entity,
		        {.isVillager = true, .skeleton = registry.AllOf<WalkingSkeleton>(entity), .helpless = OutOfTheHunt(entity)}));
	});
	// An animal has food for the hunter when its kind's food is of a type that the hunter eats and is worth something
	const auto eats = static_cast<uint32_t>(InfoOf(animal.type).needsFoodTypes);
	registry.Each<const Animal, const Transform>([&](entt::entity entity, const Animal& other, const Transform& t) {
		if (entity != wolf)
		{
			const auto& otherInfo = InfoOf(other.type);
			const bool food = (eats & static_cast<uint32_t>(otherInfo.foodType)) != 0 && otherInfo.foodValue != 0.0f;
			add(entity, t,
			    withCommonFacts(
			        entity, {.isOtherAnimal = other.type != animal.type, .hasMeat = food, .helpless = OutOfTheHunt(entity)}));
		}
	});
	// So many cells spiralling out from its own, and the first prey met there is its prey
	auto cell = map_coords::CellOf(Metres(animal.move.position));
	map_coords::Spiral spiral;
	for (auto left = InfoOf(animal.type).farSightDistance; left > 0; --left)
	{
		if (const auto found = cells.find({cell.x, cell.y}); found != cells.end())
		{
			for (auto [entity, facts] : found->second)
			{
				facts.onCorridor = IsHuntingTargetValid(wolf, animal, entity);
				if (!flock_rules::IsPrey(facts))
				{
					continue;
				}
				// Once it remembers where it found prey, the game's mixed measure decides whether this one is near
				// enough to take instead
				const auto preyPoint = Xz(registry.Get<const Transform>(entity).position);
				if (hunt.remembered.has_value() &&
				    !flock_rules::CloserThanRemembered(animal.move.position, Fixed(preyPoint), Fixed(*hunt.remembered)))
				{
					continue;
				}
				hunt.remembered = preyPoint;
				return entity;
			}
		}
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
	}
	return entt::null;
}

// Fading and going

void AnimalSystem::StartFading(entt::entity animal)
{
	// From full, at rest, to nothing over its fade's turns
	if (auto* spellAnimal = EntityRegistry().TryGet<SpellAnimal>(animal); spellAnimal != nullptr && !spellAnimal->Fading())
	{
		spellAnimal->fade = animals::Zoomer(flock_rules::k_FullAlpha);
		spellAnimal->fade->SetTarget(0.0f, 0.0f, static_cast<float>(flock_rules::k_FadeTurns) * k_TurnSeconds);
	}
}

void AnimalSystem::ProcessFading()
{
	auto& registry = EntityRegistry();
	std::vector<entt::entity> gone;
	registry.Each<SpellAnimal>([&gone](entt::entity entity, SpellAnimal& spellAnimal) {
		if (!spellAnimal.fade.has_value())
		{
			return;
		}
		// Gone once its alpha is nothing
		if (spellAnimal.fade->Step(k_TurnSeconds) == 0.0f)
		{
			gone.push_back(entity);
		}
	});
	for (const auto entity : gone)
	{
		Remove(entity);
	}
}

float AnimalSystem::RadiusOf(entt::entity animal) const
{
	return ::RadiusOf(animal);
}

glm::vec3 AnimalSystem::MovementOf(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	if (data == nullptr)
	{
		return glm::vec3(0.0f);
	}
	const auto step = Metres(data->move.step);
	return {step.x, 0.0f, step.y};
}

void AnimalSystem::KillByEffect(entt::entity entity, glm::vec3 position)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return;
	}
	// Where it was put down, on the land
	animal->move.position = Fixed({position.x, position.z});
	animal->move.goal = animal->move.position;
	animal->height = 0.0f;
	SyncWorld(*animal);
	animal->previousPosition = animal->position;
	if (registry.AllOf<SpellAnimal>(entity))
	{
		StartFading(entity);
		return;
	}
	SetDying(entity);
}

void AnimalSystem::StartDyingWithoutDeath(entt::entity entity)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Animals: #{} starts to die without a death", static_cast<uint32_t>(entity));
	// Only its state changes. It plays its fall again even when it was dying or dead already. Its count of turns stays,
	// so that its body lies dead for the turns that its earlier states left.
	animal->state = AnimalState::Dying;
	animal->turnsInState = 0;
	if (const auto clip = animals::DyingClip(animal->type))
	{
		animal->animation = *clip;
		animal->clipPlace = 0;
	}
}

void AnimalSystem::SetDying(entt::entity entity)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	// One killed in the air starts dying only once it has come down
	if (animal == nullptr || registry.AllOf<InPhysics>(entity))
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Animals: #{} killed", static_cast<uint32_t>(entity));
	// It has no life left and falls dying; its body lies its time once dead. A bird falls out of the sky through the
	// physics while it dies. Killed again, its body only lies its full time afresh
	animal->turnsUntilStateChange = animals::k_TurnsToDieOver;
	if (animal->dying)
	{
		return;
	}
	animal->dying = true;
	animal->life = 0.0f;
	// It lies dead on its left side, unless a landing after this shows how it lies
	animal->landingPose = physics::living::LandingPose::None;
	animal->state = AnimalState::Dying;
	animal->turnsInState = 0;
	if (const auto clip = animals::DyingClip(animal->type))
	{
		animal->animation = *clip;
		animal->clipPlace = 0;
	}
}

void AnimalSystem::FallDying(entt::entity entity, const Animal& animal)
{
	if (!Locator::dynamicsSystem::has_value())
	{
		return;
	}
	// Forward at its speed across the land, tumbling about its side axis
	constexpr glm::vec3 k_DyingTumble {5.0f, 0.0f, 0.0f};
	const auto step = Metres(animal.move.step);
	const float speed = glm::length(step) * k_TurnsPerSecond;
	const glm::vec3 forward(portable_math::Cos(animal.heading), 0.0f, portable_math::Sin(animal.heading));
	Locator::dynamicsSystem::value().InitialisePhysics(entity, {.velocity = forward * speed, .spin = k_DyingTumble});
}

void AnimalSystem::ProcessDeath(entt::entity entity, Animal& animal)
{
	if (animal.state == AnimalState::Dying)
	{
		// A dying bird falls out of the sky with the speed it flew at, tumbling, until it comes down
		if (IsBird(animal.type))
		{
			FallDying(entity, animal);
			return;
		}
		// Its fall played out once, it lies dead, out of its flock
		if (animal.turnsInState * k_TurnMilliseconds < PlayTimeOf(animal.animation))
		{
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Animals: #{} lies dead", static_cast<uint32_t>(entity));
		animal.state = AnimalState::Dead;
		animal.turnsInState = 0;
		if (const auto clip = animals::DeadClip(animal.type, animal.landingPose == physics::living::LandingPose::Front))
		{
			animal.animation = *clip;
			animal.clipPlace = 0;
		}
		LeaveFlock(entity, animal);
		return;
	}
	// A dead animal is in no flock
	LeaveFlock(entity, animal);
	// A land bird lies dead in its kind's clip
	if (const auto clip = animals::birds::DeadClip(animal.type); clip.has_value() && animal.animation != *clip)
	{
		animal.animation = *clip;
		animal.clipPlace = 0;
	}
	// Its time up, it goes in a puff of grey smoke; a body a script controls lies there until the script lets it go
	if (animals::DeadBodyGoes(animal.turnsUntilStateChange, EntityRegistry().AllOf<ScriptControlled>(entity)))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Animals: #{} dead body goes", static_cast<uint32_t>(entity));
		LeaveSmoke(entity, animal);
		Remove(entity);
	}
}

void AnimalSystem::SetFlockCentre(entt::entity flock, glm::vec2 centre)
{
	if (auto* flockData = EntityRegistry().TryGet<Flock>(flock))
	{
		flockData->place = map_coords::FromMetres(centre);
	}
}

void AnimalSystem::MoveFlockPlace(entt::entity flock, glm::ivec2 place)
{
	auto& registry = EntityRegistry();
	auto* flockData = registry.Valid(flock) ? registry.TryGet<Flock>(flock) : nullptr;
	if (flockData == nullptr)
	{
		return;
	}
	// The leader's goal is the new place, so a leader on its way somewhere turns there
	if (const auto leader = LeaderOf(flock); leader != entt::null && registry.Valid(leader))
	{
		if (auto* animal = registry.TryGet<Animal>(leader))
		{
			animal->move.goal = place;
		}
		else if (auto* wallHug = registry.TryGet<WallHug>(leader))
		{
			wallHug->goalPlace = place;
		}
	}
	flockData->place = map_coords::MapCoords {place.x, place.y, 0.0f};
}

entt::entity AnimalSystem::SeparateIntoOwnFlock(entt::entity entity)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	auto* old = animal != nullptr && registry.Valid(animal->flock) ? registry.TryGet<Flock>(animal->flock) : nullptr;
	if (old == nullptr)
	{
		return entt::null;
	}
	const auto domainRadius = old->domainRadius;
	const auto flockDistance = old->flockDistance;
	LeaveFlock(entity, *animal);
	// Its own flock is made where it stands and kept there, before taking the size of the one it left
	const auto own = CreateFlock(Metres(animal->move.position), static_cast<float>(Flock::k_DefaultDomainRadius),
	                             static_cast<float>(Flock::k_DefaultFlockDistance));
	auto& ownData = registry.Get<Flock>(own);
	ownData.place = map_coords::MapCoords {animal->move.position.x, animal->move.position.y, 0.0f};
	ownData.savedPlace = ownData.place;
	JoinLine(ownData, entity);
	animal->flock = own;
	ownData.domainRadius = domainRadius;
	ownData.flockDistance = flockDistance;
	return own;
}

void AnimalSystem::SendToSlaughter(entt::entity entity, glm::vec2 goal)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return;
	}
	// Its body goes the moment it lies dead
	animal->turnsUntilStateChange = 0;
	SetupMoveTo(*animal, goal, 0.0f, AnimalState::Dead);
}

uint16_t AnimalSystem::SpeedOf(entt::entity entity) const
{
	const auto* animal = EntityRegistry().TryGet<const Animal>(entity);
	return animal != nullptr ? animal->move.speed : 0;
}

void AnimalSystem::SetSpeedFromScript(entt::entity entity, float metresPerTurn)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	// A miracle wolf ignores a speed from a script
	if (animal == nullptr || animal->type == AnimalInfo::SpellWolf)
	{
		return;
	}
	animal->move.speed = animals::SpeedFromScript(metresPerTurn);
	// It plays the clip of its state at the new speed. A different clip starts from its beginning
	const auto clip = ClipFor(*animal, animal->state);
	if (clip != animal->animation)
	{
		animal->animation = clip;
		animal->clipPlace = 0;
	}
}

float AnimalSystem::SpeedForScript(entt::entity entity) const
{
	const auto* animal = EntityRegistry().TryGet<const Animal>(entity);
	if (animal == nullptr)
	{
		return 0.0f;
	}
	// An animal with no life, or one that lies dead, is dead
	const bool dead = animal->life == 0.0f || animal->state == AnimalState::Dead;
	return animals::SpeedForScript(animal->move.speed, dead);
}

void AnimalSystem::SetScale(entt::entity animal, float scale)
{
	if (auto* transform = EntityRegistry().TryGet<Transform>(animal))
	{
		const bool resized = transform->scale.x != scale;
		transform->scale = glm::vec3(scale);
		// An animal in the map's cells that changes its size comes out of them and goes in again
		if (resized)
		{
			ecs::thing_map_place::Reinsert(animal);
		}
	}
}

bool AnimalSystem::IsFrighteningToCreature(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	return data != nullptr && animals::FrightensCreatures(data->type);
}

bool AnimalSystem::IsAvailableForReaction(entt::entity entity) const
{
	const auto& registry = EntityRegistry();
	// The state is judged by the state it is to end up in
	const auto state = FinalGameStateOf(entity);
	if (!state.has_value())
	{
		return false;
	}
	const bool busyState = animals::IsBusyForReaction(*state);
	return animals::AvailableForReaction({
	    .scriptControlled = registry.AllOf<ScriptControlled>(entity),
	    .outOfTheWorld = registry.AnyOf<CarriedByTornado, InHand, InPhysics>(entity),
	    .busyState = busyState,
	});
}

bool AnimalSystem::SetupFleeFromObject(entt::entity entity, entt::entity object, Reaction reaction)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return false;
	}
	// It takes up the flee whatever the object; with the object gone its next step gives the flee up
	animal->reactingTo = object;
	animal->reaction = reaction;
	SetState(*animal, AnimalState::FleeingFromObject);
	return true;
}

bool AnimalSystem::SetupFleeFromPredator(entt::entity entity, entt::entity predator)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return false;
	}
	animal->reactingTo = predator;
	animal->reaction = Reaction::FleeFromPredator;
	SetTopState(*animal, AnimalState::FleeingFromPredator);
	return true;
}

std::optional<uint8_t> AnimalSystem::TopGameStateOf(entt::entity entity) const
{
	const auto* animal = EntityRegistry().TryGet<const Animal>(entity);
	return animal != nullptr ? std::optional(static_cast<uint8_t>(GameStateOf(animal->state))) : std::nullopt;
}

glm::vec3 AnimalSystem::FledThingMovement(entt::entity thing) const
{
	const auto& registry = EntityRegistry();
	if (registry.AllOf<Animal>(thing) && !registry.AllOf<InPhysics>(thing))
	{
		return MovementOf(thing);
	}
	if (const auto* walker = registry.TryGet<const WallHug>(thing); walker != nullptr && !registry.AllOf<InPhysics>(thing))
	{
		const auto step = Metres(walker->step);
		return {step.x, 0.0f, step.y};
	}
	const auto* entry = Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().Find(thing) : nullptr;
	return entry != nullptr && entry->body != nullptr ? entry->body->velocity : glm::vec3(0.0f);
}

void AnimalSystem::GiveUpReaction(entt::entity entity)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().StopReacting(entity);
	}
	StopReaction(entity, true);
}

void AnimalSystem::FleeFromPredator(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	const auto hunter = animal.reactingTo;
	const auto* at = hunter != entt::null && registry.Valid(hunter) ? registry.TryGet<const Transform>(hunter) : nullptr;
	const auto& here = registry.Get<const Transform>(entity).position;
	const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::FleeFromPredator));
	const float distance = at != nullptr ? gutils::GetDistanceInMetres(here, at->position) : 0.0f;
	switch (
	    animals::predators::AnimalFleeStep(hunter != entt::null, at != nullptr, distance, info.maxDistanceToRunAwayFromObject))
	{
	case animals::predators::AnimalFlees::Nothing:
		return;
	case animals::predators::AnimalFlees::Stop:
		GiveUpReaction(entity);
		return;
	case animals::predators::AnimalFlees::Run:
		break;
	}
	// A step away from it, across its way when it moves
	const auto movement = FledThingMovement(hunter);
	auto to = magic::FleePointFromStill(here, at->position);
	if (glm::length(movement) > 0.0f)
	{
		const float x = Random(magic::k_FleeJitter);
		const float z = Random(magic::k_FleeJitter);
		to = magic::FleePointFromMoving(here, at->position, movement, x, z);
	}
	if (!OnMap(Xz(to)))
	{
		return;
	}
	// At its kind's fleeing speed, which nothing changes until it stops fleeing; then it flees the hunter as it flees
	// anything
	const auto speed = SpeedStateOf(InfoOf(animal.type), 1);
	if (animals::grazers::IsGrazer(animal.type))
	{
		SetGrazerSpeed(animal, speed);
	}
	else
	{
		animal.move.speed = speed;
	}
	SetupMoveTo(animal, Xz(to), 0.0f, AnimalState::FleeingFromObject);
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Predator: animal {} runs from hunter {}, {:.1f} m away", static_cast<uint32_t>(entity),
		                    static_cast<uint32_t>(hunter), distance);
	}
}

bool AnimalSystem::SetupReactToFlyingObject(entt::entity entity, entt::entity object, float speed)
{
	auto& registry = EntityRegistry();
	auto* animal = registry.TryGet<Animal>(entity);
	const auto* at = registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (animal == nullptr || at == nullptr)
	{
		return false;
	}
	// Measured across the map, against how far the thing flies in two seconds
	const auto& here = registry.Get<const Transform>(entity).position;
	if (!living::AnimalFleesFlyingObject(living::MapDistance(here, at->position), speed))
	{
		return false;
	}
	animal->reactingTo = object;
	animal->reaction = Reaction::ReactToFlyingObject;
	SetState(*animal, AnimalState::FleeingFromObject);
	return true;
}

void AnimalSystem::RememberStateBeforeReaction(entt::entity entity)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (const auto state = FinalGameStateOf(entity); animal != nullptr && state.has_value())
	{
		animal->stateBeforeReaction = *state;
	}
}

void AnimalSystem::SetWanderBeforeReaction(entt::entity entity)
{
	if (auto* animal = EntityRegistry().TryGet<Animal>(entity))
	{
		animal->stateBeforeReaction = static_cast<uint8_t>(GameStateOf(AnimalState::Wander));
	}
}

bool AnimalSystem::SetupLookAtObject(entt::entity entity, entt::entity object)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return false;
	}
	animal->reactingTo = object;
	animal->reaction = Reaction::LookAtObject;
	SetTopState(*animal, AnimalState::LookingAtObject);
	return true;
}

bool AnimalSystem::SetupGotoFood(entt::entity entity, entt::entity food)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return false;
	}
	animal->reactingTo = food;
	animal->reaction = Reaction::ReactToFood;
	SetTopState(*animal, AnimalState::GotoFood);
	return true;
}

uint32_t AnimalSystem::ReactToFoodPriority(entt::entity entity, entt::entity food, uint32_t tablePriority) const
{
	const auto& registry = EntityRegistry();
	const auto* animal = registry.TryGet<const Animal>(entity);
	const auto* here = registry.TryGet<const Transform>(entity);
	const auto* there = food != entt::null && registry.Valid(food) ? registry.TryGet<const Transform>(food) : nullptr;
	if (animal == nullptr || here == nullptr || there == nullptr)
	{
		return 0;
	}
	// The game does not take food that the physics moves
	if (registry.AllOf<InPhysics>(food))
	{
		return 0;
	}
	const bool holdsFood = Locator::resourceStoreSystem::has_value() &&
	                       Locator::resourceStoreSystem::value().GetResource(food, ResourceType::Food) != 0;
	const auto& reactionInfo = Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToFood));
	const auto& info = InfoOf(animal->type);
	const auto* grazer = registry.TryGet<const Grazer>(entity);
	const auto* foodInfo = world_objects::InfoOf(food);
	// TODO(animals): the game also passes over food with a flag of its own whose meaning is unknown
	const bool interested = animals::InterestedInFood({
	    .available = true,
	    .inPhysics = false,
	    .hungerLimit = info.hunger,
	    .hunger = grazer != nullptr ? grazer->needs.hunger : static_cast<int16_t>(animal->hunger),
	    .eats = info.needsFoodTypes,
	    .food = foodInfo != nullptr ? foodInfo->foodType : FoodType::None,
	});
	return animals::FoodPriority(holdsFood, gutils::GetDistanceInMetres(here->position, there->position),
	                             reactionInfo.maxDistanceToRunAwayFromObject, interested, tablePriority);
}

bool AnimalSystem::LookOrGoToFood(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	switch (animal.state)
	{
	case AnimalState::LookingAtObject:
		LookAtThing(entity, animal);
		return true;
	case AnimalState::GotoFood:
	{
		// It goes to the edge of the food on its own side. With the food gone it stops the reaction
		const auto food = animal.reactingTo;
		const auto* there = food != entt::null && registry.Valid(food) ? registry.TryGet<const Transform>(food) : nullptr;
		if (there == nullptr)
		{
			GiveUpReaction(entity);
			return true;
		}
		const auto at = Xz(there->position);
		const auto here = Metres(animal.move.position);
		const float angle = gutils::Get3DAngleFromXZ(at, here);
		const float gap = object_measures::TwoDRadius(registry, food) + object_measures::TwoDRadius(registry, entity);
		SetupMoveRoundTo(entity, animal, villager_routine::Polar(at, angle, gap), AnimalState::ArrivesAtFood);
		return true;
	}
	case AnimalState::ArrivesAtFood:
		GiveUpReaction(entity);
		return true;
	case AnimalState::MoveToPos:
		// A grazer moves to the food by its own move; another animal moves here
		if (animal.finalState != AnimalState::ArrivesAtFood || registry.AllOf<Grazer>(entity))
		{
			return false;
		}
		if (animal.walksRound ? GroundMoveTo(entity, animal) : MoveTo3D(animal))
		{
			SetTopState(animal, animal.finalState);
		}
		return true;
	default:
		return false;
	}
}

void AnimalSystem::LookAtThing(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	// With no thing, because the reaction is gone, it stays in its state and does nothing
	if (animal.reactingTo == entt::null)
	{
		return;
	}
	const auto* there = registry.Valid(animal.reactingTo) ? registry.TryGet<const Transform>(animal.reactingTo) : nullptr;
	if (there == nullptr)
	{
		return;
	}
	const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(animal.reaction));
	const auto& here = registry.Get<const Transform>(entity).position;
	if (info.maxDistanceToRunAwayFromObject < gutils::GetDistanceInMetres(here, there->position))
	{
		GiveUpReaction(entity);
		return;
	}
	const auto target = gutils::GetAngleFromXZ(map_coords::MapCoords {.x = animal.move.position.x, .z = animal.move.position.y},
	                                           map_coords::FromMetres(Xz(there->position)));
	animal.move.angle = animals::LookTowards(animal.move.angle, target, animals::k_LookTurnStep);
	SyncWorld(animal);
}

void AnimalSystem::StopReaction(entt::entity entity, bool resetState)
{
	auto* animal = EntityRegistry().TryGet<Animal>(entity);
	if (animal == nullptr)
	{
		return;
	}
	animal->reactingTo = entt::null;
	if (!resetState)
	{
		return;
	}
	// Every animal goes into the resume state of the state that it was in before the reaction, whatever it does now. The
	// table gives deciding again for most states. It gives the empty state for a state that a script holds, and for the
	// states of a reaction.
	const auto next =
	    animals::StateAfterReacting(animal->stateBeforeReaction, Locator::infoConstants::value().animalStateTable);
	// The state of the hand refuses most changes: a held animal stays in it
	if (animal->state == AnimalState::InHand && !animals::HandStateAllows(next))
	{
		return;
	}
	if (const auto state = AnimalStateOfGame(next))
	{
		SetTopState(*animal, *state);
	}
	else if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_WARN(logger, "Reaction: animal {} has no state for the game's state {}", static_cast<uint32_t>(entity),
		                   next);
	}
}

bool AnimalSystem::Flee(entt::entity entity, Animal& animal)
{
	if (animal.state == AnimalState::FleeingFromPredator)
	{
		FleeFromPredator(entity, animal);
		return true;
	}
	const bool running = animal.state == AnimalState::MoveToPos && animal.finalState == AnimalState::FleeingAndLookingAtObject;
	if (animal.state != AnimalState::FleeingFromObject && animal.state != AnimalState::FleeingAndLookingAtObject && !running)
	{
		return false;
	}
	auto& registry = EntityRegistry();
	// Running to where it flees, it gets there first
	if (running)
	{
		if (animal.walksRound ? GroundMoveTo(entity, animal) : MoveTo3D(animal))
		{
			SetState(animal, animal.finalState);
		}
		return true;
	}
	// With no thing to flee, because its reaction is gone, it stays in its state and does nothing
	if (animal.reactingTo == entt::null)
	{
		return true;
	}
	const auto* at = registry.Valid(animal.reactingTo) ? registry.TryGet<const Transform>(animal.reactingTo) : nullptr;
	const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(animal.reaction));
	// When the thing that it flees is gone, it gives up the reaction
	if (at == nullptr)
	{
		GiveUpReaction(entity);
		return true;
	}
	auto& transform = registry.Get<Transform>(entity);
	const auto& here = transform.position;
	const float distance = living::MapDistance(here, at->position);
	// It watches the thing as it looks at any thing: it turns to the thing, and it stops once the thing is too far
	if (animal.state == AnimalState::FleeingAndLookingAtObject)
	{
		LookAtThing(entity, animal);
		return true;
	}
	const glm::vec3 velocity = FledThingMovement(animal.reactingTo);
	const bool coming = magic::ComingTowards(here, at->position, velocity);
	switch (living::FleeFromObject(distance, info.minDistanceToRunAwayFromObject, info.maxDistanceToRunAwayFromObject, coming))
	{
	case living::FleeStep::GiveUp:
		GiveUpReaction(entity);
		break;
	case living::FleeStep::Watch:
		SetState(animal, AnimalState::FleeingAndLookingAtObject);
		break;
	case living::FleeStep::Run:
	{
		// A step away across the land, across the thing's way when it moves, at its fleeing speed
		auto to = magic::FleePointFromStill(here, at->position);
		if (glm::length(velocity) > 0.0f)
		{
			const float x = Random(magic::k_FleeJitter);
			const float z = Random(magic::k_FleeJitter);
			to = magic::FleePointFromMoving(here, at->position, velocity, x, z);
		}
		if (OnMap(Xz(to)))
		{
			const auto clip = animal.animation;
			const auto place = animal.clipPlace;
			// The speed of its kind the state table gives fleeing
			const auto& row = Locator::infoConstants::value().villagerStateTable.at(
			    static_cast<size_t>(VillagerStates::FleeingFromObjectReaction));
			animal.move.speed = SpeedStateOf(InfoOf(animal.type), std::min<size_t>(row.speedIndex, 5));
			SetupMoveRoundTo(entity, animal, Xz(to), AnimalState::FleeingAndLookingAtObject);
			// A grazer runs in its moving clip at its fleeing speed; the other animals have no clip of their own for
			// moving and keep the one they had
			if (!animals::grazers::IsGrazer(animal.type))
			{
				animal.animation = clip;
				animal.clipPlace = place;
			}
		}
		break;
	}
	}
	return true;
}

std::optional<bool> AnimalSystem::BirdCanBePickedUpByCreature(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	if (data == nullptr || !IsBird(data->type))
	{
		return std::nullopt;
	}
	return animals::birds::CreatureCanPickUp(data->height);
}

bool AnimalSystem::CanBeStompedOnByCreature(entt::entity animal, float creatureHeight) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	// TODO(animals): the other kinds have a rule of their own, not ported; a creature stamps on none of them
	return data != nullptr && IsBird(data->type) && animals::birds::CreatureCanStompOn(data->height, creatureHeight);
}

void AnimalSystem::LeaveSmoke(entt::entity entity, const Animal& animal) const
{
	if (!Locator::explosionSystem::has_value())
	{
		return;
	}
	const auto& registry = EntityRegistry();
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (transform == nullptr)
	{
		return;
	}
	// Its height: its model's, times its size
	const float height = mesh != nullptr && meshes.Contains(mesh->id)
	                         ? meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.x
	                         : 0.0f;
	Locator::explosionSystem::value().AddSmoke(animals::DeadBodySmokePlace(transform->position, animal.heading, height),
	                                           animals::k_DeadBodySmokeSize, dust_puff::Kind::Smoke);
}

bool AnimalSystem::CanPlayerPickUp(entt::entity animal) const
{
	const auto* data = EntityRegistry().TryGet<const Animal>(animal);
	return data != nullptr && InfoOf(data->type).playerCanPickUp != 0;
}

void AnimalSystem::Remove(entt::entity animal)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(animal))
	{
		return;
	}
	if (const auto* data = registry.TryGet<const Animal>(animal))
	{
		if (auto* flockData = registry.Valid(data->flock) ? registry.TryGet<Flock>(data->flock) : nullptr)
		{
			std::erase(flockData->members, animal);
			if (flockData->members.empty())
			{
				registry.Destroy(data->flock);
			}
		}
	}
	registry.Destroy(animal);
}

void AnimalSystem::LeaveFlock(entt::entity entity)
{
	if (auto* animal = EntityRegistry().TryGet<Animal>(entity))
	{
		LeaveFlock(entity, *animal);
	}
}

void AnimalSystem::LeaveFlock(entt::entity entity, Animal& animal)
{
	auto& registry = EntityRegistry();
	if (auto* flockData = registry.Valid(animal.flock) ? registry.TryGet<Flock>(animal.flock) : nullptr)
	{
		std::erase(flockData->members, entity);
		if (flockData->members.empty())
		{
			registry.Destroy(animal.flock);
		}
	}
	animal.flock = entt::null;
}

// Drawing

void AnimalSystem::ShowTurnEnd()
{
	auto& registry = EntityRegistry();
	registry.Each<const Animal, Transform>([&registry](entt::entity entity, const Animal& animal, Transform& transform) {
		// What carries it, holds it or moves it in the physics places it. If nothing does, the turn sees it at the place of
		// its last turn, upright and turned to its heading, as the game sees each thing by its own angle. Only the frames
		// draw the tilt and the lean on the slope.
		if (!registry.AnyOf<CarriedByTornado, InHand, HeldByCreature, InPhysics>(entity))
		{
			transform.position = animal.position;
			transform.rotation = animals::Orientation(animal.heading, 0.0f);
		}
	});
}

void AnimalSystem::Update(std::chrono::milliseconds frameGameTime, float turnFraction)
{
	auto& registry = EntityRegistry();
	const auto elapsed = static_cast<uint32_t>(frameGameTime.count());
	// A screen refresh of the game's, when a clip held at its end plays its sounds again; none while the game is paused
	const bool refresh = Locator::time::has_value() && !Locator::time::value().IsPaused() &&
	                     _soundPace.Step(static_cast<float>(Locator::time::value().GetFrameRealTime().count()));
	const float elapsedSeconds = static_cast<float>(elapsed) * 0.001f;
	const auto& animations = Locator::resources::value().GetAnimations();
	auto& meshes = Locator::resources::value().GetMeshes();
	// The frame writes only the drawn place, the tilt, the clip and the pose. The turns go by the Transform.
	registry.Each<Animal, const Transform, const Mesh>([&](entt::entity entity, Animal& animal, const Transform& transform,
	                                                       const Mesh& mesh) {
		// A tornado carrying it places it, as does a hand or a creature holding it or the physics moving it; it still plays
		// its clip
		animal.drawn.reset();
		if (!registry.AnyOf<CarriedByTornado, InHand, HeldByCreature, InPhysics>(entity))
		{
			// Drawn between its last two turns, tilted as far as its bank has glided
			const auto between = animals::DrawnAt(animal.previousPosition, animal.position, animal.previousHeading,
			                                      animal.heading, turnFraction);
			const float bank = animal.bank.Step(elapsedSeconds);
			auto rotation = animals::Orientation(between.heading, bank);
			// An animal on the land leans to its slope: each of its x and z axes lifts or drops to the land at its end
			if (!(animal.height > animals::k_HighestToFollowSlope) && Locator::terrainSystem::has_value())
			{
				const auto turned = animals::Orientation(between.heading, 0.0f);
				const float scale = transform.scale.x;
				const auto& at = between.position;
				const auto& land = Locator::terrainSystem::value();
				const auto landAt = [&](const glm::vec3& axis) {
					return land.GetAltitude(animals::SlopeSampleUnit(at.x + (axis.x * scale)),
					                        animals::SlopeSampleUnit(at.z + (axis.z * scale)));
				};
				const float alongX = animals::SlopeLift(landAt(turned[0]), at.y);
				const float alongZ = animals::SlopeLift(landAt(turned[2]), at.y);
				rotation = turned * animals::SlopeLean(alongX, alongZ) * glm::mat3(portable_math::EulerAngleZ(bank));
			}
			animal.drawn = Animal::DrawnPlace {.position = between.position, .rotation = rotation};
		}
		const auto drawnAt = animal.drawn.has_value() ? animal.drawn->position : transform.position;
		auto* pose = registry.TryGet<AnimalPose>(entity);
		// The clips are kept by the hash of their number in the animation pack
		const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(animal.animation));
		if (pose == nullptr || animal.animation == AnimId::Invalid || !animations.Contains(clipId) || !meshes.Contains(mesh.id))
		{
			return;
		}
		// Each animal plays its own clip: by the ground it covers while it moves, else by the clock
		const auto clip = animations.Handle(clipId);
		const animals::ClipTiming timing {.playTime = clip->GetPlayTime(),
		                                  .frameCount = clip->GetFrames().size(),
		                                  .looping = clip->IsLooping(),
		                                  .playedByTime = clip->IsPlayedByTime(),
		                                  .stride = clip->GetStride()};
		const bool moving = (animal.state == AnimalState::Wander && animal.move.step != glm::ivec2(0)) ||
		                    (animal.move.stage != animals::MoveStage::AtGoal &&
		                     (animal.state == AnimalState::MoveToPos || animal.state == AnimalState::SpecialMoveToPos ||
		                      animal.state == AnimalState::FollowFlock || animal.state == AnimalState::Chase));
		// The part of a millisecond each moving frame leaves is carried on to the next, so that the legs keep pace with
		// the ground at any frame rate (the game dropped it every frame; deviations list)
		const auto carried =
		    animals::FramePlay(timing, moving, animal.move.speed, elapsed, transform.scale.x, animal.groundCarry);
		const auto played = carried.played;
		animal.groundCarry = carried.carry;
		// The sounds on the clip's frames it passes play from the animal
		ecs::clip_sound_player::Play(entity, animal.animation, *clip, animal.clipPlace,
		                             static_cast<uint32_t>(std::max(played, 0)), drawnAt, refresh);
		animal.clipPlace = animals::AdvanceClip(timing, animal.clipPlace, played);

		// A wolf fades out of sight as it goes; the doves and bats stay whole until they go
		pose->alpha = 255;
		if (const auto* spellAnimal = registry.TryGet<const SpellAnimal>(entity);
		    pose->light == AnimalLight::White && spellAnimal != nullptr)
		{
			pose->alpha = static_cast<uint8_t>(std::clamp(std::nearbyint(spellAnimal->Alpha()), 0.0f, 255.0f));
		}

		// The pose between the two keyframes around its place, each bone then placed by its parent
		const auto model = meshes.Handle(mesh.id);
		const auto& frames = clip->GetFrames();
		const auto& parents = model->GetBoneParents();
		const auto span = animals::SpanAt(timing, animal.clipPlace);
		if (!model->IsBoned() || frames.empty() || frames[span.from].bones.size() != parents.size() ||
		    frames[span.to].bones.size() != parents.size())
		{
			pose->bones.clear();
			return;
		}
		auto& bones = pose->bones;
		bones.resize(parents.size());
		const auto& from = frames[span.from].bones;
		const auto& to = frames[span.to].bones;
		for (size_t i = 0; i < bones.size(); ++i)
		{
			bones[i] = from[i] + ((to[i] - from[i]) * span.t);
			if (parents[i] != std::numeric_limits<uint32_t>::max())
			{
				bones[i] = bones[parents[i]] * bones[i];
			}
		}
	});
	registry.SetDirty();
}

void AnimalSystem::Reset()
{
	_turn = 0;
	_flocksMade = 0;
}
