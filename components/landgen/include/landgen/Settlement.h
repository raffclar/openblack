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

#include "landgen/Grid.h"
#include "landgen/HeightCurve.h"

/// The player's first town and temple on the start area, and the neutral towns of the land: the kinds of building,
/// people and field that the lands of the story use
namespace openblack::landgen::settlement
{
/// The tribes whose towns have each building of the plan, with the game's numbers
enum class Tribe : uint8_t
{
	Celtic = 0,
	Aztec = 2,
	Japanese = 3,
	Indian = 4,
	Egyptian = 5,
	Greek = 6,
	Norse = 7,
	Tibetan = 8,
};
constexpr std::array<Tribe, 8> k_Tribes {Tribe::Celtic,   Tribe::Aztec, Tribe::Japanese, Tribe::Indian,
                                         Tribe::Egyptian, Tribe::Greek, Tribe::Norse,    Tribe::Tibetan};

enum class Building : uint8_t
{
	TownCentre,
	StoragePit,
	Creche,
	AbodeA,
	AbodeC,
	AbodeD,
	AbodeE,
	AbodeF,
	Workshop,
	Graveyard,
	Wonder,
	AbodeB,
};

/// The kinds of house of the town of the start
constexpr std::array<Building, 5> k_Houses {Building::AbodeA, Building::AbodeC, Building::AbodeD, Building::AbodeE,
                                            Building::AbodeF};
/// All the kinds of house of the towns of the story, in the order of the weights of `TownMix::houseWeights`
constexpr std::array<Building, 6> k_AllHouses {Building::AbodeA, Building::AbodeB, Building::AbodeC,
                                               Building::AbodeD, Building::AbodeE, Building::AbodeF};

enum class Villager : uint8_t
{
	Forester,
	Housewife,
};

/// A building of the town. The place is in metres, and the turn is in thousandths of a radian, as a land script gives it.
/// A planned building is a place where the town can build later: nothing of it stands at the start.
struct Placed
{
	Building building;
	float x;
	float z;
	int32_t rotation;
	bool planned {false};
};

/// A person of the town, at the place in metres where the person starts
struct Person
{
	Villager kind;
	float x;
	float z;
	int32_t age;
};

/// A field of the town: a square of 10 metres about the place in metres
struct Field
{
	float x;
	float z;
};

/// A town: its tribe, its centre in metres, and its buildings, people and fields
struct Town
{
	Tribe tribe {Tribe::Celtic};
	float x {0.0f};
	float z {0.0f};
	/// The town centre first, then the storage pit, the creche, the houses, and the workshop and the graveyard if the
	/// town has them
	std::vector<Placed> buildings;
	std::vector<Person> villagers;
	std::vector<Field> fields;
	/// The fish farms of the town: squares of 10 metres on the shore, about the place in metres
	std::vector<Field> fishFarms;
};

struct Plan
{
	/// The temple of the player, in metres, and its turn in thousandths of a radian
	float citadelX {0.0f};
	float citadelZ {0.0f};
	int32_t citadelRotation {0};
	/// The first town of the player
	Town town;
	/// The towns that belong to no player
	std::vector<Town> neutral;
};

struct Settings
{
	/// The distances from the middle of the start area to the temple and to the town centre, in metres. They are on one
	/// line through the middle, on the two sides of it.
	float citadelDistance {45.0f};
	float townDistance {50.0f};
	/// The distance between two places for buildings, in metres. Each building moves by a random amount up to the second
	/// value from its place.
	float buildingSpacing {22.0f};
	float buildingShift {3.0f};
	/// The count of houses is from the first value to the second value
	uint32_t fewestHouses {5};
	uint32_t mostHouses {7};
	/// The count of adults in a house is from the first value to the second value
	uint32_t fewestPeople {2};
	uint32_t mostPeople {4};
	/// The ages of the adults
	int32_t youngest {18};
	int32_t oldest {45};
	/// The fields are in a block of rows and columns of 10 metre squares, past the houses on the far side from the temple
	int fieldRows {2};
	int fieldColumns {3};
	float fieldGap {20.0f};
	/// The share of the towns of the story with five buildings or more that have a workshop (35 of 59) and a graveyard
	/// (31 of 59)
	float workshopShare {35.0f / 59.0f};
	float graveyardShare {31.0f / 59.0f};
};

/// The shares of the towns of the story that have a kind of building, as a building that stands and as a planned
/// building. A town that has neither does not have the building.
struct Share
{
	float built;
	float planned;
};

/// The buildings of a neutral town, from the 60 towns of the story with five buildings or more
struct TownMix
{
	Share storagePit {.built = 53.0f / 60.0f, .planned = 2.0f / 60.0f};
	Share creche {.built = 43.0f / 60.0f, .planned = 9.0f / 60.0f};
	Share workshop {.built = 19.0f / 60.0f, .planned = 16.0f / 60.0f};
	Share graveyard {.built = 12.0f / 60.0f, .planned = 19.0f / 60.0f};
	/// A wonder stands only in a town with many houses: 3 of the 14 towns with 15 houses or more. The other towns have a
	/// planned wonder at most.
	Share wonder {.built = 0.0f, .planned = 14.0f / 60.0f};
	Share largeWonder {.built = 3.0f / 14.0f, .planned = 14.0f / 60.0f};
	uint32_t largeTown {15};
	/// The houses of the towns of the story of each tribe, in the order of k_Tribes, and for each tribe in the order of
	/// k_AllHouses. The Indian towns have no house of kind B.
	std::array<std::array<uint32_t, 6>, 8> houseWeights {{
	    {40, 22, 17, 13, 20, 27},
	    {16, 8, 16, 12, 4, 3},
	    {18, 24, 21, 27, 20, 21},
	    {49, 0, 7, 6, 9, 50},
	    {7, 3, 6, 8, 1, 5},
	    {13, 10, 12, 5, 4, 7},
	    {36, 23, 12, 9, 9, 13},
	    {12, 24, 17, 8, 9, 13},
	}};
};

/// The rules for the fish farms of a neutral town, from the 84 fish farms of the towns of the story
struct FarmSettings
{
	/// The share of the towns with fish farms for the distance from the centre to the sea. The distances are the upper
	/// limits in metres of the first three bands: 8 of 13 towns nearer than 100 m, 8 of 20, 4 of 15, and 2 of 12 towns
	/// at 200 m or more.
	std::array<float, 3> seaBands {100.0f, 150.0f, 200.0f};
	std::array<float, 4> shares {8.0f / 13.0f, 8.0f / 20.0f, 4.0f / 15.0f, 2.0f / 12.0f};
	/// The towns with fish farms that have 2, 3, 4, 5 and 6 fish farms
	std::array<uint32_t, 5> countWeights {3, 7, 7, 1, 4};
	uint32_t fewest {2};
	/// The distance from the town centre to a fish farm in metres (the 5th and the 95th percentiles)
	float nearest {51.0f};
	float farthest {199.0f};
	/// The height of a fish farm above the sea in metres (the 5th and the 95th percentiles), and the steepest slope in
	/// degrees (the 95th percentile)
	float lowest {0.7f};
	float highest {2.7f};
	float steepest {3.8f};
};

/// A place for a town, in corners
struct Site
{
	float x;
	float z;
};

/// A circle of ground in corners where no town goes, such as the start area
struct Area
{
	float x;
	float z;
	float radius;
};

/// The rules for the places of the towns, from the towns of the story with five buildings or more
struct SiteSettings
{
	/// The towns of each land of the story. The seed picks one of these counts, and the land has that many towns with
	/// the player's town.
	std::array<uint32_t, 8> townCounts {6, 6, 8, 8, 10, 13, 13, 15};
	/// The most neutral towns of a land of the story. A land of the story with more towns gives some of them to other
	/// players, so a land with only the player keeps this many neutral towns at most.
	uint32_t mostNeutral {10};
	/// The tribes of each land of the story. The seed picks one of these counts for the land.
	std::array<uint32_t, 8> tribeCounts {4, 4, 5, 5, 6, 6, 4, 4};
	/// The towns of the story of each tribe, in the order of k_Tribes. The tribes of a land come from these weights.
	std::array<uint32_t, 8> tribeWeights {22, 4, 13, 13, 2, 4, 18, 5};
	/// The heights in metres of the centres of the towns of the story at each tenth: the lowest, the 10th percentile and
	/// so on to the highest. Each town takes a height from one tenth that the seed picks, so that the heights of the towns
	/// spread as the towns of the story do.
	std::array<float, 11> heightTenths {6.0f, 11.4f, 21.1f, 28.6f, 32.3f, 34.8f, 40.5f, 50.4f, 75.7f, 89.9f, 146.7f};
	/// The centre of a town is between these distances from the sea in metres (the 5th and the 95th percentiles of the
	/// towns of the story)
	float nearestSea {71.9f};
	float farthestSea {272.1f};
	/// The centre keeps this far from a river, in corners, so that the buildings keep off its banks
	float riverDistance {4.0f};
	/// The most rise or fall of the ground in metres from the centre to the corners of the flat ground, before the
	/// generator makes it flat (the median of the towns of the story at 60 m)
	float roughest {32.5f};
	/// The distance from a town to the nearest other town, in metres: the 5th and the 95th percentiles of the towns of the
	/// story. Each new town is at most the second distance from a town that the land has, so that the towns make a chain
	/// from the player's town.
	float nearestTown {221.8f};
	float farthestTown {592.4f};
	/// The farthest town of the story from its nearest town, in metres. A town can go this far from the chain to stand at
	/// the height that the seed picks.
	float farthestNearest {841.0f};
	/// The flat ground of a town, and the ground beyond it that slopes back to the land, in corners
	float flatRadius {6.0f};
	float blendRadius {10.0f};
	/// The step between two places that the search tries, in corners
	int step {4};
};

/// The tribes of a land: the tribe of the player's town first, then the other tribes that the seed picks with the
/// weights of the settings
[[nodiscard]] std::vector<Tribe> TribesOf(Tribe first, uint64_t seed, const SiteSettings& settings);

/// The places for the neutral towns: dry, flat ground in the limits of the settings, with each town in the reach of a
/// town before it. The first town is the player's town at `first`. The ground that a town makes flat stays out of each
/// area of `keepOut`. The count comes from the seed. The heights are altitudes, the curve gives their metres, and the
/// distances are in corners.
[[nodiscard]] std::vector<Site> FindSites(const Grid<float>& heights, const HeightCurve& curve, const Grid<float>& seaDistance,
                                          const Grid<float>& riverDistance, Site first, std::span<const Area> keepOut,
                                          uint64_t seed, const SiteSettings& settings);
/// The places for the neutral towns of a land for more than one player. The towns of the players at `firsts` start the
/// chain. The land gets the neutral towns of a land for one player. When `lakes` is given, the flat ground of a town
/// covers no corner that it marks, so that no town stands in a lake.
[[nodiscard]] std::vector<Site> FindSites(const Grid<float>& heights, const HeightCurve& curve, const Grid<float>& seaDistance,
                                          const Grid<float>& riverDistance, std::span<const Site> firsts,
                                          std::span<const Area> keepOut, uint64_t seed, const SiteSettings& settings,
                                          const Grid<uint8_t>* lakes = nullptr);

/// The tribe of each neutral town: each other tribe of the land once, then tribes of the land that the seed picks
[[nodiscard]] std::vector<Tribe> TribesOfTowns(std::span<const Tribe> landTribes, size_t towns, uint64_t seed);

/// The plan of the start: the temple on one side of the middle of the start area, and the town on the other side. The
/// seed picks the tribe, the direction of the line, the houses, the people and each turn. A building or a field does not
/// go where `keepClear` is not 0, such as water and the banks of rivers. The start area is in corners.
[[nodiscard]] Plan Make(float startX, float startZ, const Grid<uint8_t>& keepClear, uint64_t seed, const Settings& settings);

/// The place in corners of the centre of the player's town that Make gives for the same start, seed and settings
[[nodiscard]] Site TownOfStart(float startX, float startZ, uint64_t seed, const Settings& settings);

/// A neutral town at a place in corners, with the buildings, people and fields of a town of the start. The seed and the
/// number of the town pick the direction of the town, the houses, the people and each turn.
[[nodiscard]] Town MakeTown(Site site, Tribe tribe, const Grid<uint8_t>& keepClear, uint64_t seed, uint32_t number,
                            const Settings& settings, const TownMix& mix);

/// The fish farms of a neutral town: a chain of 10 metre squares on the shore near the town. The seed and the number of
/// the town decide if the town has fish farms, and how many. A town far from the sea has fish farms less often.
/// `aboveSea` gives the height of each corner above the sea in metres, and `seaDistance` the distance to the sea in
/// corners. A fish farm does not go where `keepClear` is not 0, or on a square of `taken`.
[[nodiscard]] std::vector<Field> FishFarms(const Town& town, const Grid<float>& aboveSea, const Grid<float>& seaDistance,
                                           const Grid<uint8_t>& keepClear, std::span<const Field> taken, uint64_t seed,
                                           uint32_t number, const FarmSettings& settings);
} // namespace openblack::landgen::settlement
