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
#include <optional>

#include <EDTFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Following the ways of the camera editor's tracks (camera.edt): each way is a chain of cubic Beziers through its
/// points, each point reached at its time, the speed changing evenly from one point's speed to the next
namespace openblack::camera_track
{

/// A way's Bezier from point `segment` to the next, at `t` from 0 to 1, summed in the game's order (which differs for x)
[[nodiscard]] glm::vec3 Bezier(const edt::EDTWay& way, uint32_t segment, float t);

/// Where a runner along a way has got to: the segment it is on, how its length is spread along that segment, and the
/// last Bezier parameter it took
class WayRunner
{
public:
	/// At the start of the way
	explicit WayRunner(const edt::EDTWay& way);

	/// The point `sample` milliseconds along the way. The segment is the first whose end time isn't before the sample;
	/// at or after the way's duration it is the last point, at or before 0 the first. In between, the distance along
	/// the segment's chord grows evenly faster or slower from the segment's starting speed so that the whole chord is
	/// covered at its end time, and the share of the chord covered gives the Bezier parameter.
	glm::vec3 Get(const edt::EDTWay& way, int32_t sample);

	[[nodiscard]] uint32_t GetSegment() const noexcept { return _segment; }
	[[nodiscard]] float GetParameter() const noexcept { return _t; }

private:
	void BuildTable(const edt::EDTWay& way);

	uint32_t _segment {0};
	/// The segment's length up to t = i / 127, as a share of its whole length times 127; the last is 1
	std::array<float, 128> _table {};
	float _t {0.0f};
};

/// A thing walking a track as a script told it to: it goes where the track's camera looks, 100 milliseconds of the
/// track a turn, until it has gone its share of the way
struct Walk
{
	WayRunner runner;
	/// The share of the track's way it stops at
	float to {1.0f};
	/// Along the track, or from its end back
	bool forward {true};
	/// How far it has got, in milliseconds of the track
	float current {0.0f};
	/// How far it gets each turn, in milliseconds of the track
	float step {100.0f};
};

/// A walk from `from` to `to`, shares of the track's way
[[nodiscard]] Walk StartWalk(const edt::EDTTrack& track, bool forward, float from, float to);

/// How much of the track's way it has walked
[[nodiscard]] float Percentage(const Walk& walk, const edt::EDTTrack& track);

/// A turn of a walk: where on the land it goes this turn, or nothing once it has gone its share of the way and stops.
/// It goes where the track's camera looks when the camera is where the walk has got to.
[[nodiscard]] std::optional<glm::vec2> WalkTurn(Walk& walk, const edt::EDTTrack& track);

} // namespace openblack::camera_track
