/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDevelopment.h"

using namespace openblack;

std::optional<uint32_t> creature_development::StageFromScript(int32_t stage)
{
	if (stage < 0 || static_cast<uint32_t>(stage) > k_FullyGrownUp)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(stage);
}

bool creature_development::ReleasedFromHome(uint32_t stage, float confinementRadius, bool leashed)
{
	return confinementRadius > 0.0f && !leashed && stage >= k_LeavesHomeStage;
}

bool creature_development::KeptAtHome(const YoungCreature& creature)
{
	return !creature.leashed && creature.stage < k_LeavesHomeStage && creature.localPlayers && creature.land == k_TrainingLand;
}
