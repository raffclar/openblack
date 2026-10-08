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

#include <functional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The disciples: the disciple table, SetVillagerDisciple, DiscipleDecideWhatToDo and the state 221
// DISCIPLE_NOTHING_TO_DO. A disciple has the villager flag k_FlagDisciple, with its VILLAGER_DISCIPLE in
// Villager::discipleType; another flag marks a disciple follower. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the disciple table (13 records) -----------------------------------------------------------------------------

/// One record of the disciple table, as the original has it (7 dwords)
struct DiscipleInfo
{
	uint32_t startState;         ///< A VILLAGER_STATES byte: the state set when the disciple lands, 0: inspect
	uint32_t createsJobReaction; ///< != 0: DecideWhatToDo creates the reaction 0x18
	uint32_t marker;             ///< The value given to the 3D object when it becomes a disciple
	uint32_t heldAtJob;          ///< == 1: held at its job (read by the hunger and periodic checks, among others)
	uint32_t fetchesWood;        ///< != 0: interested in wood objects
	int32_t townDesire;          ///< The TOWN_DESIRE_INFO it serves, -1 none
	uint32_t movesIntoTown;      ///< != 0: on landing it checks moving into the town
};

/// The number of records (SetVillagerDisciple accepts 0 <= disciple < 13)
inline constexpr size_t k_DiscipleInfoCount = 13;

/// The disciple's record. (guard) Past the table: record 0 (the original never indexes past 12: SetVillagerDisciple
/// keeps the type in 0..12)
[[nodiscard]] const DiscipleInfo& GetDiscipleInfo(uint8_t disciple);
/// The start state, read as a byte: the state the disciple starts in
[[nodiscard]] VillagerStates DiscipleStartState(uint8_t disciple);
/// The reaction field != 0: 1, 2, 3, 4, 6, 8
[[nodiscard]] bool DiscipleCreatesJobReaction(uint8_t disciple);
/// The value SetVillagerDisciple gives the 3D object (0 for 0, 11, 12). (inferred) a disciple marker
[[nodiscard]] uint32_t DiscipleMarker(uint8_t disciple);
/// == 1: a working disciple held at its job (1..6, 8, 9); 0 lives normally (0, 7, 10, 11, 12)
[[nodiscard]] bool DiscipleHeldAtJob(uint8_t disciple);
/// != 0: FORESTER, BUILDER and CRAFTSMAN fetch wood
[[nodiscard]] bool DiscipleFetchesWood(uint8_t disciple);
/// The town desire the disciple serves, -1 none (FOOD 0, WOOD 1, 8, 9)
[[nodiscard]] int32_t DiscipleTownDesire(uint8_t disciple);
/// != 0: dropped on another town's object, it moves into that town
[[nodiscard]] bool DiscipleMovesIntoTown(uint8_t disciple);

// ---- the disciple ------------------------------------------------------------------------------------------------

/// Outside 0..12 -> 0, nothing done. Else (not ported, approximate) the town's TownStats counts; a disciple: the
/// disciple flag set and the follower flag cleared, (pending) the 3D object's marker, the type stored; 0: both flags
/// cleared, (pending) the marker cleared, the type 0. 1. `thing` and `h` are not read
uint32_t SetVillagerDisciple(entt::entity villager, entt::entity thing, VillagerDisciple disciple, int32_t h);
/// One case per disciple type 1..12 (any other -> the fallback). 1 when the disciple found something to do
uint32_t DiscipleDecideWhatToDo(entt::entity villager);

// ---- 221 DISCIPLE_NOTHING_TO_DO ----------------------------------------------------------------------------------

/// FindDisciplePrayerPos -> the countdown reset, SetupMoveToWithHug(p, 221); 1. 0 when there is no point
uint32_t SetDiscipleNothingToDo(entt::entity villager);
/// No town -> 0; no town centre -> out = the town's position; else out = the centre + GetPosFromAngle(
/// Get3DAngleFromXZ(centre, me) + GameFloatRand(pi / 2) - pi / 4, GameFloatRand(4) + the centre's Get2DRadius). 1
uint32_t FindDisciplePrayerPos(entt::entity villager, glm::ivec2& out);
/// 221: the town's build pulse -> countdown = GameRand(10); countdown - 1 (signed 16 bits) > 0 -> 1; at 0 the clip
/// not done -> countdown = 1; else DiscipleDecideWhatToDo == 0 -> countdown = 300. Always 1
uint32_t DiscipleNothingToDo(components::LivingAction& action);
/// 221's entry: a town with a centre -> look at the centre. Always 1
uint32_t EnterDiscipleNothingToDo(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The breeder's job check (not ported yet: 0)
uint32_t SetupBreederDisciple(entt::entity villager);
/// The trader's job check (not ported yet: 0)
uint32_t CheckTrader(entt::entity villager);
} // namespace openblack::ecs::villager
