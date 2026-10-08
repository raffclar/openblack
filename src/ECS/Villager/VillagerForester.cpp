/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerForester.h"

#include <array>
#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHugMoveState.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerBuildingSitesInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "Locator.h"

// The foresters (VillagerForester.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// The BUILDER disciple type
constexpr uint8_t k_DiscipleBuilder = 4;
/// In game angles (2048 a turn): ForesterArrivesAtForest's turn before chopping
constexpr uint16_t k_ChopTurn = 0x124;
/// The empty forest's reach
constexpr float k_EmptyForestReach = 50.0f;
/// ForesterMoveToForest's probe distance and first best
constexpr float k_ProbeMetres = 10.0f;
constexpr float k_ProbeBest = 10000.0f;
/// The probe's angle offsets: a quarter turn either side and straight ahead
constexpr std::array<uint16_t, 3> k_ProbeAngles = {0xFE00, 0, 0x200};
/// The wall-hug move byte that makes the forester look ahead for trees (components::WallHugMoveState; never set in the
/// game)
constexpr uint8_t k_LookAheadMoveByte = 2;
/// FIND_TYPE 6 (Tree)
constexpr auto k_TreeType = ObjectType::ForestTree;
/// AreWeThere's reach at the BigForest
constexpr float k_BigForestReach = 0.0f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// The villager's town: a valid town entity, or null
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the town field: stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

/// The villager's position as a world point (the forest finders')
glm::vec3 PositionOf(entt::entity villager)
{
	const auto* transform = Entities().TryGet<const Transform>(villager);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

glm::vec2 Xz(glm::vec3 p)
{
	return {p.x, p.z};
}

/// The wall-hug move byte of a villager: its components::WallHugMoveState, 0 without one (no writer of 2 for villagers
/// was found)
uint8_t WallHugByte5E(entt::entity villager)
{
	const auto* moveByte = Entities().TryGet<const WallHugMoveState>(villager);
	return moveByte != nullptr ? moveByte->value : 0;
}

/// The look-ahead of ForesterMoveToForest (never reached in the game: see WallHugByte5E)
void ForesterLookAhead(entt::entity villager)
{
	float best = k_ProbeBest;
	entt::entity bestTree = entt::null;
	const auto me = tq::PosOf(villager);
	const map_coords::MapCoords meCoords {me.x, me.y, 0.0f};
	for (const auto offset : k_ProbeAngles)
	{
		// probe = me + 10 m along the villager's angle + offset.
		// (approximate) the angle wraps with the 2048 mask (the original indexes past its table for a negative one)
		const auto angle = static_cast<uint16_t>((GetGameAngle(villager) + offset) & 0x7FF);
		const auto probe = meCoords + gutils::GetPosFromGameAngle(angle, k_ProbeMetres);
		const glm::ivec2 cell {map_coords::CellOf(probe.x), map_coords::CellOf(probe.z)};
		// Every tree object of the probe's cell (the chain) that belongs to a forest
		for (auto tree = map_cells::FindType(cell, k_TreeType); tree != entt::null;
		     tree = map_cells::FindType(cell, k_TreeType, tree))
		{
			const auto* t = Entities().TryGet<const Tree>(tree);
			if (t == nullptr || t->forestId == 0)
			{
				continue;
			}
			// d = GetDistanceInMetres(TreeWorkingPos(tree, me), probe) < best
			const auto working = object::TreeGetWorkingPos(tree, villager);
			const float d = gutils::GetDistanceInMetres(working, probe);
			if (d < best)
			{
				best = d;
				bestTree = tree;
			}
		}
	}
	// SetupMoveToWithHug(TreeWorkingPos(best, me), 49)
	if (bestTree != entt::null)
	{
		SetupMoveToWithHug(villager, Xz(TreeWorkingPos(bestTree, villager)), VillagerStates::ForesterArrivesAtForest);
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

uint32_t TreeSearchResult(bool found, bool touching)
{
	return !found ? 0u : touching ? 10u : 1u;
}

bool TouchingRule(float distanceFromObject, float speedInMetres)
{
	return speedInMetres > distanceFromObject;
}

// ---- the functions -----------------------------------------------------------------------------------------------

uint32_t FindTreeNearVillager(entt::entity villager, entt::entity& tree)
{
	// The 9 cells' nearest tree (ecs::FindTreeNearVillager)
	const auto found = ecs::FindTreeNearVillager(villager);
	if (found == entt::null)
	{
		return 0;
	}
	// *tree = it; IsTouching(its working point) ? 10 : 1
	tree = found;
	return TreeSearchResult(true, IsTouching(villager, TreeWorkingPos(found, villager)));
}

bool IsTouching(entt::entity villager, glm::vec3 pos)
{
	// GetDistanceFromObject(pos) (distance - radius) against the speed in metres
	return TouchingRule(object::GetDistanceFromObject(villager, pos), SpeedInMetres(villager));
}

uint32_t VillagerGotoForest(entt::entity villager, uint32_t forestId, VillagerStates state)
{
	// The forest's centre tree; with one its working point, else the forest's centre
	const auto centreTree = ForestCentreTree(forestId);
	const auto p = centreTree != entt::null ? TreeWorkingPos(centreTree, villager) : ForestCentre(forestId);
	// SetupMoveToWithHug(p, state)
	if (SetupMoveToWithHug(villager, Xz(p), state) == 0)
	{
		villager::TraceFormatted(villager, "forest: goto {} refused", forestId);
		return 0;
	}
	// The top state set to 47 raw (the top index also zeroes the turns since the change)
	if (auto* action = Entities().TryGet<LivingAction>(villager); action != nullptr)
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) =
		    static_cast<uint8_t>(VillagerStates::ForesterMoveToForest);
		action->turnsSinceStateChange = 0;
	}
	villager::TraceFormatted(villager, "forest: goto {} tree {} ({:.2f}, {:.2f}) -> 47", forestId,
	                         static_cast<uint32_t>(centreTree), p.x, p.z);
	return 1;
}

uint32_t GotWoodDecideWhatToDo(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// No wood held (read unsigned) -> SetTopState(163)
	if (static_cast<uint16_t>(v->resourceHeld.at(1)) == 0)
	{
		TraceIf(villager, "gotwood: wood 0 -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// A disciple whose disciple info ignores needs -> 163
	if ((v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType))
	{
		TraceIf(villager, "gotwood: disciple -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// Its building site valid in its town, (builders needed || a BUILDER disciple) and GotoBuildingSite == 1 -> 1
	const auto site = v->buildingSite;
	const auto held = v->resourceHeld.at(1);
	const auto discipleType = v->discipleType;
	const auto town = site != entt::null ? TownOf(villager) : entt::entity(entt::null);
	// through the building side service, as the builders do
	auto& siteSide = Locator::villagerBuildingSites::value();
	if (site != entt::null && town != entt::null && siteSide.IsBuildingSiteValid(town, site) &&
	    (siteSide.NeedsBuilders(site) || discipleType == k_DiscipleBuilder) && GotoBuildingSite(villager, site) == 1)
	{
		villager::TraceFormatted(villager, "gotwood: wood {} -> site {}", held, static_cast<uint32_t>(site));
		return 1;
	}
	// CheckNeededForBuilding != 0 -> 1
	if (CheckNeededForBuilding(villager) != 0)
	{
		villager::TraceFormatted(villager, "gotwood: wood {} -> build", held);
		return 1;
	}
	// SetTopState(31 GOTO_STORAGE_PIT_FOR_DROP_OFF), 1
	villager::TraceFormatted(villager, "gotwood: wood {} -> 31", held);
	SetTopState(villager, VillagerStates::GotoStoragePitForDropOff);
	return 1;
}

uint32_t CheckSatisfyWoodDesire(entt::entity villager)
{
	// DecideHowToGetWood (VillagerBuild.cpp)
	const auto source = DecideHowToGetWood(villager, false);
	villager::TraceFormatted(villager, "wood-desire: store {:.9f} forest {} {:.9f} big {} -> {}", source.store,
	                         source.forest.has_value() ? static_cast<int64_t>(*source.forest) : -1, source.forestScore,
	                         static_cast<uint32_t>(source.bigForest), source.how);
	switch (source.how)
	{
	case 1:
		// GotoStoragePitForDropOff (its result)
		return GotoStoragePitForDropOff(villager);
	case 2:
	{
		// SetupMoveToOnFootpath(big, the BigForest's arrive point, 53), 1
		const auto arrive = BigForestArrivePos(source.bigForest, villager);
		SetupMoveToOnFootpath(villager, source.bigForest, tq::ToMapCoords(Xz(arrive)), VillagerStates::ArrivesAtBigForest);
		return 1;
	}
	case 3:
		// VillagerGotoForest(forest, 49) (its result)
		return source.forest.has_value() ? VillagerGotoForest(villager, *source.forest, VillagerStates::ForesterArrivesAtForest)
		                                 : 0;
	default:
		return 0;
	}
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t ForesterMoveToForest(LivingAction& action, uint32_t moveResult)
{
	const auto villager = Entities().ToEntity(action);
	// The move result 7 && the wall-hug move byte 2 -> the look-ahead (literal; never in practice)
	if (moveResult == 7 && WallHugByte5E(villager) == k_LookAheadMoveByte)
	{
		ForesterLookAhead(villager);
	}
	return 1;
}

uint32_t ForesterGotoForest(LivingAction& action)
{
	// Just CheckSatisfyWoodDesire
	return CheckSatisfyWoodDesire(Entities().ToEntity(action));
}

uint32_t ForesterArrivesAtForest(LivingAction& action)
{
	return ForesterArrivesAtForest(Entities().ToEntity(action));
}

uint32_t ForesterArrivesAtForest(entt::entity villager)
{
	entt::entity tree = entt::null;
	const auto code = FindTreeNearVillager(villager, tree);
	if (code == 10)
	{
		// The target thing cleared; turn by k_ChopTurn; PlayAnimThenSetState(50), 1
		SetTargetThing(villager, entt::null);
		SetGameAngle(villager, static_cast<uint16_t>(GetGameAngle(villager) - k_ChopTurn));
		villager::TraceFormatted(villager, "forest 49: tree {} touch -> 50", static_cast<uint32_t>(tree));
		PlayAnimThenSetState(villager, VillagerStates::ForesterChopsTree);
		return 1;
	}
	if (code == 1)
	{
		// SetupMoveToWithHug(the tree's working point, 49) != 0
		villager::TraceFormatted(villager, "forest 49: tree {} walk", static_cast<uint32_t>(tree));
		return SetupMoveToWithHug(villager, Xz(TreeWorkingPos(tree, villager)), VillagerStates::ForesterArrivesAtForest) != 0
		           ? 1
		           : 0;
	}
	if (code == 0)
	{
		// An empty forest within 50 of the villager is deleted
		if (const auto empty = FindForest(PositionOf(villager), k_EmptyForestReach, true); empty.has_value())
		{
			villager::TraceFormatted(villager, "forest 49: none (deleted {})", *empty);
			DeleteForest(*empty);
		}
		else
		{
			TraceIf(villager, "forest 49: none");
		}
	}
	// SetTopState(52); ForesterFinishedForestering (its result)
	SetTopState(villager, VillagerStates::ForesterFinishedForestering);
	return ForesterFinishedForestering(villager);
}

uint32_t ForesterChopsTree(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// No target thing && FindTreeNearVillager != 0 (touching or not)
	entt::entity tree = entt::null;
	if (v->targetThing == entt::null && FindTreeNearVillager(villager, tree) != 0)
	{
		// The felled tree is created and the tree deleted: ecs::FellTree does both (the same entity becomes the
		// DeadTree). Its reactions 12 spread inside: a villager that takes one changes state before the target thing
		// is written, as in the original
		const auto felled = FellTree(tree, villager);
		if (felled == entt::null)
		{
			// No felled tree for a tree without its 3D object, and the tree is still deleted: the target thing is
			// cleared (openblack's other nulls, a mesh not loaded or a chopper without a Transform, take the same
			// path)
			DeleteTree(tree);
		}
		SetTargetThing(villager, felled);
		villager::TraceFormatted(villager, "forest 50: fell {} -> {}", static_cast<uint32_t>(tree),
		                         static_cast<uint32_t>(felled));
		return 1;
	}
	// A target thing in the physics and available -> 1 (still falling: clip 217 loops). The in-physics flag is
	// PhysicsObjects::IsFlying (in the physics and not a resting proxy), as in 22 (VillagerResourceReactions
	// InPhysics)
	const auto target = v->targetThing;
	if (target != entt::null && Entities().Valid(target) && physics::PhysicsObjects::IsFlying(target) &&
	    fire::traits::IsAvailable(target))
	{
		return 1;
	}
	// The target thing cleared; SetTopState(52), 1
	TraceIf(villager, "forest 50: -> 52");
	SetTargetThing(villager, entt::null);
	SetTopState(villager, VillagerStates::ForesterFinishedForestering);
	return 1;
}

uint32_t ForesterChopsTreeForBuilding(LivingAction& action)
{
	SetTopState(Entities().ToEntity(action), VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t ForesterFinishedForestering(LivingAction& action)
{
	return ForesterFinishedForestering(Entities().ToEntity(action));
}

uint32_t ForesterFinishedForestering(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// Wood held > 0 (signed) -> GotWoodDecideWhatToDo
	if (v != nullptr && v->resourceHeld.at(1) > 0)
	{
		return GotWoodDecideWhatToDo(villager);
	}
	TraceIf(villager, "forest 52: no wood -> 163");
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t ArrivesAtBigForest(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// cap = the wood capacity (signed); a town && cap != 0 (a negative one goes on)
	const int32_t cap = GetWoodCapacity(villager);
	const auto town = TownOf(villager);
	if (town != entt::null && cap != 0)
	{
		// The town's nearest forest to the villager and its BigForest
		const auto forest = FindNearestForestToPos(Entities().Get<const Town>(town).id, PositionOf(villager));
		const auto big = forest.has_value() ? ForestBigForest(*forest) : entt::entity(entt::null);
		if (big != entt::null)
		{
			// p = the BigForest's arrive point; AreWeThere(p, 0)
			const auto p = BigForestArrivePos(big, villager);
			if (!AreWeThere(villager, Xz(p), k_BigForestReach))
			{
				// SetupMoveToWithHug(p, GetFinalState), 1
				villager::TraceFormatted(villager, "forest 53: big {} walk", static_cast<uint32_t>(big));
				SetupMoveToWithHug(villager, Xz(p), GetFinalState(villager));
				return 1;
			}
			// n = the wood taken from the BigForest, cap read unsigned; a negative cap is a huge unsigned (literal)
			const auto tree = static_cast<uint8_t>(TreeCarriedType(big));
			const auto n = BigForestRemoveWood(big, static_cast<uint32_t>(cap));
			villager::TraceFormatted(villager, "forest 53: big {} took {} -> 163", static_cast<uint32_t>(big), n);
			// n != 0 -> PickupWood(n, the carried tree type); GotWoodDecideWhatToDo.
			// (openblack) the type is read before: the BigForest that gives its last wood is deleted
			if (n != 0)
			{
				PickupWood(villager, static_cast<int16_t>(n), tree);
				GotWoodDecideWhatToDo(villager);
			}
		}
	}
	// SetTopState(163) (also after GotWoodDecideWhatToDo: what it set up is overwritten, literal), 1
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t ArrivesAtBigForestForBuilding([[maybe_unused]] LivingAction& action)
{
	return 1;
}

uint32_t TakeWoodFromTree(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// FindTreeNearVillager == 0 -> SetTopState(163), 1
	entt::entity tree = entt::null;
	if (FindTreeNearVillager(villager, tree) == 0)
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// With a player, the player's creature may come to share the town's need for wood (0.5, at the villager)
	creature_mimic::EmpathiseWithTownDesire(GetPlayerOf(villager), TownDesireInfo::ForWood, 0.5f, villager);
	// SetTopState(49); ForesterArrivesAtForest (its result)
	SetTopState(villager, VillagerStates::ForesterArrivesAtForest);
	return ForesterArrivesAtForest(villager);
}

uint32_t TakeWoodFromPot([[maybe_unused]] LivingAction& action)
{
	return 1;
}

uint32_t ExitForesting(LivingAction& action, [[maybe_unused]] VillagerStates next)
{
	// The target thing cleared; 1
	SetTargetThing(Entities().ToEntity(action), entt::null);
	return 1;
}
} // namespace openblack::ecs::villager
