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

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The footpath or stream that the editor draws, point by point, before it becomes a thing on the land, as plain rules
namespace openblack::editor::paths
{

enum class Kind : uint8_t
{
	/// A path that villagers walk from node to node
	Footpath,
	/// A river laid into the land from point to point
	Stream,
};

/// Two points of a path closer than this, in metres, are one point: a second click on the same place adds nothing
constexpr float k_MinimumSpacing = 2.0f;

/// The points of a path that the editor draws, on the land, in the order of the clicks
class Draft
{
public:
	/// Adds a point after the last, unless it is too near the last. Gives whether it added the point.
	bool Add(glm::vec3 point);
	/// Takes away the newest point, if there is one
	void RemoveLast();
	void Clear() { _points.clear(); }
	/// A path needs at least two points
	[[nodiscard]] bool CanFinish() const { return _points.size() >= 2; }
	[[nodiscard]] std::span<const glm::vec3> Points() const { return _points; }

private:
	std::vector<glm::vec3> _points;
};

/// The streams and footpaths that the editor drew on the land that is open, to save
struct DrawnPaths
{
	/// The numbers of the streams
	std::vector<int> streams;
	std::vector<entt::entity> footpaths;
	/// The list stays at the next reset of the registry: the editor found the paths again in a world that it loaded
	bool keepAtNextReset {false};
};

/// The number for a new stream: one more than the highest number that a stream has, or 0 when there are none
[[nodiscard]] int NextStreamId(std::span<const int> ids);

/// The place of a point in map units, as the land's script gives the places of footpath nodes and stream points
[[nodiscard]] glm::ivec2 PlaceOf(glm::vec3 point);

/// The length of a path along its points, in metres across the land
[[nodiscard]] float Length(std::span<const glm::vec3> points);

} // namespace openblack::editor::paths
