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
#include <vector>

#include "landgen/Grid.h"

/// The player's first town and temple on the start area: the kinds of building, people and field that the lands of the
/// story use
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

struct Plan
{
	Tribe tribe {Tribe::Celtic};
	/// The temple of the player, in metres, and its turn in thousandths of a radian
	float citadelX {0.0f};
	float citadelZ {0.0f};
	int32_t citadelRotation {0};
	/// The place of the town, which is the place of its centre
	float townX {0.0f};
	float townZ {0.0f};
	/// The town centre first, then the storage pit, the creche and the houses
	std::vector<Placed> buildings;
	std::vector<Person> villagers;
	std::vector<Field> fields;
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
};

/// The plan of the start: the temple on one side of the middle of the start area, and the town on the other side. The
/// seed picks the tribe, the direction of the line, the houses, the people and each turn. A building or a field does not
/// go where `keepClear` is not 0, such as water and the banks of rivers. The start area is in corners.
[[nodiscard]] Plan Make(float startX, float startZ, const Grid<uint8_t>& keepClear, uint64_t seed, const Settings& settings);
} // namespace openblack::landgen::settlement
