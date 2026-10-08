/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerDeath.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

namespace openblack::ecs::components
{
struct Transform;
struct WallHug;
} // namespace openblack::ecs::components

// The core of the villager's state machine (docs/bw1-notes/villagers.md): the turn (ProcessState, CheckEveryTime), the
// state changes (SetTopState, SetCurrentAndDestinationState, SetState, the exit and entry calls) with their return
// codes, the creation (RollSpecialVillager, Construct), CREATED (85) and PAUSE_FOR_A_SECOND (239). The rows of the state
// table are reached through LivingActionSystemInterface, so a test can put a fake one in the Locator.

namespace openblack::ecs::villager
{
using Index = components::LivingAction::Index;

/// The original's return codes
inline constexpr uint32_t k_Done = 1;            ///< done / accepted
inline constexpr uint32_t k_EntryNoSet = 0x23;   ///< an entry function accepted and set the states itself
inline constexpr uint32_t k_ExitRefused = 0x2E;  ///< an exit function refused the change: nothing changed
inline constexpr uint32_t k_EntryRefused = 0x2F; ///< the entry function refused (Villager then enters 163)
/// k_TurnsPerYear (1500) is in VillagerAge.h

// ---- clock and random --------------------------------------------------------------------------------------------

/// The game turn, from the game clock
[[nodiscard]] uint32_t CurrentTurn();
/// 0 .. n - 1 (0 for 0); forwards to game_random::GameRand
[[nodiscard]] uint32_t GameRand(uint32_t n);
/// 0 for 0, signed like x; forwards to game_random::GameFloatRand
[[nodiscard]] float GameFloatRand(float x);

// ---- data --------------------------------------------------------------------------------------------------------

/// The villager's info.dat entry; villager[0] if openblack has none for it (never in the game)
[[nodiscard]] const GVillagerInfo& InfoOf(entt::entity villager);
[[nodiscard]] VillagerStates GetState(entt::entity villager, Index index);
/// TOP if it is a final state, else FINAL
[[nodiscard]] VillagerStates GetFinalState(entt::entity villager);
/// (turn - birthTurn) / 1500, unsigned (AgeFromBirthTurn)
[[nodiscard]] uint32_t GetAge(entt::entity villager);
/// birthTurn = turn - age * 1500 (BirthTurnForAge)
void SetAgeBirthTurn(entt::entity villager, uint32_t age, uint32_t turn);
/// flags & 8
[[nodiscard]] bool IsChild(entt::entity villager);
/// info sex FEMALE and not a child
[[nodiscard]] bool IsWoman(entt::entity villager);
/// food <= info.hungryForFood
[[nodiscard]] bool IsHungry(entt::entity villager);
/// The turns since the last periodic check, and marking it done
[[nodiscard]] uint32_t GetGameTurnsSinceLastChecked(entt::entity villager, uint32_t turn);
void SetGameTurnLastChecked(entt::entity villager, uint32_t turn);
/// Controlled by a script (ecs::script_held)
[[nodiscard]] bool IsScriptControlled(entt::entity villager);
/// 1 - min(x, 1)^3
[[nodiscard]] float Power(float x);
/// Power(food)
[[nodiscard]] float GetDesireForFood(entt::entity villager);
/// A working disciple held at its job (VillagerDisciple.h DiscipleHeldAtJob), which does not leave it for its needs
[[nodiscard]] bool DiscipleIgnoresNeeds(uint8_t discipleType);

// ---- creation ----------------------------------------------------------------------------------------------------

/// The creation's first draw: GameRand(10) <= 1 tries a special villager. TODO: no special villagers yet, so a
/// normal one is always made; returns whether the original would have tried.
bool RollSpecialVillager();
/// A child below grownUpAge (flags |= 8), else an adult of at least 18 (flags &= ~8); lifeStage mirrors the bit; then
/// the meshes and scale (`meshesAndScale`, which draws FloatRand) and the birth turn. Returns the age set.
uint32_t SetAge(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn,
                const std::function<void(uint32_t age)>& meshesAndScale = {});
/// The villager's construction, once the entity has its Villager (with life = info.life) and a LivingAction in state 0:
/// the fields cleared, SetAge (`meshesAndScale` sets the meshes and scale, drawing FloatRand), food, lastCheckTurn, the
/// state counter and the water rule: SetState(TOP, inWater ? 16 DROWNING : 85 CREATED) without entry, clips or speed.
void Construct(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn, bool inWater,
               const std::function<void(uint32_t age)>& meshesAndScale);

// ---- the turn ----------------------------------------------------------------------------------------------------

/// villager_reactions::ProcessSlotReaction for the slot types (7 / 12); nothing for the miracle maps yet
/// (TODO(reactions))
void ProcessReaction(entt::entity villager);
/// One turn: the validate calls, CheckEveryTime and the TOP state's function
uint32_t ProcessState(entt::entity villager, uint32_t turn);
/// Every turn: the life's wear, the periodic checks and the idle disciple's new decision
uint32_t CheckEveryTime(entt::entity villager, uint32_t turn);
/// One foodSpeedUp off every 10 turns
void ProcessFoodSpeedup(entt::entity villager, uint32_t turn);

// ---- state changes -----------------------------------------------------------------------------------------------

/// The pause, then EnterTopState; k_EntryRefused -> CallEntryStateFunction(163)
uint32_t SetTopState(entt::entity villager, VillagerStates state);
/// Exit, out-of clip, entry, speed and clips. k_Done, k_ExitRefused or k_EntryRefused
uint32_t EnterTopState(entt::entity villager, VillagerStates state);
/// ApplyCurrentAndDestinationState; k_EntryRefused -> CallEntryStateFunction(163)
uint32_t SetCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination);
/// The exit, the out-of clip and the into clip all get the destination `d`; only the entry gets both (c, d).
/// So: out = VillagerCallOutOfAnimation(e, d); ...; VillagerApplyStateClips(e, d, out), whose into function is the
/// TOP's (= c after the entry) with (1, d).
uint32_t ApplyCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination);
/// One index, with the town's modifiers; setting TOP clears FINAL
void SetState(entt::entity villager, Index index, VillagerStates state);
/// +-the state's served desire in the town
void AdjustTownModifier(entt::entity villager, VillagerStates state, bool entering);
/// TOP's and (if different) the final state's exit, told `next`
uint32_t CallExitStateFunction(entt::entity villager, VillagerStates next);
/// The entry of `state`; 1 -> SetState(TOP, state)
uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates state);
/// `current`'s, then `destination`'s -> SetState(FINAL, ...)
uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates current, VillagerStates destination);
/// GetFinalState's row and `next`'s row have the same exit function (compared whole, as listed in
/// VillagerOriginalFns.h: 0 = none, two 0s are the same), or else `next` is not a final state
[[nodiscard]] bool IsStateExitFunctionSameAs(entt::entity villager, VillagerStates next);
/// A subset: its final state takes reactions and it is not held or thrown (the flags, the life threshold and the
/// living checks are not ported). Used by the miracle types (fire, teleport, shield, mourning) on a villager with no
/// slot reaction, until the miracle maps move them to the overload below
[[nodiscard]] bool IsAvailableForReaction(entt::entity villager);
/// The whole check, for the slot types (VillagerReactions.cpp): at the worship site -> 0; the final state takes no
/// reactions -> 0; the football flag -> 0; a death state final (13..17) -> 0; life <= lifeWhenCrawlsWounded -> 0 unless
/// type 7 REACT_TO_FOOD; then the living checks (functional, not scripted, not dancing, final not 5 / 14..18 / 23, not
/// dead, not on a structure). For the slot types, and for a miracle type meeting a slot holder (the spread runs it for
/// every type). No TOP FLYING / IN_HAND test, as in the original: a held or thrown villager is out of the map cells
/// SpreadReaction walks (the hand and the physics take it off the map)
[[nodiscard]] bool IsAvailableForReaction(entt::entity villager, Reaction type);
/// Villager::targetThing: the field, fish farm or felled tree a job works on; entt::null clears it. The building side
/// writes it too: fields::RemoveFarmer and a field's deletion (null), fish_farms::AddFisherman (the farm). The original
/// shares the field with the builder's ring index (Villager::buildPosIndex) and the death reason: they are kept
/// apart here
void SetTargetThing(entt::entity villager, entt::entity thing);
/// Villager::targetThing (entt::null none)
[[nodiscard]] entt::entity GetTargetThing(entt::entity villager);
/// Whether a change to `state` may first pause for a second
[[nodiscard]] bool CanPauseForASecond(entt::entity villager, VillagerStates state);
/// SetCurrentAndDestinationState(239, state) == 1
uint32_t SetupPauseForASecond(entt::entity villager, VillagerStates state);
/// SetTopState(the raw FINAL)
void SetTopStateToFinal(entt::entity villager);
/// SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final) == 1 and then the state counter
/// (LivingAction::turnsUntilStateChange) = `turns`; 0 when the state change was refused
uint32_t SetupWaitForCounter(entt::entity villager, uint16_t turns, VillagerStates final);
/// The state function of 57: one turn off the counter and, at 0, the final state
uint32_t WaitForCounter(components::LivingAction& action);
/// SetCurrentAndDestinationState(the info's moveState = MOVE_TO_POS, final) first and, only if it returns 1, the walk
/// (openblack: the WallHug goal, a fresh step and a LINEAR move). Returns 1 if the walk was set up, else 0.
uint32_t SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final);

/// One turning step towards pos (MapCoords x / z, ecs::town_queries), of at most 0x40 / 0x80 / 0x100 (mode
/// 0 / 1 / 2) or the mode itself (any other mode) 2048ths, the short way; 1 when it faces it. The angle is
/// WallHug::yAngle in openblack (approximate: kept in radians, rounded to 2048ths)
uint32_t LookAtPos(entt::entity villager, glm::ivec2 pos, uint32_t mode);
/// The game angle (2048ths): WallHug::yAngle rounded to 2048ths, as LookAtPos reads it (approximate)
[[nodiscard]] uint16_t GetGameAngle(entt::entity villager);
/// The angle (& 0x7FF) and the drawn rotation, as LookAtPos sets it (the one copy: LookAtPos and InitStepsXZ call it)
void SetGameAngle(entt::entity villager, uint16_t angle);
/// The same on the villager's components (InitStepsXZ, ECS/Villager/VillagerScript.h)
void SetGameAngle(components::Transform& transform, components::WallHug& wallHug, uint16_t angle);
/// The drawn rotation (affine::AngleY(angle + pi / 2)) and the game angle = ConvertAngle3DToGame(angle). The
/// heading set when the physics ends
void SetYAngle(entt::entity villager, float angle);

/// State 85 CREATED: counts the state counter down, then DECIDE_WHAT_TO_DO
uint32_t VillagerCreated(components::LivingAction& action);
/// State 239 PAUSE_FOR_A_SECOND: the final state once the animation is ready
uint32_t PauseForASecond(components::LivingAction& action);

// ---- the periodic checks -----------------------------------------------------------------------------------------
// CheckHungry is in VillagerFood.h; CheckChildGrownUp, UpdatePregnancy and CheckDeathFromOldAge are in VillagerAge.h
// (included above)

// ---- death ----------------------------------------------------------------------------------------------------------
// VillagerDead, SetDying, the states 13 / 14 / 15, IsDead, GetPlayerOf, DeleteDependants and Delete are in
// VillagerDeath.h (included above)

// ---- test hooks (VillagerDebugHooks.cpp) -------------------------------------------------------------------------

/// OPENBLACK_VILLAGER_TRACE=1 (all) or =<n> (the villager with creation index n)
[[nodiscard]] bool TraceOn(entt::entity villager);
void Trace(entt::entity villager, const std::string& line);
/// Once a turn, before the villagers: OPENBLACK_TEST_VILLAGER_LIFE / _STATE / _BORN_IN_WATER / _POISONED / _CARRY and the
/// trace's summary every 100 turns
void RunDebugHooks(uint32_t turn);
} // namespace openblack::ecs::villager
