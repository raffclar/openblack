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

#include <span>

#include <glm/vec2.hpp>

/// A player's heartbeat: how fast their god's heart beats, quickening with the danger their people are in. The beat's
/// pulse makes the temple's column of light throb; while the temple is destroyed a clock of its own drives it instead.
namespace openblack::ecs::heartbeat
{

/// The danger is measured afresh every this many turns
inline constexpr uint32_t k_MeasureEveryTurns = 10;
/// The rate the beat eases to: this much with no danger, and this much more at the most
inline constexpr float k_CalmRate = 30.0f;
inline constexpr float k_DangerRate = 70.0f;
/// Each turn the rate goes this share of the way to where the danger puts it
inline constexpr float k_RateEase = 0.1f;
/// Beats a second for each unit of rate
inline constexpr float k_BeatsPerRate = 0.025f;
/// The smoothed shares of the world's people and of the players' influence go this share of the way each measure
inline constexpr float k_ShareEase = 0.1f;
/// Kept from dividing by nothing
inline constexpr float k_ShareFloor = 0.001f;
/// An enemy creature this many cells from the nearest town, when that town is the player's, adds to the danger: all
/// it can within the near distance, less beyond, and nothing from the far distance on
inline constexpr uint32_t k_CreatureNearCells = 100;
inline constexpr uint32_t k_CreatureFarCells = 400;
/// A destruction clock turns to beats a second through this rate per beat a second
inline constexpr float k_OverrideRate = 100.0f * 0.4f;

struct State
{
	/// The danger last measured, 0 to 1
	float stress {0.0f};
	/// How fast it beats, against k_BeatsPerRate
	float rate {k_CalmRate};
	/// How far through a beat it is, 0 to 1
	float phase {0.0f};
	/// The beat's swell this turn and the last: 0 between beats, 1 at the top
	float pulse {0.0f};
	float previousPulse {0.0f};
	/// The smoothed share of the world's people who believe in the player, and of all influence over the player's
	float believerShare {0.0f};
	float influenceShare {0.0f};
	/// Beats a second the temple's destruction holds it at, none (0) when it beats as the danger has it
	float destructionBeats {0.0f};
};

/// What the danger is measured from
struct Danger
{
	/// The player's towns' raw desires for protection, added up
	float protectionDesire {0.0f};
	/// The share of the world's people who believe in the player now
	float believerShare {0.0f};
	/// All the players' influence over the player's own, none when the player has none
	float influenceShare {0.0f};
	/// For each other player's creature whose nearest town is the player's, how many cells it is from that town
	std::span<const uint32_t> creatureDistances;
};

/// Whether the danger is measured this turn
[[nodiscard]] bool IsMeasureTurn(uint32_t turn);

/// How far apart two cells are as the game measures a creature from a town: the longer way across plus half the
/// shorter, in whole cells
[[nodiscard]] uint32_t CellDistance(glm::ivec2 a, glm::ivec2 b);

/// What an enemy creature this many cells from one of the player's towns adds to the danger
[[nodiscard]] float CreatureDanger(uint32_t cells);

/// Measures the danger: the protection the towns want, the falls of the player's share of the people and of the
/// influence since the last measures, and the enemy creatures near the player's towns, kept between 0 and 1. The
/// smoothed shares move towards the new ones.
void Measure(State& state, const Danger& danger);

/// A turn of the beat: the rate eases towards the danger's (or is held by the destruction), the beat goes on by the
/// turn's length and the pulse is taken
void Step(State& state, float secondsPerTurn);

/// The pulse between the last turn and this one, a fraction of the way through the turn
[[nodiscard]] float Pulse(const State& state, float turnFraction);

/// The beats a second the temple's destruction holds the heart at, by how far into it the clock is: quickening from
/// about 0.64 to 1.27 over nineteen seconds, and to about 3.98 at twenty-five
[[nodiscard]] float DestructionBeats(float clock);

} // namespace openblack::ecs::heartbeat
