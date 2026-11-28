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

#include "landgen/Features.h"
#include "landgen/Grid.h"

/// The start areas of the other players of a land for more than one player, as on the skirmish lands. Each player gets
/// a temple and a town on ground that is as good as the ground of player one.
namespace openblack::landgen::players
{
/// The most players of a generated land. The skirmish lands have two to four players.
constexpr uint32_t k_MostPlayers = 4;

/// The middle of a start area, in corners
struct Start
{
	float x;
	float z;
};

/// The ground that a start area needs, as for the start of player one. The heights are altitudes and the distances are
/// in corners.
struct Ground
{
	float lowest;
	float highest;
	float nearestSea;
	float farthestSea;
	float riverDistance;
	/// The radius of the flat ground of the start area
	float flatRadius;
	/// The part of the land of each corner, as `paths::Parts` gives it. A start is on the part of the start of player
	/// one, so that a path can join the two starts. Without the parts, a start can be on any part.
	const Grid<uint32_t>* parts {nullptr};
	/// The corners under a lake or on its shore. A lake corner is not dry land in the ground of a start. Without the
	/// lakes, all ground above the sea is dry land.
	const Grid<uint8_t>* lakes {nullptr};
	/// The height units that the game draws at each corner. The test for flat ground uses their slope, because the
	/// curve of the heights makes the high ground steeper. Without them, the test uses the heights.
	const Grid<float>* drawnUnits {nullptr};
};

/// The least and the most value of a share
struct ShareRange
{
	float least;
	float most;
};

/// The rules for the start areas of the other players, from the five skirmish lands (TwoGods, ThreeGods, FourGods,
/// Island Wars and Firestorm)
struct Settings
{
	/// The players of the land, with player one. One player gives the land of player one only.
	uint32_t count {1};
	/// The distance in metres from the town of a start to the nearest town of another start: the least and the most of
	/// the skirmish lands
	float nearestRival {508.0f};
	float farthestRival {1079.0f};
	/// The ground in this distance in metres from the middle of a start is the ground of that start
	float groundRadius {500.0f};
	/// A corner is flat when its rise over its run is below this value: the slope of 10 degrees
	float flatRise {0.17632698f};
	/// What a difference from the start of player one costs: one altitude of roughness costs 1. The values are for one
	/// altitude of height, for the full difference of north to south, and for a share of the land or of the flat ground
	/// that is all different.
	float heightCost {1.0f};
	float climateCost {40.0f};
	float landCost {100.0f};
	float flatCost {100.0f};
	/// The fairness of the dry land and of the flat ground of the starts on the skirmish lands: the start with the
	/// least ground has this share of the ground of the start with the most. The search keeps the starts in the two
	/// ranges when a place can do this.
	ShareRange landShare {.least = 0.84f, .most = 0.98f};
	ShareRange flatShare {.least = 0.78f, .most = 0.98f};
	/// The step in corners between two places that the search tries, and between two corners that it counts
	int step {4};
	int countStep {2};
	/// The far woods and the near woods of a start are the trees in these distances in metres from its town. A start
	/// whose woods have fewer trees than this share of the most trees of a start gets more trees. On the skirmish lands,
	/// the start with the fewest trees has 0.60 to 0.94 of the trees of the start with the most in 300 m, and 0.45 to
	/// 0.87 in 500 m.
	std::array<float, 2> woodRadii {500.0f, 300.0f};
	float woodShare {0.75f};
	/// A tree that the woods get keeps this distance in metres from each other thing, and this distance from a big
	/// forest of scale 1
	float treeGap {2.5f};
	float bigForestClearing {33.0f};
};

/// The ground in the reach of a start: the dry land and the flat dry land, in square corners
struct StartGround
{
	float land;
	float flat;
};

/// The least value over the most value, or 0 when there is no value or the most value is 0
[[nodiscard]] float Fairness(std::span<const float> values);

/// How far the fairness of the dry land and of the flat ground of the starts is from settings.landShare and
/// settings.flatShare: the sum of the two distances. The distance is 0 when the two values are in their ranges.
[[nodiscard]] float DistanceToRanges(std::span<const StartGround> grounds, const Settings& settings);

/// The rise over the run at each corner, from the heights in altitudes. The rise along each axis is the difference of
/// the two corners next to the corner, or of the corner and the next corner at the edge of the map.
[[nodiscard]] Grid<float> Rises(const Grid<float>& heights);

/// The dry land and the flat dry land within settings.groundRadius of a place in corners. The heights are altitudes, and
/// `rises` comes from Rises. A corner that `lakes` marks is not dry land.
[[nodiscard]] StartGround GroundAt(const Grid<float>& heights, const Grid<float>& rises, float x, float z,
                                   const Settings& settings, const Grid<uint8_t>* lakes = nullptr);

/// The start areas of players two and up, in the order of the players. `townOffsets` gives the place of the town of
/// each player from the middle of its start, in corners, player one first. The town of each start is at least
/// settings.nearestRival from each town before it. It is at most settings.farthestRival from the nearest town before it
/// when such a place exists. Of these places, the start is the place that keeps the fairness of the ground of the starts
/// nearest to settings.landShare and settings.flatShare. Of those, it is the place whose roughness, height, climate and
/// ground are nearest to the start of player one. The list is shorter when the land has no room for all players.
[[nodiscard]] std::vector<Start> FindStarts(const Grid<float>& heights, const Grid<float>& seaDistance,
                                            const Grid<float>& riverDistance, Start first, std::span<const Start> townOffsets,
                                            const Ground& ground, const Settings& settings);

/// The living trees in a distance in metres from a town in metres
[[nodiscard]] size_t TreesNear(std::span<const features::Feature> features, float townX, float townZ, float radius);

/// Gives more trees to the woods of each start that has fewer trees than settings.woodShare of the start with the most
/// trees, first for the far woods and then for the near woods. A new tree is a copy of a tree of the woods with the
/// most trees, at the same place from the town or at that place turned about the town. The copy goes only where the
/// ground is free and suits its kind of tree, and where no other thing is near. `towns` gives the town of each start in
/// corners.
void EvenTheWoods(std::vector<features::Feature>& features, const features::Ground& ground, std::span<const Start> towns,
                  const Settings& settings);

/// The seed of the start of a player (from 1 for player two). Player one has the seed of the land.
[[nodiscard]] uint64_t SeedOfPlayer(uint64_t seed, uint32_t player);
} // namespace openblack::landgen::players
