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

/// How a creature grows up. It is at one of fourteen stages, from the fresh young creature (0) to fully mature (13).
/// Only the story's scripts move it from stage to stage: each lesson of the first land sets its stage as it begins.
/// A new stage switches its desires to the ones its stages so far have brought, frees every held-back desire and drops
/// the plan it was carrying out. On the first land, a young creature of the player's is kept near its home until it
/// reaches the stage it may leave from, and reaching that stage lets it go.
namespace openblack::creature_development
{

/// The stage every creature is made at, and the last
constexpr uint32_t k_FullyGrownUp = 13;
/// From this stage on a creature is no longer kept near its home
constexpr uint32_t k_LeavesHomeStage = 5;
/// How close to its home a young creature of the player's is kept on the first land
constexpr float k_YoungHomeRadius = 10.0f;
/// The land a young creature is kept at home on
constexpr int32_t k_TrainingLand = 1;

/// The stage a script asks for, or nothing for a number that is no stage
[[nodiscard]] std::optional<uint32_t> StageFromScript(int32_t stage);

/// Moving to a stage lets the creature leave the area it was kept in, when it is kept in one, isn't on a leash and the
/// stage is one it may leave home from
[[nodiscard]] bool ReleasedFromHome(uint32_t stage, float confinementRadius, bool leashed);

/// What decides whether a creature is kept near its home this turn
struct YoungCreature
{
	uint32_t stage {k_FullyGrownUp};
	bool leashed {false};
	/// It belongs to the player at this computer
	bool localPlayers {false};
	int32_t land {0};
};

/// Every turn a creature of the local player's, off the leash, on the first land and younger than the leaving stage
/// is kept within the young home radius of its home
[[nodiscard]] bool KeptAtHome(const YoungCreature& creature);

} // namespace openblack::creature_development
