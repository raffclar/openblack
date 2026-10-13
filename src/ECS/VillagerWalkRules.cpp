/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerWalkRules.h"

#include <algorithm>
#include <array>

using namespace openblack;

namespace
{
/// The states that walk a villager: the walks themselves, and the dances, whose dancers walk to their places
constexpr std::array k_WalkingStates {
    VillagerStates::MoveToPos,
    VillagerStates::MoveToObject,
    VillagerStates::MoveOnStructure,
    VillagerStates::MoveOnPath,
    VillagerStates::ForesterMoveToForest,
    VillagerStates::FootballWalkToPosition,
    VillagerStates::MoveTowardsObjectToLookAt,
    VillagerStates::MoveToDancePos,
    VillagerStates::MoveTowardsCreatureReaction,
    VillagerStates::FootballMoveToBall,
    VillagerStates::InDance,
    VillagerStates::WorshippingAtWorshipSite,
    VillagerStates::RestartWorshippingCreature,
    VillagerStates::WorshippingCreature,
    VillagerStates::DanceForEditingPurposes,
    VillagerStates::DanceButNotWorship,
    VillagerStates::DanceWhileReacting,
    VillagerStates::ArtifactDance,
    // TODO(teleport): the game walks a villager to a teleport stone in the plain walk, which ends in these states again;
    // openblack walks it while it stays in them
    VillagerStates::GoTowardsTeleportReaction,
    VillagerStates::GoTowardsTeleportReactionQuickly,
};

} // namespace

bool ecs::villager_walk::Walks(VillagerStates state)
{
	return std::ranges::find(k_WalkingStates, state) != k_WalkingStates.end();
}
