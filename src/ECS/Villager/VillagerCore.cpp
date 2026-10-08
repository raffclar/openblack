/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCore.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <vector>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/VillagerRulesInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "Game.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"

// The villager's turn and its state changes (VillagerCore.h; docs/bw1-notes/villagers.md).

namespace openblack::ecs::villager
{
using namespace components;
using state_info::StateInfo;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

systems::LivingActionSystemInterface& System()
{
	return Locator::livingActionSystem::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

VillagerStates Raw(const LivingAction& action, Index index)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

uint32_t Number(VillagerStates state)
{
	return static_cast<uint32_t>(state);
}

std::string StateText(VillagerStates state)
{
	const auto n = std::min<size_t>(Number(state), k_VillagerStateStrings.size() - 1);
	return fmt::format("{} {}", Number(state), k_VillagerStateStrings.at(n));
}

/// The villager's town
Town* TownOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town))
	{
		return nullptr;
	}
	return Entities().TryGet<Town>(v->town);
}

/// The town's pulse (`Town::buildPulse`, "the town changed"), read by the disciple check of CheckEveryTime: written by
/// fields, scaffolds, the storage pit, workshops and a villager's death; also read by the town's turn and the idle
/// disciple's state
uint32_t TownField0x5E8(const Town& town)
{
	return town.buildPulse;
}

/// At the worship site. Its writers (a villager added to / removed from the worship site) are on the miracle side and
/// keep it in WorshipVillager::atSite until it moves into `flags`
bool AtWorshipSite(entt::entity villager, const Villager& v)
{
	if ((v.flags & Villager::k_FlagAtWorshipSite) != 0)
	{
		return true;
	}
	const auto* worship = Entities().TryGet<const WorshipVillager>(villager);
	return worship != nullptr && worship->atSite;
}

/// The object's unique key. (approximate) openblack has no unique key heap: the creation index stands for it
uint32_t UniqueId(entt::entity villager)
{
	return static_cast<uint32_t>(std::max<int64_t>(object_index::Of(villager), 0));
}
} // namespace

// ---- clock and random --------------------------------------------------------------------------------------------

uint32_t CurrentTurn()
{
	return game_clock::Turn();
}

uint32_t GameRand(uint32_t n)
{
	return game_random::GameRand(n);
}

float GameFloatRand(float x)
{
	return game_random::GameFloatRand(x);
}

// ---- data --------------------------------------------------------------------------------------------------------

const GVillagerInfo& InfoOf(entt::entity villager)
{
	if (const auto* info = ecs::VillagerInfoOf(villager); info != nullptr)
	{
		return *info;
	}
	return Locator::infoConstants::value().villager.at(0);
}

VillagerStates GetState(entt::entity villager, Index index)
{
	const auto* action = ActionOf(villager);
	return action != nullptr ? Raw(*action, index) : VillagerStates::InvalidState;
}

VillagerStates GetFinalState(entt::entity villager)
{
	// TOP if its row is final, else FINAL
	const auto top = GetState(villager, Index::Top);
	return state_info::IsFinal(StateInfo(top)) ? top : GetState(villager, Index::Final);
}

uint32_t GetAge(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? AgeFromBirthTurn(v->birthTurn, CurrentTurn()) : 0;
}

void SetAgeBirthTurn(entt::entity villager, uint32_t age, uint32_t turn)
{
	if (auto* v = VillagerOf(villager))
	{
		v->birthTurn = BirthTurnForAge(age, turn);
	}
}

bool IsChild(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->flags & Villager::k_FlagChild) != 0;
}

bool IsWoman(entt::entity villager)
{
	// info sex FEMALE && !IsChild
	return InfoOf(villager).sex == SexType::Female && !IsChild(villager);
}

bool IsHungry(entt::entity villager)
{
	// food <= hungryForFood
	const auto* v = VillagerOf(villager);
	return v != nullptr && v->food <= InfoOf(villager).hungryForFood;
}

uint32_t GetGameTurnsSinceLastChecked(entt::entity villager, uint32_t turn)
{
	// turn - lastCheckTurn (unsigned)
	const auto* v = VillagerOf(villager);
	return v != nullptr ? turn - v->lastCheckTurn : 0;
}

void SetGameTurnLastChecked(entt::entity villager, uint32_t turn)
{
	if (auto* v = VillagerOf(villager))
	{
		v->lastCheckTurn = turn;
	}
}

bool IsScriptControlled(entt::entity villager)
{
	// set when a script takes control of the thing (script_held: AddScriptThing, the release, and the vortex)
	return script_held::IsControlledByScript(villager);
}

float Power(float x)
{
	// m = min(x, 1); 1 - m * m * m
	const float m = x < 1.0f ? x : 1.0f;
	return 1.0f - m * m * m;
}

float GetDesireForFood(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return Power(v != nullptr ? v->food : 1.0f);
}

bool DiscipleIgnoresNeeds(uint8_t discipleType)
{
	// the disciple table's held-at-job field (VillagerDisciple.cpp, the one table). Types past the table: (guard)
	// record 0, no
	return DiscipleHeldAtJob(discipleType);
}

// ---- creation ----------------------------------------------------------------------------------------------------

bool RollSpecialVillager()
{
	// GameRand(10) <= 1 -> try a special villager; a special one that is made successfully is the result.
	// TODO: special villagers are not ported: a normal villager is always made
	return GameRand(10) <= 1;
}

uint32_t SetAge(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn,
                const std::function<void(uint32_t age)>& meshesAndScale)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return age;
	}
	// below grownUpAge a child, flags |= 8; else max(age, 18) and flags &= ~8
	if (age < info.grownUpAge)
	{
		v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagChild);
		v->lifeStage = Villager::LifeStage::Child;
	}
	else
	{
		age = std::max<uint32_t>(age, 18);
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagChild);
		v->lifeStage = Villager::LifeStage::Adult;
	}
	// the meshes and the scale for the age (FloatRand)
	if (meshesAndScale)
	{
		meshesAndScale(age);
	}
	// the birth turn
	SetAgeBirthTurn(villager, age, turn);
	return age;
}

void Construct(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn, bool inWater,
               const std::function<void(uint32_t age)>& meshesAndScale)
{
	auto* v = VillagerOf(villager);
	auto* action = ActionOf(villager);
	if (v == nullptr || action == nullptr)
	{
		return;
	}
	// clear the fields: flags, food, foodSpeedUp, resourceHeld, the building site, abode, discipleType, town,
	// lastCheckTurn, mother and the target = 0
	v->flags = 0;
	v->food = 0.0f;
	v->foodSpeedUp = 0;
	v->resourceHeld = {};
	v->abode = entt::null;
	v->discipleType = 0;
	v->town = entt::null;
	v->lastCheckTurn = 0;
	v->mother = entt::null;
	v->targetThing = entt::null;
	v->buildPosIndex = 0; // the original's union with the target
	v->buildingSite = entt::null;
	v->pregnancy = 0;
	// age < grownUpAge -> mother = 0 again (already 0 after the clearing)
	if (age < info.grownUpAge)
	{
		v->mother = entt::null;
	}
	SetAge(villager, info, age, turn, meshesAndScale);
	// foodSpeedUp = 0; a woman: pregnancy = 0
	v->foodSpeedUp = 0;
	if (info.sex == SexType::Female)
	{
		v->pregnancy = 0;
	}
	// min(1, GameFloatRand(0.6) + hungryForFood) is a macro that evaluates twice: a first draw below 1 draws again and
	// keeps the second unclamped (0.5 .. 1.1)
	const float first = GameFloatRand(0.6f) + info.hungryForFood;
	if (first < 1.0f)
	{
		const float second = GameFloatRand(0.6f);
		v->food = second + info.hungryForFood;
	}
	else
	{
		v->food = 1.0f;
	}
	// lastCheckTurn = turn - (GameRand(processChecksEvery) < turn ? GameRand(...) : turn)
	uint32_t back = turn;
	if (GameRand(info.processChecksEvery) < turn)
	{
		back = GameRand(info.processChecksEvery);
	}
	v->lastCheckTurn = turn - back;
	// the state counter = GameRand(500) + 1, before the water test
	action->turnsUntilStateChange = static_cast<uint16_t>(GameRand(500) + 1);
	// in water -> SetState(0, 16 DROWNING), else SetState(0, 85 CREATED): SetState only (no entry, clips or speed;
	// nothing of the water's: the drowning state does the rest)
	SetState(villager, Index::Top, inWater ? VillagerStates::Drowning : VillagerStates::Created);
	// the world population: openblack counts the Villager entities that are not counted out
	// (magic::players::WorldPopulation, villager::IsCountedOut)
	// SetSkeleton (VillagerDeath.h). (pending) not called here: the archetype has no skeleton argument and assigns the
	// Mesh after Construct, and SetSkeleton's scale for the age draws GameFloatRand once more, which would move every
	// later draw; to be done with VillagerArchetype's draw order
	if (TraceOn(villager))
	{
		const auto* transform = Entities().TryGet<const Transform>(villager);
		const glm::vec3 at = transform != nullptr ? transform->position : glm::vec3(0.0f);
		Trace(villager,
		      fmt::format("created at ({:.1f}, {:.1f}, {:.1f}): age {} food {:.4f} lastCheckTurn {} counter {} state {}{}",
		                  at.x, at.y, at.z, GetAge(villager), v->food, v->lastCheckTurn, action->turnsUntilStateChange,
		                  StateText(GetState(villager, Index::Top)), inWater ? " (born in water)" : ""));
	}
}

// ---- the turn ----------------------------------------------------------------------------------------------------

void ProcessReaction(entt::entity villager)
{
	// the slot types. TODO(reactions): the miracle maps (fire, teleport, shield, death) still end
	// through their own states and validate only
	villager_reactions::ProcessSlotReaction(villager);
}

void ProcessFoodSpeedup(entt::entity villager, uint32_t turn)
{
	// foodSpeedUp != 0 && turn % 10 == 0 -> --foodSpeedUp
	auto* v = VillagerOf(villager);
	if (v != nullptr && v->foodSpeedUp != 0 && turn % 10 == 0)
	{
		--v->foodSpeedUp;
	}
}

uint32_t ProcessState(entt::entity villager, uint32_t turn)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// one more turn in the state (u16)
	++action->turnsSinceStateChange;
	ProcessFoodSpeedup(villager, turn);
	// the validate function of TOP and of the raw FINAL, if any; the result is unused
	System().VillagerCallValidate(*action, Index::Top);
	System().VillagerCallValidate(*action, Index::Final);
	// flags & 0x800 -> once ready for a new animation, finish the into / out-of animation; return 1 (no
	// CheckEveryTime, no state). The flags 0x800 / 0x1000 live in SkeletalAnimation::transitionFlags
	if (ecs::VillagerWaitsForTransition(villager, action->turnsSinceStateChange))
	{
		return 1;
	}
	// (the result is unused). A death there (VillagerDead -> SetDying: TOP 14) keeps the villager, and the state call
	// runs the new TOP's function (dying) this same turn
	CheckEveryTime(villager, turn);
	// a zombie (marked there) still runs its state this turn, as the original: no test between the checks and the state
	if (!Entities().Valid(villager))
	{
		return 1;
	}
	// the TOP's state function
	auto* again = ActionOf(villager);
	if (again == nullptr)
	{
		return 1;
	}
	return System().VillagerCallState(*again, Index::Top);
}

uint32_t CheckEveryTime(entt::entity villager, uint32_t turn)
{
	auto* action = ActionOf(villager);
	auto* v = VillagerOf(villager);
	if (action == nullptr || v == nullptr)
	{
		return 1;
	}
	// controlled by a script -> 1
	if (IsScriptControlled(villager))
	{
		return 1;
	}
	const auto& info = InfoOf(villager);
	const GVillagerStateTableInfo* st = &StateInfo(Raw(*action, Index::Top));
	// the life's wear (life::ReduceLife)
	if (state_info::IsMoving(*st))
	{
		// a moving TOP wears the TOP's drain, and from here on `st` is the raw FINAL's row (not GetFinalState)
		life::ReduceLife(villager, state_info::LifeDrainPerTurn(*st));
		st = &StateInfo(Raw(*action, Index::Final));
	}
	else
	{
		// GetFinalState's drain; `st` stays the TOP's
		life::ReduceLife(villager, state_info::LifeDrainPerTurn(StateInfo(GetFinalState(villager))));
	}
	// the periodic checks of the state
	if (state_info::DoPeriodicChecks(*st))
	{
		// life == 0 exactly
		if (v->life == 0.0f)
		{
			// final 248, 249, 250 (from worship) or at the worship site -> CHANT, else EXHAUSTION
			const auto final = GetFinalState(villager);
			const bool chant = final == VillagerStates::GoHomeFromWorship || final == VillagerStates::ArrivesHomeFromWorship ||
			                   final == VillagerStates::SleepInTentFromWorship || AtWorshipSite(villager, *v);
			// VillagerDead(reason, the villager's player, 0.0, 1)
			VillagerDead(villager, chant ? DeathReason::Chant : DeathReason::Exhaustion, GetPlayerOf(villager), 0.0f, 1);
			return 1;
		}
		// turns since the last check > processChecksEvery, strictly
		const uint32_t turns = GetGameTurnsSinceLastChecked(villager, turn);
		if (turns > info.processChecksEvery)
		{
			if (TraceOn(villager))
			{
				Trace(villager,
				      fmt::format("check: turn {} t {} life {:.6f} food {:.4f} top {} final {}", turn, turns, v->life, v->food,
				                  StateText(Raw(*action, Index::Top)), StateText(Raw(*action, Index::Final))));
			}
			// (turn + UniqueId) % 800 < t (unsigned)
			if ((turn + UniqueId(villager)) % 800 < turns)
			{
				if (CheckDeathFromOldAge(villager))
				{
					return 1;
				}
			}
			// hurt -> GO_HOME, and on: life < damageThresholdToGoHome, not at home (flags & 4), the state goes home when
			// hurt, not downed (components::DownedVillager), the state does not forbid it, and not reacting to food
			// (rows 19 / 20) unless food > hungryForFood
			const auto& table = Locator::infoConstants::value().villagerStateTable;
			const bool foodReaction = st == &table.at(static_cast<size_t>(VillagerStates::GotoFoodReaction)) ||
			                          st == &table.at(static_cast<size_t>(VillagerStates::ArrivesAtFoodReaction));
			if (v->life < info.damageThresholdToGoHome && (v->flags & Villager::k_FlagAtHome) == 0 &&
			    state_info::GoHomeWhenHurt(*st) && !Entities().AllOf<DownedVillager>(villager) &&
			    !state_info::NoGoHomeWhenHurt(*st) && (!foodReaction || v->food > info.hungryForFood))
			{
				// SetTopState(36 GO_HOME): the go-home state walks to the abode's door (VillagerHome.cpp); its arrival 37
				// goes in and 38 / 119 / 120 rest. A test may switch the rule off (villager rules)
				if (Locator::villagerRules::value().GoHomeEnabled())
				{
					if (TraceOn(villager))
					{
						Trace(villager, fmt::format("hurt (life {:.4f}): GO_HOME", v->life));
					}
					SetTopState(villager, VillagerStates::GoHome);
				}
				else if (TraceOn(villager))
				{
					Trace(villager, fmt::format("hurt (life {:.4f}): GO_HOME off", v->life));
				}
			}
			if (IsChild(villager))
			{
				CheckChildGrownUp(villager);
			}
			if (IsWoman(villager))
			{
				UpdatePregnancy(villager);
			}
			return CheckHungry(villager, turn) ? 1 : 0;
		}
	}
	// a disciple (flags & 0x200) of a type that ignores the needs, with the raw FINAL 221 DISCIPLE_NOTHING_TO_DO, and a
	// town whose pulse is set -> DECIDE_WHAT_TO_DO
	if ((v->flags & Villager::k_FlagDisciple) != 0 && DiscipleIgnoresNeeds(v->discipleType) &&
	    Raw(*action, Index::Final) == VillagerStates::DiscipleNothingToDo)
	{
		if (const auto* town = TownOf(villager); town != nullptr && TownField0x5E8(*town) != 0)
		{
			SetTopState(villager, VillagerStates::DecideWhatToDo);
		}
	}
	return 1;
}

// ---- state changes -----------------------------------------------------------------------------------------------

bool IsStateExitFunctionSameAs(entt::entity villager, VillagerStates next)
{
	// GetFinalState's row's exit function and next's, compared whole (the first 0 -> equal at once) -> 1
	const auto exitOf = [](VillagerStates s) {
		const auto i = static_cast<size_t>(s);
		return i < k_OriginalStateFns.size() ? k_OriginalStateFns.at(i).exit : 0u;
	};
	if (exitOf(GetFinalState(villager)) == exitOf(next))
	{
		return true;
	}
	// next a final state -> 0, else 1
	return !state_info::IsFinal(StateInfo(next));
}

bool IsAvailableForReaction(entt::entity villager)
{
	// the part ported: not held or thrown, and the final
	// state takes reactions
	const auto top = GetState(villager, Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::InHand)
	{
		return false;
	}
	return StateInfo(GetFinalState(villager)).availableForReaction != 0;
}

bool IsAvailableForReaction(entt::entity villager, Reaction type)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || ActionOf(villager) == nullptr)
	{
		return false;
	}
	// at the worship site -> 0
	const auto final = GetFinalState(villager);
	if (AtWorshipSite(villager, *v))
	{
		return false;
	}
	// the final state takes no reactions -> 0
	if (StateInfo(final).availableForReaction == 0)
	{
		return false;
	}
	// football / script -> 0
	if ((v->flags & Villager::k_FlagFootball) != 0)
	{
		return false;
	}
	// a death state final (13 <= s <= 17, unsigned) -> 0
	const auto f = Number(final);
	if (f >= 13 && f <= 17)
	{
		return false;
	}
	// life above lifeWhenCrawlsWounded goes on; at or below (or NaN) only type 7 REACT_TO_FOOD goes on
	if (!(life::LifeOf(villager) > InfoOf(villager).lifeWhenCrawlsWounded) && type != Reaction::ReactToFood)
	{
		return false;
	}
	// the living checks. Functional: available and TOP not 13 / 14
	const auto top = Number(GetState(villager, Index::Top));
	if (!IsAvailable(villager) || top == 13 || top == 14)
	{
		return false;
	}
	// a script controls it -> 0
	if (IsScriptControlled(villager))
	{
		return false;
	}
	// used in the dance editor (dancing and editing a dance): TODO(dance), never in openblack
	// the final state 15, 16, 17, 18, 14, 23 or 5 -> 0
	if (f == 15 || f == 16 || f == 17 || f == 18 || f == 14 || f == 23 || f == 5)
	{
		return false;
	}
	// dead -> 0
	if ((v->status & Villager::k_StatusDead) != 0)
	{
		return false;
	}
	// on a structure (only moving on a structure sets it, not ported) -> 0
	return true;
}

void SetTargetThing(entt::entity villager, entt::entity thing)
{
	if (auto* v = VillagerOf(villager); v != nullptr)
	{
		v->targetThing = thing;
	}
}

entt::entity GetTargetThing(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? v->targetThing : entt::entity(entt::null);
}

bool CanPauseForASecond(entt::entity villager, VillagerStates state)
{
	// TOP != 239 && the state can pause && not controlled by a script
	return GetState(villager, Index::Top) != VillagerStates::PauseForASecond &&
	       state_info::CanPauseForASecond(StateInfo(state)) && !IsScriptControlled(villager);
}

uint32_t SetTopState(entt::entity villager, VillagerStates state)
{
	if (ActionOf(villager) == nullptr)
	{
		return 0;
	}
	const auto before = GetState(villager, Index::Top);
	if (CanPauseForASecond(villager, state))
	{
		// x = 1 - life (life * 0.5 if poisoned); x^3
		const auto* v = VillagerOf(villager);
		float x = v != nullptr ? v->life : 0.0f;
		if (Entities().AllOf<Poisoned>(villager))
		{
			x *= 0.5f;
		}
		x = 1.0f - x;
		const float x3 = x * x * x;
		// GameFloatRand(1) - 0.5 * x^3 < pauseForASecondChance
		const float r = GameFloatRand(1.0f);
		const float threshold = InfoOf(villager).pauseForASecondChance;
		if (r - 0.5f * x3 < threshold)
		{
			if (TraceOn(villager))
			{
				Trace(villager, fmt::format("pause 239 -> {} (rand {:.4f}, threshold {:.4f})", StateText(state), r,
				                            threshold + 0.5f * x3));
			}
			// SetupPauseForASecond; 0 -> on to EnterTopState
			if (SetupPauseForASecond(villager, state) != 0)
			{
				return k_Done;
			}
		}
	}
	const auto result = EnterTopState(villager, state);
	if (result == k_EntryRefused)
	{
		// CallEntryStateFunction(163), its result unused
		CallEntryStateFunction(villager, VillagerStates::DecideWhatToDo);
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("SetTopState {} -> {} = {:#x}", StateText(before), StateText(state), result));
	}
	return result;
}

uint32_t EnterTopState(entt::entity villager, VillagerStates state)
{
	// the exit refuses -> k_ExitRefused, nothing changed
	if (CallExitStateFunction(villager, state) == 0)
	{
		return k_ExitRefused;
	}
	// the out-of clip: may set the flags 0x1800 already
	const auto out = ecs::VillagerCallOutOfAnimation(villager, state);
	// the entry refuses -> k_EntryRefused (the flags and the out-of clip stay touched: literal)
	if (CallEntryStateFunction(villager, state) == 0)
	{
		return k_EntryRefused;
	}
	// the state's speed and the clips
	ecs::VillagerApplyStateClips(villager, state, out);
	return k_Done;
}

uint32_t SetCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination)
{
	const auto before = GetState(villager, Index::Top);
	const auto result = ApplyCurrentAndDestinationState(villager, current, destination);
	if (result == k_EntryRefused)
	{
		// CallEntryStateFunction(163)
		CallEntryStateFunction(villager, VillagerStates::DecideWhatToDo);
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("SetCurrentAndDestinationState {} -> {}, {} = {:#x}", StateText(before), StateText(current),
		                            StateText(destination), result));
	}
	return result;
}

uint32_t ApplyCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination)
{
	if (ActionOf(villager) == nullptr)
	{
		return 0;
	}
	// the exit told d
	if (CallExitStateFunction(villager, destination) == 0)
	{
		return k_ExitRefused;
	}
	// the out-of clip for d
	const auto out = ecs::VillagerCallOutOfAnimation(villager, destination);
	// CallEntryStateFunction(c, d)
	if (CallEntryStateFunction(villager, current, destination) == 0)
	{
		return k_EntryRefused;
	}
	// the state's speed and the clips, the into function told d
	ecs::VillagerApplyStateClips(villager, destination, out);
	return k_Done;
}

void SetState(entt::entity villager, Index index, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// PREVIOUS never keeps a state marked as not stored as previous
	if (index == Index::Previous && state_info::NotStoredAsPrevious(StateInfo(state)))
	{
		return;
	}
	const auto old = Raw(*action, index);
	// the old one of any index, if final and not 0, leaves the town's modifiers
	if (state_info::IsFinal(StateInfo(old)) && old != VillagerStates::InvalidState)
	{
		AdjustTownModifier(villager, old, false);
	}
	// the new one, if final and not 0, enters them. Also for PREVIOUS (literal: the original counts a stored state as
	// served)
	if (state_info::IsFinal(StateInfo(state)) && state != VillagerStates::InvalidState)
	{
		AdjustTownModifier(villager, state, true);
	}
	// setting TOP clears FINAL first (SetState(1, 0), with its own adjustment)
	if (index == Index::Top)
	{
		SetState(villager, Index::Final, VillagerStates::InvalidState);
	}
	// states[index] = s; index 0 resets turnsSinceStateChange
	action->states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	if (index == Index::Top)
	{
		action->turnsSinceStateChange = 0;
	}
	if (TraceOn(villager) && old != state)
	{
		Trace(villager, fmt::format("SetState {} {} -> {}", LivingAction::k_IndexStrings.at(static_cast<size_t>(index)),
		                            StateText(old), StateText(state)));
	}
}

void AdjustTownModifier(entt::entity villager, VillagerStates state, bool entering)
{
	auto* town = TownOf(villager);
	if (town == nullptr)
	{
		return;
	}
	// the state's served desire, -1 none
	const auto& row = StateInfo(state);
	const int desire = state_info::ServedDesire(row);
	if (desire == -1)
	{
		return;
	}
	// (approximate) a guard: info.dat has desires 0..16 only
	if (desire < 0 || static_cast<size_t>(desire) >= town->desire.doingNow.size())
	{
		return;
	}
	// k = entering ? 1 : -1; doingNow[d] += k * the served amount; doingNowCount[d] += k
	const float k = entering ? 1.0f : -1.0f;
	town->desire.doingNow.at(static_cast<size_t>(desire)) += k * state_info::ServedDesireAmount(row);
	town->desire.doingNowCount.at(static_cast<size_t>(desire)) += k;
	// the original's debug print -> the trace
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("AdjustTownModifier {} desire {} {:+.0f}: doingNow {:.3f} count {:.0f}", StateText(state),
		                            desire, k, town->desire.doingNow.at(static_cast<size_t>(desire)),
		                            town->desire.doingNowCount.at(static_cast<size_t>(desire))));
	}
}

uint32_t CallExitStateFunction(entt::entity villager, VillagerStates next)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// the raw TOP and GetFinalState, read first
	const auto top = Raw(*action, Index::Top);
	const auto final = GetFinalState(villager);
	// exit[top](next), else 1
	const auto a = System().VillagerCallExit(*action, top, next);
	// and exit[final](next) when final != top
	uint32_t b = 1;
	if (top != final)
	{
		if (auto* still = ActionOf(villager); still != nullptr)
		{
			b = System().VillagerCallExit(*still, final, next);
		}
	}
	// 1 only if both are 1
	return a == 1 && b == 1 ? 1 : 0;
}

uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// GetFinalState first
	const auto final = GetFinalState(villager);
	// entry[s](final, s), else 1
	const auto result = System().VillagerCallEntry(*action, state, final, state);
	if (result == 1)
	{
		// SetState(0, s)
		SetState(villager, Index::Top, state);
		return 1;
	}
	// k_EntryNoSet -> 1 (the function set the states), else 0
	return result == k_EntryNoSet ? 1 : 0;
}

uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates current, VillagerStates destination)
{
	// GetFinalState before the first entry
	const auto final = GetFinalState(villager);
	// CallEntryStateFunction(c) -> 0: 0
	if (CallEntryStateFunction(villager, current) == 0)
	{
		return 0;
	}
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	// entry[d](final, d), else 1
	const auto result = System().VillagerCallEntry(*action, destination, final, destination);
	if (result == 1)
	{
		// SetState(1, d)
		SetState(villager, Index::Final, destination);
		return 1;
	}
	// k_EntryNoSet -> 1; else 0 (TOP stays c, FINAL 0: SetCurrentAndDestinationState's k_EntryRefused then enters 163)
	return result == k_EntryNoSet ? 1 : 0;
}

uint32_t SetupPauseForASecond(entt::entity villager, VillagerStates state)
{
	// SetCurrentAndDestinationState(239, s) == 1
	return SetCurrentAndDestinationState(villager, VillagerStates::PauseForASecond, state) == 1 ? 1 : 0;
}

void SetTopStateToFinal(entt::entity villager)
{
	// SetTopState(the raw FINAL)
	SetTopState(villager, GetState(villager, Index::Final));
}

uint32_t SetupWaitForCounter(entt::entity villager, uint16_t turns, VillagerStates final)
{
	// SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final); anything but 1 -> 0 with the counter untouched
	if (SetCurrentAndDestinationState(villager, VillagerStates::WaitForCounter, final) != 1)
	{
		return 0;
	}
	// the state counter (u16) = the count
	if (auto* action = Entities().TryGet<LivingAction>(villager); action != nullptr)
	{
		action->turnsUntilStateChange = turns;
	}
	return 1;
}

uint32_t WaitForCounter(LivingAction& action)
{
	// one turn off the counter and, once it is not above 0 (signed), SetTopStateToFinal. Always 1
	--action.turnsUntilStateChange;
	if (static_cast<int16_t>(action.turnsUntilStateChange) <= 0)
	{
		SetTopStateToFinal(Entities().ToEntity(action));
	}
	return 1;
}

uint32_t SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto& registry = Entities();
	if (!registry.AllOf<LivingAction, WallHug>(villager))
	{
		return 0;
	}
	// SetCurrentAndDestinationState with the info's moveState (1 MOVE_TO_POS in all 63 villager rows of info.dat) and
	// final first (TOP, whose SetState clears FINAL, then FINAL)
	const auto moveState = static_cast<VillagerStates>(static_cast<uint8_t>(InfoOf(villager).moveState));
	if (SetCurrentAndDestinationState(villager, moveState, final) != 1)
	{
		return 0;
	}
	// the walk: the goal, a fresh step and a LINEAR move (openblack's WallHug)
	auto& wallHug = registry.Get<WallHug>(villager);
	wallHug.goal = goal;
	wallHug.step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	registry.Assign<MoveStateLinearTag>(villager);
	return 1;
}

uint32_t LookAtPos(entt::entity villager, glm::ivec2 pos, uint32_t mode)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	auto* transform = registry.TryGet<Transform>(villager);
	if (wallHug == nullptr || transform == nullptr)
	{
		return 0;
	}
	// the step: mode 0 -> 0x40, 1 -> 0x80, 2 -> 0x100, else the mode itself
	const int32_t maxStep = mode == 0 ? 0x40 : mode == 1 ? 0x80 : mode == 2 ? 0x100 : static_cast<int32_t>(mode);
	// the angle from the villager to pos
	const auto me = town_queries::PosOf(villager);
	const int32_t target = town_queries::GetAngleFromXZ(me, pos);
	// the current game angle (u16). (approximate) WallHug::yAngle in radians
	const auto current = static_cast<int32_t>(
	    static_cast<uint32_t>(std::lround(static_cast<double>(wallHug->yAngle) * 2048.0 / (2.0 * 3.14159265358979323846))) &
	    0x7FF);
	// villager::SetGameAngle (one copy)
	const auto setGameAngle = [villager](int32_t angle) { SetGameAngle(villager, static_cast<uint16_t>(angle & 0x7FF)); };
	// d = target - current; |d| < step -> SetGameAngle(target); 1
	const int32_t d = target - current;
	if (std::abs(d) < maxStep)
	{
		setGameAngle(target);
		return 1;
	}
	// d > 0: d < 0x400 (unsigned) -> +step, else -step; d <= 0: |d| < 0x400 -> -step, else +step;
	// & 0x7FF; 0
	bool up;
	if (d > 0)
	{
		up = static_cast<uint32_t>(d) < 0x400;
	}
	else
	{
		up = !(static_cast<uint32_t>(std::abs(d)) < 0x400);
	}
	setGameAngle(up ? current + maxStep : current - maxStep);
	return 0;
}

uint16_t GetGameAngle(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	if (wallHug == nullptr)
	{
		return 0;
	}
	// (u16). (approximate) WallHug::yAngle in radians, rounded to 2048ths (as LookAtPos reads it)
	return static_cast<uint16_t>(
	    static_cast<uint32_t>(std::lround(static_cast<double>(wallHug->yAngle) * 2048.0 / (2.0 * 3.14159265358979323846))) &
	    0x7FF);
}

void SetGameAngle(entt::entity villager, uint16_t angle)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	auto* transform = registry.TryGet<Transform>(villager);
	if (wallHug == nullptr || transform == nullptr)
	{
		return;
	}
	SetGameAngle(*transform, *wallHug, angle);
}

void SetGameAngle(Transform& transform, WallHug& wallHug, uint16_t angle)
{
	// the angle and the drawn rotation from ConvertGameAngleTo3D(a). (approximate) the original stores the whole u16 and
	// converts it; openblack keeps no separate game angle, and the & 0x7FF only folds an angle >= 2048 (GetAngleFromXZ
	// gives none)
	const auto a = static_cast<uint16_t>(angle & 0x7FF);
	wallHug.yAngle = gutils::ConvertGameAngleTo3D(a);
	animal_ai::detail::FaceAngle(transform, a);
}

void SetYAngle(entt::entity villager, float angle)
{
	auto& registry = Entities();
	// the drawn rotation (set when it changes), drawn at angle + pi / 2 (FaceAngle's convention)
	if (auto* transform = registry.TryGet<Transform>(villager); transform != nullptr)
	{
		transform->rotation = affine::AngleY(angle + glm::half_pi<float>());
	}
	// game angle = ConvertAngle3DToGame(a) (u16). (approximate) openblack keeps the game angle as WallHug::yAngle in
	// radians (GetGameAngle rounds it back): the game angle's ConvertGameAngleTo3D, so GetGameAngle returns it exactly
	if (auto* wallHug = registry.TryGet<WallHug>(villager); wallHug != nullptr)
	{
		wallHug->yAngle = gutils::ConvertGameAngleTo3D(static_cast<int32_t>(gutils::ConvertAngle3DToGame(angle)));
	}
}

uint32_t VillagerCreated(LivingAction& action)
{
	// v = counter; counter = v - 1 (u16); v == 0 -> counter = 0, SetTopState(163). Returns 1
	const auto v = action.turnsUntilStateChange;
	action.turnsUntilStateChange = static_cast<uint16_t>(v - 1);
	if (v == 0)
	{
		action.turnsUntilStateChange = 0;
		SetTopState(Entities().ToEntity(action), VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t PauseForASecond(LivingAction& action)
{
	// ready for a new animation -> SetTopStateToFinal. Returns 1
	const auto villager = Entities().ToEntity(action);
	if (ecs::VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		SetTopStateToFinal(villager);
	}
	return 1;
}

// ---- the periodic checks are in VillagerFood.cpp and VillagerAge.cpp --------------------------------------------

// ---- death: VillagerDeath.cpp ---------------------------------------------------------------------------------------
} // namespace openblack::ecs::villager
