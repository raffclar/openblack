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

#include <entt/entity/fwd.hpp>

#include "ECS/StatisticsRules.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The players' statistics in the world: what the game's systems count for each player as it happens, the work of each
/// game turn, and the figures the temple's world scroll shows. The rules are statistics::'s (StatisticsRules.h); the
/// counters are each player's PlayerStatistics.
namespace openblack::player_statistics
{

/// A game starts: a new game, a saved game or a skirmish, but not the next land of a game. The game notes each player's
/// alignment and their creature's alignment (nothing without a creature), and counts the towns on the land. On the next
/// land of the same game, these stay at nothing, as the new land clears them.
void GameStarts(ecs::Registry& registry);

/// What the game's systems count, each in the registry its players are in. A child is born to one of the player's people
void CountBirth(ecs::Registry& registry, PlayerNames player);
/// A villager of a town dies: one more death for the town's player and one more killed for the player it is put down
/// to, the neutral player for nobody
void CountDeath(ecs::Registry& registry, PlayerNames owner, PlayerNames killer);
/// A villager or an animal is sacrificed for the player
void CountSacrifice(ecs::Registry& registry, PlayerNames player);
/// A building of the player's town is finished
void CountBuilt(ecs::Registry& registry, PlayerNames player, AbodeType type);
/// A building the player put out of action
void CountDestroyed(ecs::Registry& registry, PlayerNames player);
/// A villager dropped by the hand is made a disciple of a new kind, for its town's player
void CountDiscipleMade(ecs::Registry& registry, PlayerNames player, VillagerDisciple kind);
/// Food the player's people ate, and wood their towns built with
void CountFoodUsed(ecs::Registry& registry, PlayerNames player, uint32_t amount);
void CountWoodUsed(ecs::Registry& registry, PlayerNames player, uint32_t amount);
/// The player took over a town of so many people
void CountConverted(ecs::Registry& registry, PlayerNames player, uint32_t people);
/// The player cast a miracle of a type
void CountCast(ecs::Registry& registry, PlayerNames player, MagicType type);
/// A worship site of the player's gave prayer power
void CountChantsUsed(ecs::Registry& registry, PlayerNames player, float amount);

/// The player's influence power goes into their influence history as one more sample, as a game turn does it
void CountInfluence(ecs::Registry& registry, PlayerNames player, float power);

/// A game turn: each player's towns' people are checked for their most and fewest and go into the population history,
/// their influence into the influence history, their creature's size is noted the first time it is seen, and every 50
/// turns each town with a player samples how satisfied its people are
void ProcessTurn(uint32_t turn);

/// Every so many game turns the figures kept once for everyone change: the invented lines of code and the frame rate's
/// highest and lowest
void ProcessGlobalTurn(uint32_t turn, int32_t frameRate);

/// Everything the Game Statistics box shows now, for the first four players. As in the game, when the box takes the figures, it
/// increases the lines of code on once more.
[[nodiscard]] statistics::BoxFigures Box();

/// A creature ate a magic mushroom or a toadstool, or dropped a lump of poo
void CountCreatureMushroom(ecs::Registry& registry, entt::entity creature);
void CountCreaturePoo(ecs::Registry& registry, entt::entity creature);
/// What the player's creature, the first they got, counts of its life; its fights and the fights it won are its
/// fight record's. Nothing without a creature.
[[nodiscard]] statistics::CreatureTallies CreatureTalliesOf(PlayerNames player);

/// What the temple's world scroll tells the player now
[[nodiscard]] statistics::WorldFigures World(PlayerNames player);

} // namespace openblack::player_statistics
