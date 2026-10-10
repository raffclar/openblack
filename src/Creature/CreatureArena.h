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

#include <functional>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Where creatures fight: an arena, a circle on the land ringed by a sheet of light while a fight is on in it.
///
/// Two creatures about to fight look for an arena from the middle between them. The nearest arena closer than a
/// distance that grows with the starting creature's size and running speed is taken, shrunk to the size the fight wants
/// if it is bigger. Without one, a clear place is searched for nearby, for a circle of the size wanted, then of nine,
/// eight, seven and six tenths of it; the new arena stands there for that fight only. The land's own arenas, made by its
/// script, stay for good.
///
/// The search for a clear place looks at the land's cells within a distance round the middle: a cell is usable when
/// nothing stands in the way there. Of the squares of cells the circle covers, the one whose every cell under the circle
/// is usable and whose middle is nearest the start is taken.
namespace openblack::creature_arena
{

/// An arena is looked for this far round the middle between the fighters
constexpr float k_SearchRange = 500.0f;
/// Without room for the arena wanted, a smaller one is tried, a tenth smaller each time, while it is more than this share
constexpr float k_ScaleStep = 0.1f;
constexpr float k_SmallestScale = 0.59f;
/// The distance within which an arena is taken is this many times the creature's running pace
constexpr float k_ReusePerPace = 10.0f;

/// The ring of light round an arena while its fight is on: this many points round the circle, as bright as this grey,
/// standing a sixth of the radius high and spread out by a hundredth at the top, its strength moving one point along
/// every few hundredths of a second
constexpr uint32_t k_RingPoints = 32;
constexpr uint32_t k_RingRgb = 0xB4B4B4;
constexpr float k_RingHeightShare = 1.0f / 6.0f;
constexpr float k_RingSpread = 1.01f;
constexpr float k_RingShiftSeconds = 0.03f;
constexpr float k_RingStrength = 1.0f;

/// The sound played as a fight takes its arena, from the game's own sounds
constexpr uint32_t k_ArenaDrawnSample = 176;

/// The distance within which an arena is taken: ten times the pace the starting creature runs at, from its species'
/// running share and its size
[[nodiscard]] float ReuseDistance(float runShare, float size);

/// The nearest of the arenas there are (by their middles in 16.16 map units, in the order they are looked through) to a
/// point closer than a distance in metres, the first of those equally near: its place in the list
[[nodiscard]] std::optional<size_t> Nearest(std::span<const glm::ivec2> arenas, glm::ivec2 point, float within);

/// Whether a land cell may lie under an arena
using UsableCell = std::function<bool(glm::ivec2 cell)>;
/// The middle of the nearest clear circle of a diameter to a point within a range, in 16.16 map units (x, z)
[[nodiscard]] std::optional<glm::ivec2> FindClearPlace(glm::ivec2 start, float diameter, float range, const UsableCell& usable);

/// Where an arena goes and its radius: found clear for one of the sizes tried, from the size wanted
struct Placed
{
	glm::ivec2 centre;
	float radius;
	float scale;
};
[[nodiscard]] std::optional<Placed> Place(glm::ivec2 start, float wantedRadius, const UsableCell& usable);

/// The ring's points round an arena, each on the land's height there; the last closes the ring on the first
using GroundHeight = std::function<float(glm::vec2)>;
[[nodiscard]] std::vector<glm::vec3> RingPoints(glm::vec2 centre, float radius, const GroundHeight& ground);

} // namespace openblack::creature_arena
