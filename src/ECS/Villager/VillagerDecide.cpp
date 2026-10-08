/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDecide.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <unordered_set>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ForcedNothingRoll.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/VillagerDecideEvents.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/VillagerWorshipCheckInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerChild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "InfoConstants.h"
#include "Locator.h"

// The villager's decision and idle states (VillagerDecide.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;
namespace aq = abode_queries;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// Each check DecideWhatToDo makes is published, so a test can follow the order
void Log(const char* step)
{
	events::Publish(events::VillagerDecideStep {step});
}

/// The villager's town entity (entt::null without one)
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the stored town: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

Town* TownOf(entt::entity villager)
{
	const auto town = TownEntityOf(villager);
	return town != entt::null ? &Entities().Get<Town>(town) : nullptr;
}

/// The villager's abode (entt::null without one, or when the abode is gone)
entt::entity AbodeOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the stored abode: this stands for the unlinking when the abode is deleted
	if (v == nullptr || v->abode == entt::null || !ecs::IsAvailable(v->abode))
	{
		return entt::null;
	}
	return v->abode;
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

const GTownInfo& TownInfo()
{
	return Locator::infoConstants::value().town;
}

/// What the villager trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct VillagerDecideDebugHooksState
{
	std::unordered_set<int64_t> childTraced; // OPENBLACK_VILLAGER_TRACE: children already traced as staying put
};

VillagerDecideDebugHooksState& VillagerDecideDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::villager: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<VillagerDecideDebugHooksState>();
}
} // namespace

// ---- the state functions -----------------------------------------------------------------------------------------

uint32_t DecideWhatToDo(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// The town's emergency -> SetTopState(242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY)
	if (const auto* town = TownOf(villager); town != nullptr && tq::IsInStateOfEmergency(*town))
	{
		Log("emergency");
		TraceIf(villager, "decide: emergency -> 242");
		SetTopState(villager, VillagerStates::GotoCongregateInTownAfterEmergency);
		return 1;
	}
	// A disciple or a disciple follower
	if ((v->flags & Villager::k_FlagDisciple) != 0 || (v->flags & Villager::k_FlagDiscipleFollower) != 0)
	{
		Log("disciple");
		if (DiscipleDecideWhatToDo(villager) == 1)
		{
			TraceIf(villager, "decide: disciple");
			// A follower loses its disciple type
			if ((v->flags & Villager::k_FlagDiscipleFollower) != 0)
			{
				v->discipleType = 0;
				return 1;
			}
			// The disciple type's reaction flag -> a reaction 0x18 for its player; 1 either way. (pending)
			// the reaction 0x18 is not made
			if (DiscipleCreatesJobReaction(v->discipleType))
			{
				TraceIf(villager, "decide: disciple reaction 0x18 (pending)");
			}
			return 1;
		}
		// A follower that has nothing to do stops following
		if ((v->flags & Villager::k_FlagDiscipleFollower) != 0)
		{
			v->flags = static_cast<uint16_t>(v->flags & 0xF9FF);
			v->discipleType = 0;
		}
	}
	// SetTopState(163) (163 does not pause, FINAL is cleared)
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	// A child
	if (IsChild(villager))
	{
		Log("child");
		return ChildDecideWhatToDo(villager);
	}
	Log("something");
	if (CheckNeededForSomething(villager) == 1)
	{
		return 1;
	}
	Log("resources");
	if (CheckTakeResourcesToStoragePit(villager) != 0)
	{
		return 1;
	}
	// SetupNothingToDo always gives 1, so the SetTopState(36) after it is dead code
	Log("nothing");
	if (SetupNothingToDo(villager) == 0)
	{
		SetTopState(villager, VillagerStates::GoHome);
	}
	return 1;
}

uint32_t NothingToDo([[maybe_unused]] LivingAction& action)
{
	return 1;
}

uint32_t GoAndChilloutOutsideHome(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// No abode or no town -> SetTopState(163)
	const auto abode = AbodeOf(villager);
	const auto townEntity = TownEntityOf(villager);
	if (abode == entt::null || townEntity == entt::null)
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// R = the town info's maxDistanceFromHouseThatPeopleChillOut
	const float r = TownInfo().maxDistanceFromHouseThatPeopleChillOut;
	// The door (GetArrivePos)
	const auto door = aq::GetArrivePos(abode);
	// T = door + GetPosFromAngle(Get3DAngleFromXZ(abode, door), R x 10): out of the door
	const auto lookAt = door + tq::GetPosFromAngle(tq::Get3DAngleFromXZ(tq::PosOf(abode), door), r * 10.0f);
	GetMeToMyChillOutPos(villager, &GetPosOutsideMyHouse, door, r, &lookAt);
	return 1;
}

uint32_t SitAndChillout(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// old = the counter; counter = old - 1; (int16) old > 0 -> 1
	const auto old = static_cast<int16_t>(action.turnsUntilStateChange);
	action.turnsUntilStateChange = static_cast<uint16_t>(old - 1);
	if (old > 0)
	{
		return 1;
	}
	action.turnsUntilStateChange = 0;
	// The town's emergency -> SetTopState(242)
	if (const auto* town = TownOf(villager); town != nullptr && tq::IsInStateOfEmergency(*town))
	{
		TraceIf(villager, "sit 246: check -> emergency 242");
		SetTopState(villager, VillagerStates::GotoCongregateInTownAfterEmergency);
		return 1;
	}
	// CheckNeededForSomething (worship, civic, own desires)
	if (CheckNeededForSomething(villager) != 0)
	{
		TraceIf(villager, "sit 246: check -> something");
		return 1;
	}
	// GameRand(10) == 0 -> SetupNothingToDo, without going through 163
	const auto r = GameRand(10);
	if (r == 0)
	{
		TraceIf(villager, "sit 246: check -> nothing(r10=0)");
		SetupNothingToDo(villager);
		return 1;
	}
	// The counter = subsequentChillOutTime
	villager::TraceFormatted(villager, "sit 246: check -> again (r10={})", r);
	action.turnsUntilStateChange = InfoOf(villager).subsequentChillOutTime;
	return 1;
}

uint32_t EnterSitAndChillOut(LivingAction& action, [[maybe_unused]] VillagerStates final, [[maybe_unused]] VillagerStates next)
{
	// The counter = initialChillOutTime; 1
	action.turnsUntilStateChange = InfoOf(Entities().ToEntity(action)).initialChillOutTime;
	return 1;
}

uint32_t GoAndChilloutInTown(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// No town -> SetTopState(163)
	const auto townEntity = TownEntityOf(villager);
	if (townEntity == entt::null)
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// c = GetCongregationPos(); GetMeToMyChillOutPos(GetChillOutPos, c, the town's chill-out distance, c)
	const auto c = tq::GetCongregationPos(townEntity);
	GetMeToMyChillOutPos(villager, &GetChillOutPos, c, TownInfo().maxDistanceFromCongreationPosThatPeopleChillOut, &c);
	return 1;
}

uint32_t ChildFollowsMother(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// CheckChild, CheckNeededForTownDesire == 1, ChildGotoCreche -> 1
	if (CheckChild(villager) != 0 || CheckNeededForTownDesire(villager) == 1 || ChildGotoCreche(villager) != 0)
	{
		return 1;
	}
	// The mother if available (IsAvailable), else the abode; neither: CheckNeedNewAbode
	glm::ivec2 pos(0);
	const char* around = "mother";
	const auto* v = VillagerOf(villager);
	if (v != nullptr && v->mother != entt::null && villager::IsAvailable(v->mother))
	{
		pos = tq::PosOf(v->mother);
	}
	else if (const auto abode = AbodeOf(villager); abode != entt::null)
	{
		pos = tq::PosOf(abode);
		around = "home";
	}
	else
	{
		// CheckNeedNewAbode (a child: 0, it stays): a child with no mother and no abode stands still here
		// (literal)
		if (TraceOn(villager) && VillagerDecideDebugHooksData().childTraced.insert(object_index::Of(villager)).second)
		{
			Trace(villager, "child 114: no mother, no abode -> CheckNeedNewAbode (a child: 0, it stays)");
		}
		CheckNeedNewAbode(villager);
		return 1;
	}
	// pos += GetPosFromAngle(GameFloatRand(2 pi), 10 x 0.5)
	const float angle = GameFloatRand(glm::two_pi<float>());
	pos += tq::GetPosFromAngle(angle, 10.0f * 0.5f);
	// IsNavigable: Collide() & 2 (dry land) and not & 8. (approximate) openblack's Collide
	// (ecs::animal_ai::detail::Collides) knows water / land only: bit 8 is never set
	// (the tests have no land: navigable)
	const auto metres = tq::ToMetres(pos);
	if (Locator::terrainSystem::has_value() &&
	    (!animal_ai::detail::Collides(metres, 2) || animal_ai::detail::Collides(metres, 8)))
	{
		return 1;
	}
	// SetupMoveToWithHug(pos, 114)
	villager::TraceFormatted(villager, "child 114: around {} -> {}", around, Xz(pos));
	MoveTo(villager, pos, VillagerStates::ChildFollowsMother);
	return 1;
}

// ---- DecideWhatToDo's checks ---------------------------------------------------------------------------------------

uint32_t CheckNeededForSomething(entt::entity villager)
{
	// Homeless and CheckHomelessMoveIntoAbode -> 1
	if (AbodeOf(villager) == entt::null && CheckHomelessMoveIntoAbode(villager) != 0)
	{
		TraceIf(villager, "decide: homeless");
		return 1;
	}
	// CheckNeededForSpecial == 1
	return CheckNeededForSpecial(villager) == 1 ? 1 : 0;
}

// CheckHomelessMoveIntoAbode is in VillagerHome.cpp

uint32_t CheckNeededForSpecial(entt::entity villager)
{
	// CheckNeededForWorship (VillagerWorship.cpp, through the worship check service) == 1
	Log("worship");
	const bool worship = Locator::villagerWorshipCheck::value().WorshipCheck(villager);
	if (worship)
	{
		TraceIf(villager, "decide: worship");
		return 1;
	}
	// CheckNeededForCivic == 1
	Log("civic");
	if (CheckNeededForCivic(villager) == 1)
	{
		TraceIf(villager, "decide: civic");
		return 1;
	}
	// CheckSatisfyOwnDesire(ownDesireThreshold) == 1
	Log("own");
	return CheckSatisfyOwnDesire(villager, InfoOf(villager).ownDesireThreshold) == 1 ? 1 : 0;
}

uint32_t CheckNeededForCivic(entt::entity villager)
{
	// A town and CheckNeededForTownDesire == 1
	if (TownEntityOf(villager) == entt::null)
	{
		return 0;
	}
	return CheckNeededForTownDesire(villager) == 1 ? 1 : 0;
}

uint32_t CheckNeededForTownDesire(entt::entity villager)
{
	// No town -> 0
	if (TownEntityOf(villager) == entt::null)
	{
		return 0;
	}
	// The trigger
	const float trigger = GetOwnDesiresTrigger(villager);
	// The town desire's CheckVillagerNeededForTownDesire(this, trigger), the jobs' share-out (ECS/Town/TownDesire).
	// Declared void in the original, but it leaves 0 or 1 as its result, which the caller compares with 1. At night
	// Sleep (16) is first and CheckSatisfySleep sends the villagers with an abode to 36, which takes them in:
	// 37 -> 38 -> CheckSatisfySleep inside -> 119 -> 120
	const uint32_t result = town_desire::CheckVillagerNeededForTownDesire(TownEntityOf(villager), villager, trigger);
	// flags &= ~1 (the tap on its abode is forgotten), always with a town
	if (auto* v = VillagerOf(villager))
	{
		v->flags = static_cast<uint16_t>(v->flags & 0xFFFE);
	}
	return result;
}

float GetOwnDesiresTrigger(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0.0f;
	}
	// flags & 1 (after a tap on its abode) -> 0
	if ((v->flags & Villager::k_FlagAfterTapOnAbode) != 0)
	{
		return 0.0f;
	}
	// f = IsHungry ? GetDesireForFood : 0
	const float f = IsHungry(villager) ? GetDesireForFood(villager) : 0.0f;
	// l = GetDesireForLife - ownDesireThreshold when > 0, else 0
	const float threshold = InfoOf(villager).ownDesireThreshold;
	const float lifeDesire = GetDesireForLife(villager) - threshold;
	const float l = lifeDesire > 0.0f ? lifeDesire : 0.0f;
	// max(f, l) + 0.5 x min(f, l)
	const float high = f > l ? f : l;
	const float low = f < l ? f : l;
	const float t = low * 0.5f + high;
	// A child gets at least 0.11
	if (IsChild(villager) && t <= 0.11f)
	{
		return 0.11f;
	}
	// min(t, 1)
	return t < 1.0f ? t : 1.0f;
}

float GetDesireForLife(entt::entity villager)
{
	// GetLifeDesireFromLife(GetLife())
	return GetLifeDesireFromLife(villager, life::LifeOf(villager));
}

float GetLifeDesireFromLife(entt::entity villager, float life)
{
	// D = damageThresholdToGoHome; m = D < life ? D : life
	const float d = InfoOf(villager).damageThresholdToGoHome;
	const float m = d < life ? d : life;
	// x = (life - m) / (1 - D); 1 - x^2
	const float x = (life - m) / (1.0f - d);
	return 1.0f - x * x;
}

uint32_t CheckSatisfyOwnDesire(entt::entity villager, float trigger)
{
	// dF = GetDesireForFood - t, dL = GetDesireForLife - t
	const float dF = GetDesireForFood(villager) - trigger;
	const float dL = GetDesireForLife(villager) - trigger;
	villager::TraceFormatted(villager, "decide: own(dF {:.4f}, dL {:.4f})", dF, dL);
	// dF > dL and dF > 0: food first
	if (dF > dL && dF > 0.0f)
	{
		if (CheckSatisfyOwnFoodDesire(villager) != 0)
		{
			return 1;
		}
		return dL > 0.0f ? CheckSatisfySleep(villager) : 0;
	}
	// (dF <= dL, or dF <= 0) life first
	if (dL > 0.0f)
	{
		if (CheckSatisfySleep(villager) != 0)
		{
			return 1;
		}
		return dF > 0.0f ? CheckSatisfyOwnFoodDesire(villager) : 0;
	}
	return 0;
}

// CheckSatisfyOwnFoodDesire and ChangeStateToFindFoodToEat: VillagerFood.cpp

uint32_t CheckSatisfySleep(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// flags & 1 and life >= D -> 0
	if ((v->flags & Villager::k_FlagAfterTapOnAbode) != 0 &&
	    !(life::LifeOf(villager) < InfoOf(villager).damageThresholdToGoHome))
	{
		return 0;
	}
	// Inside its home (flags & 4): CheckWhenGoingToBed -> SetTopState(119 GOTO_BED_AT_HOME); 1
	if ((v->flags & Villager::k_FlagAtHome) != 0)
	{
		if (CheckWhenGoingToBed(villager) != 0)
		{
			SetTopState(villager, VillagerStates::GotoBedAtHome);
		}
		return 1;
	}
	// An abode -> SetTopState(36 GO_HOME); 1
	if (AbodeOf(villager) != entt::null)
	{
		TraceIf(villager, "decide: own sleep -> 36");
		SetTopState(villager, VillagerStates::GoHome);
		return 1;
	}
	// TOP 238 SLEEP_IN_TENT -> 1
	return GetState(villager, Index::Top) == VillagerStates::SleepInTent ? 1 : 0;
}

// CheckWhenGoingToBed: VillagerHome.cpp

uint32_t CheckTakeResourcesToStoragePit(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// Signed compare with minWoodToShowGraphic / minFoodToShowGraphic
	const auto& info = InfoOf(villager);
	const int32_t wood = v->resourceHeld.at(1);
	const int32_t food = v->resourceHeld.at(0);
	if (wood > static_cast<int32_t>(info.minWoodToShowGraphic) || food > static_cast<int32_t>(info.minFoodToShowGraphic))
	{
		// SetTopState(31 GOTO_STORAGE_PIT_FOR_DROP_OFF) (VillagerResources.cpp)
		villager::TraceFormatted(villager, "decide: resources (wood {}, food {}) -> 31", wood, food);
		SetTopState(villager, VillagerStates::GotoStoragePitForDropOff);
		return 1;
	}
	return 0;
}

// DiscipleDecideWhatToDo: VillagerDisciple.cpp

uint32_t ChildDecideWhatToDo(entt::entity villager)
{
	// CheckChild == 1
	if (CheckChild(villager) == 1)
	{
		TraceIf(villager, "decide: child CheckChild");
		return 1;
	}
	// CheckNeededForTownDesire == 1
	if (CheckNeededForTownDesire(villager) == 1)
	{
		return 1;
	}
	// ChildGotoCreche
	if (ChildGotoCreche(villager) != 0)
	{
		return 1;
	}
	// SetTopState(114 CHILD_FOLLOWS_MOTHER)
	TraceIf(villager, "decide: child -> 114");
	SetTopState(villager, VillagerStates::ChildFollowsMother);
	return 1;
}

uint32_t CheckChild(entt::entity villager)
{
	// Not a child -> GoHome (its result)
	if (!IsChild(villager))
	{
		return GoHome(villager);
	}
	// IsMotherAlive == 0 -> forget the mother
	if (IsMotherAlive(villager) == 0)
	{
		if (auto* v = VillagerOf(villager))
		{
			v->mother = entt::null;
		}
	}
	// Hungry -> GoHome
	if (IsHungry(villager))
	{
		return GoHome(villager);
	}
	return 0;
}

// IsMotherAlive and ChildGotoCreche: VillagerChild.cpp

// CheckNeedNewAbode: VillagerHome.cpp

// ---- the idle branch ---------------------------------------------------------------------------------------------

uint32_t SetupNothingToDo(entt::entity villager)
{
	// GetTown, GetAbode, GameRand(9)
	const auto townEntity = TownEntityOf(villager);
	const auto abode = AbodeOf(villager);
	uint32_t r = GameRand(9);
	if (const auto* forced = Entities().TryGet<const ForcedNothingRoll>(villager); forced != nullptr)
	{
		r = forced->roll;
		Entities().RemoveState<ForcedNothingRoll>(villager);
	}
	// The original's jump table {0, 1, 1, 1, 2, 2, 2, 2, 2} (> 8: SetTopState(36))
	static constexpr std::array<uint8_t, 9> k_Branch = {0, 1, 1, 1, 2, 2, 2, 2, 2};
	const int branch = r <= 8 ? k_Branch[r] : -1;
	std::string roll = fmt::format("decide: nothing r={}", r);
	if (branch == 0)
	{
		// A functional abode -> 36
		if (abode != entt::null && aq::IsFunctional(abode))
		{
			TraceIf(villager, roll + " -> 36");
			SetTopState(villager, VillagerStates::GoHome);
			return 1;
		}
		// GameRand(100) < 10 -> 36; else on to branch 1
		const auto r100 = GameRand(100);
		roll += fmt::format(" r100={}", r100);
		if (r100 < 10)
		{
			TraceIf(villager, roll + " -> 36");
			SetTopState(villager, VillagerStates::GoHome);
			return 1;
		}
	}
	if (branch == 0 || branch == 1)
	{
		// An abode (functional or not) -> 245; else on to branch 2
		if (abode != entt::null)
		{
			TraceIf(villager, roll + " -> 245");
			SetTopState(villager, VillagerStates::GoAndChilloutOutsideHome);
			return 1;
		}
	}
	if (branch >= 0)
	{
		// A town and GetChillOutPos -> SetupMoveToWithHug(pos, 246); 1 whatever it returns
		if (const auto pos = townEntity != entt::null ? GetChillOutPos(villager) : std::nullopt; pos.has_value())
		{
			TraceIf(villager, roll + " -> 246" + Xz(*pos));
			MoveTo(villager, *pos, VillagerStates::SitAndChillout);
			return 1;
		}
	}
	// SetTopState(36)
	TraceIf(villager, roll + " -> 36");
	SetTopState(villager, VillagerStates::GoHome);
	return 1;
}

std::optional<glm::ivec2> GetChillOutPos(entt::entity villager)
{
	const auto townEntity = TownEntityOf(villager);
	if (townEntity == entt::null)
	{
		return std::nullopt;
	}
	// c = GetCongregationPos
	const auto c = tq::GetCongregationPos(townEntity);
	// R = maxDistanceFromCongreationPosThatPeopleChillOut x 0.1
	const float r = TownInfo().maxDistanceFromCongreationPosThatPeopleChillOut * 0.1f;
	// Get3DAngleFromXZ(c, me)
	const float toMe = tq::Get3DAngleFromXZ(c, tq::PosOf(villager));
	// GameFloatRand(pi / 4) - pi / 8 + that
	const float angle = GameFloatRand(glm::quarter_pi<float>()) - 0.39269909f + toMe;
	// GameFloatRand(9 R) + R
	const float distance = GameFloatRand(r * 9.0f) + r;
	// c + GetPosFromAngle(angle, distance)
	return c + tq::GetPosFromAngle(angle, distance);
}

std::optional<glm::ivec2> GetPosOutsideMyHouse(entt::entity villager)
{
	// A town
	if (TownEntityOf(villager) == entt::null)
	{
		return std::nullopt;
	}
	// R' = maxDistanceFromHouseThatPeopleChillOut x 0.5
	const float r = TownInfo().maxDistanceFromHouseThatPeopleChillOut * 0.5f;
	// An abode
	const auto abode = AbodeOf(villager);
	if (abode == entt::null)
	{
		return std::nullopt;
	}
	// GetPosOutside(3, R', R')
	return aq::GetPosOutside(abode, 3.0f, r, r);
}

void GetMeToMyChillOutPos(entt::entity villager, ChillOutPosFn pmf, glm::ivec2 a, float r, const glm::ivec2* c)
{
	const auto me = tq::PosOf(villager);
	// dist = GetDistanceInMetres(A, me); dist <= R -> near
	const float distance = tq::GetDistanceInMetres(a, me);
	if (!(distance <= r))
	{
		// Far: pmf(tmp) -> SetupMoveToWithHug(tmp, GetFinalState); else nothing
		if (const auto tmp = pmf(villager); tmp.has_value())
		{
			villager::TraceFormatted(villager, "chill {}: far d={:.2f} R={:.2f} -> {}",
			                         static_cast<uint32_t>(GetFinalState(villager)), distance, r, Xz(*tmp));
			MoveTo(villager, *tmp, GetFinalState(villager));
		}
		return;
	}
	// tmp = me; radius = Get2DRadius x 1.2
	glm::ivec2 tmp = me;
	const float radius = tq::Get2DRadius(villager) * 1.2f;
	// CheckForClearArea(tmp, radius, IsObject, this)
	entt::entity blocker = entt::null;
	if (tq::CheckForClearArea(tmp, radius, &tq::IsObject, villager, &blocker))
	{
		// C -> LookAtPos(C, 2)
		if (c != nullptr)
		{
			LookAtPos(villager, *c, 2);
		}
		// SetTopState(246)
		villager::TraceFormatted(villager, "chill {}: near clear -> 246", static_cast<uint32_t>(GetFinalState(villager)));
		SetTopState(villager, VillagerStates::SitAndChillout);
		return;
	}
	// R + 5 < dist -> tmp += GetPosFromAngle(Get3DAngleFromXZ(tmp, A), 5). Dead code:
	// this branch only runs with dist <= R. (The original reads A here, not C)
	if (r + 5.0f < distance)
	{
		tmp += tq::GetPosFromAngle(tq::Get3DAngleFromXZ(tmp, a), 5.0f);
	}
	// FindClearArea(tmp, tmp, 5, 1, radius, IsObject, this) -> SetupMoveToWithHug(tmp, final)
	if (TraceOn(villager) && blocker != entt::null)
	{
		const auto& registry = Entities();
		const char* kind = registry.AllOf<Villager>(blocker) ? "villager" : registry.AllOf<Abode>(blocker) ? "abode" : "object";
		const auto at = tq::ToMetres(tq::PosOf(blocker));
		Trace(villager, fmt::format("chill: blocker {} at ({:.1f}, {:.1f})", object_index::Of(blocker), at.x, at.y));
		const float door =
		    registry.AllOf<Abode>(blocker) ? tq::GetDistanceInMetres(tq::PosOf(blocker), aq::GetArrivePos(blocker)) : -1.0f;
		Trace(villager,
		      fmt::format(
		          "chill {}: near blocked by {} {} (d {:.2f}, radius {:.2f}, its door {:.2f}, mine x 1.2 {:.2f}, R {:.2f})",
		          static_cast<uint32_t>(GetFinalState(villager)), kind, object_index::Of(blocker),
		          tq::GetDistanceInMetres(tq::PosOf(blocker), me), tq::Get2DRadius(blocker), door, radius, r));
	}
	if (const auto clear = tq::FindClearArea(tmp, 5.0f, 1.0f, radius, &tq::IsObject, villager); clear.has_value())
	{
		tmp = *clear;
		villager::TraceFormatted(villager, "chill {}: near blocked -> {}", static_cast<uint32_t>(GetFinalState(villager)),
		                         Xz(tmp));
		MoveTo(villager, tmp, GetFinalState(villager));
		return;
	}
	// pmf(tmp) -> SetupMoveToWithHug(tmp, final)
	if (const auto found = pmf(villager); found.has_value())
	{
		tmp = *found;
		villager::TraceFormatted(villager, "chill {}: near blocked, no clear area -> {}",
		                         static_cast<uint32_t>(GetFinalState(villager)), Xz(tmp));
		MoveTo(villager, tmp, GetFinalState(villager));
	}
}

// ---- test hooks --------------------------------------------------------------------------------------------------

void ForceNextNothingRoll(entt::entity villager, uint32_t r)
{
	Entities().AssignOrReplaceState<ForcedNothingRoll>(villager, r);
}
} // namespace openblack::ecs::villager
