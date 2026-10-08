/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerHome.h"

#include <cstdint>

#include <optional>
#include <unordered_set>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/VillagerTentQueriesInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/Villager/VillagerTrace.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// The villager's home (VillagerHome.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
using state_info::StateInfo;

namespace
{
/// DoGoingHome: more than 100 m from the town -> towards it
constexpr float k_FarFromTown = 100.0f;
/// GetTentPos: the nearest tree within 50 m; the villagers sleeping in a tent within 5 m
constexpr float k_TreeRadius = 50.0f;
constexpr float k_TentNeighbour = 5.0f;
/// GetTentPos's collide test (0x10 off the game map, 8 a fixed, 1 water)
constexpr uint32_t k_TentCollide = 0x19;
/// GetTentPos: 3 tries, 9 spiral cells each
constexpr int k_TentTries = 3;
constexpr int k_TentCells = 9;
/// TentNextToTree: 9 cells, occupants within 4 m, the tent 2 m from the tree
constexpr int k_TreeCells = 9;
constexpr float k_TreeOccupant = 4.0f;
constexpr float k_TentFromTree = 2.0f;
/// pi / 8
constexpr float k_EighthPi = 0.39269909f;
/// VagrantStart: towns within 200 m
constexpr float k_VagrantTownRadius = 200.0f;
/// GoHomeAndChange: a scale below 0.95 is reset for the age
constexpr float k_GrownUpScale = 0.95f;
/// CheckNeedsAtHome: the desire threshold factor
constexpr float k_NeedsAtHome = 0.9f;
/// VillagerDisciple 10 CHANGE_HOUSE (ExitGoHomeAndChange)
constexpr uint8_t k_DiscipleChangeHouse = 10;
/// VillagerDisciple 5 BREEDER (HomeDecideWhatToDo)
constexpr uint8_t k_DiscipleBreeder = 5;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// The villager's abode (entt::null without one, or when it is gone)
entt::entity AbodeOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: stands for the unlinking when the abode is deleted
	return v != nullptr && v->abode != entt::null && ecs::IsAvailable(v->abode) ? v->abode : entt::null;
}

/// The villager's town
entt::entity TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: stands for the missing town unlinking
	return v != nullptr && v->town != entt::null && ecs::IsAvailable(v->town) && Entities().AllOf<Town>(v->town) ? v->town
	                                                                                                             : entt::null;
}

bool Inside(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->flags & Villager::k_FlagAtHome) != 0;
}

std::string Xz(glm::ivec2 pos)
{
	const auto m = tq::ToMetres(pos);
	return fmt::format("({:.1f}, {:.1f})", m.x, m.y);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// SetupMoveToWithHug on a MapCoords goal
uint32_t MoveTo(entt::entity villager, glm::ivec2 goal, VillagerStates final)
{
	return SetupMoveToWithHug(villager, tq::ToMetres(goal), final);
}

/// The state counter (LivingAction::turnsUntilStateChange)
uint16_t& Counter(entt::entity villager)
{
	return Entities().Get<LivingAction>(villager).turnsUntilStateChange;
}

/// Whether the object is a villager
bool IsVillagerObject(entt::entity object)
{
	return Entities().AllOf<Villager>(object);
}

/// The TOP state of a villager
VillagerStates TopOf(entt::entity object)
{
	const auto* action = Entities().TryGet<const LivingAction>(object);
	return action != nullptr ? static_cast<VillagerStates>(action->states.at(0)) : VillagerStates::InvalidState;
}

/// Whether the object is a tree. (inferred) the tree classes are openblack's Tree component; a dead tree is
/// another class
bool IsTree(entt::entity object)
{
	return Entities().AllOf<Tree>(object);
}

/// The map's collide flags at a position, through the villagerTentQueries service
uint32_t CollideAt(glm::ivec2 pos)
{
	return Locator::villagerTentQueries::value().Collide(pos);
}

/// Adds a JustMapXZ cell step to the x / z MapCoords pair
void AddCell(glm::ivec2& pos, const map_coords::JustMapXZ& step)
{
	map_coords::MapCoords c {pos.x, pos.y, 0.0f};
	map_coords::AddCells(c, step);
	pos = {c.x, c.z};
}

/// The villager's position as MapCoords x / z
glm::ivec2 Me(entt::entity villager)
{
	return tq::PosOf(villager);
}

/// The villager's heading. (approximate) openblack's WallHug::yAngle, in radians
float YAngleOf(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? wallHug->yAngle : 0.0f;
}
} // namespace

entt::entity GetTown(entt::entity villager)
{
	return TownOf(villager);
}

// ---- the villager's links ----------------------------------------------------------------------------------------

void SetAbode(entt::entity villager, entt::entity abode)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// Sets the abode and clears the town; with an abode, takes the abode's town
	v->abode = abode;
	v->town = entt::null;
	if (abode != entt::null)
	{
		v->town = abode_villagers::TownOf(abode);
	}
}

void SetTown(entt::entity villager, entt::entity town)
{
	if (auto* v = VillagerOf(villager))
	{
		v->town = town;
	}
}

bool IsAtHome(entt::entity villager)
{
	return Inside(villager);
}

bool IsReachable(entt::entity villager)
{
	// IsAvailable (not being deleted, the final state not 14 DYING): a corpse playing its
	// dying clip (TOP 23, FINAL 15) or lying in 15 is reachable (literal)
	if (!IsAvailable(villager))
	{
		return false;
	}
	// Inside its home -> 0
	if (Inside(villager))
	{
		return false;
	}
	// In the hand -> 0 (fire::traits::InHand, as VillagerReactions.cpp and Influence.cpp read it)
	if (fire::traits::InHand(villager))
	{
		return false;
	}
	// TOP != 236 GO_AND_HIDE_IN_NEARBY_BUILDING
	return TopOf(villager) != VillagerStates::GoAndHideInNearbyBuilding;
}

bool IsVillagerAvailable(entt::entity villager)
{
	// Controlled by a script -> 0
	if (script_held::IsControlledByScript(villager))
	{
		return false;
	}
	// Not available for a state change while in the hand
	if (fire::traits::InHand(villager))
	{
		return false;
	}
	// The available-state flags of the final state's row & 1
	return (state_info::AvailableState(StateInfo(GetFinalState(villager))) & 1) != 0;
}

void ArriveHome(entt::entity villager)
{
	// With an abode: flagged inside and counted present at home
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return;
	}
	auto* v = VillagerOf(villager);
	v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagAtHome);
	abode_villagers::ArriveHome(abode);
}

void LeaveHome(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	// Only when inside
	if (v == nullptr || (v->flags & Villager::k_FlagAtHome) == 0)
	{
		return;
	}
	// Clears the at-home and going-to-bed flags; with an abode, counted out
	v->flags = static_cast<uint16_t>(v->flags & 0xDFFB);
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		abode_villagers::LeaveHome(abode);
	}
}

// ---- going home --------------------------------------------------------------------------------------------------

uint32_t GoHome(entt::entity villager)
{
	// DoGoingHome(37 ARRIVES_HOME, 238 SLEEP_IN_TENT)
	return DoGoingHome(villager, VillagerStates::ArrivesHome, VillagerStates::SleepInTent);
}

uint32_t GoHomeState(LivingAction& action)
{
	return GoHome(Entities().ToEntity(action));
}

uint32_t DoGoingHome(entt::entity villager, VillagerStates arrive, VillagerStates tent)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// A dancing villager leaves its dance first. TODO(dance):
	// openblack's villagers dance only at the worship site, not in a dance group that GO_HOME leaves
	const auto abode = AbodeOf(villager);
	if (abode != entt::null)
	{
		// Inside -> SetTopState(38 AT_HOME)
		if (Inside(villager))
		{
			SetTopState(villager, VillagerStates::AtHome);
			return 1;
		}
		// The final state == arrive -> 1 (already going)
		if (GetFinalState(villager) == arrive)
		{
			return 1;
		}
		// SetupMoveToOnFootpath(abode, its arrive point, arrive)
		const auto door = abode_queries::GetArrivePos(abode);
		villager::TraceFormatted(villager, "home 36: to the door {} -> {}", Xz(door), static_cast<uint32_t>(arrive));
		SetupMoveToOnFootpath(villager, abode, door, arrive);
		return 1;
	}
	// No town -> SetTopState(130 VAGRANT_START)
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		TraceIf(villager, "home 36: no abode -> vagrant 130");
		SetTopState(villager, VillagerStates::VagrantStart);
		return 1;
	}
	const auto me = Me(villager);
	const auto townPos = tq::PosOf(town);
	// More than 100 m (<= 100 or unordered -> near)
	if (tq::GetDistanceInMetres(me, townPos) > k_FarFromTown)
	{
		// The TOP read first; a = (GameFloatRand(pi/2) - pi/4) + Get3DAngleFromXZ(town, me);
		// d = GameFloatRand(25) + 10; pos = town + GetPosFromAngle(a, d); SetupMoveToWithHug(pos, TOP)
		const auto top = TopOf(villager);
		const float spread = GameFloatRand(glm::half_pi<float>()) - glm::quarter_pi<float>();
		const float angle = tq::Get3DAngleFromXZ(townPos, me) + spread;
		const float distance = GameFloatRand(25.0f) + 10.0f;
		const auto pos = townPos + tq::GetPosFromAngle(angle, distance);
		villager::TraceFormatted(villager, "home 36: no abode -> far(town d={:.1f}) {}", tq::GetDistanceInMetres(me, townPos),
		                         Xz(pos));
		MoveTo(villager, pos, top);
		return 1;
	}
	// d = GameFloatRand(8) + 2, a = GameFloatRand(2 pi): pos = me + GetPosFromAngle
	const float near = GameFloatRand(8.0f) + 2.0f;
	const float around = GameFloatRand(glm::two_pi<float>());
	auto pos = me + tq::GetPosFromAngle(around, near);
	// GetTentPos(pos) -> SetupMoveToWithHug(pos, tent)
	if (GetTentPos(villager, pos))
	{
		villager::TraceFormatted(villager, "home 36: no abode -> tent {}", Xz(pos));
		MoveTo(villager, pos, tent);
		return 1;
	}
	// The TOP read; d = GameFloatRand(20) + 10, a = GameFloatRand(2 pi); pos +=
	// GetPosFromAngle(a, d); SetupMoveToWithHug(pos, TOP): a stroll, then 36 again
	const auto top = TopOf(villager);
	const float strollDistance = GameFloatRand(20.0f) + 10.0f;
	const float strollAngle = GameFloatRand(glm::two_pi<float>());
	pos += tq::GetPosFromAngle(strollAngle, strollDistance);
	villager::TraceFormatted(villager, "home 36: no abode -> wander {}", Xz(pos));
	MoveTo(villager, pos, top);
	return 1;
}

void SetupMoveToOnFootpath(entt::entity villager, entt::entity object, glm::ivec2 pos, VillagerStates final)
{
	// me == the object's arrive point && pos != me -> SetupMoveToWithHug(pos, final)
	const auto me = Me(villager);
	if (me == abode_queries::GetArrivePos(object) && pos != me)
	{
		SetupMoveToWithHug(villager, tq::ToMetres(pos), final);
		return;
	}
	// Else the object's footpath walk when it has a footpath link; without one SetupMoveToWithHug(pos, final).
	// (approximate) the footpath walk is not ported: always the direct walk
	SetupMoveToWithHug(villager, tq::ToMetres(pos), final);
}

uint32_t ArrivesHome(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// No abode -> SetTopState(129 HOMELESS_START); 0
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		TraceIf(villager, "home 37: no abode -> 129");
		SetTopState(villager, VillagerStates::HomelessStart);
		return 0;
	}
	const auto door = abode_queries::GetArrivePos(abode);
	const auto in = [villager, abode]() {
		// ArriveHome; SetTopState(38)
		ArriveHome(villager);
		villager::TraceFormatted(villager, "home 37: arrive (present {})", abode_villagers::PresentAtHome(abode));
		SetTopState(villager, VillagerStates::AtHome);
		return 1u;
	};
	// Not there (AreWeThere(door, 0)) -> SetupMoveToOnFootpath(abode, door, 37): the literal
	// 37, also from 249
	if (!AreWeThere(villager, tq::ToMetres(door), 0.0f))
	{
		TraceIf(villager, "home 37: not there");
		SetupMoveToOnFootpath(villager, abode, door, VillagerStates::ArrivesHome);
		return 1;
	}
	// Built && repaired (life not below 1)
	if (abode_queries::IsBuilt(abode) && abodes::IsRepaired(abode))
	{
		return in();
	}
	const auto& info = InfoOf(villager);
	// life < DamageThresholdToGoHome
	if (life::LifeOf(villager) < info.damageThresholdToGoHome)
	{
		// A functional abode -> in
		if (abode_queries::IsFunctional(abode))
		{
			return in();
		}
		// a = Get3DAngleFromXZ(abode, me) + (pi/8 - GameFloatRand(pi/4)); d = GameFloatRand(5) + 5;
		// pos = me + GetPosFromAngle(a, d)
		const auto me = Me(villager);
		const float spread = k_EighthPi - GameFloatRand(glm::quarter_pi<float>());
		const float angle = tq::Get3DAngleFromXZ(tq::PosOf(abode), me) + spread;
		const float distance = GameFloatRand(5.0f) + 5.0f;
		auto pos = me + tq::GetPosFromAngle(angle, distance);
		// GetTentPos -> SetupMoveToWithHug(pos, 238), the literal 238 (also from 249); 1
		if (GetTentPos(villager, pos))
		{
			villager::TraceFormatted(villager, "home 37: tent {}", Xz(pos));
			MoveTo(villager, pos, VillagerStates::SleepInTent);
		}
		return 1;
	}
	// food < HungryForFood (strict)
	if (v->food < info.hungryForFood)
	{
		// Not functional -> SetTopState(163) and on to ArriveHome / 38 (no jump between: the
		// 163 is overwritten in the same turn, literal)
		if (!abode_queries::IsFunctional(abode))
		{
			TraceIf(villager, "home 37: hungry 163+arrive");
			SetTopState(villager, VillagerStates::DecideWhatToDo);
		}
		return in();
	}
	// SetupBuildingObject(abode) == 1 -> 1 (the abode's site, made now if it has none:
	// VillagerBuild.cpp). So a villager arriving at a home that is not (built and at full life),
	// with life >= 0.3 and food >= 0.5, repairs it when the site takes it (it is a builder of it already, the site
	// needs builders: home life <= 0.9 with inhabitants, or it is a BUILDER disciple); else (full, or life in
	// (0.9, 1)) it goes in
	const uint32_t repairs = SetupBuildingObjectForBuilding(villager, abode);
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("home 37: repair abode {} built {} life {:.4f} -> {}", static_cast<uint32_t>(abode),
		                            abode_queries::IsBuilt(abode) ? 1 : 0, life::LifeOf(abode), repairs == 1 ? 1 : 0));
	}
	if (repairs == 1)
	{
		return 1;
	}
	return in();
}

uint32_t ArrivesHomeState(LivingAction& action)
{
	ArrivesHome(Entities().ToEntity(action));
	return 1;
}

// ---- at home -----------------------------------------------------------------------------------------------------

uint32_t AtHome(LivingAction& action)
{
	HomeDecideWhatToDo(Entities().ToEntity(action));
	return 1;
}

uint32_t HomeDecideWhatToDo(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// An abode, a town, and the town's emergency -> SetTopState(119); 1 (it hides in bed)
	const auto town = TownOf(villager);
	if (AbodeOf(villager) != entt::null && town != entt::null && tq::IsInStateOfEmergency(Entities().Get<const Town>(town)))
	{
		TraceIf(villager, "home 38: emergency -> 119");
		SetTopState(villager, VillagerStates::GotoBedAtHome);
		return 1;
	}
	// CheckNeedsAtHome == 1 -> 1
	if (CheckNeedsAtHome(villager) == 1)
	{
		return 1;
	}
	// A disciple whose type ignores the needs
	if ((v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType))
	{
		// A BREEDER (5) with Sleep (16) first in the town's order 1 and CheckSatisfySleep -> 1
		if (v->discipleType == k_DiscipleBreeder && town != entt::null &&
		    town_desire::GetSortedDesires(town).at(0).index == static_cast<uint32_t>(TownDesireInfo::ForSleep) &&
		    CheckSatisfySleep(villager) != 0)
		{
			return 1;
		}
		// DecideWhatToDo: its result
		TraceIf(villager, "home 38: disciple");
		return DecideWhatToDo(Entities().Get<LivingAction>(villager));
	}
	// CheckNeededForSomething == 1 -> 1
	if (CheckNeededForSomething(villager) == 1)
	{
		TraceIf(villager, "home 38: something");
		return 1;
	}
	// HomeNothingToDo; 0
	HomeNothingToDo(villager);
	return 0;
}

uint32_t CheckNeedsAtHome(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// A woman: UpdatePregnancy == 1 -> 1; pregnant -> 1 (she stays at home doing nothing)
	if (IsWoman(villager))
	{
		if (UpdatePregnancy(villager) == 1)
		{
			return 1;
		}
		if (IsPregnant(villager))
		{
			return 1;
		}
	}
	// t = the larger of GetLifeDesireFromLife(D) and POWER(F) (POWER not below it keeps POWER): a disciple that
	// ignores the needs (D, F) = (DamageThresholdToGoHome, StarvingForFood), else (DamageThresholdToSleepUntil,
	// HungryForFood)
	const auto& info = InfoOf(villager);
	const bool ignores = (v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType);
	const float d = ignores ? info.damageThresholdToGoHome : info.damageThresholdToSleepUntil;
	const float f = ignores ? info.starvingForFood : info.hungryForFood;
	const float lifeDesire = GetLifeDesireFromLife(villager, d);
	const float foodDesire = Power(f);
	const float t = foodDesire < lifeDesire ? lifeDesire : foodDesire;
	// CheckSatisfyOwnDesire(t x 0.9) == 1 -> 1
	const float trigger = t * k_NeedsAtHome;
	if (CheckSatisfyOwnDesire(villager, trigger) == 1)
	{
		villager::TraceFormatted(villager, "home 38: needs(t={:.6f})", trigger);
		return 1;
	}
	// A child: CheckChildActivity (= ChildDecideWhatToDo; 1) == 1 -> 1
	if (IsChild(villager))
	{
		ChildDecideWhatToDo(villager);
		return 1;
	}
	return 0;
}

uint32_t HomeNothingToDo(entt::entity villager)
{
	// Inside: GameRand(4) == 0 -> counter = 0, SetTopState(119)
	if (Inside(villager))
	{
		const auto r = GameRand(4);
		if (r == 0)
		{
			TraceIf(villager, "home 38: nothing r4=0 -> 119");
			Counter(villager) = 0;
			SetTopState(villager, VillagerStates::GotoBedAtHome);
			return 1;
		}
		villager::TraceFormatted(villager, "home 38: nothing r4={}", r);
	}
	SetupNothingToDo(villager);
	return 1;
}

uint32_t ExitAtHome(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// The next state's StaysAtHomeOnExit == 0 -> LeaveHome
	const bool stays = state_info::StaysAtHomeOnExit(StateInfo(next));
	const bool wasInside = Inside(villager);
	if (!stays)
	{
		LeaveHome(villager);
	}
	if (TraceOn(villager) && wasInside)
	{
		const auto abode = AbodeOf(villager);
		Trace(villager, fmt::format("exit-home {} -> {} ({}, present {})", action.states.at(0), static_cast<uint32_t>(next),
		                            stays ? "stay" : "leave", abode != entt::null ? abode_villagers::PresentAtHome(abode) : 0));
	}
	return 1;
}

uint32_t GotoBedAtHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// SetTopState(120), then the counter = RestAtHomeTime
	SetTopState(villager, VillagerStates::SleepingAtHome);
	if (Entities().AllOf<LivingAction>(villager))
	{
		Counter(villager) = static_cast<uint16_t>(InfoOf(villager).restAtHomeTime);
	}
	return 1;
}

uint32_t SleepingAtHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// No town -> nothing (it sleeps for ever, literal)
	if (TownOf(villager) == entt::null)
	{
		return 1;
	}
	// --counter (u16); not 0 -> 1
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange != 0)
	{
		return 1;
	}
	// DoSleeping(1) == 0 -> SetTopState(38)
	if (DoSleeping(villager, 1.0f) == 0)
	{
		villager::TraceFormatted(villager, "sleep 120: life {:.6f} -> wake", life::LifeOf(villager));
		SetTopState(villager, VillagerStates::AtHome);
	}
	else
	{
		villager::TraceFormatted(villager, "sleep 120: life {:.6f} -> keep", life::LifeOf(villager));
	}
	return 1;
}

uint32_t DoSleeping(entt::entity villager, float f)
{
	// Poisoned -> 0 (no sleep, no healing)
	if (life::IsPoisoned(villager))
	{
		return 0;
	}
	const auto& info = InfoOf(villager);
	// life < the info's life -> IncreaseLife(f x RestAtHomeRestoresLifeBy)
	if (life::LifeOf(villager) < info.life)
	{
		life::IncreaseLife(villager, f * info.restAtHomeRestoresLifeBy);
	}
	// (a town and Sleep (16) first in its sorted desires) or life < DamageThresholdToSleepUntil ->
	// counter = RestAtHomeTime; 1
	const auto town = TownOf(villager);
	const bool sleepFirst = town != entt::null &&
	                        town_desire::GetSortedDesires(town).at(0).index == static_cast<uint32_t>(TownDesireInfo::ForSleep);
	if (sleepFirst || life::LifeOf(villager) < info.damageThresholdToSleepUntil)
	{
		Counter(villager) = static_cast<uint16_t>(info.restAtHomeTime);
		return 1;
	}
	return 0;
}

uint32_t WakeUpAtHome(LivingAction& action)
{
	return GoHome(Entities().ToEntity(action));
}

uint32_t CheckWhenGoingToBed(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// Already going to bed -> 1; else the flag is set (LeaveHome clears it)
	if ((v->flags & Villager::k_FlagGoingToBed) != 0)
	{
		return 1;
	}
	v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagGoingToBed);
	// CheckDeathFromOldAge -> 0
	if (CheckDeathFromOldAge(villager))
	{
		return 0;
	}
	// No town, GetRawDesire(16) < 1 or not sexually active -> 1
	const auto town = TownOf(villager);
	if (town == entt::null || town_desire::GetRawDesire(town, TownDesireInfo::ForSleep) < 1.0f || !IsSexuallyActive(villager))
	{
		return 1;
	}
	const auto abode = AbodeOf(villager);
	const auto& list = abode_villagers::VillagersOf(abode);
	const auto sex = InfoOf(villager).sex;
	if (sex == SexType::Female)
	{
		// The first man of the abode's list that is inside -> CheckGetPregnantAtHome (hers); 1
		for (const auto other : list)
		{
			if (InfoOf(other).sex == SexType::Male && Inside(other))
			{
				CheckGetPregnantAtHome(villager);
				return 1;
			}
		}
		return 1;
	}
	if (sex == SexType::Male)
	{
		// Each woman of the list that is inside -> her CheckGetPregnantAtHome
		const auto copy = list;
		for (const auto other : copy)
		{
			if (InfoOf(other).sex == SexType::Female && Inside(other))
			{
				CheckGetPregnantAtHome(other);
			}
		}
	}
	return 1;
}

uint32_t CheckGetPregnantAtHome(entt::entity villager)
{
	// WillHousewifeGetPregnant == 0 -> 0
	if (!WillHousewifeGetPregnant(villager))
	{
		return 0;
	}
	// HousewifeGetsPregnant (pregnancy = TimePregnantFor, GoHome if outside), its result
	TraceIf(villager, "home: CheckGetPregnantAtHome 0x760C80 -> pregnant");
	return HousewifeGetsPregnant(villager);
}

// ---- the tent ----------------------------------------------------------------------------------------------------

uint32_t SleepInTent(entt::entity villager)
{
	if (!Entities().AllOf<LivingAction>(villager))
	{
		return 1;
	}
	auto& counter = Counter(villager);
	// counter != 0 -> --counter; 1
	if (counter != 0)
	{
		--counter;
		return 1;
	}
	// DoSleeping(1) != 0 -> 1 (the counter was reset)
	if (DoSleeping(villager, 1.0f) != 0)
	{
		return 1;
	}
	// No abode and CheckHomelessMoveIntoAbode -> 1
	if (AbodeOf(villager) == entt::null && CheckHomelessMoveIntoAbode(villager) != 0)
	{
		return 1;
	}
	// r = HomeDecideWhatToDo; r == 0 or TOP still 238 -> counter = RestAtHomeTime, then --counter
	const auto r = HomeDecideWhatToDo(villager);
	if (r == 0 || TopOf(villager) == VillagerStates::SleepInTent)
	{
		if (Entities().AllOf<LivingAction>(villager))
		{
			auto& again = Counter(villager);
			again = static_cast<uint16_t>(InfoOf(villager).restAtHomeTime);
			--again;
		}
	}
	return 1;
}

uint32_t SleepInTentState(LivingAction& action)
{
	return SleepInTent(Entities().ToEntity(action));
}

bool GetTentPos(entt::entity villager, glm::ivec2& pos)
{
	// The nearest tree to me within 50 m and TentNextToTree -> pos = its point; 1
	const auto me = Me(villager);
	if (const auto tree = Locator::villagerTentQueries::value().NearestTree(me, k_TreeRadius); tree != entt::null)
	{
		if (const auto out = TentNextToTree(tree, villager); out.has_value())
		{
			villager::TraceFormatted(villager, "tent: tree {} {}", object_index::Of(tree), Xz(*out));
			pos = *out;
			return true;
		}
	}
	// tmp = pos; three tries
	auto tmp = pos;
	for (int attempt = 0; attempt < k_TentTries; ++attempt)
	{
		// A blocked cell -> on to the next try's move
		if ((CollideAt(tmp) & k_TentCollide) == 0)
		{
			// free = 1; 9 cells from tmp's, tmp moved one spiral step after each
			bool free = true;
			map_coords::Spiral spiral;
			for (int cell = 0; cell < k_TentCells; ++cell)
			{
				const map_coords::MapCoords at {tmp.x, tmp.y, 0.0f};
				// The cell's objects (the fixed list, then the mobile one): a villager within 5 m of tmp
				// whose TOP is 238 -> free = 0, on to the next cell
				for (const auto object : tq::ObjectsInCell(map_coords::Cell(at)))
				{
					if (!IsVillagerObject(object))
					{
						continue;
					}
					if (!(tq::GetDistanceInMetres(tmp, tq::PosOf(object)) < k_TentNeighbour))
					{
						continue;
					}
					if (TopOf(object) == VillagerStates::SleepInTent)
					{
						free = false;
						break;
					}
				}
				// tmp += the next spiral step
				AddCell(tmp, spiral.Next());
			}
			// free -> pos = tmp (9 spiral steps from the cell tried); 1
			if (free)
			{
				villager::TraceFormatted(villager, "tent: spiral try {} {}", attempt, Xz(tmp));
				pos = tmp;
				return true;
			}
		}
		// tmp += GetPosFromAngle(GameFloatRand(2 pi), GameFloatRand(5) + 3) (the 5 is drawn
		// first, it is the last argument pushed)
		const float distance = GameFloatRand(5.0f) + 3.0f;
		const float angle = GameFloatRand(glm::two_pi<float>());
		tmp += tq::GetPosFromAngle(angle, distance);
	}
	TraceIf(villager, "tent: fail");
	return false;
}

entt::entity FindNearestTree(glm::ivec2 pos, float radius)
{
	// The nearest tree in a spiral, nothing excluded
	return map_cells::FindNearestInSpiral(map_coords::MapCoords {pos.x, pos.y, 0.0f}, &IsTree, radius, entt::null);
}

std::optional<glm::ivec2> TentNextToTree(entt::entity tree, entt::entity villager)
{
	const auto treePos = tq::PosOf(tree);
	// 9 spiral cells from the tree's
	glm::ivec2 walk = treePos;
	map_coords::Spiral spiral;
	std::optional<glm::ivec2> occupant;
	for (int cell = 0; cell < k_TreeCells; ++cell)
	{
		const map_coords::MapCoords at {walk.x, walk.y, 0.0f};
		for (const auto object : tq::ObjectsInCell(map_coords::Cell(at)))
		{
			// distance(tree, object) - the object's 2D radius < 4
			const float d = tq::GetDistanceInMetres(treePos, tq::PosOf(object));
			if (!(d - tq::Get2DRadius(object) < k_TreeOccupant))
			{
				continue;
			}
			// A villager in 238, or a non-villager with the MultiMapFixed bit
			const bool counts = IsVillagerObject(object) ? TopOf(object) == VillagerStates::SleepInTent
			                                             : map_cells::IsMultiCellStaticClass(object);
			if (!counts)
			{
				continue;
			}
			// A second occupant -> 0; the first one's position kept
			if (occupant.has_value())
			{
				return std::nullopt;
			}
			occupant = tq::PosOf(object);
		}
		AddCell(walk, spiral.Next());
	}
	// out = the tree + GetPosFromAngle(a, 2), a = Get3DAngleFromXZ(occupant, tree) (the other
	// side) or Get3DAngleFromXZ(tree, villager)
	const float angle =
	    occupant.has_value() ? tq::Get3DAngleFromXZ(*occupant, treePos) : tq::Get3DAngleFromXZ(treePos, Me(villager));
	return treePos + tq::GetPosFromAngle(angle, k_TentFromTree);
}

// ---- the homeless and the vagrants -------------------------------------------------------------------------------

uint32_t HomelessStart(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// CheckHungry == 1 (the direct call resets LastCheckTurn: the periodic check's phase moves),
	// CheckNeededForSomething == 1, CheckHomelessMoveIntoAbode != 0 -> 1; else SetupNothingToDo
	if (CheckHungry(villager, CurrentTurn()))
	{
		return 1;
	}
	if (CheckNeededForSomething(villager) == 1)
	{
		return 1;
	}
	if (CheckHomelessMoveIntoAbode(villager) != 0)
	{
		return 1;
	}
	SetupNothingToDo(villager);
	return 1;
}

uint32_t VagrantStart(entt::entity villager)
{
	const auto me = Me(villager);
	// The nearest town within 200 m; of my tribe and AddVillagerToTown -> SetTopState(163); 1
	if (const auto town = town_villagers::GetNearestTown(me, k_VagrantTownRadius); town != entt::null)
	{
		const auto* tribe = Entities().TryGet<const Tribe>(town);
		if (tribe != nullptr && *tribe == InfoOf(villager).tribeType && town_villagers::AddVillagerToTown(town, villager))
		{
			villager::TraceFormatted(villager, "vagrant 130: joins town {}", Entities().Get<const Town>(town).id);
			SetTopState(villager, VillagerStates::DecideWhatToDo);
			return 1;
		}
	}
	// life < DamageThresholdToGoHome
	if (life::LifeOf(villager) < InfoOf(villager).damageThresholdToGoHome)
	{
		// d = GameFloatRand(5), a = GameFloatRand(2 pi) (in that order);
		// pos = me + GetPosFromAngle(a, d); GetTentPos -> SetupMoveToWithHug(pos, 238)
		const float distance = GameFloatRand(5.0f);
		const float angle = GameFloatRand(glm::two_pi<float>());
		auto pos = me + tq::GetPosFromAngle(angle, distance);
		if (GetTentPos(villager, pos))
		{
			MoveTo(villager, pos, VillagerStates::SleepInTent);
		}
		return 1;
	}
	// a = heading + (GameFloatRand(pi/4) - pi/8); d = GameFloatRand(20) + 10; pos =
	// me + GetPosFromAngle(a, d); InBounds -> SetupMoveToWithHug(pos, 130)
	const float spread = GameFloatRand(glm::quarter_pi<float>()) - k_EighthPi;
	const float angle = YAngleOf(villager) + spread;
	const float distance = GameFloatRand(20.0f) + 10.0f;
	const auto pos = me + tq::GetPosFromAngle(angle, distance);
	if (map_coords::InBounds(map_coords::MapCoords {pos.x, pos.y, 0.0f}))
	{
		villager::TraceFormatted(villager, "vagrant 130: stroll {}", Xz(pos));
		MoveTo(villager, pos, VillagerStates::VagrantStart);
	}
	return 1;
}

uint32_t VagrantStartState(LivingAction& action)
{
	return VagrantStart(Entities().ToEntity(action));
}

uint32_t CheckHomelessMoveIntoAbode(entt::entity villager)
{
	// A town
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// FindAbodeWithSpaceInTown(me, 0)
	const auto abode = town_villagers::FindAbodeWithSpaceInTown(town, villager, 0.0f);
	if (abode == entt::null)
	{
		return 0;
	}
	// Out of the homeless list (if there); AddVillagerToAbode; SetTopState(36)
	town_villagers::RemoveFromHomelessList(town, villager);
	villager::TraceFormatted(villager, "homeless: into abode {} (score {:.6f})", object_index::Of(abode),
	                         abode_villagers::CalculateScoreForAddingVillagerToAbode(abode, villager));
	abode_villagers::AddVillagerToAbode(abode, villager);
	SetTopState(villager, VillagerStates::GoHome);
	return 1;
}

bool MakeHomeless(entt::entity villager)
{
	// MakeHomelessNoStateChange; SetTopState(129); its result
	const bool made = MakeHomelessNoStateChange(villager);
	SetTopState(villager, VillagerStates::HomelessStart);
	return made;
}

bool MakeHomelessNoStateChange(entt::entity villager)
{
	// The town, read before leaving the abode
	const auto town = TownOf(villager);
	// An abode -> RemoveAliveVillagerFromAbode, SetAbode(null), SetTown(town)
	if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		abode_villagers::RemoveAliveVillagerFromAbode(abode, villager);
		SetAbode(villager, entt::null);
		SetTown(villager, town);
	}
	// No town -> 0; already in its list -> 0
	if (town == entt::null || town_villagers::IsVillagerInHomelessList(town, villager))
	{
		return false;
	}
	// Out of the vagrants; at the head of the town's homeless list; 1
	town_villagers::RemoveFromVagrants(villager);
	town_villagers::AddToHomelessList(town, villager);
	TraceIf(villager, "homeless: list");
	return true;
}

void HomeDeleted(entt::entity villager)
{
	// The original also clears another link of the villager to that abode. TODO: that link is not identified
	// An abode -> MakeHomeless; else the town's deletion (TODO: villager death)
	if (AbodeOf(villager) != entt::null)
	{
		MakeHomeless(villager);
	}
}

uint32_t CheckNeedNewAbode(entt::entity villager)
{
	// A child -> 0 (also from 114 without a mother nor an abode: it stays there, literal)
	if (IsChild(villager))
	{
		return 0;
	}
	// An abode that is not too crowded -> 0
	const auto abode = AbodeOf(villager);
	if (abode != entt::null && !abode_villagers::IsTooCrowded(abode))
	{
		return 0;
	}
	// No town -> VagrantStart; 1
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		VagrantStart(villager);
		return 1;
	}
	// s = the abode's score for me (0 without one); FindAbodeWithSpaceInTown(me, s)
	const float score = abode != entt::null ? abode_villagers::CalculateScoreForAddingVillagerToAbode(abode, villager) : 0.0f;
	const auto better = town_villagers::FindAbodeWithSpaceInTown(town, villager, score);
	// Found and MoveVillagerToAbode == 1 -> IsVillagerAvailable ? SetTopState(36); 1
	if (better != entt::null && MoveVillagerToAbode(villager, better) == 1)
	{
		villager::TraceFormatted(villager, "abode: moves to {} (score {:.6f})", object_index::Of(better), score);
		if (IsVillagerAvailable(villager))
		{
			SetTopState(villager, VillagerStates::GoHome);
		}
		return 1;
	}
	// Not in the town's homeless list -> MakeHomeless; 1
	if (const auto now = TownOf(villager); now == entt::null || !town_villagers::IsVillagerInHomelessList(now, villager))
	{
		TraceIf(villager, "abode: too crowded, none better -> homeless 129");
		MakeHomeless(villager);
	}
	return 1;
}

uint32_t MoveVillagerToAbode(entt::entity villager, entt::entity abode)
{
	// A child GetRoomLeftForChildren, an adult GetRoomLeftForAdults; > 0 -> Force; 1
	const int32_t room =
	    IsChild(villager) ? abode_villagers::GetRoomLeftForChildren(abode) : abode_villagers::GetRoomLeftForAdults(abode);
	if (room <= 0)
	{
		return 0;
	}
	ForceMoveVillagerToAbode(villager, abode);
	return 1;
}

void ForceMoveVillagerToAbode(entt::entity villager, entt::entity abode)
{
	// My town and the abode's
	const auto mine = TownOf(villager);
	const auto theirs = abode_villagers::TownOf(abode);
	if (mine == theirs)
	{
		abode_villagers::AddVillagerToAbode(abode, villager);
		return;
	}
	// My town's RemoveVillager
	if (mine != entt::null)
	{
		town_villagers::RemoveVillager(mine, villager);
	}
	// The abode's GetPercentAbodeFullWithChildren for a child, WithAdults for an adult, below 1 ->
	// AddVillagerToAbode; else its town's AddVillagerToTown
	const float full = IsChild(villager) ? abode_villagers::GetPercentAbodeFullWithChildren(abode)
	                                     : abode_villagers::GetPercentAbodeFullWithAdults(abode);
	if (full < 1.0f)
	{
		abode_villagers::AddVillagerToAbode(abode, villager);
		return;
	}
	if (theirs != entt::null)
	{
		town_villagers::AddVillagerToTown(theirs, villager);
	}
}

// ---- growing up --------------------------------------------------------------------------------------------------

uint32_t GoHomeAndChange(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto abode = AbodeOf(villager);
	if (abode != entt::null)
	{
		// door = GetArrivePos; not AreWeThere(door, 0) -> SetupMoveToWithHug(door, 234); 1
		const auto door = abode_queries::GetArrivePos(abode);
		if (!AreWeThere(villager, tq::ToMetres(door), 0.0f))
		{
			MoveTo(villager, door, VillagerStates::GoHomeAndChange);
			return 1;
		}
		// SetTopState(inside ? 38 : 37)
		SetTopState(villager, Inside(villager) ? VillagerStates::AtHome : VillagerStates::ArrivesHome);
	}
	else
	{
		// SetTopState(163)
		SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
	// GetScale < 0.95 -> SetScaleForAge(GetAge())
	if (const auto* t = Entities().TryGet<const Transform>(villager); t != nullptr && t->scale.x < k_GrownUpScale)
	{
		SetScaleForAge(villager, GetAge(villager));
	}
	return 1;
}

uint32_t ExitGoHomeAndChange(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// IsStateExitFunctionSameAs(next) -> nothing
	if (!IsStateExitFunctionSameAs(villager, next))
	{
		// The town's tribe or, without one, the info's
		const auto town = TownOf(villager);
		const auto* townTribe = town != entt::null ? Entities().TryGet<const Tribe>(town) : nullptr;
		const auto tribe = townTribe != nullptr ? *townTribe : InfoOf(villager).tribeType;
		// ChangeTribeIfRequired(tribe, the next state's StaysAtHomeOnExit == 0)
		const bool leaving = !state_info::StaysAtHomeOnExit(StateInfo(next));
		ChangeTribeIfRequired(villager, tribe, leaving);
	}
	// discipleType 10 CHANGE_HOUSE -> SetVillagerDisciple(0, 0, 0)
	if (const auto* v = VillagerOf(villager); v != nullptr && v->discipleType == k_DiscipleChangeHouse)
	{
		TraceIf(villager, "234: SetVillagerDisciple(0) 0x756000");
		SetVillagerDisciple(villager, entt::null, VillagerDisciple::None, 0);
	}
	return 1;
}

void ChangeTribeIfRequired(entt::entity villager, Tribe tribe, bool leaving)
{
	const auto& info = InfoOf(villager);
	// KeepMeshWhenChangeTown != 0 -> nothing
	if (info.keepMeshWhenChangeTown != 0)
	{
		return;
	}
	// FindVillagerInfo(tribe, my number)
	const auto* found = FindVillagerInfo(tribe, info.villagerNumber);
	if (found == nullptr)
	{
		return; // (openblack, guard) the original passes a null info to ChangeInfo
	}
	// With a town, a town total moves by the difference between the new and old info (recomputed with the
	// town stats each town update: nothing to add here)
	ChangeInfo(villager, *found);
	// Leaving -> disappear_smoke::Create(point, 1, 1.0, colour -1) at the object's point half its height up
	// (inferred)
	if (leaving)
	{
		if (const auto* t = Entities().TryGet<const Transform>(villager))
		{
			const float half = object::ObjectHeight(villager) * 0.5f;
			disappear_smoke::Create(t->position + glm::vec3(0.0f, half, 0.0f), 1, 1.0f, 0xFFFFFFFFu);
		}
	}
}

uint32_t ChangeInfo(entt::entity villager, const GVillagerInfo& info)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// The info (openblack finds it by tribe and number: VillagerInfoOf)
	v->tribe = info.tribeType;
	v->number = info.villagerNumber;
	v->sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	// A child: the three meshes = ChildMeshHigh (odd: SetAge uses three different child meshes); an adult: the
	// detail meshes 2 / 1 / 0
	SetVillagerMeshes(villager, info, IsChild(villager), true);
	villager::TraceFormatted(villager, "234: ChangeInfo -> {} {} ({})", static_cast<int>(info.tribeType),
	                         static_cast<int>(info.villagerNumber), IsChild(villager) ? "child mesh" : "adult mesh");
	return 1;
}

void SetVillagerMeshes(entt::entity villager, const GVillagerInfo& info, bool child, bool childHighOnly)
{
	auto& registry = Entities();
	const auto meshId = child ? (childHighOnly ? info.childMeshHigh : info.childMeshMedium) : info.stdDetail;
	const auto id = resources::HashIdentifier(meshId);
	if (auto* mesh = registry.TryGet<Mesh>(villager))
	{
		mesh->id = id;
		return;
	}
	// not drawn now (a -4 clip): the mesh it gets back when it is drawn again (VillagerAnimations' hiddenMesh)
	if (auto* animation = registry.TryGet<SkeletalAnimation>(villager); animation != nullptr && animation->hiddenMesh != 0)
	{
		animation->hiddenMesh = id;
	}
}

const GVillagerInfo* FindVillagerInfo(Tribe tribe, VillagerNumber number)
{
	// The first info record with that tribe and number
	for (const auto& info : Locator::infoConstants::value().villager)
	{
		if (info.tribeType == tribe && info.villagerNumber == number)
		{
			return &info;
		}
	}
	return nullptr;
}

glm::ivec2 FindPosOutsideAbode(entt::entity villager, entt::entity abode)
{
	// No abode given -> mine
	if (abode == entt::null)
	{
		abode = AbodeOf(villager);
	}
	if (abode == entt::null)
	{
		return Me(villager); // (openblack, guard) the original dereferences the null abode
	}
	// Get3DAngleFromXZ(abode, door), stored
	const auto door = abode_queries::GetArrivePos(abode);
	const float toDoor = tq::Get3DAngleFromXZ(tq::PosOf(abode), door);
	// d = GameFloatRand(1.5) + 1.5, stored
	const float distance = GameFloatRand(1.5f) + 1.5f;
	// a = (pi/8 - GameFloatRand(pi/4)) + toDoor
	const float spread = k_EighthPi - GameFloatRand(glm::quarter_pi<float>());
	const float angle = spread + toDoor;
	// door + GetPosFromAngle(a, d)
	return door + tq::GetPosFromAngle(angle, distance);
}

bool IsSexuallyActive(entt::entity villager)
{
	// StartHavingSexAge <= age < StopHavingSexAge
	const auto& info = InfoOf(villager);
	const uint32_t age = GetAge(villager);
	return !(age < info.startHavingSexAge) && age < info.stopHavingSexAge;
}
} // namespace openblack::ecs::villager
