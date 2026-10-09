/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightView.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::fight_view;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_TwoPi = 2.0f * k_Pi;
/// A line nearer upright than this across the land has no heading, and looks almost straight down
constexpr float k_Upright = 0.01f;
constexpr float k_UprightPitch = 1.5393804f;
/// The best heading is looked for among this many round the point, the land sampled at these eighths of the distance
constexpr int k_Headings = 32;
constexpr int k_FirstSample = 3;
constexpr int k_EndSample = 8;
constexpr float k_SampleShare = 0.125f;
/// Headings near the one given are favoured by this much, by the cosine of the turn from it
constexpr float k_FacingWeight = 50.0f;
/// The pitch eases a fifth of the way from where it was towards a tenth of the land's lean, raised by this
constexpr float k_PitchKeep = 0.2f;
constexpr float k_PitchLean = 0.5f * 0.2f;
constexpr float k_PitchRaise = 0.37699114f;
constexpr float k_LeastPitch = k_Pi / 8.0f;
constexpr float k_MostPitch = k_Pi / 3.0f;
/// A suggested view is eased to between these distances: eight tenths of the way up from nearer, a tenth of the way
/// down from further
constexpr float k_NearDistance = 25.0f;
constexpr float k_FarDistance = 50.0f;
constexpr float k_NearEase = 0.8f;
constexpr float k_FarEase = 0.1f;
/// The fight view starts at the arena's radius from its middle, and half that up
constexpr float k_StartRise = 0.5f;
/// Too far from an arena, in radii: the eye, where it looks, and where it looks alone
constexpr float k_EyeRadii = 3.2f;
constexpr float k_LookRadii = 4.2f;
constexpr float k_LookAloneRadii = 6.0f;
/// A unit of zoom input moves the camera this much of its zoom scale, three times its height above where it looks, kept
/// from 60 to 2000, or to 240 below it
constexpr float k_ZoomPerInput = 0.0015f;
constexpr float k_ZoomHeightFactor = 3.0f;
constexpr float k_ZoomScaleMin = 60.0f;
constexpr float k_ZoomScaleMax = 2000.0f;
constexpr float k_ZoomScaleMaxBelow = 240.0f;
} // namespace

float fight_view::ZoomDistance(float zoomInput, float heightAboveFocus)
{
	const auto most = heightAboveFocus < 0.0f ? k_ZoomScaleMaxBelow : k_ZoomScaleMax;
	const auto scale = std::clamp(k_ZoomHeightFactor * std::abs(heightAboveFocus), k_ZoomScaleMin, most);
	return zoomInput * k_ZoomPerInput * scale;
}

float fight_view::AngleOf(glm::vec2 line)
{
	return std::atan2(line.x, -line.y);
}

float fight_view::Wrapped(float angle)
{
	if (angle >= -k_Pi && angle <= k_Pi)
	{
		return angle;
	}
	auto turns = angle / k_TwoPi;
	auto wrapped = (turns - std::trunc(turns)) * k_TwoPi;
	for (int i = 0; i < 2; ++i)
	{
		if (wrapped > k_Pi)
		{
			wrapped -= k_TwoPi;
		}
		if (wrapped < -k_Pi)
		{
			wrapped += k_TwoPi;
		}
	}
	return wrapped;
}

glm::vec3 fight_view::PointFrom(const glm::vec3& from, float distance, float heading, float pitch)
{
	const auto level = std::cos(pitch);
	return {(std::sin(heading) * level * distance) + from.x, (std::sin(pitch) * distance) + from.y,
	        (std::cos(heading) * level * distance) + from.z};
}

HeadingPitch fight_view::HeadingAndPitch(const glm::vec3& origin, const glm::vec3& focus)
{
	const auto across = glm::vec2(origin.x - focus.x, origin.z - focus.z);
	if (std::abs(across.x) < k_Upright && std::abs(across.y) < k_Upright)
	{
		return {.heading = 0.0f, .pitch = k_UprightPitch};
	}
	return {.heading = k_Pi - AngleOf(across), .pitch = std::atan2(origin.y - focus.y, glm::length(across))};
}

float fight_view::BestHeading(float heading, float distance, const glm::vec3& focus, float& pitch, const GroundHeight& ground,
                              const glm::vec3& landNormal)
{
	std::array<float, k_Headings> scores {};
	for (int i = 0; i < k_Headings; ++i)
	{
		const auto turned = (static_cast<float>(i) * (k_TwoPi / k_Headings)) + heading;
		for (int sample = k_FirstSample; sample < k_EndSample; ++sample)
		{
			const auto point = PointFrom(focus, static_cast<float>(sample) * distance * k_SampleShare, turned, 0.0f);
			scores.at(static_cast<size_t>(i)) += focus.y - ground({point.x, point.z});
		}
	}
	float best = -1e20f;
	int bestIndex = 0;
	for (int i = 0; i < k_Headings; ++i)
	{
		auto& score = scores.at(static_cast<size_t>(i));
		score += std::cos(static_cast<float>(i) * (k_TwoPi / k_Headings)) * k_FacingWeight;
		if (best < score)
		{
			best = score;
			bestIndex = i;
		}
	}
	const auto lean = HeadingAndPitch(landNormal + focus, focus).pitch;
	const auto eased = (lean * k_PitchLean) + (pitch * k_PitchKeep) + k_PitchRaise;
	pitch = eased <= k_LeastPitch ? k_LeastPitch : (eased >= k_MostPitch ? k_MostPitch : eased);
	return (static_cast<float>(bestIndex) * (k_TwoPi / k_Headings)) + heading;
}

View fight_view::Suggest(const glm::vec3& from, const glm::vec3& focus, const GroundHeight& ground, const glm::vec3& landNormal)
{
	auto [heading, pitch] = HeadingAndPitch(from, focus);
	auto distance = glm::distance(focus, from);
	if (distance < k_NearDistance)
	{
		distance = ((k_NearDistance - distance) * k_NearEase) + distance;
	}
	if (k_FarDistance < distance)
	{
		distance = ((k_FarDistance - distance) * k_FarEase) + distance;
	}
	heading = BestHeading(heading, distance, focus, pitch, ground, landNormal);
	return {.origin = PointFrom(focus, distance, heading, pitch), .focus = focus};
}

View fight_view::Start(glm::vec2 arenaCentre, float arenaRadius, float ground, const GroundHeight& groundAt,
                       const glm::vec3& landNormal)
{
	const auto from = glm::vec3(arenaRadius + arenaCentre.x, (arenaRadius * k_StartRise) + ground, arenaCentre.y);
	return Suggest(from, glm::vec3(arenaCentre.x, ground, arenaCentre.y), groundAt, landNormal);
}

bool fight_view::TooFar(glm::vec2 eye, glm::vec2 looking, glm::vec2 arenaCentre, float arenaRadius, float scale)
{
	const auto eyeAway = glm::distance(eye, arenaCentre);
	const auto lookAway = glm::distance(looking, arenaCentre);
	if (arenaRadius * scale * k_EyeRadii < eyeAway && arenaRadius * scale * k_LookRadii < lookAway)
	{
		return true;
	}
	return arenaRadius * scale * k_LookAloneRadii < lookAway;
}

bool fight_view::WithinArena(glm::vec2 eye, glm::vec2 looking, glm::vec2 arenaCentre, float arenaRadius)
{
	const auto radiusSquared = arenaRadius * arenaRadius;
	const auto eyeAway = eye - arenaCentre;
	const auto lookAway = looking - arenaCentre;
	return glm::dot(eyeAway, eyeAway) < radiusSquared && glm::dot(lookAway, lookAway) < radiusSquared;
}

Tracker::Step Tracker::Follow(const Fighter& first, const Fighter& second, const Input& input, float seconds)
{
	// The player turns and tilts it
	_headingOffset += (input.turn * k_TurnPerScreen) / input.screenWidth;
	_pitch -= input.tilt * k_TiltPerInput;

	const auto middle = (first.position + second.position) * 0.5f;
	const auto focus = glm::vec3(middle.x, ((first.height + second.height) * k_HeightShare) + middle.y, middle.z);
	const auto apart =
	    glm::distance(glm::vec2(second.position.x, second.position.z), glm::vec2(first.position.x, first.position.z));
	// A zoom moves it back by a share of the zoom's distance, by changing the scale of the first creature's radius
	if (input.zoom != 0.0f && first.radius != 0.0f)
	{
		_scale = (((input.zoom * k_ZoomShare) + (first.radius * _scale) + apart + second.radius) - apart - second.radius) /
		         first.radius;
	}
	const bool zoomedOut = k_MaxScale < _scale;
	_scale = _scale > 0.0f ? (_scale < k_MaxScale ? _scale : k_MaxScale) : 0.0f;
	_pitch = _pitch > k_MinPitch ? (_pitch < k_MaxPitch ? _pitch : k_MaxPitch) : k_MinPitch;
	const auto distance = (first.radius * _scale) + apart + second.radius;

	// It turns to keep the line between them across the view, once they are apart
	auto line = _heading.GetValue();
	const auto across = glm::vec2(second.position.x - first.position.x, second.position.z - first.position.z);
	if (k_TurnApart < std::abs(across.x) || k_TurnApart < std::abs(across.y))
	{
		line = AngleOf(across);
	}
	if (_snap)
	{
		_heading.Reset(line);
		_focus.Reset(focus);
		_snap = false;
	}
	else
	{
		const auto current = _heading.GetValue();
		_heading.SetDestination(Wrapped(line - current) + current, k_EaseSeconds);
		_focus.SetDestination(focus, k_EaseSeconds);
	}
	_heading.Update(seconds);
	_focus.Update(seconds);
	const auto looking = _focus.GetValue();
	return {.view = {.origin = PointFrom(looking, distance, _headingOffset - _heading.GetValue(), _pitch), .focus = looking},
	        .zoomedOut = zoomedOut};
}
