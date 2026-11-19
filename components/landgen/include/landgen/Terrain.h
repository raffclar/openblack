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

/// The shape of the land: the first heights and the erosion that wears them. Heights are in the altitude units of a
/// landscape (0.67 m each), with the sea bed below 0. The corners are 10 m apart.
namespace openblack::landgen::terrain
{
/// The highest altitude that a landscape can hold
constexpr float k_HighestAltitude = 255.0f;
/// The highest altitude that the game draws at sea level
constexpr float k_SeaLevel = 3.0f;

struct BaseSettings
{
	/// The corners next to each edge of the map that stay under the sea, so that the land is an island
	int seaMargin {40};
	/// How many mountain ranges cross the island
	int ranges {0};
	/// The height of a crest of a range, before the erosion wears it
	float mountainHeight {300.0f};
	/// The height of the inland plains, and the part of the land from the coast inwards where the ground rises to them
	float plainHeight {42.0f};
	float plainRise {0.1f};
	/// The height of the hills on the plains, up or down
	float hillHeight {100.0f};
	/// How much of the map the island takes, from about -0.3 (small) to 0.3 (large)
	float landAmount {0.0f};
	/// How many hollows the low land has, each a radius in corners, to hold the lakes and lagoons
	int hollows {6};
	float hollowRadius {16.0f};
	/// The highest ground where a hollow can be, and the altitude of its floor
	float hollowGround {15.0f};
	float hollowFloor {-6.0f};
};

/// The first heights of the island: an outline of warped noise, plains and hills, and mountain ranges along random
/// lines, each with ridged noise for its crests and valleys. Hollows in the low land below the hollow ground hold the
/// lakes.
[[nodiscard]] Grid<float> BaseHeights(int side, uint64_t seed, const BaseSettings& settings);

struct HydraulicSettings
{
	/// How many drops of rain fall, each at a random corner
	int drops {400000};
	/// The most steps that a drop takes before it dries up
	int steps {80};
	/// How much a drop keeps its direction rather than flow down the slope, from 0 to 1
	float inertia {0.05f};
	/// How much sediment a drop can hold, for its speed, water and the drop of its height
	float capacity {4.0f};
	float minCapacity {0.01f};
	/// The part of the free capacity that a drop takes from the ground in a step
	float erosion {0.3f};
	/// The part of the sediment above the capacity that a drop puts down in a step
	float deposition {0.3f};
	/// The part of its water that a drop loses in a step
	float evaporation {0.012f};
	float gravity {4.0f};
	/// The radius in corners from which a drop takes ground
	int radius {3};
	/// The scale from altitude units to the units of the drop's slope. Lower values make the slopes gentler for the drops.
	float heightScale {0.067f};
};

/// Rain wears the land: drops run down the slopes, take ground where they speed up and put it down where they slow
/// down or go into the sea. Valleys cut into the slopes and the sediment fills the low ground.
void ErodeHydraulic(Grid<float>& heights, uint64_t seed, const HydraulicSettings& settings);

struct ThermalSettings
{
	int passes {30};
	/// The steepest difference of altitude to the next corner along an axis that the ground keeps on the plains and on
	/// the mountains. Between them the limit changes with the altitude.
	float plainTalus {9.0f};
	float mountainTalus {18.0f};
	/// The part of the ground above the limit that slides down in a pass
	float rate {0.4f};
};

/// The ground of the slopes that are too steep slides down to the corners below them, until no slope is steeper than
/// its limit. All the corners slide at the same time in a pass, so that the order of the corners has no effect. The
/// corners that have a value other than 0 in the fixed span do not move, and no ground slides to or from them.
void ErodeThermal(Grid<float>& heights, const ThermalSettings& settings, std::span<const uint8_t> fixed = {});

struct TerraceSettings
{
	/// The altitude of the lowest edge of a terrace, from the lowest to the highest that the seed can pick
	float firstLevelLow {13.073f};
	float firstLevelHigh {30.53f};
	/// The difference of altitude between two edges, from the lowest to the highest that the seed can pick
	float spacingLow {51.999f};
	float spacingHigh {57.87f};
	/// No edge is higher than this altitude
	float highestLevel {235.0f};
	/// The part of the shape of the ground that a terrace keeps: less on the low land, more on the mountains
	float keepLow {1.6f};
	float keepHigh {0.84f};
	float keepLowAltitude {90.0f};
	float keepHighAltitude {190.0f};
	/// The noise that moves the edges of the terraces: its largest change of altitude, and its waves across the map
	float edgeNoise {310.0f};
	float edgeFrequency {16.0f};
	/// How far the warp moves the noise of the edges, in waves of the noise, and the part of the noise that is ridged
	float edgeWarp {2.0f};
	float edgeRidge {0.7f};
	/// The largest change of the edge noise on the calm parts of the land, and the waves of the calm and rough parts
	/// across the map. The edge noise is the change on the rough parts.
	float calmNoise {120.0f};
	float roughFrequency {3.0f};
	/// An area of a step with fewer corners than this goes down one step
	int smallestArea {17};
	/// The low land rises slower and slower above this part of the first edge, and stays below the first edge
	float lowKnee {0.6f};
	/// The rise of the face of a cliff in altitude units for each corner, from the lowest to the highest that the noise
	/// gives. 16 is about 47 degrees and 34 is about 66 degrees.
	float cliffRiseLow {28.744f};
	float cliffRiseHigh {36.749f};
	float cliffFrequency {11.0f};
};

/// The altitudes of the edges of the terraces for the seed, from the lowest up
[[nodiscard]] std::vector<float> TerraceLevels(uint64_t seed, const TerraceSettings& settings);

/// The step of a terrace for each corner: the index of the edge of the terrace that the corner is on (0 below the first
/// edge). The edge noise moves each corner up or down before the step is found. A corner that is higher or lower than
/// its four neighbours along the axes takes the nearest step of the neighbours, so that no step holds one corner only.
/// A corner that has a value in the highest edge span is on no step whose edge is above that altitude.
[[nodiscard]] Grid<uint8_t> TerraceSteps(const Grid<float>& heights, std::span<const float> levels, uint64_t seed,
                                         const TerraceSettings& settings, std::span<const float> highestEdge = {});

/// Cuts the land above the first edge into flat terraces with sharp cliffs between them, as on the lands of the story.
/// Each terrace keeps a part of the shape of its ground. The cliff is on the low side of an edge: the ground rises to the
/// edge in the width of the cliff. The land below the first edge, and the sea, do not change. The result has 1 for each
/// corner of a cliff, so that the erosion after this stage can keep the cliffs.
/// The highest edge span limits the steps as TerraceSteps does.
[[nodiscard]] Grid<uint8_t> Terrace(Grid<float>& heights, uint64_t seed, const TerraceSettings& settings,
                                    std::span<const float> highestEdge = {});

struct MassifSettings
{
	/// How many massifs the land has, from the lowest to the highest that the seed can pick
	int countLow {9};
	int countHigh {13};
	/// The radius of the main peak of a massif as a part of the side of the map, from the lowest to the highest that the
	/// seed can pick
	float radiusLow {0.45f};
	float radiusHigh {0.5f};
	/// The altitude of the main summit of a massif, from the lowest to the highest that the seed can pick
	float summitLow {240.0f};
	float summitHigh {254.0f};
	/// The power of the profile of a peak: 1 is a straight cone, more is steep at the summit and gentle at the foot
	float profile {1.244f};
	/// How many lower peaks stand next to the main peak of a massif, from the lowest to the highest that the seed can pick
	int sidePeaksLow {1};
	int sidePeaksHigh {2};
	/// The radius and the summit of a lower peak, as parts of those of the main peak
	float sideRadius {0.6f};
	float sideSummit {0.8f};
	/// The distance of a lower peak from the main peak, from the lowest to the highest part of the main radius
	float sideDistanceLow {0.35f};
	float sideDistanceHigh {0.7f};
	/// The main peak of a massif is at least this many of its radii from the sea
	float seaClearance {0.3f};
	/// How many places the seed tries for each massif. The place farthest from the sea takes the massif.
	int tries {12};
	/// The spurs: the waves of the spur noise round a peak, and the depth of the gullies between the spurs as a part of
	/// the height of the peak
	float spurWaves {2.5f};
	float gullyDepth {0.473f};
	/// How much the spurs turn from the summit to the foot, in waves of the spur noise
	float spurBend {5.0f};
	/// How much the noise moves the outline of a peak, as a part of its radius, and the waves of that noise across the map
	float outline {0.2f};
	float outlineFrequency {6.0f};
	/// The cliff bands of the massifs: the rise of a band in altitude units, from the lowest to the highest that the seed
	/// can pick, the part of the width of a band that its cliff takes, and the part of the rise of the band that the cliff
	/// takes. The rest of the band is a slope.
	float bandLow {44.184f};
	float bandHigh {55.541f};
	float faceWidth {0.1f};
	float faceRise {0.72f};
	/// How far the noise moves the cliff bands up or down in altitude units, and the waves of that noise across the map
	float bandNoise {35.0f};
	float bandFrequency {14.0f};
	/// Away from the massifs, the edge noise of the terraces lifts the ground by this many altitude units at most
	float lowlandRise {84.0f};
	/// The land away from the massifs rises slower and slower above the knee, and stays below the ceiling
	float lowlandKnee {60.572f};
	float lowlandCeiling {85.907f};
};

/// A peak of a massif, in corners, and the altitude of its summit
struct Peak
{
	float x;
	float z;
	float radius;
	float summit;
};

/// The peaks of the massifs of the land. The main peak of each massif is on the place farthest from the sea of the
/// places that the seed tries, away from the other massifs. The lower peaks stand next to it.
[[nodiscard]] std::vector<Peak> PlaceMassifs(const Grid<float>& heights, uint64_t seed, const MassifSettings& settings);

/// The height of the massifs at each corner, and 0 away from them. Each peak is a cone that is steep at the summit and
/// gentle at the foot. Spurs go out from the summit with gullies between them. The height always goes down from a summit
/// to the foot, so that the water can flow off a massif.
[[nodiscard]] Grid<float> MassifHeights(int side, std::span<const Peak> peaks, uint64_t seed, const MassifSettings& settings);

/// Puts the massifs on the land: each corner takes the height of the massifs where they are higher. A flood from the
/// sea and the edges of the map then fills the hollows that the massifs close. The corners that the massifs take are no
/// longer cliffs.
void RaiseMassifs(Grid<float>& heights, const Grid<float>& massifs, Grid<uint8_t>& cliffs);

/// The ground above the knee rises slower and slower, and stays below the ceiling. The ground below the knee does not
/// change. A ceiling that is not above the knee changes nothing.
void LowerHighGround(Grid<float>& heights, float knee, float ceiling);

struct RollSettings
{
	/// The largest change of altitude of the rolling ground, up or down, on the calm and on the rough parts of the land
	float calmAmount {10.015f};
	float roughAmount {21.934f};
	/// The waves of the noise of the rolling ground across the map, its octaves and the part of it that is ridged
	float frequency {18.0f};
	int octaves {3};
	float ridge {0.946f};
	/// The waves across the map of the noise that divides the calm and rough parts, and the part of the land that is calm
	float partFrequency {4.0f};
	float calmShare {0.302f};
	/// The rolling ground starts at this distance from the sea in corners, and gets its full change at the second distance
	float seaStart {4.0f};
	float seaFull {12.0f};
	/// The change is full below the first altitude and stops at the second, so that the high ground keeps its summits
	float highStart {57.302f};
	float highEnd {87.543f};
	/// The part of a fall of the ground that the high ground keeps, so that valleys cut into the high plateaus
	float highFall {0.984f};
	/// The rolling ground rises in steps: the rise of a step in altitude units (0 for no steps), the part of the width
	/// of a step that its face takes, and the part of the rise that the face takes
	float stepRise {23.961f};
	float stepFace {0.166f};
	float stepFaceRise {0.569f};
};

/// Makes the ground roll: noise of short waves moves the land up and down, more on the rough parts and on the high
/// ground. The shore and the sea do not change. A flood from the sea then fills the pits that the noise makes.
void RollGround(Grid<float>& heights, uint64_t seed, const RollSettings& settings);

/// The altitudes of the first corners from the water, at distances 1 to 4, as on all the lands of the story. The game
/// draws the first three at the sea level, and the fourth is the first ground above the water.
constexpr std::array<float, 4> k_ShoreSteps {2.0f, 3.0f, 3.2f, 4.0f};

struct ShelfSettings
{
	/// The sea next to the land is this high or lower, so that each corner of the sea holds water
	float seaFloor {1.0f};
	/// The waves across the map of the noise that divides the coast into beaches, banks and cliffs
	float sectorFrequency {7.0f};
	/// The coast is a beach where the noise is above the start, and all beach above the full value
	float beachStart {0.17f};
	float beachFull {0.27f};
	/// The coast is a cliff where the noise is below the start, and all cliff below the full value
	float cliffStart {0.15f};
	float cliffFull {-0.05f};
	/// The width of a beach in corners from the water, from the lowest to the highest that the noise gives
	float beachLow {25.0f};
	float beachHigh {45.0f};
	/// The altitude of the back of a beach. The front of the beach is at the fourth shore step.
	float beachBack {6.0f};
	/// The rise of the dunes behind a beach, in altitude units for each corner
	float duneRise {4.0f};
	/// The rise of the bank behind the strip, in altitude units for each corner, from the lowest to the highest that the
	/// noise gives. 10 is about 34 degrees and 22 is about 56 degrees.
	float bankLow {10.0f};
	float bankHigh {22.0f};
	float frequency {9.0f};
	/// The cliffs: the rise of the face for each corner, and the altitude of the top, from the lowest to the highest
	/// that the noise gives
	float cliffRise {28.0f};
	float cliffTopLow {40.0f};
	float cliffTopHigh {70.0f};
	/// The top of a cliff stays level for this many corners, then falls by the back slope for each corner
	float cliffDepth {6.0f};
	float cliffBack {3.0f};
	/// A cliff does not rise within the first distance of a river, and rises fully from the second distance, in corners
	float riverStart {2.0f};
	float riverFull {8.0f};
};

/// Makes the coast as on the lands of the story: the sea, the shore steps, then a beach, a bank or a cliff. The noise
/// divides the coast into sectors. A beach is a long flat strip of low ground with dunes behind it. A cliff rises
/// steeply from the shore steps, also where the land behind is low, but not at the rivers. On a bank, the ground near
/// the water goes down to the strip. The result is the width of the flat strip at each corner, in corners from the
/// water. riverDistance can be null.
[[nodiscard]] Grid<float> ShoreShelf(Grid<float>& heights, const Grid<float>* riverDistance, uint64_t seed,
                                     const ShelfSettings& settings);

/// The largest difference of height to the next corner along an axis, at each corner
[[nodiscard]] Grid<float> Slopes(const Grid<float>& heights);
} // namespace openblack::landgen::terrain
