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

#include "ECS/Components/LivingAction.h"

/// A villager in a flock a script made (FLOCK_ATTACH sets state 27). The flock itself is ECS/Flocks.h.
namespace openblack::ecs::villager
{

/// State 27 MOVE_IN_FLOCK (no entry, exit or validate function): no flock or within the domain
/// (PosWithinDomain(Pos, 1)) -> 1. The leader (the tail) -> CalcRandomPos(the domain
/// centre, 0, domainRadius), SetupMoveToPos(p, 27), 0x23. Another member: flockDistance >= its distance to the leader
/// -> 0; else p = CalcRandomPos(the leader's Pos, 0, flockDistance), and unless p is outside the domain while the
/// leader is inside (-> 0), SetupMoveToPos(p, 27), 0x23
uint32_t MoveInFlock(components::LivingAction& action);

} // namespace openblack::ecs::villager
