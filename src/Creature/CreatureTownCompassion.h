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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

/// How a creature's compassion is spent on a town. Compassion is always about something: of the towns and other
/// creatures it knows, the one most worth it for the distance. Only a town gives it anything to do. It then helps the
/// town with one of the town's desires at a time, each desire having its own list of actions in the game's tables. It
/// takes the desires the town feels in the town's own order, most felt first, stopping at the first the town doesn't feel
/// at all, and goes round them: after half a minute of game turns, when it plans compassion while compassion is what it
/// wants most and it isn't already being compassionate, or each time it has finished helping. Having finished, it stays
/// with a desire the town still feels more than when it took it up (more than 85 hundredths of that) a few times more,
/// except the desire to relax. A town with hurt people may make it think of healing first, once it has seen the healing
/// miracle well over half the times it needs to learn it.
namespace openblack::creature_town_compassion
{
/// The things a town can want
constexpr size_t k_TownDesireCount = 17;
/// The town's desire to relax, which it never stays with
constexpr uint32_t k_Relaxation = 15;
/// Game turns that must have gone, and more, before it moves on to the next desire as it plans
constexpr uint32_t k_TurnsBeforeMovingOn = 30;
/// The share of the town's desire it remembers as it takes one up
constexpr float k_RememberedShare = 0.85f;
/// It stays with a desire after helping at most this many times more, or one more on a coin's toss
constexpr uint32_t k_MostTimesKept = 2;
/// The share of the healing miracle's sightings it needs before hurt people make it think of healing first
constexpr float k_HealSightingsShare = 0.6f;

/// The action lists for each of a town's desires, each as the table lists them up to its first empty slot
using DesireActions = std::array<std::vector<uint32_t>, k_TownDesireCount>;

/// Which of a town's desires a creature is helping with, kept on the creature
struct State
{
	/// The town desire it helps with, if any, and its place in the town's list of desires when it took it up
	std::optional<uint32_t> desire;
	uint32_t index {0};
	/// The town's desire as it took it up, times 85 hundredths
	float remembered {0.0f};
	/// The game turn it last moved on or finished helping, and how many times running it has stayed with the desire
	uint32_t lastTurn {0};
	uint32_t timesKept {0};
	/// Whether it chooses the desire itself; a desire shown to it on the leash holds it to that one
	bool choosesFreely {true};
};

/// The town's desires it can help with, in the town's order (most felt first): those with actions, stopping at the first
/// of them the town doesn't feel at all. `order` lists the desires by how much the town feels them, `felt` is how much it
/// feels each now.
[[nodiscard]] std::vector<uint32_t> DesiresToHelp(std::span<const uint32_t> order,
                                                  std::span<const float, k_TownDesireCount> felt, const DesireActions& actions);

/// Takes up the first desire when the one it helps with isn't among those it can help with (nothing changes when there
/// are none)
void Settle(State& state, std::span<const uint32_t> desires, std::span<const float, k_TownDesireCount> felt);

/// Moves on to the next desire, going round; with none to help with, it helps with none
void MoveOn(State& state, std::span<const uint32_t> desires, std::span<const float, k_TownDesireCount> felt);

/// Whether, planning compassion for a town at a game turn, it moves on first: it chooses freely, compassion is what it
/// wants most, it isn't being compassionate already, and more than 30 turns have gone since it last moved on or helped
[[nodiscard]] bool MovesOnWhilePlanning(const State& state, bool compassionMostWanted, bool beingCompassionate, uint32_t turn);
/// It moves on as it plans, at a game turn
void MoveOnWhilePlanning(State& state, std::span<const uint32_t> desires, std::span<const float, k_TownDesireCount> felt,
                         uint32_t turn);

/// Having finished helping the town at a game turn: it stays with the desire if the town still feels it more than it
/// remembered and it hasn't stayed too many times, else moves on. `random(n)` gives a number below n.
void FinishedHelping(State& state, std::span<const uint32_t> desires, std::span<const float, k_TownDesireCount> felt,
                     uint32_t turn, const std::function<uint32_t(uint32_t)>& random);

/// Whether hurt people make it think of healing first: some of the town's people are hurt, no town desire is shown to it
/// on the leash, it has seen the healing miracle more than 60 hundredths of the times it needs to learn it, and a coin's
/// toss comes up heads
[[nodiscard]] bool HealFirst(uint32_t hurt, bool leashedToTownDesire, float sightings, float sightingsNeeded,
                             const std::function<uint32_t(uint32_t)>& random);

/// The actions it weighs for the town: those for the desire it helps with, the first of them replaced by healing when it
/// thinks of healing first
[[nodiscard]] std::vector<uint32_t> Actions(const State& state, const DesireActions& actions,
                                            std::optional<uint32_t> healFirst);

/// What a creature makes of a town for its decision trees: how much the town believes in the creature's player, 0 to 3: under
/// 0.2, under 0.4, under 0.6, or more
[[nodiscard]] uint32_t ReligiousBelief(float belief);
/// What the town wants most, 0 to 16 (16 when it wants nothing)
[[nodiscard]] uint32_t NeedsMost(int mostDesired);
/// How big a town is, 0 to 2: under 20 people, under 40, or more
[[nodiscard]] uint32_t TownSize(uint32_t people);

} // namespace openblack::creature_town_compassion
