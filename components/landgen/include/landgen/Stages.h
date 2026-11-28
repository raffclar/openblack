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

#include <algorithm>
#include <array>
#include <span>

#include "Random.h"

namespace openblack::landgen
{
/// The stages of the generator that use random numbers. Each stage has a number that no other stage has. Thus a change
/// in one stage does not change the random numbers of a different stage. Do not change a number, because a change gives
/// a different land for each seed. Add each new stage to k_Stages.
enum class Stage : uint64_t
{
	// Base heights
	BaseShape = 1,
	OutlineWarpU = 2,
	OutlineWarpV = 3,
	Outline = 4,
	Hills = 5,
	Crests = 6,

	// Erosion and terraces
	HydraulicErosion = 10,
	TerraceLevels = 11,
	TerraceEdge = 12,
	TerraceWidth = 13,
	TerraceWarp = 16,
	TerraceRidge = 17,
	TerraceRough = 18,

	// Mountain massifs
	MassifPlaces = 19,
	MassifOutline = 20,
	MassifSpurs = 21,
	MassifBands = 22,
	MassifBandNoise = 23,

	// Rolled ground
	RollShape = 24,
	RollRidge = 25,
	RollPart = 26,

	// The land as a whole
	Wind = 30,
	Beaches = 31,
	Blight = 32,
	RiverBends = 33,
	HillHollows = 35,

	// Trees and rocks
	DeadTrees = 40,
	Woods = 41,
	BigForests = 42,
	TreeStands = 43,
	PlainRocks = 44,
	RockClumps = 45,
	StoneRing = 46,
	SeaStacks = 47,

	// Towns
	StartTown = 50,
	TownSites = 51,
	LandTribes = 52,
	TownTribes = 53,
	Towns = 54,
	FishFarms = 55,

	// Shores
	ShoreBanks = 15,
	ShoreSectors = 60,
	ShoreBeaches = 61,
	ShoreTops = 62,

	// Swamps
	SwampSheets = 70,
	SwampCover = 71,

	// Paths
	PathWander = 80,
	PathLay = 81,
	PathLanterns = 82,

	// Rock formations
	FormationCounts = 90,
	FormationRings = 91,
	FormationPillars = 92,
	FormationClumps = 120,

	// Cliff faces
	CliffWave = 100,

	// Types of land
	LandTypePick = 110,
	LandTypeIslandScale = 111,
	LandTypeArchipelago = 112,
	LandTypeOneIsland = 113,
	LandTypeApply = 114,

	// Players
	Players = 130,

	// Streams
	StreamCount = 140,
	StreamPoints = 141,
	StreamBankTrees = 142,
	StreamSprings = 143,

	// Climate
	Temperature = 150,
	Moisture = 151,
	TemperatureEdges = 152,

	// Cursed ground
	CursedCount = 160,
	CursedSites = 161,
	CursedParts = 162,
	CursedEdgeWave = 101,
};

/// All the stages. The check below makes sure that no two stages have the same number.
inline constexpr std::array k_Stages {
    Stage::BaseShape,
    Stage::OutlineWarpU,
    Stage::OutlineWarpV,
    Stage::Outline,
    Stage::Hills,
    Stage::Crests,
    Stage::HydraulicErosion,
    Stage::TerraceLevels,
    Stage::TerraceEdge,
    Stage::TerraceWidth,
    Stage::TerraceWarp,
    Stage::TerraceRidge,
    Stage::TerraceRough,
    Stage::MassifPlaces,
    Stage::MassifOutline,
    Stage::MassifSpurs,
    Stage::MassifBands,
    Stage::MassifBandNoise,
    Stage::RollShape,
    Stage::RollRidge,
    Stage::RollPart,
    Stage::Wind,
    Stage::Beaches,
    Stage::Blight,
    Stage::RiverBends,
    Stage::HillHollows,
    Stage::DeadTrees,
    Stage::Woods,
    Stage::BigForests,
    Stage::TreeStands,
    Stage::PlainRocks,
    Stage::RockClumps,
    Stage::StoneRing,
    Stage::SeaStacks,
    Stage::StartTown,
    Stage::TownSites,
    Stage::LandTribes,
    Stage::TownTribes,
    Stage::Towns,
    Stage::FishFarms,
    Stage::ShoreBanks,
    Stage::ShoreSectors,
    Stage::ShoreBeaches,
    Stage::ShoreTops,
    Stage::SwampSheets,
    Stage::SwampCover,
    Stage::PathWander,
    Stage::PathLay,
    Stage::PathLanterns,
    Stage::FormationCounts,
    Stage::FormationRings,
    Stage::FormationPillars,
    Stage::FormationClumps,
    Stage::CliffWave,
    Stage::LandTypePick,
    Stage::LandTypeIslandScale,
    Stage::LandTypeArchipelago,
    Stage::LandTypeOneIsland,
    Stage::LandTypeApply,
    Stage::Players,
    Stage::StreamCount,
    Stage::StreamPoints,
    Stage::StreamBankTrees,
    Stage::StreamSprings,
    Stage::Temperature,
    Stage::Moisture,
    Stage::TemperatureEdges,
    Stage::CursedCount,
    Stage::CursedSites,
    Stage::CursedParts,
    Stage::CursedEdgeWave,
};

/// True when no two of the stages have the same number
[[nodiscard]] constexpr bool AllDifferent(std::span<const Stage> stages)
{
	for (size_t i = 0; i < stages.size(); ++i)
	{
		if (std::find(stages.begin() + static_cast<std::ptrdiff_t>(i) + 1, stages.end(), stages[i]) != stages.end())
		{
			return false;
		}
	}
	return true;
}

static_assert(AllDifferent(k_Stages), "Two stages of the land generator have the same number");

/// The seed of one stage of the generator, from the land's seed and the stage
[[nodiscard]] constexpr uint64_t StageSeed(uint64_t seed, Stage stage)
{
	return PartSeed(seed, static_cast<uint64_t>(stage));
}
} // namespace openblack::landgen
