/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <vector>

#include <entt/entity/fwd.hpp>

// The living turn: ONE loop over the villagers and the animals together, in the living list's order, and the walk
// inside the state functions that walk (PathfindingSystem::Step).

namespace openblack::ecs::living_turn
{
/// The original's living list, head first. Every new living goes at the head, so it runs newest first, villagers and
/// animals mixed; a deleted one is unlinked and its next is cleared. openblack keeps no list: its order is the
/// creation counter's (object_index, which the living part follows at once), highest first, among the available
/// villagers (Villager + LivingAction) and animals (Animal + Transform); unlinked = not ecs::IsAvailable (marked or
/// gone). Removing a living from the game unlinks too, but nothing calls it. (pending) a loaded game keeps the saved
/// list order
[[nodiscard]] std::vector<entt::entity> LivingList();

/// The loop of the living turn over `list` (head first, as LivingList gives it): the next one is read before each
/// living's turn, so
/// - one unlinked by an earlier turn is skipped (its predecessor's next already points past it);
/// - the current one unlinked in its own turn does not stop the loop;
/// - the next one unlinked during the current turn still takes its turn if it is still there (the dead list keeps it),
///   and the loop ends after it: its next is cleared;
/// - one made during the loop went to the head, which is past: it runs from the next turn on.
void WalkList(const std::vector<entt::entity>& list, const std::function<void(entt::entity)>& run);

/// The living turn for the current game turn: openblack's villager test hooks first, then WalkList(LivingList())
/// with, for each living,
/// - the start of this turn's move: ecs::BeginLivingTurn (ECS/MobileDrawing.h);
/// - a villager: ProcessReaction, ProcessState, its walk inside the state functions that walk (MoveToStep);
/// - an animal: animal_ai::ProcessAnimal (its reaction and state, its walk inside them, ECS/AnimalWallHug);
/// then the animals' tail (animal_ai::EndAnimalsTurn). `visualTime`: the game's visual time in hours, for the animals.
/// (pending) the original then sets a game value to 20 whose reader is not known. The creatures' part (a value from
/// the count of creatures in two states, and whether there are fewer than 3 creatures): (pending, no creature in
/// openblack)
void ProcessLiving(float visualTime);

/// One step of a villager's walk, from the state functions that walk (moving to a position or an object, on a
/// footpath, the football and dance states, moving towards a creature: the rows of k_WalkStates).
/// PathfindingSystem::Step; nothing without the system (the unit tests)
void MoveToStep(entt::entity villager);

/// The villager state rows whose state function walks (directly or through another): 1 MOVE_TO_POS,
/// 2 MOVE_TO_OBJECT, 3 MOVE_ON_STRUCTURE, 5 IN_DANCE, 29 MOVE_ON_PATH, 47 FORESTER_MOVE_TO_FOREST,
/// 60 WORSHIPPING_AT_WORSHIP_SITE, 66 RESTART_WORSHIPPING_CREATURE, 71 FOOTBALL_WALK_TO_POSITION,
/// 90 WORSHIPPING_CREATURE, 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT, 153 DANCE_FOR_EDITING_PURPOSES, 154 MOVE_TO_DANCE_POS,
/// 167 MOVE_TOWARDS_CREATURE_REACTION, 193 DANCE_BUT_NOT_WORSHIP, 203 DANCE_WHILE_REACTING, 222 FOOTBALL_MOVE_TO_BALL,
/// 230 ARTIFACT_DANCE. No other villager state walks
inline constexpr std::array<uint8_t, 18> k_WalkStates = {1,  2,   3,   5,   29,  47,  60,  66,  71,
                                                         90, 140, 153, 154, 167, 193, 203, 222, 230};
} // namespace openblack::ecs::living_turn
