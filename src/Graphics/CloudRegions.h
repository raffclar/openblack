/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): the weather of the regions between 20 and 500 km for the volumetric clouds. Each region
/// has its own cover, its own size of cells and its own pattern: small fair-weather cumulus, larger congestus, flat
/// sheets, open cells and streets along the wind. Thus no one size of cell repeats to the horizon when the eye is high.
/// These are the CPU twins of the shader's rules in clouds.sh (CloudRegionAt, CloudCellsAt), for the tests.
namespace openblack::graphics::cloud_regions
{

/// Near the island the clouds keep the game's own weather. The regions start at this distance from the map's middle,
/// in world units, and are complete at the second distance.
inline constexpr float k_KeepNear = 10000.0f;
inline constexpr float k_KeepFar = 30000.0f;
/// The size of the small and the large cells, as a part of the size of the usual cells
inline constexpr float k_SmallCells = 0.38f;
inline constexpr float k_LargeCells = 2.3f;
/// The width of the usual cells, in world units (the weather tile has six cells across)
inline constexpr float k_UsualCellWidth = 14000.0f / 6.0f;
/// The streets lie along the two nearest of this many fixed axes round a half turn
inline constexpr int k_StreetAxes = 8;
/// The share of the field round its middle that the ring of an open cell holds
inline constexpr float k_RingWidth = 0.3f;

/// How much of the region's own weather shows at a distance from the map's middle: 0 near the island, 1 far
[[nodiscard]] float RegionShare(float distance);
/// The weights of the small, the usual and the large cells for the size of the cells of a region, 0 (small) to 1
/// (large). Two sizes next to each other share the weight; the weights add up to 1.
[[nodiscard]] glm::vec3 SizeWeights(float size);
/// Two patterns of cells, each 0 to 1 round a middle of 0.5, mixed by weights. The mix keeps the contrast of one
/// pattern, so that the clouds do not go flat where two sizes meet.
[[nodiscard]] float MixPatterns(glm::vec3 values, glm::vec3 weights);
/// The width of the cells of a region, in world units, for the weights of its sizes (SizeWeights)
[[nodiscard]] float CellWidth(glm::vec3 weights);
/// How much the edge of a cell softens for the size of a pixel on the clouds, in world units. The edge goes over a
/// part of the field that grows with the pixel, so that a far cloud has no hard edge.
[[nodiscard]] float EdgeSoftness(float pixel, float cellWidth);
/// The field of open cells: cloud in rings round clear middles, where the field of closed cells has its edges
[[nodiscard]] float OpenRing(float field);
/// The change of the cover in a region, as a share of the cover, for two octaves of billows (each about -0.3 to 0.3
/// round 0) and the region's share. Clear regions keep a little cloud.
[[nodiscard]] float CoverShare(float billows, float regionShare);

/// How much a read of the noise that adds detail to the far sheet shows, 0 to 1, for the size of a pixel on the clouds
/// and the tile of the read, in world units. The shapes of the noise are about a quarter of its tile wide. A read shows
/// fully where a pixel is smaller than 0.12 of its shapes, and not at all where it is larger than 0.45, so that no shape
/// shimmers.
[[nodiscard]] float SheetDetailWeight(float pixel, float tile);

/// The two fixed axes that the streets of a region lie along, and how much of the second one shows
struct StreetBlend
{
	/// The first axis, 0 to k_StreetAxes - 1
	int first {0};
	/// The second axis, the next one round the half turn
	int second {1};
	/// How much of the second axis's pattern shows, 0 to 1
	float share {0.0f};
};

/// The two fixed axes nearest to the direction of the wind, in radians, which the streets lie along. A street has no front and
/// back, so a half turn gives the same axes. As the direction turns, the share moves smoothly from one axis to the next.
[[nodiscard]] StreetBlend StreetAxes(float angle);
/// The direction along a fixed axis of the streets, a unit vector across the ground. The axis after the last is the
/// first again.
[[nodiscard]] glm::vec2 StreetAxisDirection(int axis);

} // namespace openblack::graphics::cloud_regions
