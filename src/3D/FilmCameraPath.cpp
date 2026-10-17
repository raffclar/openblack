/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FilmCameraPath.h"

#include <CM2File.h>
#include <glm/gtc/type_ptr.hpp>

using namespace openblack;

FilmCameraPath::FilmCameraPath(const cam::CM2Path& path)
    : _durationMs(path.durationMilliseconds)
{
	_points.reserve(path.points.size());
	for (const auto& point : path.points)
	{
		_points.push_back({.position = glm::make_vec3(point.position.data()),
		                   .focus = glm::make_vec3(point.focus.data()),
		                   .frame = point.frame});
	}
}

FilmCameraPath::Point FilmCameraPath::SampleAt(uint32_t timeMs) const
{
	if (_points.empty())
	{
		return {};
	}
	const auto count = static_cast<uint32_t>(_points.size());
	if (count < 2 || _durationMs == 0)
	{
		return _points.front();
	}
	if (timeMs > _durationMs)
	{
		timeMs = _durationMs - 1;
	}
	// The point before the time, in 32-bit whole numbers
	auto index = ((count - 1) * timeMs) / _durationMs;
	if (index >= count)
	{
		index -= (index / count) * count;
	}
	const auto next = index + 1 < count ? index + 1 : index;

	// The fraction of the way to the next point, from the whole milliseconds of a span between two points
	const float span = static_cast<float>(_durationMs) / static_cast<float>(count - 1);
	const float spans = static_cast<float>(timeMs) / span;
	const auto spansDone = static_cast<int32_t>(spans);
	const auto spanMs = static_cast<int32_t>(span);
	const float fraction = static_cast<float>(static_cast<int32_t>(timeMs) - (spansDone * spanMs)) / span;
	const float rest = 1.0f - fraction;

	const auto& from = _points[index];
	const auto& to = _points[next];
	const auto blend = [rest, fraction](float a, float b) {
		const float fromPart = rest * a;
		const float toPart = b * fraction;
		return fromPart + toPart;
	};
	Point sample;
	for (int axis = 0; axis < 3; ++axis)
	{
		sample.position[axis] = blend(from.position[axis], to.position[axis]);
		sample.focus[axis] = blend(from.focus[axis], to.focus[axis]);
	}
	for (size_t i = 0; i < sample.frame.size(); ++i)
	{
		sample.frame.at(i) = blend(from.frame.at(i), to.frame.at(i));
	}
	return sample;
}
