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

#include <array>
#include <span>
#include <vector>

#include "ECS/Components/PlayerMagic.h"

// A player's GameStats: the data of the in-game menu's Statistics page. One per player; the owners' code calls the
// functions below where the original updates a field.

namespace openblack::game_stats
{

/// One of the two adaptive histories: 500 buckets of the average over `period` samples; at 500 buckets they are
/// averaged in pairs and the period doubles
struct History
{
	static constexpr uint32_t k_Capacity = 500;
	std::array<float, k_Capacity> buckets {};
	uint32_t capacity {k_Capacity};
	uint32_t count {0}; ///< The bucket being filled
	uint16_t period {50};
	uint16_t samples {0};

	/// One sample: bucket[count] += value / period, then the bucket and pair rules
	void Add(float value);
};

/// The per-player counters. The town totals are not kept: they are recomputed each turn from the player's towns
/// (TownTotals below)
struct Stats
{
	uint32_t finishPlace {0};
	uint32_t finishValue {0};
	uint32_t births {0}; ///< "Total Births"
	uint32_t deaths {0}; ///< "Total Deaths"
	uint32_t maxPopulation {0};
	uint32_t maxMales {0};                ///< "Maximum Male Population" (TownStats::males)
	uint32_t maxFemales {0};              ///< "Maximum Female Population" (TownStats::females)
	uint32_t minPopulation {0xFFFFFFFFu}; ///< Reset to -1
	uint32_t minMales {0xFFFFFFFFu};
	uint32_t minFemales {0xFFFFFFFFu};
	uint32_t townTakenOverPopulation {0};
	float desireSum {0.0f};
	uint32_t desireCount {0}; ///< The row shows desireSum / desireCount
	uint32_t buildingsBuilt {0};
	std::array<uint32_t, 3> buildingsBuiltByKind {}; ///< By the abode's type bit 2 / 4 / 0x100
	std::array<uint32_t, 8> disciples {};            ///< Index 5 unused, see DiscipleMade
	uint32_t foodEaten {0};
	uint32_t woodUsed {0};
	History influence;
	History population;
	uint32_t playersOutBefore {0};
	float alignmentAtStart {0.0f}; ///< The player's alignment at Init
	uint32_t killedVillagers {0};  ///< Villagers this player killed
	float maxWorldBelief {0.0f};
	float minWorldBelief {3.40282347e+38f}; ///< FLT_MAX
	float creatureValue {0.0f};             ///< Not ported
	float creatureAtStart {0.0f};           ///< The creature's value at Init, else 0
	uint32_t buildingStopped {0};
	std::array<uint32_t, 3> u1084 {}; ///< Unknown (to read)
	// The spells cast and the chants used: in the original they live here, in openblack in PlayerMagic (castCount,
	// written by Spell::InitWithPos; chantsUsed, by WorshipSite::UseChants), the only store until the Statistics page
	// moves them here. Read them with SpellsCast / Serialize.
	uint32_t killed {0};
};

/// The sums over the player's towns (recomputed once a turn in the original)
struct TownTotals
{
	uint32_t population {0}; ///< TownStats adults + children: "Final Total Population"
	uint32_t males {0};      ///< TownStats::males: "Total Male Population"
	uint32_t females {0};    ///< TownStats::females: "Total Female Population"
	uint32_t capacity {0};   ///< adultPlaces + childPlaces: "Population Capacity"
	uint32_t towns {0};      ///< "Towns Owned"
	uint32_t buildings {
	    0};               ///< "Total Buildings" += the running abodes + civic (as the original: counts the earlier towns again)
	uint32_t abodes {0};  ///< abodesWithPlaces: "Total Abodes"
	uint32_t civic {0};   ///< civicBuildings: "Total Civic Buildings"
	uint32_t wonders {0}; ///< += Town::stats.abodesByNumber[AbodeNumber::Wonder]: "Total Wonders"
};

constexpr size_t k_Players = 8;

/// GameStats' statics (the technical page; Save writes them in the middle of every player's block)
struct Statics
{
	uint32_t linesOfCode {0};  ///< "Total Lines of Code Executed" (AddToTotalLinesOfCodeExecuted)
	float maxFrameRate {0.0f}; ///< TrackFrameRate
	float minFrameRate {0.0f};
	uint32_t objectsCreated {0}; ///< "Total Objects Created" (ObjectCreated)
	int32_t startTime {0};       ///< The time at Init: "Time Played"
	uint32_t version {113};      ///< A constant 113 (never written)
	/// The number of towns, copied at Init, 0 after ClearAll; AddToTotalLinesOfCodeExecuted's v
	uint32_t townCount {0};
	uint32_t playersOut {0}; ///< The finishing places counted
};
[[nodiscard]] Statics& GetStatics();

/// Stores the player's starting alignment and creature value, the start time and the town count
void Init(size_t player, float alignment, float creatureValue, int32_t now, uint32_t townCount);
/// linesOfCode += int(localRand(int(v x 10130 x 0.2)) + v x 10130) with v = townCount. Called every UpdateInterval
/// turns (10, 5 in a special game mode) and once by the Statistics page. `localRand` is the local random stream
void AddToTotalLinesOfCodeExecuted(uint32_t (*localRand)(int32_t));
/// After AddToTotalLinesOfCodeExecuted: the frame rate goes into the maximum, else into the minimum (a new maximum
/// skips it, and the minimum starts at 0 so it never moves: as the original)
void TrackFrameRate(int32_t frameRate);
/// One object created
void ObjectCreated();

/// The player's stats (PlayerNames as an index 0..7)
[[nodiscard]] Stats& Of(size_t player);
/// Clears every player's stats and the statics (on a map clear)
void ClearAll();

// The writers
void ChildBorn(size_t player);
/// The town's player gets a death, the killer (no null test) a killed villager
void VillagerDied(size_t townPlayer, size_t killer);
/// The new owner's townTakenOverPopulation += the town's adults + children
void TownTakenOver(size_t player, uint32_t population);
void TownDesire(size_t player, float value);
/// buildingsBuilt, and buildingsBuiltByKind by the first of the abode's type bits (the info's ABODE_TYPE) 2 / 4 / 0x100
void BuildingBuilt(size_t player, uint32_t abodeTypeBits);
void FoodEaten(size_t player, uint32_t amount);
void WoodUsed(size_t player, uint32_t amount);
/// buildingStopped ++ (the caller tests the player and the abode's state >= 200)
void BuildingStoppedFunctional(size_t player);
/// The player's summed belief: above maxWorldBelief it is the new maximum (and the minimum is skipped), else below
/// minWorldBelief the new minimum
void WorldBelief(size_t player, float belief);
void Killed(size_t player);
/// The spells cast, read from PlayerMagic::castCount: MagicType 1..35 and 41 count, the others 0 (the original counts
/// nothing for them; openblack's castCount does, so they are filtered here)
[[nodiscard]] uint32_t SpellsCast(const ecs::components::PlayerMagic& magic, uint32_t magicType);
/// VILLAGER_DISCIPLE 1..8 -> disciples[0..7], except 6, which counts nothing (disciples[5] is never written)
void DiscipleMade(size_t player, uint32_t disciple);
/// Males and females summed over the player's towns: the max / min of the sum, of the males and of the females (a new
/// maximum skips the minimum)
void TrackPopulation(size_t player, uint32_t males, uint32_t females);

/// The save order (after the object header and the player, which the save system writes): the fields as 4-byte
/// values, the town totals as 0x28 bytes, the two histories as 0x7DC bytes each, and GameStats' statics where the
/// original puts them (4 bytes each). (pending) openblack's save files
/// `magic` is the same player's PlayerMagic (magic::players::MagicOf): the spells cast and the chants used come from it
[[nodiscard]] std::vector<uint8_t> Serialize(const Stats& stats, const ecs::components::PlayerMagic& magic);

} // namespace openblack::game_stats
