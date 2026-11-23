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

#include <optional>
#include <span>
#include <vector>

#include "3D/MapCoords.h"

/// The rules the land's forests are chosen and ranked by
namespace openblack::ecs::land_forests
{

/// A forest's worth as a home for a flock, by its distance in the map's whole units. Only the distance counts (how many
/// trees it has is weighed and then thrown away), and anything further than a few centimetres scores the same.
[[nodiscard]] float LairScore(int32_t distance);

/// Which of the forests, met in order, a flock makes its home by: the first is taken without a score, then each one that
/// scores strictly more than the best score so far (which starts at 0) replaces it. With every forest scoring the same,
/// the second forest wins.
[[nodiscard]] std::optional<size_t> LairForest(std::span<const float> scores);

/// The nearest, met in order, by whole distances: only a strictly nearer one replaces the one before
[[nodiscard]] std::optional<size_t> Nearest(std::span<const int32_t> distances);

/// A forest of the land as a search near a place measures it: the distance from the place to the forest's place, and
/// whether the forest is of the kind that the search looks for
struct ReachMeasure
{
	float distance;
	bool fits;
};
/// The forest that a search near a place finds, met in order: the nearest that fits and that is strictly nearer than
/// the reach. Only a strictly nearer one replaces the one before, so the first of equals stays.
[[nodiscard]] std::optional<size_t> NearestWithin(std::span<const ReachMeasure> forests, float reach);

/// The forests within a town's reach: their nearest point strictly nearer than the reach, and their wood not nothing
[[nodiscard]] bool NearTown(float distance, float reach, float wood);
/// The point on a big forest's edge towards a place, in whole map units: the centre plus a step of the radius at the
/// angle from the centre to the place
[[nodiscard]] map_coords::MapCoords EdgeTowards(const map_coords::MapCoords& centre, float radius,
                                                const map_coords::MapCoords& place);
/// A forest of the land as a town measures it: the distance from the town to its nearest point, and its wood
struct TownMeasure
{
	float distance;
	float wood;
};
/// The forests that a town lists, given the land's forests newest first: each one near the town (NearTown). The town puts
/// each one at the front of its list, so the list holds them oldest first.
[[nodiscard]] std::vector<size_t> TownList(std::span<const TownMeasure> forests, float reach);
/// The land's forest count once a forest is made with a number of its own: moved past the number only when the number is
/// beyond the count
[[nodiscard]] uint32_t CountAfter(uint32_t count, uint32_t number);

} // namespace openblack::ecs::land_forests
