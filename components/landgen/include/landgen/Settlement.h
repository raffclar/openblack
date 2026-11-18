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
};

/// The kinds of house that every tribe of the plan has
constexpr std::array<Building, 5> k_Houses {Building::AbodeA, Building::AbodeC, Building::AbodeD, Building::AbodeE,
                                            Building::AbodeF};

enum class Villager : uint8_t
{
	Forester,
	Housewife,
};

/// A building of the town. The place is in metres, and the turn is in thousandths of a radian, as a land script gives it.
struct Placed
{
	Building building;
	float x;
	float z;
	int32_t rotation;
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
                            const Settings& settings);
} // namespace openblack::landgen::settlement
