/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GesturesModel.h"

#include <algorithm>
#include <array>

#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"

using namespace openblack::magic::gestures;

namespace openblack::debug::gestures_window
{

namespace
{
constexpr std::array<std::string_view, k_GestureCount> k_Names {
    "NONE",
    "SPIRAL",
    "INVERSE_SPIRAL",
    "S_SHAPE",
    "CIRCLE",
    "SCRIBBLE",
    "THREE",
    "VERTICAL_SCRIBBLE",
    "STAR",
    "FORK_RIGHT",
    "FORK_UP",
    "FORK_LEFT",
    "FORK_DOWN",
    "HEART",
    "R_SHAPE",
    "SQUARE_SPIRAL",
    "CYRILLIC_L",
    "E_SHAPE",
    "REVERSE_S",
    "INFINITY",
    "W_SHAPE",
    "HOUSE",
    "INVERSE_SQUARE_SPIRAL",
    "SQUARE_WAVE",
};
} // namespace

std::string_view GestureName(Gesture gesture)
{
	return gesture < k_Names.size() ? k_Names.at(gesture) : std::string_view("?");
}

KeyPoint KeyPointOf(uint32_t flags)
{
	if ((flags & Sample::k_Start) != 0)
	{
		return KeyPoint::Start;
	}
	if ((flags & Sample::k_Corner) != 0)
	{
		return KeyPoint::Corner;
	}
	if ((flags & Sample::k_End) != 0)
	{
		return KeyPoint::End;
	}
	if ((flags & Sample::k_Anchor) != 0)
	{
		return KeyPoint::Anchor;
	}
	return KeyPoint::None;
}

std::string LookingForName(int type)
{
	switch (type)
	{
	case 0:
		return {};
	case 0x2:
		return "to start a leash";
	case 0x8:
		return "a spiral";
	case 0x9:
		return "an inverse spiral";
	case 0xA:
		return "to repeat the last miracle";
	case 0xB:
		return "a stage of the miracle selection";
	case 0xC:
		return "a circle";
	case 0xD:
		return "to power down";
	default:
		break;
	}
	if (type >= 0xE)
	{
		return fmt::format("to power up to level {}", type - 0xE);
	}
	return fmt::format("type {}", type);
}

std::vector<Match> MatchesNow(const std::vector<GestureData>& templates, const GestureData& keyPoints, float screenRatio)
{
	std::vector<Match> matches;
	for (Gesture gesture = 1; gesture < k_GestureCount; ++gesture)
	{
		Result result;
		if (MatchGesture(templates, gesture, keyPoints, result, screenRatio))
		{
			matches.push_back({.gesture = gesture, .mirrored = result.reversed, .templateIndex = result.templateIndex});
		}
	}
	return matches;
}

const GestureData* FirstTemplate(std::span<const GestureData> templates, Gesture gesture)
{
	const auto found = std::ranges::find(templates, gesture, &GestureData::gesture);
	return found != templates.end() ? &*found : nullptr;
}

std::vector<glm::vec2> TemplateStroke(const GestureData& gestureTemplate, float size, glm::vec2 centre, float screenRatio)
{
	const auto samples = std::span(gestureTemplate.samples).first(gestureTemplate.count);
	float maxX = 0.0f;
	float maxZ = 0.0f;
	for (const auto& sample : samples)
	{
		maxX = std::max(maxX, sample.x);
		maxZ = std::max(maxZ, sample.z);
	}
	std::vector<glm::vec2> points;
	points.reserve(samples.size());
	for (const auto& sample : samples)
	{
		const float x = (sample.x - 0.5f * maxX) * size;
		const float z = (sample.z - 0.5f * maxZ) * size / screenRatio;
		points.emplace_back(centre.x + x, centre.y + z);
	}
	return points;
}

std::vector<glm::ivec2> MousePositions(std::span<const glm::vec2> points)
{
	if (points.empty())
	{
		return {};
	}
	float length = 0.0f;
	for (size_t k = 1; k < points.size(); ++k)
	{
		length += glm::distance(points[k - 1], points[k]);
	}
	const float step = std::max(5.0f, length / 50.0f);
	std::vector<glm::ivec2> pixels;
	glm::vec2 at = points.front();
	pixels.emplace_back(glm::round(at));
	for (size_t k = 1; k < points.size(); ++k)
	{
		while (glm::distance(at, points[k]) > step)
		{
			at += glm::normalize(points[k] - at) * step;
			pixels.emplace_back(glm::round(at));
		}
	}
	pixels.emplace_back(glm::round(points.back()));
	return pixels;
}

} // namespace openblack::debug::gestures_window
