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

#include <functional>
#include <optional>

#include <glm/vec2.hpp>

#include "ECS/CreatureAttitudeRules.h"

/// Villagers watching a creature fight. Every arena makes a reaction round itself that villagers heed while a fight is
/// on in it. Each villager that takes it up goes to a place on a ring a little wider than the arena, by how many of the
/// watchers joined after it: the newest at the ring's start, the one before it opposite, then the quarters between, the
/// eighths and so on. There it faces the arena, in the one clip it picks as it arrives: when either creature took a
/// blow in the last fifteen seconds it cheers if the other creature was struck last, or despairs if the one it supports
/// was; otherwise it chats. A villager supports the creature its town doesn't fear, and either at random when its town
/// fears both or neither. Villagers reacting to a creature that goes into a fight leave it to watch the fight. Pure
/// rules, tested without the game.
namespace openblack::ecs::fight_watch
{

/// The watchers stand this many times the arena's radius from its middle
constexpr float k_RingPerRadius = 1.4f;

/// The game angle (2048 to the circle) of the place of the watcher joined so many places after the last one to join
[[nodiscard]] uint16_t PlaceAngle(uint32_t index);
/// A watcher's place: at a game angle on the ring round an arena with this middle (in 16.16 map units, x and z) and
/// radius in metres, in map units
[[nodiscard]] glm::ivec2 PlaceOnRing(glm::ivec2 middle, float arenaRadius, uint16_t angle);

/// How urgently a villager heeds an arena: as its table says while a fight is on in it, else not at all
[[nodiscard]] uint8_t Priority(bool fightOn, uint8_t tablePriority);

/// Which of the two fighters (the one that took the arena, or the other) a villager supports
enum class Side : uint8_t
{
	First,
	Second,
};
using IntRandom = std::function<uint32_t(uint32_t)>;
/// By how its town feels about each of the two, if it has a town
struct TownFeelings
{
	creature_attitude::Attitude first;
	creature_attitude::Attitude second;
};
[[nodiscard]] Side Supports(std::optional<TownFeelings> town, const IntRandom& random);

/// Whole seconds since a game turn, as the game counts them
[[nodiscard]] uint32_t SecondsSince(uint32_t now, uint32_t then, uint32_t turnsPerSecond);

} // namespace openblack::ecs::fight_watch
