/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraTrack.h"

#include <cmath>

#include <algorithm>

#include "3D/MapCoords.h"

using namespace openblack;
using namespace openblack::camera_track;

namespace
{
constexpr float k_OneOver127 = 0.007874015718698502f;
/// Milliseconds to seconds, as the game's float has it
constexpr float k_Milliseconds = 0.0010000000474974513f;
constexpr int k_Steps = 127;

glm::vec3 ToVec3(const std::array<float, 3>& point)
{
	return {point[0], point[1], point[2]};
}

/// A vector's length, summed z, y, then x
float Length(const glm::vec3& v)
{
	return std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
}

/// The runner's Bezier: x, y and z all summed ((p3 t³ + h2 3ut²) + h1 3u²t) + p0 u³
glm::vec3 RunnerBezier(const edt::EDTWay& way, uint32_t segment, float t)
{
	const float u = 1.0f - t;
	const float uu = u * u;
	const float tt = t * t;
	const float uuu = u * uu;
	const float c1 = (uu * t) * 3.0f;
	const float c2 = (u * tt) * 3.0f;
	const float ttt = t * tt;
	const auto p0 = ToVec3(way.points[segment]);
	const auto h1 = ToVec3(way.handles[segment][0]);
	const auto h2 = ToVec3(way.handles[segment][1]);
	const auto p3 = ToVec3(way.points[segment + 1]);
	glm::vec3 out;
	for (int c = 0; c < 3; ++c)
	{
		out[c] = ((p3[c] * ttt + h2[c] * c2) + h1[c] * c1) + p0[c] * uuu;
	}
	return out;
}
} // namespace

glm::vec3 camera_track::Bezier(const edt::EDTWay& way, uint32_t segment, float t)
{
	const float u = 1.0f - t;
	const float uu = u * u;
	const float tt = t * t;
	const float uuu = u * uu;
	const float c1 = (uu * t) * 3.0f;
	const float c2 = (u * tt) * 3.0f;
	const float ttt = tt * t;
	const auto p0 = ToVec3(way.points[segment]);
	const auto h1 = ToVec3(way.handles[segment][0]);
	const auto h2 = ToVec3(way.handles[segment][1]);
	const auto p3 = ToVec3(way.points[segment + 1]);
	return {((h2.x * c2 + h1.x * c1) + p3.x * ttt) + p0.x * uuu, ((p3.y * ttt + h2.y * c2) + h1.y * c1) + p0.y * uuu,
	        ((p3.z * ttt + h2.z * c2) + h1.z * c1) + p0.z * uuu};
}

WayRunner::WayRunner(const edt::EDTWay& way)
{
	BuildTable(way);
}

void WayRunner::BuildTable(const edt::EDTWay& way)
{
	// The segment's whole length in 127 steps, then its length up to each step as a share of that, times 127
	float total = 0.0f;
	auto previous = ToVec3(way.points[_segment]);
	for (int i = 0; i <= k_Steps; ++i)
	{
		const auto point = RunnerBezier(way, _segment, static_cast<float>(i) * k_OneOver127);
		total = Length(point - previous) + total;
		previous = point;
	}
	_table[0] = 0.0f;
	_table[k_Steps] = 1.0f;
	float running = 0.0f;
	previous = ToVec3(way.points[_segment]);
	for (int i = 0; i < k_Steps; ++i)
	{
		const auto point = RunnerBezier(way, _segment, static_cast<float>(i) * k_OneOver127);
		running = Length(point - previous) + running;
		previous = point;
		if (i != 0)
		{
			_table[static_cast<size_t>(i)] = running * 127.0f / total;
		}
	}
}

glm::vec3 WayRunner::Get(const edt::EDTWay& way, int32_t sample)
{
	const auto last = static_cast<uint32_t>(way.points.size() - 1);
	const auto f = static_cast<float>(sample);
	uint32_t segment = 0;
	while (segment + 1 < last && way.times[segment + 1] < f)
	{
		++segment;
	}
	if (segment != _segment)
	{
		_segment = segment;
		BuildTable(way);
	}
	if (sample >= way.duration)
	{
		return ToVec3(way.points[last]);
	}
	if (sample <= 0)
	{
		_t = 0.0f;
		return ToVec3(way.points[0]);
	}
	// The share of the chord covered by this time
	const float chord = Length(ToVec3(way.points[segment + 1]) - ToVec3(way.points[segment]));
	const float startSpeed = way.speeds[segment];
	const float span = (way.times[segment + 1] - way.times[segment]) * k_Milliseconds;
	const float into = (f - way.times[segment]) * k_Milliseconds;
	float twice = chord - span * startSpeed;
	twice = twice + twice;
	const float acceleration = twice / (span * span);
	const float share = (acceleration * into * 0.5f + startSpeed) * into / chord;
	// The table's entries around the share give the parameter back: the share itself, up to the rounding of the
	// table's scale. A share outside the table (a starting speed of more than twice the chord's mean) is used as it is.
	const auto index = static_cast<int32_t>(share * 127.0f);
	float t = share;
	if (index >= 0 && index <= k_Steps)
	{
		const float low = _table[static_cast<size_t>(index)];
		const float high = index < k_Steps ? _table[static_cast<size_t>(index) + 1] : _t;
		if (share == low)
		{
			t = low;
		}
		else if (high != low)
		{
			const float weight = (high - share) / (high - low);
			t = weight * low + (1.0f - weight) * high;
		}
	}
	_t = t;
	return RunnerBezier(way, segment, t);
}

Walk camera_track::StartWalk(const edt::EDTTrack& track, bool forward, float from, float to)
{
	return {.runner = WayRunner(track.position),
	        .to = to,
	        .forward = forward,
	        .current = static_cast<float>(track.position.duration) * from};
}

float camera_track::Percentage(const Walk& walk, const edt::EDTTrack& track)
{
	return walk.current / static_cast<float>(track.position.duration);
}

std::optional<glm::vec2> camera_track::WalkTurn(Walk& walk, const edt::EDTTrack& track)
{
	const auto duration = track.position.duration;
	// The time along the track, from its end when walking it backwards, whole milliseconds within the track
	auto sample =
	    walk.forward ? static_cast<int32_t>(walk.current) : static_cast<int32_t>(static_cast<float>(duration) - walk.current);
	sample = std::clamp(sample, 0, duration);
	// The camera's way is run to that time, and the point its look-at way has at the same segment and parameter is where
	// it goes
	static_cast<void>(walk.runner.Get(track.position, sample));
	const auto point = Bezier(track.focus, walk.runner.GetSegment(), walk.runner.GetParameter());
	if (!(Percentage(walk, track) < walk.to))
	{
		return std::nullopt;
	}
	walk.current = std::min(walk.current + walk.step, static_cast<float>(duration));
	// On the land's grid
	return glm::vec2(map_coords::Quantise(point.x), map_coords::Quantise(point.z));
}
