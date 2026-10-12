/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerHome.h"

#include <cmath>

#include <algorithm>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Map.h"
#include "ECS/NearestSearch.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WalkArrival.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerBreeding.h"
#include "VillagerBuild.h"
#include "VillagerChecks.h"
#include "VillagerDisciples.h"
#include "VillagerFire.h"
#include "VillagerFisherman.h"
#include "VillagerFood.h"
#include "VillagerFootpath.h"
#include "VillagerForester.h"
#include "VillagerWorship.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace components = openblack::ecs::components;
namespace villager_home = openblack::ecs::villager_home;
namespace routine = openblack::ecs::villager_routine;
namespace needs = openblack::ecs::villager_needs;
namespace villager_age = openblack::ecs::villager_age;
namespace world_objects = openblack::ecs::world_objects;
namespace villager_fire = openblack::ecs::villager_fire;
namespace villager_build = openblack::ecs::villager_build;
namespace villager_footpath = openblack::ecs::villager_footpath;
namespace villager_forester = openblack::ecs::villager_forester;
namespace villager_fisherman = openblack::ecs::villager_fisherman;
namespace villager_disciple = openblack::ecs::villager_disciple;
namespace villager_food = openblack::ecs::villager_food;
namespace villager_checks = openblack::ecs::villager_checks;
namespace villager_worship = openblack::ecs::villager_worship;
namespace wall_hug = openblack::ecs::wall_hug;
namespace walk_arrival = openblack::ecs::walk_arrival;
using ClearAreaFilter = openblack::ecs::systems::TownSystemInterface::ClearAreaFilter;

namespace
{

/// At home a villager sees to its needs when they come to this share of the more pressing of: rest at the life it
/// sleeps on to, and food at the hungry mark
constexpr float k_AtHomeNeedShare = 0.9f;
/// A villager sitting down wants this much room, times its own size
constexpr float k_SitDownRoom = 1.2f;
/// Looking for room to sit down, a villager tries this far round where it is, in steps of this
constexpr float k_SitDownSearch = 5.0f;
constexpr float k_SitDownStep = 1.0f;
/// Far from where it means to sit, it first steps this far towards it
constexpr float k_SitDownTowards = 5.0f;
/// It sits within this share of the house's chill-out distance of its home, looking out this many times as far
constexpr float k_OutsideHouseShare = 0.5f;
constexpr float k_OutsideHouseLookOut = 10.0f;
/// Spots outside a house are a third of a turn either side of straight out
constexpr float k_OutsideHouseDivisions = 3.0f;
/// Sitting down it turns to face the way it looks at most this far, in the game's angles
constexpr int32_t k_SitDownTurn = 0x100;
/// While its town is in an emergency a gathered villager goes home one time in this many, and after it, decides
/// afresh one time in this many
constexpr uint32_t k_EmergencyGoHomeChance = 12;
constexpr uint32_t k_AfterEmergencyDecideChance = 5;
/// The share of a turn's work per thing at most a crowd of them gathering takes to spread round the gathering place
constexpr float k_CongregateMost = 1.99999f;
constexpr float k_CongregatePerVillager = 0.01f;
constexpr float k_CongregateSpread = 10.0f;
constexpr float k_CongregateLeast = 10.0f;

auto& WorldRegistry()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return WorldRegistry().ToEntity(action);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

VillagerStates State(const LivingAction& action, LivingAction::Index index)
{
	return Locator::livingActionSystem::value().VillagerGetState(action, index);
}

VillagerStates FinalState(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetFinalState(action);
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

const GVillagerInfo& InfoOf(entt::entity villager)
{
	return InfoOf(WorldRegistry().Get<const Villager>(villager));
}

entt::entity AbodeOf(entt::entity villager)
{
	auto& registry = WorldRegistry();
	const auto abode = registry.Get<const Villager>(villager).abode;
	return abode != entt::null && registry.Valid(abode) && registry.AllOf<Abode>(abode) ? abode : entt::null;
}

entt::entity TownOf(entt::entity villager)
{
	auto& registry = WorldRegistry();
	const auto town = registry.Get<const Villager>(villager).town;
	return town != entt::null && registry.Valid(town) && registry.AllOf<Town>(town) ? town : entt::null;
}

bool IsChild(entt::entity villager)
{
	return WorldRegistry().Get<const Villager>(villager).lifeStage == Villager::LifeStage::Child;
}

bool IsAtHome(entt::entity villager)
{
	return WorldRegistry().AnyOf<AtHome>(villager);
}

float LifeOf(entt::entity villager)
{
	return WorldRegistry().Get<const Villager>(villager).life;
}

glm::vec2 PositionOf(entt::entity entity)
{
	return glm::xz(WorldRegistry().Get<const Transform>(entity).position);
}

bool InEmergency(entt::entity town)
{
	return town != entt::null && Locator::townSystem::value().IsInStateOfEmergency(town);
}

auto FloatRandom()
{
	return [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); };
}

auto IntRandom()
{
	return [](uint32_t limit) { return Locator::gameRandom::value().GameRand(limit); };
}

/// Whether the villager can be called on: not held, flying, carried off or dead
bool IsAvailable(entt::entity villager)
{
	return !WorldRegistry().AnyOf<InHand, InPhysics, CarriedByTornado, VillagerDeath>(villager);
}

/// The way the villager faces, as an angle in radians on the ground
float FacingOf(entt::entity villager)
{
	const auto x = WorldRegistry().Get<const Transform>(villager).rotation * glm::vec3(1.0f, 0.0f, 0.0f);
	return -std::atan2(-x.z, x.x) - glm::radians(90.0f);
}

/// Turns the villager towards a point, by at most a step of the game's angles
void LookAtPos(entt::entity villager, glm::vec2 point, int32_t step)
{
	auto& transform = WorldRegistry().Get<Transform>(villager);
	const auto target = static_cast<int32_t>(gutils::GetAngleFromXZ(glm::xz(transform.position), point));
	const auto facing = static_cast<int32_t>(gutils::ConvertAngle3DToGame(FacingOf(villager)));
	const int32_t difference = target - facing;
	int32_t angle = target;
	if (std::abs(difference) >= step)
	{
		const bool forwards = difference < 1 ? std::abs(difference) > 0x3FF : difference < 0x400;
		angle = (facing + (forwards ? step : -step)) & gutils::k_GameAngleMask;
	}
	transform.rotation = glm::eulerAngleY(-gutils::ConvertGameAngleTo3D(angle) - glm::radians(90.0f));
	WorldRegistry().SetDirty();
}

/// Where the walk holds the villager, in whole map units: where it last put it, unless something else has moved it since
glm::ivec2 WalkPositionOf(entt::entity villager)
{
	const auto metres = PositionOf(villager);
	const auto* wallHug = WorldRegistry().TryGet<const WallHug>(villager);
	if (wallHug != nullptr && wallHug->placedAt == metres)
	{
		return wallHug->position;
	}
	return wall_hug::ToWhole(metres);
}

/// Whether the villager is closer to a point than the step its walk makes in a turn
bool AreWeThere(entt::entity villager, glm::vec2 point)
{
	const auto* wallHug = WorldRegistry().TryGet<const WallHug>(villager);
	const float speed = wallHug != nullptr ? wallHug->speed : 0.0f;
	return walk_arrival::WithinAStep(WalkPositionOf(villager), wall_hug::ToWhole(point), speed);
}

/// The state of the first of these walk tags the villager carries
template <typename... Tags>
std::optional<MoveState> FirstWalkTag(entt::entity villager)
{
	const auto& registry = WorldRegistry();
	std::optional<MoveState> state;
	((state = !state.has_value() && registry.AnyOf<Tags>(villager) ? std::optional(Tags::k_Value) : state), ...);
	return state;
}

/// The state of the villager's walk, none when it has no walk under way
std::optional<MoveState> WalkStateOf(entt::entity villager)
{
	return FirstWalkTag<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                    MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
}

const GAbodeInfo& AbodeInfoOf(entt::entity abode)
{
	// Its own row, its tribe's kind of building; the first row of its kind should its model not be known
	if (const auto* info = world_objects::AbodeInfoOf(abode); info != nullptr)
	{
		return *info;
	}
	return Locator::infoConstants::value().abode.at(static_cast<size_t>(WorldRegistry().Get<const Abode>(abode).type));
}

bool IsBuilt(entt::entity abode)
{
	const auto* progress = WorldRegistry().TryGet<const BuildProgress>(abode);
	return progress == nullptr || progress->built >= 1.0f;
}

bool IsRepaired(entt::entity abode)
{
	return world_objects::LifeOf(abode) >= 1.0f;
}

/// How strongly the villager's own needs press it before its town's desires
float OwnDesiresTrigger(entt::entity villager)
{
	const auto& person = WorldRegistry().Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	return needs::OwnDesiresTrigger({
	    .woken = person.woken,
	    .hungry = needs::IsHungry(person.food, info.hungryForFood),
	    .child = person.lifeStage == Villager::LifeStage::Child,
	    .foodDesire = needs::DesireForFood(person.food),
	    .lifeDesire = needs::LifeDesireFromLife(person.life, info.damageThresholdToGoHome),
	    .ownDesireThreshold = info.ownDesireThreshold,
	});
}

/// Offers the villager to what its town wants most; a knock on its home no longer holds it back after
uint32_t CheckNeededForTownDesire(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	const auto result = Locator::townDesireSystem::value().OfferVillager(
	    town, IsChild(villager), OwnDesiresTrigger(villager), [&action](TownDesireInfo desire) -> uint32_t {
		    // TODO(villagers): the game has villagers play and supply the workshop for their town too
		    switch (desire)
		    {
		    case TownDesireInfo::ForRelaxation:
			    // The town offers the villager something to relax at: its artefacts' dances
			    return Locator::artefactSystem::has_value() &&
			                   Locator::artefactSystem::value().OfferRelaxation(TownOf(EntityOf(action)), EntityOf(action))
			               ? 1
			               : 0;
		    case TownDesireInfo::ForFood:
			    return villager_fisherman::CheckSatisfyFoodDesire(action);
		    case TownDesireInfo::ForWood:
			    return villager_forester::CheckSatisfyWoodDesire(action);
		    case TownDesireInfo::ForSleep:
			    return villager_home::CheckSatisfySleep(action);
		    case TownDesireInfo::ForSupplyWorship:
			    return villager_worship::CheckSatisfySupplyWorship(action);
		    case TownDesireInfo::ToBuild:
			    return villager_build::CheckSatisfyToBuild(action);
		    case TownDesireInfo::ToRepair:
			    return villager_build::CheckSatisfyToRepair(action);
		    case TownDesireInfo::ForAbodes:
			    return villager_build::CheckSatisfyAbodes(action);
		    case TownDesireInfo::ForCivicBuilding:
			    return villager_build::CheckSatisfyCivicBuildings(action);
		    default:
			    return 0;
		    }
	    });
	WorldRegistry().Get<Villager>(villager).woken = false;
	return result;
}

/// The villager sees to its own hunger or rest past a threshold, the more pressing first
bool CheckSatisfyOwnDesire(LivingAction& action, float threshold)
{
	const auto villager = EntityOf(action);
	const auto& person = WorldRegistry().Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	return needs::SatisfyOwnDesire(
	    needs::DesireForFood(person.food), needs::LifeDesireFromLife(person.life, info.damageThresholdToGoHome), threshold,
	    [&action] { return villager_home::CheckSatisfySleep(action) == 1; },
	    [&action] { return villager_food::CheckSatisfyOwnFoodDesire(action); });
}

/// A homeless villager moves into a building of its town with room, and goes home to it
bool CheckHomelessMoveIntoAbode(LivingAction& action)
{
	auto& registry = WorldRegistry();
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return false;
	}
	auto& towns = Locator::townSystem::value();
	const auto abode = towns.FindAbodeWithSpace(town, villager, 0.0f);
	if (abode == entt::null)
	{
		return false;
	}
	std::erase(registry.Get<Town>(town).homelessVillagers, villager);
	towns.AddVillagerToAbode(abode, villager);
	SetTopState(action, VillagerStates::GoHome);
	return true;
}

/// Going to bed, once a stay: it may die of old age first
bool CheckWhenGoingToBed(entt::entity villager)
{
	auto& atHome = WorldRegistry().Get<AtHome>(villager);
	if (atHome.beenToBed)
	{
		return true;
	}
	atHome.beenToBed = true;
	const auto& person = WorldRegistry().Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	if (villager_age::DiesOfOldAge(villager_age::AgeNow(person),
	                               {.grownUp = info.grownUpAge, .old = info.oldAge, .oldest = info.retirementAge},
	                               FloatRandom(), IntRandom()))
	{
		villager_fire::DieByEffect(villager, villager_fire::DeathCause {.reason = DeathReason::OldAge, .weight = info.life});
		return false;
	}
	// When its town wants sleep with all its heart, a couple in bed may make a child
	ecs::villager_breeding::BedTimeConceptions(villager);
	return true;
}

/// A sleeper's check: unless poisoned, which wakes it, it gains life, and sleeps on while its town wants sleep most or
/// it is still hurt
bool DoSleeping(LivingAction& action, float multiplier)
{
	const auto villager = EntityOf(action);
	const auto& person = WorldRegistry().Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	const auto town = TownOf(villager);
	const auto result = routine::CheckSleep({
	    .poisoned = WorldRegistry().AllOf<Poisoned>(villager),
	    .life = person.life,
	    .fullLife = info.life,
	    .restores = info.restAtHomeRestoresLifeBy,
	    .multiplier = multiplier,
	    .townWantsSleep =
	        town != entt::null && Locator::townDesireSystem::value().GetMostWanted(town) == TownDesireInfo::ForSleep,
	    .sleepUntil = info.damageThresholdToSleepUntil,
	});
	if (result.gain > 0.0f)
	{
		world_objects::IncreaseLife(villager, result.gain);
	}
	if (result.sleepsOn)
	{
		action.turnsUntilStateChange = static_cast<uint16_t>(info.restAtHomeTime);
	}
	return result.sleepsOn;
}

/// The villager's own needs at home: past nine tenths of the more pressing of rest at the life it sleeps on to, and
/// food at the hungry mark
bool CheckNeedsAtHome(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// A woman's own business comes first: her time may come, and while she is pregnant she stays in
	if (WorldRegistry().Get<const Villager>(villager).sex == Villager::Sex::FEMALE && !IsChild(villager) &&
	    (ecs::villager_breeding::WomanSpecial(action) == 1 || ecs::villager_breeding::IsPregnant(villager)))
	{
		return true;
	}
	const auto& info = InfoOf(villager);
	// A disciple at work weighs its needs only at the life it goes home hurt and the food it starves at
	const auto* role = WorldRegistry().TryGet<const Disciple>(villager);
	const bool working = role != nullptr && openblack::disciple::KindOf(role->type).waitsWhenIdle;
	const float restLife = working ? info.damageThresholdToGoHome : info.damageThresholdToSleepUntil;
	const float hungerMark = working ? info.starvingForFood : info.hungryForFood;
	const float pressing =
	    std::max(needs::LifeDesireFromLife(restLife, info.damageThresholdToGoHome), needs::DesireForFood(hungerMark));
	if (CheckSatisfyOwnDesire(action, k_AtHomeNeedShare * pressing))
	{
		return true;
	}
	// A child has its own things to do
	if (IsChild(villager))
	{
		ecs::villager_breeding::ChildDecideWhatToDo(action);
		return true;
	}
	return false;
}

/// With nothing to do at home it goes to bed one time in four, otherwise as anywhere
uint32_t HomeNothingToDo(LivingAction& action)
{
	if (IsAtHome(EntityOf(action)) && Locator::gameRandom::value().GameRand(routine::k_GoToBedChance) == 0)
	{
		action.turnsUntilStateChange = 0;
		SetTopState(action, VillagerStates::GotoBedAtHome);
		return 1;
	}
	return villager_home::SetupNothingToDo(action);
}

/// What blocks a spot to lie down by a tree: a sleeper lying down, or something fixed, close to the tree past its size.
/// The tree counts itself.
bool BlocksTreeTent(entt::entity thing, glm::vec2 tree)
{
	auto& registry = WorldRegistry();
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (transform == nullptr ||
	    glm::distance(tree, glm::xz(transform->position)) - world_objects::SizeOf(thing).radius >= routine::k_TentTreeCrowd)
	{
		return false;
	}
	if (const auto* action = registry.TryGet<const LivingAction>(thing); action != nullptr && registry.AllOf<Villager>(thing))
	{
		return State(*action, LivingAction::Index::Top) == VillagerStates::SleepInTent;
	}
	return registry.AllOf<Fixed>(thing);
}

/// A spot to lie down by a tree, on its far side from what is near it; none when two things are near it
std::optional<glm::vec2> TreeTentPos(entt::entity tree)
{
	const auto& map = Locator::entitiesMap::value();
	const auto at = PositionOf(tree);
	std::optional<glm::vec2> blocker;
	auto cell = map_coords::CellOf(at);
	map_coords::Spiral spiral;
	for (int32_t count = 9; count != 0; --count)
	{
		for (const auto thing : map.GetAllInCell(cell))
		{
			if (!BlocksTreeTent(thing, at))
			{
				continue;
			}
			if (blocker.has_value())
			{
				return std::nullopt;
			}
			blocker = PositionOf(thing);
		}
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
	}
	// TODO(villagers): with nothing near, it lies on the side towards the villager; the tree always counts itself, so
	// the game never meets that case
	const float angle = gutils::Get3DAngleFromXZ(blocker.value_or(at), at);
	return routine::Polar(at, angle, routine::k_TentTreeGap);
}

} // namespace

std::optional<glm::vec2> villager_home::PosOutsideAbode(entt::entity villager)
{
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return std::nullopt;
	}
	// Out from its door, about an eighth of a half turn either side, a step or two from it
	return routine::PosOutsideDoor(PositionOf(abode), ArrivePosition(abode), FloatRandom());
}

bool villager_home::Collides(glm::vec2 point)
{
	if (!map_coords::InBounds(map_coords::CellOf(point)))
	{
		return true;
	}
	// TODO(villagers): the game reads the cell's own collision of water, the edge and fixed things; openblack tests
	// the sea and the fixed things' footprints
	if (Locator::terrainSystem::has_value() && Locator::terrainSystem::value().GetHeightAt(point) <= 0.0f)
	{
		return true;
	}
	auto& registry = WorldRegistry();
	for (const auto thing : Locator::entitiesMap::value().GetFixedInGridCell(glm::vec3(point.x, 0.0f, point.y)))
	{
		const auto* fixed = registry.TryGet<const Fixed>(thing);
		if (fixed != nullptr && glm::distance(fixed->boundingCenter, point) < fixed->boundingRadius)
		{
			return true;
		}
	}
	return false;
}

namespace
{

/// Whether a villager lies down in the open within reach of a spiral point
bool SleeperNear(glm::ivec2 cell, glm::vec2 point)
{
	auto& registry = WorldRegistry();
	for (const auto thing : Locator::entitiesMap::value().GetAllInCell(cell))
	{
		const auto* action = registry.TryGet<const LivingAction>(thing);
		if (action != nullptr && registry.AllOf<Villager>(thing) &&
		    glm::distance(point, PositionOf(thing)) < routine::k_TentSleeperGap &&
		    State(*action, LivingAction::Index::Top) == VillagerStates::SleepInTent)
		{
			return true;
		}
	}
	return false;
}

/// A spot for the villager to lie down for the night without a home: by a tree near it, or in the open about a point
std::optional<glm::vec2> GetTentPos(entt::entity villager, glm::vec2 spot)
{
	if (!Locator::entitiesMap::has_value())
	{
		return std::nullopt;
	}
	auto& registry = WorldRegistry();
	const auto& map = Locator::entitiesMap::value();
	const auto tree = ecs::nearest_search::FindNearest(
	    map_coords::FromMetres(PositionOf(villager)), routine::k_TentTreeSearch, [&](glm::ivec2 cell) {
		    std::vector<ecs::nearest_search::Candidate> found;
		    for (const auto thing : map.GetAllInCell(cell))
		    {
			    if (registry.AllOf<Tree, Transform>(thing))
			    {
				    found.push_back({.entity = thing, .at = map_coords::FromMetres(PositionOf(thing))});
			    }
		    }
		    return found;
	    });
	if (tree != entt::null)
	{
		if (const auto byTree = TreeTentPos(tree); byTree.has_value())
		{
			return byTree;
		}
	}
	for (uint32_t tries = 0; tries < routine::k_TentTries; ++tries)
	{
		if (!villager_home::Collides(spot))
		{
			// The spot is taken if a sleeper lies near any point of a spiral walked from it; it ends where the walk does
			bool free = true;
			auto coords = map_coords::FromMetres(spot);
			map_coords::Spiral spiral;
			for (int32_t count = 9; count != 0; --count)
			{
				if (free && SleeperNear(map_coords::Cell(coords), map_coords::ToMetres(coords)))
				{
					free = false;
				}
				map_coords::AddCells(coords, spiral.Next());
			}
			if (free)
			{
				return map_coords::ToMetres(coords);
			}
		}
		spot = routine::TentRetry(spot, FloatRandom());
	}
	return std::nullopt;
}

/// Gets the villager to where it sits about: down where it is when there is room, else to room near it, else to the
/// spot its kind of sitting about finds
template <typename SpotFinder>
void GetMeToMyChillOutPos(LivingAction& action, SpotFinder findSpot, glm::vec2 centre, float radius,
                          std::optional<glm::vec2> lookAt)
{
	const auto villager = EntityOf(action);
	const auto position = PositionOf(villager);
	const float distance = glm::distance(centre, position);
	auto& towns = Locator::townSystem::value();
	if (distance <= radius)
	{
		auto spot = position;
		const float room = k_SitDownRoom * world_objects::SizeOf(villager).radius;
		if (towns.CheckForClearArea(spot, room, ClearAreaFilter::AnyObject, villager))
		{
			if (lookAt.has_value())
			{
				LookAtPos(villager, *lookAt, k_SitDownTurn);
			}
			SetTopState(action, VillagerStates::SitAndChillout);
			return;
		}
		if (radius + k_SitDownTowards < distance)
		{
			spot = routine::Polar(spot, gutils::Get3DAngleFromXZ(spot, centre), k_SitDownTowards);
		}
		if (const auto clear =
		        towns.FindClearArea(spot, k_SitDownSearch, k_SitDownStep, room, ClearAreaFilter::AnyObject, villager);
		    clear.has_value())
		{
			villager_home::SetupMoveTo(action, *clear, FinalState(action));
			return;
		}
	}
	if (const auto spot = findSpot(); spot.has_value())
	{
		villager_home::SetupMoveTo(action, *spot, FinalState(action));
	}
}

/// Where a villager sits about its town, near the gathering place on its own side
std::optional<glm::vec2> GetChillOutPos(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	return routine::ChillOutPos(Locator::townSystem::value().GetCongregationPos(town), PositionOf(villager),
	                            Locator::infoConstants::value().town.maxDistanceFromCongreationPosThatPeopleChillOut,
	                            FloatRandom());
}

/// Where a villager sits outside its house, about its door
std::optional<glm::vec2> GetPosOutsideMyHouse(entt::entity villager)
{
	const auto abode = AbodeOf(villager);
	if (abode == entt::null || TownOf(villager) == entt::null)
	{
		return std::nullopt;
	}
	const float half = k_OutsideHouseShare * Locator::infoConstants::value().town.maxDistanceFromHouseThatPeopleChillOut;
	return routine::PosOutside(PositionOf(abode), villager_home::ArrivePosition(abode), k_OutsideHouseDivisions, half, half,
	                           FloatRandom());
}

/// The town a vagrant can join: the nearest within reach, of its own tribe
entt::entity NearbyTownToJoin(entt::entity villager)
{
	auto& registry = WorldRegistry();
	const auto& transform = registry.Get<const Transform>(villager);
	const auto town = Locator::townSystem::value().FindClosestTown(transform.position);
	if (town == entt::null || !registry.AllOf<Town, Transform>(town) ||
	    glm::distance(PositionOf(town), glm::xz(transform.position)) > routine::k_VagrantTownSearch)
	{
		return entt::null;
	}
	// TODO(villagers): the town's own tribe; openblack's towns don't keep one yet, so its people's is used
	const auto& person = registry.Get<const Villager>(villager);
	const auto& data = registry.Get<const Town>(town);
	for (const auto abode : data.abodes)
	{
		if (const auto* building = registry.TryGet<const Abode>(abode); building != nullptr)
		{
			for (const auto other : building->inhabitants)
			{
				if (const auto* resident = registry.TryGet<const Villager>(other); resident != nullptr)
				{
					return resident->tribe == person.tribe ? town : entt::null;
				}
			}
		}
	}
	return entt::null;
}
} // namespace

namespace
{
/// The state a villager walks in: its kind's own walking state
VillagerStates WalkingStateOf(entt::entity villager)
{
	const auto* person = WorldRegistry().TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value())
	{
		return VillagerStates::MoveToPos;
	}
	// TODO(villagers): on a structure it walks in the state for that; openblack has no villagers on structures yet
	return static_cast<VillagerStates>(InfoOf(*person).moveState);
}
} // namespace

bool villager_home::SetupMoveTo(LivingAction& action, glm::vec2 goal, VillagerStates final, WalkWay way)
{
	// It goes into the walk and the state it walks to at once, so the states it leaves hear where it is heading, and it
	// never stops for a second before setting off. The walk itself is laid out only once it is walking.
	if (!Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, WalkingStateOf(EntityOf(action)),
	                                                                                final))
	{
		return false;
	}
	SetupMobileMoveTo(action, goal, way);
	return true;
}

void villager_home::SetupMobileMoveTo(LivingAction& action, glm::vec2 goal, WalkWay way)
{
	auto& registry = WorldRegistry();
	const auto villager = EntityOf(action);
	auto& wallHug = registry.Get<WallHug>(villager);
	wallHug.goal = goal;
	// A fresh step is worked out on the next pathfinding turn
	wallHug.step = {0, 0};
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	// A new walk may be tried both ways round again; one walking straight through no longer waits for its trial, unless
	// it is there already
	if (const auto* lookahead = registry.TryGet<const WallHugLookahead>(villager);
	    lookahead != nullptr && (lookahead->tried || (way == WalkWay::StraightThrough && !AreWeThere(villager, goal))))
	{
		registry.Remove<WallHugLookahead>(villager);
	}
	if (way == WalkWay::StraightThrough)
	{
		// Aimed at the goal on its first step, then again only now and then
		wallHug.turnsUntilStepRebuild = 1;
		registry.Assign<MoveStateStepThroughTag>(villager);
	}
	else
	{
		registry.Assign<MoveStateLinearTag>(villager);
	}
}

bool villager_home::WalkIsOverAt(entt::entity villager, glm::vec2 goal)
{
	return walk_arrival::WalkIsOver(WalkStateOf(villager), WalkPositionOf(villager), wall_hug::ToWhole(goal));
}

glm::ivec2 villager_home::WalkPosition(entt::entity villager)
{
	return WalkPositionOf(villager);
}

void villager_home::ArriveHome(entt::entity villager)
{
	auto& registry = WorldRegistry();
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return;
	}
	// As in the game, arriving twice counts twice
	registry.AssignOrReplace<components::AtHome>(villager);
	++registry.Get<Abode>(abode).presentAtHome;
	registry.SetDirty();
}

void villager_home::LeaveHome(entt::entity villager)
{
	auto& registry = WorldRegistry();
	if (!IsAtHome(villager))
	{
		return;
	}
	registry.Remove<components::AtHome>(villager);
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		auto& present = registry.Get<Abode>(abode).presentAtHome;
		present = present > 0 ? present - 1 : 0;
	}
	registry.SetDirty();
}

bool villager_home::IsFunctional(entt::entity abode)
{
	return IsBuilt(abode) && world_objects::LifeOf(abode) > AbodeInfoOf(abode).thresholdForStopBeingFunctional;
}

glm::vec2 villager_home::ArrivePosition(entt::entity abode)
{
	auto& registry = WorldRegistry();
	const auto& transform = registry.Get<const Transform>(abode);
	glm::vec3 point = transform.position;
	if (const auto* mesh = registry.TryGet<const Mesh>(abode); mesh != nullptr)
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(mesh->id))
		{
			if (const auto& door = meshes.Handle(mesh->id)->GetDoorPos(); door.has_value())
			{
				point = transform.position + transform.rotation * (transform.scale * *door);
			}
		}
	}
	return glm::xz(point);
}

void villager_home::LeavingHome(entt::entity villager)
{
	auto& registry = WorldRegistry();
	if (auto* action = registry.TryGet<LivingAction>(villager); action != nullptr && IsAtHome(villager))
	{
		SetTopState(*action, VillagerStates::DecideWhatToDo);
	}
}

void villager_home::HomeDeleted(entt::entity villager)
{
	auto& registry = WorldRegistry();
	if (!registry.Valid(villager) || !registry.AllOf<Villager>(villager) || AbodeOf(villager) == entt::null)
	{
		// TODO(villagers): one with no home leaves a town that goes, as its homeless do
		return;
	}
	Locator::townSystem::value().MakeHomeless(villager);
	if (auto* action = registry.TryGet<LivingAction>(villager); action != nullptr)
	{
		SetTopState(*action, VillagerStates::HomelessStart);
	}
}

bool villager_home::SetStateWhenTappedOnAbode(entt::entity villager)
{
	auto& registry = WorldRegistry();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr || !IsAvailable(villager) || !IsAtHome(villager))
	{
		return false;
	}
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return false;
	}
	const auto spot = routine::PosOutsideDoor(PositionOf(abode), ArrivePosition(abode), FloatRandom());
	// It decides afresh once it has yawned outside, and its needs and bed leave it be for that decision
	Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Previous,
	                                                      VillagerStates::DecideWhatToDo, true);
	SetupMoveTo(*action, spot, VillagerStates::AfterTapOnAbode);
	registry.Get<Villager>(villager).woken = true;
	return true;
}

uint32_t villager_home::CheckSatisfySleep(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& person = WorldRegistry().Get<const Villager>(villager);
	if (!routine::MaySleep(person.woken, person.life, InfoOf(person).damageThresholdToGoHome))
	{
		return 0;
	}
	if (IsAtHome(villager))
	{
		if (CheckWhenGoingToBed(villager))
		{
			SetTopState(action, VillagerStates::GotoBedAtHome);
		}
		return 1;
	}
	if (AbodeOf(villager) != entt::null)
	{
		SetTopState(action, VillagerStates::GoHome);
		return 1;
	}
	return State(action, LivingAction::Index::Top) == VillagerStates::SleepInTent ? 1 : 0;
}

uint32_t villager_home::OfferToTownDesires(LivingAction& action)
{
	return CheckNeededForTownDesire(action);
}

uint32_t villager_home::CheckNeededForSomething(LivingAction& action)
{
	if (AbodeOf(EntityOf(action)) == entt::null && CheckHomelessMoveIntoAbode(action))
	{
		return 1;
	}
	return CheckNeededForSpecial(action);
}

uint32_t villager_home::CheckNeededForSpecial(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// Worship first, then the town's desires, then its own needs
	if (villager_worship::CheckNeededForWorship(action) == 1)
	{
		return 1;
	}
	if (TownOf(villager) != entt::null && CheckNeededForTownDesire(action) == 1)
	{
		return 1;
	}
	return CheckSatisfyOwnDesire(action, InfoOf(villager).ownDesireThreshold) ? 1 : 0;
}

uint32_t villager_home::SetupNothingToDo(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	const auto abode = AbodeOf(villager);
	switch (
	    routine::NothingToDo(abode != entt::null, abode != entt::null && IsFunctional(abode), town != entt::null, IntRandom()))
	{
	case routine::Idle::GoHome:
		break;
	case routine::Idle::ChillOutsideHome:
		SetTopState(action, VillagerStates::GoAndChilloutOutsideHome);
		return 1;
	case routine::Idle::SitInTown:
		if (const auto spot = GetChillOutPos(villager); spot.has_value())
		{
			SetupMoveTo(action, *spot, VillagerStates::SitAndChillout);
			return 1;
		}
		break;
	}
	SetTopState(action, VillagerStates::GoHome);
	return 1;
}

uint32_t villager_home::HomeDecideWhatToDo(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (AbodeOf(villager) != entt::null && InEmergency(TownOf(villager)))
	{
		SetTopState(action, VillagerStates::GotoBedAtHome);
		return 1;
	}
	if (CheckNeedsAtHome(action))
	{
		return 1;
	}
	// A working disciple decides as a disciple; a breeder sleeps first when its town wants sleep most
	if (const auto* role = WorldRegistry().TryGet<const Disciple>(villager);
	    role != nullptr && openblack::disciple::KindOf(role->type).waitsWhenIdle)
	{
		if (role->type == VillagerDisciple::Breeder && TownOf(villager) != entt::null &&
		    Locator::townDesireSystem::value().GetMostWanted(TownOf(villager)) == TownDesireInfo::ForSleep &&
		    CheckSatisfySleep(action) != 0)
		{
			return 1;
		}
		return DecideWhatToDo(action);
	}
	if (CheckNeededForSomething(action) == 1)
	{
		return 1;
	}
	HomeNothingToDo(action);
	return 0;
}

uint32_t villager_home::DecideWhatToDo(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (InEmergency(TownOf(villager)))
	{
		SetTopState(action, VillagerStates::GotoCongregateInTownAfterEmergency);
		return 1;
	}
	// A disciple sets about its work first, or waits by the town centre for some
	if (villager_disciple::DecideAsDisciple(action) == 1)
	{
		return 1;
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	// A child has its own things to do
	if (IsChild(villager))
	{
		return ecs::villager_breeding::ChildDecideWhatToDo(action);
	}
	if (CheckNeededForSomething(action) == 1)
	{
		return 1;
	}
	// One carrying enough wood or food to show takes it to the storage pit
	if (villager_build::CheckTakeResourcesToStoragePit(action) == 1)
	{
		return 1;
	}
	return SetupNothingToDo(action);
}

uint32_t villager_home::MoveToPos(LivingAction& action)
{
	auto& registry = WorldRegistry();
	const auto villager = EntityOf(action);
	// The walk goes on until its last step has put the villager on its goal
	const auto goal = wall_hug::ToWhole(registry.Get<const WallHug>(villager).goal);
	if (!walk_arrival::WalkIsOver(WalkStateOf(villager), WalkPositionOf(villager), goal))
	{
		return 0;
	}
	registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	// It goes on into the state it was heading for, which stays what it works towards, so that state's work carries
	// on rather than starting afresh
	auto final = State(action, LivingAction::Index::Final);
	if (final == VillagerStates::InvalidState)
	{
		final = VillagerStates::DecideWhatToDo;
	}
	SetTopState(action, final);
	return 0;
}

uint32_t villager_home::GoHome(LivingAction& action)
{
	return DoGoingHome(action, VillagerStates::ArrivesHome, VillagerStates::SleepInTent);
}

uint32_t villager_home::DoGoingHome(LivingAction& action, VillagerStates arrive, VillagerStates sleepInTent)
{
	const auto villager = EntityOf(action);
	// A dancer leaves its dance first
	ecs::dances::RemoveDancer(WorldRegistry(), villager);
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		if (IsAtHome(villager))
		{
			SetTopState(action, VillagerStates::AtHome);
		}
		else if (FinalState(action) != arrive)
		{
			villager_footpath::SetupMoveToOnFootpath(action, abode, ArrivePosition(abode), arrive);
		}
		return 1;
	}
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		SetTopState(action, VillagerStates::VagrantStart);
		return 1;
	}
	// Homeless: towards its town from far off; near it, somewhere to lie down for the night, or a wander about
	const auto top = State(action, LivingAction::Index::Top);
	const auto position = PositionOf(villager);
	const auto townPosition = PositionOf(town);
	auto end = top;
	glm::vec2 goal;
	if (glm::distance(townPosition, position) > routine::k_FarFromTown)
	{
		goal = routine::TowardsTown(townPosition, position, FloatRandom());
	}
	else
	{
		goal = routine::RoundAbout(position, routine::k_TentSearchLeast, routine::k_TentSearchRange, FloatRandom());
		if (const auto tent = GetTentPos(villager, goal); tent.has_value())
		{
			goal = *tent;
			end = sleepInTent;
		}
		else
		{
			goal = routine::RoundAbout(goal, routine::k_HomelessRoamLeast, routine::k_HomelessRoamRange, FloatRandom());
		}
	}
	SetupMoveTo(action, goal, end);
	return 1;
}

uint32_t villager_home::ArrivesHome(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		SetTopState(action, VillagerStates::HomelessStart);
		return 0;
	}
	const auto door = ArrivePosition(abode);
	if (!AreWeThere(villager, door))
	{
		villager_footpath::SetupMoveToOnFootpath(action, abode, door, VillagerStates::ArrivesHome);
		return 1;
	}
	if (!IsBuilt(abode) || !IsRepaired(abode))
	{
		const auto& person = WorldRegistry().Get<const Villager>(villager);
		const auto& info = InfoOf(person);
		const bool functional = IsFunctional(abode);
		if (person.life < info.damageThresholdToGoHome && !functional)
		{
			// Hurt, with a home that doesn't work: it lies down outside, trying again each turn until it finds a spot
			const auto spot = routine::BrokenHomeTentSearch(PositionOf(abode), PositionOf(villager), FloatRandom());
			if (const auto tent = GetTentPos(villager, spot); tent.has_value())
			{
				SetupMoveTo(action, *tent, VillagerStates::SleepInTent);
			}
			return 1;
		}
		if (person.food < info.hungryForFood && !functional)
		{
			// As in the game, the decision is overtaken at once: it goes in all the same
			SetTopState(action, VillagerStates::DecideWhatToDo);
		}
		// TODO(villagers): otherwise it helps build or repair its home first (the builders, jobs phase)
	}
	ArriveHome(villager);
	SetTopState(action, VillagerStates::AtHome);
	return 1;
}

uint32_t villager_home::AtHome(LivingAction& action)
{
	HomeDecideWhatToDo(action);
	return 1;
}

uint32_t villager_home::GotoBedAtHome(LivingAction& action)
{
	SetTopState(action, VillagerStates::SleepingAtHome);
	action.turnsUntilStateChange = static_cast<uint16_t>(InfoOf(EntityOf(action)).restAtHomeTime);
	return 1;
}

uint32_t villager_home::SleepingAtHome(LivingAction& action)
{
	// Without a town it never wakes, as in the game
	if (TownOf(EntityOf(action)) == entt::null)
	{
		return 1;
	}
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange == 0 && !DoSleeping(action, 1.0f))
	{
		SetTopState(action, VillagerStates::AtHome);
	}
	return 1;
}

uint32_t villager_home::HomelessStart(LivingAction& action)
{
	if (!villager_food::CheckHungry(action) && CheckNeededForSomething(action) == 0 && !CheckHomelessMoveIntoAbode(action))
	{
		SetupNothingToDo(action);
	}
	return 1;
}

uint32_t villager_home::VagrantStart(LivingAction& action)
{
	auto& registry = WorldRegistry();
	const auto villager = EntityOf(action);
	if (const auto town = NearbyTownToJoin(villager); town != entt::null)
	{
		if (Locator::townSystem::value().AddVillagerToTown(town, villager))
		{
			SetTopState(action, VillagerStates::DecideWhatToDo);
			return 1;
		}
	}
	const auto position = PositionOf(villager);
	if (registry.Get<const Villager>(villager).life >= InfoOf(villager).damageThresholdToGoHome)
	{
		const auto goal = routine::VagrantWander(position, FacingOf(villager), FloatRandom());
		if (map_coords::InBounds(map_coords::CellOf(goal)))
		{
			SetupMoveTo(action, goal, VillagerStates::VagrantStart);
		}
		return 1;
	}
	if (const auto tent = GetTentPos(villager, routine::VagrantTentSearch(position, FloatRandom())); tent.has_value())
	{
		SetupMoveTo(action, *tent, VillagerStates::SleepInTent);
	}
	return 1;
}

uint32_t villager_home::SleepInTent(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (action.turnsUntilStateChange == 0)
	{
		if (DoSleeping(action, 1.0f))
		{
			return 1;
		}
		if (AbodeOf(villager) == entt::null && CheckHomelessMoveIntoAbode(action))
		{
			return 1;
		}
		if (HomeDecideWhatToDo(action) == 1 && State(action, LivingAction::Index::Top) != VillagerStates::SleepInTent)
		{
			return 1;
		}
		action.turnsUntilStateChange = static_cast<uint16_t>(InfoOf(villager).restAtHomeTime);
	}
	--action.turnsUntilStateChange;
	return 1;
}

uint32_t villager_home::AfterTapOnAbode(LivingAction& action)
{
	Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, State(action, LivingAction::Index::Previous));
	return 1;
}

uint32_t villager_home::NothingToDo(LivingAction& /*action*/)
{
	return 1;
}

uint32_t villager_home::GoAndChilloutOutsideHome(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto abode = AbodeOf(villager);
	if (abode == entt::null || TownOf(villager) == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto door = ArrivePosition(abode);
	const float distance = Locator::infoConstants::value().town.maxDistanceFromHouseThatPeopleChillOut;
	const auto lookOut =
	    routine::Polar(door, gutils::Get3DAngleFromXZ(PositionOf(abode), door), k_OutsideHouseLookOut * distance);
	GetMeToMyChillOutPos(action, [villager] { return GetPosOutsideMyHouse(villager); }, door, distance, lookOut);
	return 1;
}

uint32_t villager_home::GoAndChilloutInTown(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto centre = Locator::townSystem::value().GetCongregationPos(town);
	GetMeToMyChillOutPos(
	    action, [villager] { return GetChillOutPos(villager); }, centre,
	    Locator::infoConstants::value().town.maxDistanceFromCongreationPosThatPeopleChillOut, centre);
	return 1;
}

uint32_t villager_home::SitAndChillout(LivingAction& action)
{
	const auto turns = static_cast<int16_t>(action.turnsUntilStateChange);
	action.turnsUntilStateChange = static_cast<uint16_t>(turns - 1);
	if (turns > 0)
	{
		return 1;
	}
	action.turnsUntilStateChange = 0;
	const auto villager = EntityOf(action);
	if (InEmergency(TownOf(villager)))
	{
		SetTopState(action, VillagerStates::GotoCongregateInTownAfterEmergency);
		return 1;
	}
	if (CheckNeededForSomething(action) == 1)
	{
		return 1;
	}
	if (Locator::gameRandom::value().GameRand(routine::k_ChillOutRethinkChance) == 0)
	{
		SetupNothingToDo(action);
		return 1;
	}
	action.turnsUntilStateChange = InfoOf(villager).subsequentChillOutTime;
	return 1;
}

bool villager_home::EnterSitAndChillOut(LivingAction& action, VillagerStates /*previous*/, VillagerStates /*next*/)
{
	action.turnsUntilStateChange = InfoOf(EntityOf(action)).initialChillOutTime;
	return true;
}

uint32_t villager_home::GotoCongregateInTownAfterEmergency(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	auto centre = Locator::townSystem::value().GetCongregationPos(town);
	// The more people the town has, the wider they spread about the gathering place
	const auto& data = WorldRegistry().Get<const Town>(town);
	uint32_t people = static_cast<uint32_t>(data.homelessVillagers.size());
	for (const auto abode : data.abodes)
	{
		if (const auto* building = WorldRegistry().TryGet<const Abode>(abode); building != nullptr)
		{
			people += static_cast<uint32_t>(building->inhabitants.size());
		}
	}
	// TODO(villagers): the game counts its adults and children as its town keeps them
	const float share = std::min(static_cast<float>(people) * k_CongregatePerVillager, k_CongregateMost);
	const float distance = share * k_CongregateSpread + k_CongregateLeast;
	const float angle = Locator::gameRandom::value().GameFloatRand(routine::k_TwoPi);
	SetupMoveTo(action, routine::Polar(centre, angle, distance), VillagerStates::CongregateInTownAfterEmergency);
	return 1;
}

uint32_t villager_home::CongregateInTownAfterEmergency(LivingAction& action)
{
	auto& living = Locator::livingActionSystem::value();
	auto& random = Locator::gameRandom::value();
	if (InEmergency(TownOf(EntityOf(action))))
	{
		living.VillagerPlayAnimThenSetState(action, random.GameRand(k_EmergencyGoHomeChance) != 0
		                                                ? VillagerStates::GotoCongregateInTownAfterEmergency
		                                                : VillagerStates::GoHome);
		return 1;
	}
	living.VillagerPlayAnimThenSetState(action, random.GameRand(k_AfterEmergencyDecideChance) != 0
	                                                ? VillagerStates::GotoCongregateInTownAfterEmergency
	                                                : VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_home::ExitAtHome(LivingAction& action, VillagerStates next)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (table.at(static_cast<size_t>(next)).staysAtHomeOnExit == 0)
	{
		LeaveHome(EntityOf(action));
	}
	// Leaving is never held up
	return false;
}

uint32_t villager_home::LookForBetterHome(LivingAction& action)
{
	const auto villager = EntityOf(action);
	switch (Locator::townSystem::value().LookForBetterHome(villager))
	{
	case town_homes::NewHome::Stays:
		return 0;
	case town_homes::NewHome::Moved:
		if (IsAvailable(villager))
		{
			SetTopState(action, VillagerStates::GoHome);
		}
		return 1;
	case town_homes::NewHome::MadeHomeless:
		SetTopState(action, VillagerStates::HomelessStart);
		return 1;
	case town_homes::NewHome::AlreadyHomeless:
		return 1;
	case town_homes::NewHome::NoTown:
		VagrantStart(action);
		return 1;
	}
	return 0;
}

void villager_home::TakeOnTribe(entt::entity villager, Tribe tribe, [[maybe_unused]] bool showEffect)
{
	auto& registry = WorldRegistry();
	auto& person = registry.Get<Villager>(villager);
	if (InfoOf(person).keepMeshWhenChangeTown != 0)
	{
		return;
	}
	const auto& infos = Locator::infoConstants::value().villager;
	const auto row = static_cast<size_t>(GVillagerInfo::Find(tribe, person.number));
	if (row >= infos.size())
	{
		return;
	}
	// It becomes the same kind of villager of the new tribe, wearing that tribe's models: a child its child model at
	// every distance, an adult its kind's model for each distance
	// TODO(villager-life-cycle): a white puff of smoke goes up as it changes when `showEffect` (the game's smoke puff isn't
	// in openblack yet; the dying villager's skeleton swap needs the same)
	const auto& info = infos.at(row);
	person.tribe = tribe;
	const bool child = person.lifeStage == Villager::LifeStage::Child;
	const auto high = resources::HashIdentifier(child ? info.childMeshHigh : info.highDetail);
	if (auto* mesh = registry.TryGet<Mesh>(villager); mesh != nullptr)
	{
		mesh->id = high;
	}
	if (auto* detail = registry.TryGet<DetailMeshes>(villager); detail != nullptr)
	{
		detail->meshes =
		    child ? std::array {high, high, high}
		          : std::array {high, resources::HashIdentifier(info.stdDetail), resources::HashIdentifier(info.lowDetail)};
	}
	registry.SetDirty();
}

uint32_t villager_home::GoHomeAndChange(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto next = VillagerStates::DecideWhatToDo;
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		const auto door = ArrivePosition(abode);
		if (!AreWeThere(villager, door))
		{
			// TODO(villagers): hugging round what stands in the way, as the game's walk home does
			SetupMoveTo(action, door, VillagerStates::GoHomeAndChange);
			return 1;
		}
		next = IsAtHome(villager) ? VillagerStates::AtHome : VillagerStates::ArrivesHome;
	}
	SetTopState(action, next);
	// One still small arriving is sized for its age
	if (WorldRegistry().Get<const Transform>(villager).scale.x < villager_age::k_FullGrownScale)
	{
		villager_checks::GrowForAge(villager);
	}
	return 1;
}

bool villager_home::ExitGoHomeAndChange(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto& nextInfo = table.at(static_cast<size_t>(next));
	// Going on to a state of its own (not a walk, nor this state again) it takes on its town's tribe, in a puff of smoke
	// unless it goes indoors
	if (next != VillagerStates::GoHomeAndChange && nextInfo.isFinalState != 0)
	{
		const auto town = TownOf(villager);
		const auto* tribe = town != entt::null ? WorldRegistry().TryGet<const Tribe>(town) : nullptr;
		TakeOnTribe(villager, tribe != nullptr ? *tribe : WorldRegistry().Get<const Villager>(villager).tribe,
		            nextInfo.staysAtHomeOnExit == 0);
	}
	if (villager_disciple::TypeOf(villager) == VillagerDisciple::ChangeHouse)
	{
		villager_disciple::SetType(villager, VillagerDisciple::None);
	}
	return false;
}
