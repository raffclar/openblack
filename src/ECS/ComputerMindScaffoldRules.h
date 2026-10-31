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

/// The maths of the computer god's scaffold rules: which scaffold the hand takes from a workshop, which town wants the
/// building of a scaffold, and how much a workshop wants wood. Each formula keeps the game's order of float operations.
namespace openblack::computer_mind_rules
{

/// The god's hand looks for scaffolds within its influence power and this many more metres
constexpr float k_ScaffoldReachMore = 50.0f;
/// A town with more adult places than this does not want a house from a scaffold
constexpr int32_t k_MostAdultPlacesForHouses = 175;
/// The god puts a scaffold at least half the radius of a town from its centre, and at most this share of the radius more
constexpr float k_ScaffoldPlaceSpread = 0x1.555556p-1f;
constexpr float k_ScaffoldPlaceNearest = 0.5f;
/// The god tries four places round a town for a scaffold, an eighth of a turn apart
constexpr int32_t k_ScaffoldPlaceTries = 4;
constexpr float k_ScaffoldPlaceStep = 0x1.921fb6p-1f;
/// A cell is too steep for a scaffold when the altitudes of its corners differ by more than this
constexpr int32_t k_ScaffoldMostRise = 10;

/// What a town wants for a scaffold rule
struct ScaffoldTownFacts
{
	/// How much the town wants houses and civic buildings, with their boosts
	float wantsAbodes {0.0f};
	float wantsCivic {0.0f};
	/// The places for adults in the houses of the town
	int32_t adultPlaces {0};
};

/// Whether the god wants to join two scaffolds: its towns together want civic buildings more than houses
[[nodiscard]] bool WantsToCombine(float wantsAbodes, float wantsCivic);

/// How good a scaffold is for the god to use, before the pull of its place. A scaffold of 1 or 2 is for a house, a
/// scaffold of 3 to 5 is for a civic building, and a scaffold of 7 is better near the hand. `nearness` is how near the
/// hand the scaffold is (see gutils::GetDistanceModifier).
[[nodiscard]] float ScaffoldUseValue(uint32_t value, const ScaffoldTownFacts& town, float nearness);

/// How much a town wants the building of a scaffold of a value: the want for houses for a value of 2 or less, the want
/// for civic buildings for 3 or 4, else 1
[[nodiscard]] float ScaffoldTownDesire(uint32_t value, const ScaffoldTownFacts& town);

/// The distance from the centre of a town that the god tries for a scaffold, from a random share of the spread
[[nodiscard]] float ScaffoldPlaceDistance(float randomSpread, float radius);

/// Whether a cell is too steep for a scaffold, from the altitudes of its four corners
[[nodiscard]] bool TooSteepForScaffold(const std::array<uint8_t, 4>& corners);

/// A 16.16 map value with its cell kept on the map: the cell, read signed, is kept between 0 and `cells` - 1. The part
/// of the cell does not change.
[[nodiscard]] int32_t KeepCellOnMap(int32_t fixed, int32_t cells);

/// How much a workshop wants wood for the god: its room for more scaffolds, as a share of the most that it makes
[[nodiscard]] float WorkshopFillValue(int32_t room, float mostToProduce);

} // namespace openblack::computer_mind_rules
