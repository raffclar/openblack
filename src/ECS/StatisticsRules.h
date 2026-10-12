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
#include <limits>
#include <optional>
#include <span>

#include "Enums.h"

/// The rules of the game's statistics: how each player's counters move, how the histories of their influence and
/// population are kept, and how the figures shown are worked out from the towns. Free of the game's state so they can be
/// tested on their own.
namespace openblack::statistics
{

/// The highest and lowest of a value checked over the game. A new highest isn't tested as a lowest, so the lowest of a
/// value that only ever grew stays at its start, the largest number.
template <typename T>
struct Extremes
{
	T highest {0};
	T lowest {std::numeric_limits<T>::max()};

	constexpr void Check(T value)
	{
		if (value > highest)
		{
			highest = value;
		}
		else if (value < lowest)
		{
			lowest = value;
		}
	}
};

/// A history of a value over the whole game: 500 points, each the average of a run of samples, 50 to start with. When
/// the last point is done the neighbouring pairs are averaged into the first half and each later point takes twice as
/// many samples, so the history always spans the whole game.
///
/// As the game keeps it, clearing works on bytes where it means points: a history newly made clears only its first
/// quarter, and after the pairs are averaged only the 62 points after the first half are cleared (and the low bytes of
/// one more), so the later points go on from the old values left in them.
struct History
{
	static constexpr size_t k_Points = 500;
	static constexpr uint16_t k_FirstSamplesPerPoint = 50;

	std::array<float, k_Points> points {};
	/// The point being filled, and how many samples it takes and has had
	uint32_t index {0};
	uint16_t samplesPerPoint {k_FirstSamplesPerPoint};
	uint16_t samplesInPoint {0};

	/// One more sample
	void Add(float value);
	/// The finished points, oldest first
	[[nodiscard]] std::span<const float> Finished() const { return {points.data(), index}; }
};

/// The figures the game keeps of a building of each kind going up: every building, then the first of living quarters,
/// civic building or wonder its kind is
struct BuildingsBuilt
{
	uint32_t total {0};
	uint32_t abodes {0};
	uint32_t civic {0};
	uint32_t wonders {0};
};
/// One more building of a kind finished
void CountBuilt(BuildingsBuilt& built, AbodeType type);

/// A building's age: its town's passes over its buildings it has stood through, every so many turns its town's kind
/// gives, up to the age at which it counts as settled. A football pitch never ages; nor does a building of no town.
inline constexpr uint32_t k_SettledAge = 200;
/// Whether a town passes over its buildings at a turn
[[nodiscard]] bool TownPassesItsBuildings(uint32_t turn, uint32_t every);
/// A building's age after one more pass of its town
[[nodiscard]] uint32_t AgedOnePass(uint32_t age);
/// Whether harm took a building from working to out of action: from above the share of its life under which it stops
/// working to at or under it, left under its full life
[[nodiscard]] bool StopsWorking(float lifeBefore, float lifeAfter, float stopsWorkingAt);
/// Whether a building put out of action counts as destroyed by a player: only when a player did it and only once the
/// building has settled, so a building put up and knocked down at once doesn't count
[[nodiscard]] bool CountsAsDestroyed(bool byAPlayer, uint32_t age);

/// The disciples made of each kind, as the game counts them: farmers, foresters, fishermen, builders, breeders, traders,
/// missionaries and craftsmen. Protection, worship and the other kinds aren't counted.
using DisciplesMade = std::array<uint32_t, 8>;
/// Where a disciple of a kind is counted in the disciples made, none for the kinds that aren't
[[nodiscard]] std::optional<size_t> DiscipleMadeIndex(VillagerDisciple kind);

/// What a miracle counts towards in the player's totals of miracles cast
enum class MiracleKind
{
	Aggressive,
	Nice,
	Creature,
};
/// The total a magic type counts towards, none for those the game doesn't count (the hungry, frightened, tired, ill and
/// thirsty creature miracles, and none)
[[nodiscard]] std::optional<MiracleKind> MiracleKindOf(MagicType type);
/// How many miracles of a kind a player has cast, from their count of each magic type
[[nodiscard]] uint32_t MiraclesCast(std::span<const uint32_t> castsOfType, MiracleKind kind);

/// A town's desires the satisfaction of its people is weighed by: what it wants of food, wood, protection, mercy, homes
/// and civic buildings, each as it feels them with their boosts
struct SatisfactionDesires
{
	float food {0.0f};
	float wood {0.0f};
	float protection {0.0f};
	float mercy {0.0f};
	float abodes {0.0f};
	float civic {0.0f};
};
/// The town's average desire: food twice, wood, the greater of protection and mercy and the greater of homes and civic
/// buildings, over the game's divisor
[[nodiscard]] float AverageDesire(const SatisfactionDesires& desires, float divisor);
/// Every this many game turns each town with a player adds to its player's satisfaction samples
inline constexpr uint32_t k_SatisfactionEvery = 50;
/// Every this many game turns the belief in each player is checked for its highest and lowest
inline constexpr uint32_t k_BeliefCheckEvery = 10;

/// The players' satisfaction, its samples summed: one less the town's average desire each time
struct Satisfaction
{
	float total {0.0f};
	uint32_t samples {0};

	void Sample(float averageDesire)
	{
		total += 1.0f - averageDesire;
		++samples;
	}
	/// The average satisfaction shown, none sampled being 0
	[[nodiscard]] float Average() const { return samples != 0 ? total / static_cast<float>(samples) : 0.0f; }
};

/// What one of the player's towns has now, for the figures worked out from the towns
struct TownCounts
{
	uint32_t men {0};
	uint32_t women {0};
	/// Its homes with places for people, its civic buildings and its wonders
	uint32_t abodes {0};
	uint32_t civic {0};
	uint32_t wonders {0};
	/// Its disciples of each kind (VillagerDisciple)
	std::array<uint32_t, 13> disciples {};
};

/// The game's count of the player's buildings: through the towns in order it keeps running counts of homes and civic
/// buildings and adds both to the total town by town, so with more than one town the earlier towns count again
[[nodiscard]] uint32_t TotalBuildings(std::span<const TownCounts> towns);

/// A share written as a whole percentage: the share of 100, rounded towards zero
[[nodiscard]] int32_t Percent(float share);

/// What the world scroll in the temple's main room tells the player
struct WorldFigures
{
	/// Every villager in the world not dying
	int32_t population {0};
	int32_t believersPercent {0};
	int32_t malePercent {0};
	int32_t deaths {0};
	int32_t births {0};
	int32_t sacrifices {0};
	/// The running count of buildings, and the wonders standing
	int32_t buildings {0};
	int32_t wonders {0};
	int32_t disciples {0};
	/// Builders, breeders, fishermen, farmers, foresters, missionaries, craftsmen and traders
	std::array<int32_t, 8> discipleKinds {};
};

/// What the world scroll is worked out from
struct WorldInputs
{
	uint32_t worldPopulation {0};
	/// The player's towns
	std::span<const TownCounts> towns;
	/// The player's missionaries preaching in towns that aren't theirs, and in any town
	uint32_t missionariesInOtherTowns {0};
	uint32_t missionaries {0};
	/// The player's counted deaths, births and sacrifices
	uint32_t deaths {0};
	uint32_t births {0};
	uint32_t sacrifices {0};
};
[[nodiscard]] WorldFigures World(const WorldInputs& inputs);

} // namespace openblack::statistics
