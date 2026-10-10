/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightSheet.h"

#include <cmath>

#include <algorithm>
#include <span>
#include <utility>

using namespace openblack::particles;

namespace
{
constexpr uint32_t k_Opaque = 0xFF000000u;
/// The added colour is half the light, its alpha an eighth
constexpr uint32_t k_HalfMask = 0xFEFEFEu;
constexpr uint32_t k_SpecularAlpha = 0x40000000u;
constexpr uint32_t k_ByteMax = 255u;
constexpr uint32_t k_CornersPerPoint = 3;

/// Each channel of a colour times a light of 0 to 255, over 256
uint32_t Lit(uint32_t argb, uint32_t light)
{
	const uint32_t red = ((argb & 0xFF0000u) * light) & 0xFF0000FFu;
	const uint32_t green = ((argb & 0xFF00u) * light) & 0xFF0000u;
	const uint32_t blue = ((argb & 0xFFu) * light) & 0xFF00u;
	return (red | green | blue) >> 8u;
}
} // namespace

void LightSheet::Start(std::vector<glm::vec3> points, uint32_t rgb, float height, float shiftSeconds)
{
	_points = std::move(points);
	_pulsed = false;
	_strengths.assign(_points.size(), 0.0f);
	_heights.assign(_points.size(), 0.0f);
	_rgb = rgb & 0xFFFFFFu;
	_height = height;
	_shiftSeconds = shiftSeconds;
	_strength = 0.0f;
	_time = 0.0f;
	_sinceShift = 0.0f;
	_slide = 0.0f;
}

void LightSheet::Update(float seconds)
{
	_time += seconds;
	const double since = static_cast<double>(seconds) + _sinceShift;
	_sinceShift = static_cast<float>(since);
	_slide = static_cast<float>(_slide - (static_cast<double>(seconds) * k_SlideSpeed));
	if (_pulsed)
	{
		UpdatePulsed(seconds);
		return;
	}
	if (_shiftSeconds < since && !_strengths.empty())
	{
		do
		{
			_sinceShift -= _shiftSeconds;
			std::shift_right(_strengths.begin(), _strengths.end(), 1);
			_strengths.front() = _strength;
		} while (_shiftSeconds < _sinceShift);
	}
	const auto count = static_cast<int>(_points.size());
	for (int i = 0; i < count; ++i)
	{
		const double wave = std::cos(((static_cast<double>(i) * k_WaveSpan) / (count - 1)) - (_time * k_WaveSpeed));
		_heights[static_cast<size_t>(i)] = static_cast<float>(((wave * k_WaveDepth) + k_WaveMiddle) * _height);
	}
}

void LightSheet::Build(std::vector<Vertex>& vertices, std::vector<uint32_t>& triangles)
{
	vertices.clear();
	triangles.clear();
	const auto count = _points.size();
	if (count < 2)
	{
		return;
	}
	if (_pulsed)
	{
		BuildPulsed(vertices, triangles);
		return;
	}
	glm::vec3 sum(0.0f);
	for (const auto& point : _points)
	{
		sum += point;
	}
	const double share = 1.0 / static_cast<double>(count);
	_middle = {static_cast<float>(share * sum.x), static_cast<float>(share * sum.y), static_cast<float>(share * sum.z)};
	_slide -= static_cast<float>(static_cast<int>(_slide));

	vertices.reserve(count * k_CornersPerPoint);
	float along = 0.0f;
	for (size_t i = 0; i < count; ++i)
	{
		const auto& point = _points[i];
		auto top = ((point - _middle) * _spread) + _middle;
		const double rise = static_cast<double>(_heights[i]) * _strengths[i];
		top.y = static_cast<float>(rise + top.y);
		const auto bright = ((top - point) * k_BrightLine) + point;
		const auto light = std::min(static_cast<uint32_t>(static_cast<int64_t>(rise * k_LightPerHeight)), k_ByteMax);
		const uint32_t lit = Lit(_rgb, light);
		vertices.push_back({.position = point, .uv = {along, 1.0f - _slide}, .argb = k_Opaque, .specularArgb = 0});
		vertices.push_back({.position = bright,
		                    .uv = {along, k_BrightRow - _slide},
		                    .argb = lit | k_Opaque,
		                    .specularArgb = ((lit & k_HalfMask) | k_SpecularAlpha) >> 1u});
		vertices.push_back({.position = top, .uv = {along, -_slide}, .argb = k_Opaque, .specularArgb = 0});
		const auto step = _points[(i + 1) % count] - point;
		const double distance = std::sqrt((static_cast<double>(step.x) * step.x) + (static_cast<double>(step.y) * step.y) +
		                                  (static_cast<double>(step.z) * step.z));
		along = static_cast<float>((distance * k_StarsPerUnit) + along);
	}

	triangles.reserve((count - 1) * 12);
	for (uint32_t s = 0; s + 1 < count; ++s)
	{
		const uint32_t b = s * k_CornersPerPoint;
		triangles.insert(triangles.end(), {b, b + 1, b + 3, b + 1, b + 4, b + 3, b + 1, b + 2, b + 4, b + 2, b + 5, b + 4});
	}
}

void LightSheet::StartPulsed(uint32_t rgb, float height, size_t capacity)
{
	_points.clear();
	_pulsed = true;
	_strengths.assign(capacity, 0.0f);
	_heights.assign(capacity, height);
	_rgb = rgb & 0xFFFFFFu;
	_height = height;
	_spread = 1.0f;
	_shiftSeconds = 0.0f;
	_strength = 0.0f;
	_time = 0.0f;
	_sinceShift = 0.0f;
	_slide = 0.0f;
}

void LightSheet::SetPoints(std::vector<glm::vec3> points)
{
	_points = std::move(points);
	if (_points.size() > _strengths.size())
	{
		_points.resize(_strengths.size());
	}
}

void LightSheet::Pulse(const glm::vec3& centre, float radius)
{
	if (!(radius > 0.0f) || !_pulsed)
	{
		return;
	}
	for (size_t i = 0; i < _points.size(); ++i)
	{
		const float x = _points[i].x - centre.x;
		const float z = _points[i].z - centre.z;
		const float distance = std::sqrt(x * x + z * z);
		if (distance < radius)
		{
			_strengths[i] += 1.0f - distance / radius;
		}
	}
}

bool LightSheet::Showing() const
{
	if (!_pulsed)
	{
		return true;
	}
	return std::ranges::any_of(std::span(_strengths).first(_points.size()),
	                           [](float strength) { return strength > k_PulseShowsAbove; });
}

void LightSheet::UpdatePulsed(float seconds)
{
	// Each point's strength fades towards nothing
	const auto kept = static_cast<float>(std::exp(-static_cast<double>(seconds) / k_PulseFadeSeconds));
	for (auto& strength : std::span(_strengths).first(_points.size()))
	{
		strength += (0.0f - strength) * (1.0f - kept);
	}
}

void LightSheet::BuildPulsed(std::vector<Vertex>& vertices, std::vector<uint32_t>& triangles)
{
	const auto count = _points.size();
	glm::vec3 sum(0.0f);
	for (const auto& point : _points)
	{
		sum += point;
	}
	const double share = 1.0 / static_cast<double>(count);
	_middle = {static_cast<float>(share * sum.x), static_cast<float>(share * sum.y), static_cast<float>(share * sum.z)};
	_slide *= k_PulseSlideKept;
	_slide -= static_cast<float>(static_cast<int>(_slide));
	if (!Showing())
	{
		return;
	}

	vertices.reserve(count * k_CornersPerPoint * 2);
	triangles.reserve((count - 1) * 12 * 2);
	for (uint32_t pass = 0; pass < 2; ++pass)
	{
		const float slide = pass == 0 ? _slide : -_slide;
		const auto first = static_cast<uint32_t>(vertices.size());
		float along = 0.0f;
		for (size_t i = 0; i < count; ++i)
		{
			const auto& point = _points[i];
			auto top = ((point - _middle) * _spread) + _middle;
			top.y += _heights[i];
			const auto middle = ((top - point) * k_PulseMiddleLine) + point;
			const auto light =
			    std::clamp(static_cast<int>(_strengths[i] * k_PulseLightPerStrength), 0, static_cast<int>(k_ByteMax));
			const uint32_t argb = _rgb + (static_cast<uint32_t>(light) << 24u);
			// The distance along is counted a tenth at a time, and the texture runs a tenth of that
			const float u = along * k_StarsPerUnit + slide;
			vertices.push_back({.position = point, .uv = {u, 0.0f}, .argb = argb, .specularArgb = 0});
			vertices.push_back({.position = middle,
			                    .uv = {u, k_PulseMiddleLine},
			                    .argb = argb,
			                    .specularArgb = ((argb & k_HalfMask) | k_SpecularAlpha) >> 1u});
			vertices.push_back({.position = top, .uv = {u, 1.0f}, .argb = argb & 0xFFFFFFu, .specularArgb = 0});
			const auto step = _points[(i + 1) % count] - point;
			const double distance = std::sqrt((static_cast<double>(step.x) * step.x) + (static_cast<double>(step.y) * step.y) +
			                                  (static_cast<double>(step.z) * step.z));
			along = static_cast<float>((distance * k_StarsPerUnit) + along);
		}
		for (uint32_t s = 0; s + 1 < count; ++s)
		{
			const uint32_t b = first + s * k_CornersPerPoint;
			triangles.insert(triangles.end(), {b, b + 1, b + 3, b + 1, b + 4, b + 3, b + 1, b + 2, b + 4, b + 2, b + 5, b + 4});
		}
	}
}
