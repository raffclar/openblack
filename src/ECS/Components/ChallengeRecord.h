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

#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptChallengeSnapshots.h"

namespace openblack::ecs::components
{

/// The player's record of a challenge, as its script keeps it: started when the challenge first records itself and
/// brought up to date as it goes on. The temple's challenge room lists them, and tapping a started scroll again runs its
/// reminder script with its arguments.
struct ChallengeRecord
{
	uint32_t challenge {0};
	/// Its place among the records, the first started first
	uint32_t order {0};
	/// The script that reminds the player of it, and that script's arguments, the last given first
	std::string reminderScript;
	std::vector<script::challenge_snapshots::Value> arguments;
	uint32_t title {0};
	/// How far it has got, from 0 to 1: once done it stays done
	float success {0.0f};
	/// From -1 (evil) to 1 (good), as its script last gave it
	float alignment {0.0f};
	/// Where the camera was and looked when its picture was taken
	std::optional<glm::vec3> position;
	std::optional<glm::vec3> focus;
	bool quest {false};
	/// The game turn it started, and the turn it was done on
	uint32_t startedTurn {0};
	std::optional<uint32_t> doneTurn;
};

} // namespace openblack::ecs::components
