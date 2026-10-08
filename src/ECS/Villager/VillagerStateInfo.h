/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>

#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

// GVillagerStateTableInfo (info.dat, one row per villager state) with names for its fields from what reads them
// (docs/bw1-notes/villagers.md, section info.dat state table). The struct in InfoConstants.h names some of its fields;
// these accessors name the rest.

namespace openblack::ecs::villager::state_info
{
/// The state's clip (ANM_, 385 P_STAND; -4 not drawn)
inline int32_t Clip(const GVillagerStateTableInfo& s)
{
	return static_cast<int32_t>(s.animation);
}
/// The town desire the state serves (TownDesireInfo, -1 none). AdjustTownModifier
inline int ServedDesire(const GVillagerStateTableInfo& s)
{
	return s.field0x4;
}
/// How much of it. AdjustTownModifier
inline float ServedDesireAmount(const GVillagerStateTableInfo& s)
{
	return s.field0x8;
}
/// A final state. GetFinalState, SetState
inline bool IsFinal(const GVillagerStateTableInfo& s)
{
	return s.isFinalState != 0;
}
/// Never kept in PREVIOUS (SetState)
inline bool NotStoredAsPrevious(const GVillagerStateTableInfo& s)
{
	return s.keepsPreviousState != 0;
}
/// A moving state (the per-turn checks, the moving animations)
inline bool IsMoving(const GVillagerStateTableInfo& s)
{
	return s.field0x14 != 0;
}
inline bool IsScriptState(const GVillagerStateTableInfo& s)
{
	return s.isScriptState != 0;
}
inline bool IsScriptInterruptable(const GVillagerStateTableInfo& s)
{
	return s.isScriptInterruptableState != 0;
}
/// The state PopFromPrevious resumes
inline VillagerStates ResumeState(const GVillagerStateTableInfo& s)
{
	return static_cast<VillagerStates>(s.resumeState);
}
/// The speed group entry (the state's speed)
inline uint32_t SpeedGroup(const GVillagerStateTableInfo& s)
{
	return s.speedIndex;
}
/// 1 / 3 / 4 (IsVillagerAvailable tests & 1): the villager's availability in the state
inline int AvailableState(const GVillagerStateTableInfo& s)
{
	return s.field0xa8;
}
/// No name yet
inline uint32_t Field0xB0(const GVillagerStateTableInfo& s)
{
	return s.field0xb0;
}
/// Read only by the housewife's call to make dinner, which the original never runs
inline uint32_t DinnerInterrupt(const GVillagerStateTableInfo& s)
{
	return s.field0xb4;
}
/// A reaction state (ExitReaction)
inline bool IsReactive(const GVillagerStateTableInfo& s)
{
	return s.isReactionState != 0;
}
/// AT_HOME's exit: 0 -> LeaveHome
inline bool StaysAtHomeOnExit(const GVillagerStateTableInfo& s)
{
	return s.staysAtHomeOnExit != 0;
}
/// It may start with a pause. CanPauseForASecond
inline bool CanPauseForASecond(const GVillagerStateTableInfo& s)
{
	return s.canPauseForASecond != 0;
}
/// The interest in a food object
inline float FoodInterest(const GVillagerStateTableInfo& s)
{
	return s.field0xc8;
}
/// The interest in a wood object
inline float WoodInterest(const GVillagerStateTableInfo& s)
{
	return s.field0xcc;
}
/// CheckHungry
inline bool InterruptWhenHungry(const GVillagerStateTableInfo& s)
{
	return s.field0xd0 != 0;
}
/// CheckHungry
inline bool InterruptWhenStarving(const GVillagerStateTableInfo& s)
{
	return s.field0xd4 != 0;
}
/// It does not go home when hurt (the per-turn checks, the exit of getting food at worship)
inline bool NoGoHomeWhenHurt(const GVillagerStateTableInfo& s)
{
	return s.field0xd8 != 0;
}
/// The carried object
inline int CarriedObject(const GVillagerStateTableInfo& s)
{
	return s.field0xdc;
}
/// Whether the villager is doing something interesting
inline uint32_t Interesting(const GVillagerStateTableInfo& s)
{
	return s.field0xe0;
}
/// The periodic checks run in this state
inline bool DoPeriodicChecks(const GVillagerStateTableInfo& s)
{
	return s.field0xe4 != 0;
}
/// It goes home when hurt (the per-turn checks)
inline bool GoHomeWhenHurt(const GVillagerStateTableInfo& s)
{
	return s.field0xe8 != 0;
}
/// Available for a reaction
inline bool AvailableForReaction(const GVillagerStateTableInfo& s)
{
	return s.availableForReaction != 0;
}
/// No out-of clip when entering it
inline bool NoOutOfClip(const GVillagerStateTableInfo& s)
{
	return s.field0xf0 != 0;
}
/// Read by EndPhysics
inline uint32_t EndPhysicsFlag(const GVillagerStateTableInfo& s)
{
	return s.field0xf4;
}
/// Life lost each turn in the state
inline float LifeDrainPerTurn(const GVillagerStateTableInfo& s)
{
	return s.field0xf8;
}
/// Its help text (6116 + n)
inline uint32_t HelpText(const GVillagerStateTableInfo& s)
{
	return s.field0xfc;
}
inline uint32_t QueryText(const GVillagerStateTableInfo& s)
{
	return s.field0x100;
}

/// The state's row: the state index is a byte in the original; info.dat has rows 0..254, so 255 reads row 254
/// (approximate: no state 255 exists)
inline const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto index = std::min<size_t>(static_cast<uint8_t>(state), table.size() - 1);
	return table.at(index);
}
} // namespace openblack::ecs::villager::state_info
