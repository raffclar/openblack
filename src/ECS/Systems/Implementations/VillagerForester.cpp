/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerForester.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerHeldObject.h"
#include "ECS/VillagerWorkRules.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerBuild.h"
#include "VillagerFootpath.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace villager_forester = openblack::ecs::villager_forester;
namespace work = openblack::ecs::villager_work;

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

Villager& VillagerOf(entt::entity villager)
{
	return Entities().Get<Villager>(villager);
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

float RadiusOf(entt::entity entity)
{
	return systems::object_measures::TwoDRadius(Entities(), entity);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

VillagerStates FinalState(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetFinalState(action);
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

/// Walks the villager to a point round what is in the way, then on into a state, as long as the states let it
bool MoveTo(LivingAction& action, glm::vec2 goal, VillagerStates final)
{
	if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, final))
	{
		villager_home::SetupMobileMoveTo(action, goal);
		return true;
	}
	return false;
}

/// The same along the town's paths
bool MoveOnPathsTo(LivingAction& action, entt::entity place, glm::vec2 goal, VillagerStates final)
{
	return villager_footpath::SetupMoveToOnFootpath(action, place, goal, final);
}

/// How far the villager walks in a turn, in metres, and in the map's whole units
float SpeedInMetres(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	return gutils::ConvertWholeDistanceToMeters(wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f));
}

float WholeSpeed(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	return static_cast<float>(wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f));
}

/// Whether the villager has got to a point, as near as its step and some slack in whole units
bool AreWeThere(entt::entity villager, glm::vec2 point, float slack)
{
	return work::AreWeThere(map_coords::FromMetres(GroundOf(villager)), map_coords::FromMetres(point), WholeSpeed(villager),
	                        slack);
}

int32_t WoodRoom(const Villager& person)
{
	const auto& info = InfoOf(person);
	// The room left is kept in 16 bits
	return static_cast<int16_t>(villager_carry::Room(person, ResourceType::Wood, static_cast<int32_t>(info.maxFoodCarried),
	                                                 static_cast<int32_t>(info.maxWoodCarried)));
}

/// It picks wood up, showing as the log of what it came from; its town counts what it carries
void PickUpWood(entt::entity villager, int16_t amount, uint8_t woodGraphic)
{
	auto& person = VillagerOf(villager);
	const auto picked = villager_carry::PickUp(person, ResourceType::Wood, amount, woodGraphic);
	if (auto* tally = Entities().TryGet<TownResourceTally>(TownOf(person)))
	{
		tally->woodCarried += static_cast<float>(picked);
	}
}

/// Which log wood from a thing shows as: a tree's kind's, a big forest's own, a fallen trunk's by its model; plain
/// wood for anything else
uint8_t CarriedTreeTypeOf(entt::entity thing)
{
	auto& registry = Entities();
	const auto& info = Locator::infoConstants::value();
	if (const auto* tree = registry.TryGet<const Tree>(thing))
	{
		return static_cast<uint8_t>(info.tree.at(static_cast<size_t>(tree->type)).carriedType);
	}
	if (const auto* big = registry.TryGet<const BigForest>(thing))
	{
		return static_cast<uint8_t>(info.bigForest.at(static_cast<size_t>(big->type)).carriedType);
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(thing))
	{
		const auto& tree = info.tree.at(static_cast<size_t>(dead->type));
		return static_cast<uint8_t>(villager_held::WoodKindOfFallenTree(dead->model.value_or(tree.normal), tree.carriedType));
	}
	return 0;
}

/// Whether a thing is in the player's hand
bool InTheHand(entt::entity thing)
{
	return Entities().AllOf<InHand>(thing);
}

bool OnFire(entt::entity thing)
{
	return Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(thing);
}

/// Wood that can still be gone for: there, not in the hand and not burning
bool WoodStillThere(entt::entity wood)
{
	return wood != entt::null && Entities().Valid(wood) && Entities().AllOf<Transform>(wood) && !InTheHand(wood) &&
	       !OnFire(wood);
}

/// Where the villager stands to work at a thing: by a tree its own size and a little more from the trunk, by anything
/// else both their sizes from its middle
glm::vec2 WorkingPosition(entt::entity thing, entt::entity villager)
{
	if (Entities().AllOf<Tree>(thing))
	{
		return work::TreeWorkingPosition(GroundOf(thing), GroundOf(villager), RadiusOf(villager));
	}
	return work::EdgeWorkingPosition(GroundOf(thing), GroundOf(villager), RadiusOf(thing), RadiusOf(villager));
}

/// What finding a tree near a forester gave
struct TreeFound
{
	entt::entity tree {entt::null};
	bool touching {false};
};

/// The forest tree nearest the forester of the first forest tree in each of the nine map cells round it, passing over
/// trees nothing can destroy
TreeFound FindTreeNearVillager(entt::entity villager)
{
	TreeFound found;
	if (!Locator::entitiesMap::has_value())
	{
		return found;
	}
	auto& registry = Entities();
	const auto& map = Locator::entitiesMap::value();
	const auto here = GroundOf(villager);
	auto cell = map_coords::FromMetres(here);
	map_coords::Spiral spiral;
	float best = 99999.0f;
	for (int32_t i = 0; i < work::k_TreeSearchCells; ++i)
	{
		if (map_coords::InBounds(cell))
		{
			// Only the first tree of a forest the cell holds is looked at
			for (const auto thing : map.GetAllInCell({map_coords::CellX(cell), map_coords::CellZ(cell)}))
			{
				if (!registry.Valid(thing) || !registry.AllOf<Tree, ForestMember>(thing))
				{
					continue;
				}
				if (!registry.AllOf<Indestructible>(thing))
				{
					const float distance = gutils::GetDistanceInMetres(here, WorkingPosition(thing, villager));
					if (distance < best)
					{
						best = distance;
						found.tree = thing;
					}
				}
				break;
			}
		}
		map_coords::AddCells(cell, spiral.Next());
	}
	if (found.tree != entt::null)
	{
		const auto at = WorkingPosition(found.tree, villager);
		found.touching = work::Touching(gutils::GetDistanceInMetres(here, at), RadiusOf(villager), SpeedInMetres(villager));
	}
	return found;
}

/// The villager's working-disciple flag: a disciple whose kind always goes back to its own work
bool IsWorkingDisciple(entt::entity villager)
{
	const auto* role = Entities().TryGet<const Disciple>(villager);
	return role != nullptr && disciple::KindOf(role->type).waitsWhenIdle;
}

void ForgetTarget(entt::entity villager)
{
	Entities().Remove<WorkTarget>(villager);
}

entt::entity TargetOf(entt::entity villager)
{
	const auto* target = Entities().TryGet<const WorkTarget>(villager);
	return target != nullptr && Entities().Valid(target->thing) ? target->thing : entt::null;
}

/// The villager turns by some of the game's angles, to its right for a positive turn
void TurnBy(entt::entity villager, int32_t gameAngle)
{
	auto& transform = Entities().Get<Transform>(villager);
	const auto x = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
	const float facingRadians = -std::atan2(-x.z, x.x) - glm::radians(90.0f);
	const auto facing = static_cast<int32_t>(gutils::ConvertAngle3DToGame(facingRadians));
	const auto angle = (facing - gameAngle) & gutils::k_GameAngleMask;
	transform.rotation = glm::eulerAngleY(-gutils::ConvertGameAngleTo3D(angle) - glm::radians(90.0f));
	Entities().SetDirty();
}

/// Whether a pile is one a building site keeps its wood in
/// A pile made for a building site's wood, whether or not its building is finished
bool IsSitePile(entt::entity pile)
{
	return Entities().AllOf<SitePile>(pile);
}

/// Where the villager's town keeps its wood, and how much it has
struct Store
{
	std::optional<glm::vec2> position;
	uint32_t wood {0};
};
Store StoreOf(const Villager& person)
{
	const auto pit = construction::StoragePitOf(TownOf(person));
	if (pit.has_value() && villager_home::IsFunctional(*pit))
	{
		return {.position = villager_home::ArrivePosition(*pit),
		        .wood = Locator::resourceStoreSystem::value().GetResource(*pit, ResourceType::Wood)};
	}
	// TODO(villager-jobs-1): without a working storage pit a town keeps a temporary pot of wood by its meeting place;
	// openblack's towns have none yet, so such a town has no store to weigh
	return {};
}
} // namespace

villager_forester::WoodPlan villager_forester::DecideHowToGetWood(entt::entity villager, bool forBuilding)
{
	auto& registry = Entities();
	const auto& person = VillagerOf(villager);
	const auto& info = InfoOf(person);
	const float reach = static_cast<float>(Locator::infoConstants::value().town.maxDistanceForTownForest);
	const auto here = PositionOf(villager);
	const auto store = StoreOf(person);
	// The town's own forests first, else any forest with trees near enough
	std::optional<entt::entity> forest;
	if (Locator::forestSystem::has_value())
	{
		auto& forests = Locator::forestSystem::value();
		const auto town = TownOf(person);
		if (town != entt::null)
		{
			forest = forests.NearestTownForest(town, here);
		}
		if (!forest.has_value())
		{
			forest = forests.NearestForestWithTrees(here, reach);
		}
	}
	entt::entity bigForest = entt::null;
	std::optional<float> forestDistance;
	if (forest.has_value())
	{
		forestDistance = gutils::GetDistanceInMetres(GroundOf(*forest), glm::xz(here));
		const auto& record = registry.Get<const LandForest>(*forest);
		if (record.bigForest != entt::null && registry.Valid(record.bigForest))
		{
			bigForest = record.bigForest;
		}
	}
	const auto source = building_site::DecideWoodSource({
	    .forBuilding = forBuilding,
	    .distanceToStore = store.position.has_value() ? gutils::GetDistanceInMetres(*store.position, glm::xz(here))
	                                                  : std::numeric_limits<float>::max(),
	    .storeWood = store.wood,
	    .room = WoodRoom(person),
	    .maxWoodCarried = static_cast<int32_t>(info.maxWoodCarried),
	    .distanceToForest = forestDistance,
	    .forestHasBigForest = bigForest != entt::null,
	    .maxDistance = reach,
	});
	return {.source = source, .forest = forest.value_or(entt::null), .bigForest = bigForest};
}

uint32_t villager_forester::CheckSatisfyWoodDesire(LivingAction& action)
{
	const auto plan = DecideHowToGetWood(EntityOf(action), false);
	switch (plan.source)
	{
	case building_site::WoodSource::Store:
		return villager_build::GotoStoragePitForDropOff(action);
	case building_site::WoodSource::BigForest:
		return GotoBigForest(action, plan.bigForest, VillagerStates::ArrivesAtBigForest);
	case building_site::WoodSource::Forest:
		return GotoForest(action, plan.forest, VillagerStates::ForesterArrivesAtForest);
	case building_site::WoodSource::None:
		break;
	}
	return 0;
}

uint32_t villager_forester::GotoForest(LivingAction& action, entt::entity forest, VillagerStates final)
{
	const auto villager = EntityOf(action);
	glm::vec2 goal = GroundOf(forest);
	if (Locator::forestSystem::has_value())
	{
		if (const auto centre = Locator::forestSystem::value().CentreTreeOf(forest))
		{
			goal = WorkingPosition(*centre, villager);
		}
	}
	if (!MoveTo(action, goal, final))
	{
		return 0;
	}
	// It walks to the forest as a forester, axe in hand
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top,
	                                                      VillagerStates::ForesterMoveToForest, true);
	return 1;
}

uint32_t villager_forester::GotoBigForest(LivingAction& action, entt::entity bigForest, VillagerStates final)
{
	const auto villager = EntityOf(action);
	const auto at = work::BigForestArrivePosition(GroundOf(bigForest), GroundOf(villager), RadiusOf(bigForest));
	MoveOnPathsTo(action, bigForest, at, final);
	return 1;
}

uint32_t villager_forester::GotWoodDecideWhatToDo(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& person = VillagerOf(villager);
	const auto town = TownOf(person);
	const auto site = person.buildingSite;
	const bool siteWants = site != entt::null && town != entt::null && construction::IsSiteValid(town, site) &&
	                       (construction::BuildersNeeded(town, site) > 0 || villager_build::IsBuilderDisciple(person));
	switch (work::AfterGettingWood(person.woodHeld, IsWorkingDisciple(villager), siteWants))
	{
	case work::GotWood::Decide:
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	case work::GotWood::BuildingSite:
		if (villager_build::GotoBuildingSite(action, site) == 1)
		{
			return 1;
		}
		break;
	case work::GotWood::LookForSiteOrStore:
		break;
	}
	if (villager_build::CheckNeededForBuilding(action) == 1)
	{
		return 1;
	}
	SetTopState(action, VillagerStates::GotoStoragePitForDropOff);
	return 1;
}

uint32_t villager_forester::ForesterMoveToForest(LivingAction& action)
{
	// TODO(villager-jobs-1): on the way the game now and then looks ten metres ahead, straight on and to either side,
	// for a nearer forest tree to make for; when it does depends on a walking result whose meaning isn't known yet
	return villager_home::MoveToPos(action);
}

uint32_t villager_forester::ForesterGotoForest(LivingAction& action)
{
	return CheckSatisfyWoodDesire(action);
}

uint32_t villager_forester::ForesterArrivesAtForest(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto found = FindTreeNearVillager(villager);
	if (found.tree != entt::null && !found.touching)
	{
		return MoveTo(action, WorkingPosition(found.tree, villager), VillagerStates::ForesterArrivesAtForest) ? 1 : 0;
	}
	if (found.tree != entt::null)
	{
		// It turns a little to its right to swing at the trunk, and plays its chopping into the next state
		ForgetTarget(villager);
		TurnBy(villager, work::k_ChopTurn);
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::ForesterChopsTree);
		return 1;
	}
	// No tree near: a forest about here left with no trees goes
	if (Locator::forestSystem::has_value())
	{
		Locator::forestSystem::value().RemoveEmptyForestNear(PositionOf(villager), work::k_EmptyForestReach);
	}
	SetTopState(action, VillagerStates::ForesterFinishedForestering);
	return ForesterFinishedForestering(action);
}

uint32_t villager_forester::ForesterChopsTree(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	auto target = TargetOf(villager);
	if (target == entt::null)
	{
		const auto found = FindTreeNearVillager(villager);
		if (found.tree != entt::null && Locator::dynamicsSystem::has_value())
		{
			// The tree falls away from it at once, and calls the villagers about to its wood
			const auto felled = object_physics::FellTree(Locator::dynamicsSystem::value(), found.tree, villager);
			registry.AssignOrReplace<WorkTarget>(villager, WorkTarget {.thing = felled});
			world_objects::Remove(found.tree);
			registry.SetLayoutDirty();
			return 1;
		}
	}
	// It watches the trunk until it comes to rest
	if (target != entt::null && registry.AllOf<InPhysics>(target) && !InTheHand(target))
	{
		return 1;
	}
	ForgetTarget(villager);
	SetTopState(action, VillagerStates::ForesterFinishedForestering);
	return 1;
}

uint32_t villager_forester::ForesterChopsTreeForBuilding(LivingAction& action)
{
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_forester::ForesterFinishedForestering(LivingAction& action)
{
	if (VillagerOf(EntityOf(action)).woodHeld > 0)
	{
		return GotWoodDecideWhatToDo(action);
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_forester::ArrivesAtBigForest(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto& person = VillagerOf(villager);
	const auto room = WoodRoom(person);
	const auto town = TownOf(person);
	if (town != entt::null && room != 0 && Locator::forestSystem::has_value())
	{
		const auto forest = Locator::forestSystem::value().NearestTownForest(town, PositionOf(villager));
		const auto* record = forest.has_value() ? registry.TryGet<const LandForest>(*forest) : nullptr;
		if (record != nullptr && record->bigForest != entt::null && registry.Valid(record->bigForest))
		{
			const auto bigForest = record->bigForest;
			const auto at = work::BigForestArrivePosition(GroundOf(bigForest), GroundOf(villager), RadiusOf(bigForest));
			if (!AreWeThere(villager, at, 0.0f))
			{
				MoveTo(action, at, FinalState(action));
				return 1;
			}
			// The big forest gives what it can carry, and shrinks
			const auto treeType = CarriedTreeTypeOf(bigForest);
			const auto taken = Locator::resourceStoreSystem::value().RemoveResource(bigForest, ResourceType::Wood,
			                                                                        static_cast<uint32_t>(room));
			if (taken != 0)
			{
				PickUpWood(villager, static_cast<int16_t>(taken), treeType);
				GotWoodDecideWhatToDo(action);
			}
		}
	}
	// Whatever it chose to do with the wood, it decides afresh
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_forester::ArrivesAtBigForestForBuilding(LivingAction& /*action*/)
{
	return 1;
}

bool villager_forester::ExitForesting(LivingAction& action, VillagerStates next)
{
	// Going on to another of the forester's states it keeps its tree
	switch (next)
	{
	case VillagerStates::ForesterMoveToForest:
	case VillagerStates::ForesterGotoForest:
	case VillagerStates::ForesterArrivesAtForest:
	case VillagerStates::ForesterChopsTree:
	case VillagerStates::ForesterFinishedForestering:
		return false;
	default:
		break;
	}
	ForgetTarget(EntityOf(action));
	// Leaving is never held up
	return false;
}

void villager_forester::SetupReactToWood(entt::entity villager, entt::entity wood)
{
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	Entities().AssignOrReplace<WoodReactionTarget>(villager, WoodReactionTarget {.wood = wood, .startTurn = turn});
}

uint32_t villager_forester::GotoWoodReaction(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto* target = Entities().TryGet<const WoodReactionTarget>(villager);
	const auto wood = target != nullptr ? target->wood : entt::null;
	if (!WoodStillThere(wood))
	{
		return systems::villager_reactions::StopReacting(action);
	}
	MoveTo(action, WorkingPosition(wood, villager), VillagerStates::ArrivesAtWoodReaction);
	return 1;
}

uint32_t villager_forester::ArrivesAtWoodReaction(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& registry = Entities();
	const auto* target = registry.TryGet<const WoodReactionTarget>(villager);
	const auto wood = target != nullptr ? target->wood : entt::null;
	if (WoodStillThere(wood))
	{
		if (!registry.AllOf<InPhysics>(wood))
		{
			// At rest: it takes what it has room for, showing the log of what the wood came from
			const auto room = WoodRoom(VillagerOf(villager));
			auto& stores = Locator::resourceStoreSystem::value();
			if (stores.ResourceOf(wood).type == ResourceType::Wood && room > 0)
			{
				const auto treeType = CarriedTreeTypeOf(wood);
				const auto taken = stores.RemoveResource(wood, ResourceType::Wood, static_cast<uint32_t>(room));
				if (static_cast<int32_t>(taken) > 0)
				{
					PickUpWood(villager, static_cast<int16_t>(taken), treeType);
				}
			}
			return GotWoodDecideWhatToDo(action);
		}
		// Still moving: it waits beside it, only so long after it took it up
		const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
		const auto millisecondsPerTurn = static_cast<uint32_t>(systems::TimeSystemInterface::k_TurnDuration.count());
		if (turn - target->startTurn <= work::TurnsToWaitForMovingWood(millisecondsPerTurn))
		{
			const auto at = WorkingPosition(wood, villager);
			if (!AreWeThere(villager, at, work::k_NearMovingWood))
			{
				MoveTo(action, at, FinalState(action));
				return 1;
			}
			if (!villager_build::FaceTowards(villager, GroundOf(wood), 0x80))
			{
				return 1;
			}
			// It waits, then goes on with what it was doing; the wait is counted only once it is waiting
			if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(
			        action, VillagerStates::WaitForCounter, FinalState(action)))
			{
				action.turnsUntilStateChange = work::k_WaitForWoodToLand;
			}
			return 1;
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_forester::ReactToWoodPriority(entt::entity villager, entt::entity wood, uint32_t priority)
{
	auto& registry = Entities();
	if (wood == entt::null || !registry.Valid(wood) || !registry.AllOf<Transform>(wood) || InTheHand(wood))
	{
		return 0;
	}
	const auto& person = VillagerOf(villager);
	const auto& info = InfoOf(person);
	const auto town = TownOf(person);
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return 0;
	}
	const auto site = person.buildingSite;
	const bool siteHasEnough = site != entt::null && town != entt::null && construction::IsSiteValid(town, site) &&
	                           construction::WoodNeededToBuild(town, site) <= 0.0f;
	const auto* role = registry.TryGet<const Disciple>(villager);
	const float reach =
	    Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToWood)).maxReactionDistance;
	const auto& townInfo = Locator::infoConstants::value().townDesire.at(static_cast<size_t>(TownDesireInfo::ForWood));
	const bool free = systems::villager_reactions::Available(villager, Reaction::ReactToWood);
	const bool interested = work::IsInterestedInWood({
	    .available = true,
	    .room = WoodRoom(person),
	    .maxCarried = static_cast<int32_t>(info.maxWoodCarried),
	    .held = person.woodHeld,
	    .hasTown = town != entt::null,
	    .healthy = person.life > info.damageThresholdToGoHome,
	    .free = free,
	    .fallenTree = registry.AllOf<DeadTree>(wood),
	    .siteWood = registry.AllOf<Pot>(wood) && IsSitePile(wood),
	    .siteHasEnough = siteHasEnough,
	    .disciplePicksUpWood = role != nullptr && disciple::KindOf(role->type).goesForAnyWood,
	    .townWoodDesire = town != entt::null && Locator::townDesireSystem::has_value()
	                          ? Locator::townDesireSystem::value().GetRawDesire(town, TownDesireInfo::ForWood)
	                          : 0.0f,
	    .stateInterest = StateInfo(FinalState(*action)).woodInterest,
	    .distanceModifier = gutils::GetDistanceModifier(glm::distance(PositionOf(villager), PositionOf(wood)), reach),
	    .townTrigger = townInfo.desireTriggersVillagerAction,
	    // TODO(villager-jobs-1): a land's script may raise every villager's interest; no shipped land does
	    .interactDesire = 0.0f,
	});
	return interested ? priority : 0;
}

bool villager_forester::ReactsToWoodAgainAtOnce(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	return action != nullptr && Locator::livingActionSystem::has_value() && work::ReactsToWoodAgainAtOnce(FinalState(*action));
}
