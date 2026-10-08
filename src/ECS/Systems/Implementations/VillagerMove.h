/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The villagers' shared move set-up (used by the miracles' villager states: fire, teleport, the teleport test hook):
// ecs::villager::SetupMoveToWithHug now lives in the villager core
// (ECS/Villager/VillagerCore.h): SetCurrentAndDestinationState(MOVE_TO_POS, final) and, if it returns 1, the walk.
#include "ECS/Villager/VillagerCore.h"
