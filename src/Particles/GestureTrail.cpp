/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTrail.h"

#include <cmath>

#include <array>
#include <utility>

#include "Common/X87.h"

using namespace openblack::particles;

namespace
{
/// The fewest points a path needs to be walked between them
constexpr size_t k_WalkedPoints = 3;
} // namespace

TrailPath::TrailPath(std::vector<glm::vec3> points)
    : _points(std::move(points))
{
	Measure();
}

void TrailPath::SetPoint(size_t index, glm::vec3 point)
{
	_points.at(index) = point;
	Measure();
}

void TrailPath::Measure()
{
	_distances.assign(_points.size(), 0.0f);
	for (size_t i = 1; i < _points.size(); ++i)
	{
		const auto step = _points[i] - _points[i - 1];
		const float length = std::sqrt((step.z * step.z) + (step.y * step.y) + (step.x * step.x));
		_distances[i] = length + _distances[i - 1];
	}
}

float TrailPath::Length() const
{
	return _distances.empty() ? 0.0f : _distances.back();
}

float gesture_trail::Transition(float t, float gain)
{
	const float eased = ((3.0f - (t + t)) * t * t) - t;
	return (eased * gain) + t;
}

uint8_t gesture_trail::RevealAlpha(float along, float grown, float maxAlpha)
{
	constexpr float k_ByteMax = 255.0f;
	const float fade = grown * maxAlpha;
	// It fades in from the start and from the end
	const float fromStart = along < grown ? ((along / grown) * -fade) + fade : 0.0f;
	const float fromEnd = 1.0f - along;
	float alpha = fromEnd < grown ? ((fromEnd / grown) * -fade) + fade : 0.0f;
	alpha += fromStart;
	alpha = alpha <= 0.0f ? 0.0f : (alpha < k_ByteMax ? alpha : k_ByteMax);
	return static_cast<uint8_t>(static_cast<int>(alpha));
}

float gesture_trail::WiggleAmount(float age, float phaseSpeed, float dispersalTime, float shrinkTime)
{
	// The cosine stays wide until the add of one
	float amount = x87::Add(x87::Cos(age * phaseSpeed), 1.0) * 0.5f;
	const float dispersed = age - dispersalTime;
	if (dispersed >= 0.0f)
	{
		const float left = 1.0f - (dispersed / shrinkTime);
		amount *= left <= 0.0f ? 0.0f : (left < 1.0f ? left : 1.0f);
	}
	return amount;
}

glm::vec3 gesture_trail::Lift(glm::vec3 point, glm::vec3 camera, float size)
{
	auto towards = camera - point;
	float distance = 0.0f;
	if (towards != glm::vec3(0.0f))
	{
		distance = std::sqrt((towards.z * towards.z) + (towards.x * towards.x) + (towards.y * towards.y));
		towards *= 1.0f / distance;
	}
	if (towards.y <= 0.0f)
	{
		return point;
	}
	const float steep = size / towards.y;
	const auto half = distance * k_MostLift;
	const float lift = steep <= half ? steep : half;
	return point + (towards * lift);
}

float gesture_trail::SheetStrength(float age, float lifetime)
{
	float share = age / lifetime;
	share = share <= 0.0f ? 0.0f : (share < 1.0f ? share : 1.0f);
	const float centred = share + share - 1.0f;
	return 1.0f - (centred * centred);
}

float gesture_trail::FlashLeft(float since, float duration)
{
	const float left = 1.0f - (since / duration);
	return left <= 0.0f ? 0.0f : (left < 1.0f ? left : 1.0f);
}

uint32_t gesture_trail::Dimmed(uint32_t argb, uint8_t level)
{
	uint32_t result = 0;
	for (uint32_t shift = 0; shift < 32; shift += 8)
	{
		const uint32_t channel = (argb >> shift) & 0xFFu;
		result |= ((channel * level) >> 8u) << shift;
	}
	return result;
}

float gesture_trail::ChainDistanceScale(float distance)
{
	constexpr std::array<float, 4> k_Distances {0.0f, 50.0f, 500.0f, 1500.0f};
	constexpr std::array<float, 4> k_Scales {0.2f, 1.0f, 1.0f, 1.5f};
	if (distance <= k_Distances.front())
	{
		return k_Scales.front();
	}
	if (distance >= k_Distances.back())
	{
		return k_Scales.back();
	}
	size_t i = 0;
	while (distance >= k_Distances.at(i + 1))
	{
		++i;
	}
	const float t = (distance - k_Distances.at(i)) / (k_Distances.at(i + 1) - k_Distances.at(i));
	return (t * (k_Scales.at(i + 1) - k_Scales.at(i))) + k_Scales.at(i);
}

glm::vec3 TrailPath::At(float fraction) const
{
	if (_points.empty())
	{
		return glm::vec3(0.0f);
	}
	const float distance = fraction * _distances.back();
	if (_distances.back() < distance)
	{
		return _points.back();
	}
	if (_points.size() < k_WalkedPoints)
	{
		return _points.front();
	}
	// The first segment that reaches the distance
	size_t i = 0;
	while (i + 2 < _points.size() && _distances[i + 1] < distance)
	{
		++i;
	}
	const auto& from = _points[i];
	const auto& to = _points[i + 1];
	const float span = _distances[i + 1] - _distances[i];
	if (span == 0.0f)
	{
		return from;
	}
	const float t = (distance - _distances[i]) / span;
	return (to - from) * t + from;
}
