/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// OPENBLACK_TEST_GESTURE="<GESTURE>[,sizePx[,cx,cy[,t]]][;<GESTURE>...]" or "=<file.txt>" (pixels "x y" per line):
// the first template of the gesture drawn back on the screen (sizePx wide, default 200, centred at the window fraction
// cx,cy, default the middle), t seconds of game time after the land exists (default 0.5), as mouse messages every
// 28 ms. A '-' before the name mirrors it (anticlockwise). The mouse is not used meanwhile.

#include "GestureDebugHooks.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <fstream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Systems/DebugHooksInterface.h"
#include "GestureInput.h"
#include "GestureMatch.h"
#include "GestureTemplates.h"
#include "Locator.h"
#include "PowerUpSystem.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic::gestures;

// The sizes, times and window below (1600 x 900, 200 px, 0.5 s, max(5, L / 50) px) are test harness values, not from
// the original.
namespace
{
/// The GESTURE_TYPE names (the Gestures.jty records use 1..23)
constexpr std::array<const char*, k_GestureCount> k_Names {"NONE",
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
                                                           "SQUARE_WAVE"};

struct PlannedStroke
{
	float time {0.5f};
	std::vector<glm::ivec2> pixels;
	std::string label;
};

/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct GestureDebugHooksState
{
	std::vector<PlannedStroke> planned {};
	bool parsed {false};
	float time {0.0f};
	bool reported {true};
};

GestureDebugHooksState& GestureDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<GestureDebugHooksState>();
}

Gesture GestureFromText(const std::string& text)
{
	char* end = nullptr;
	const long number = std::strtol(text.c_str(), &end, 10);
	if (end != text.c_str() && *end == '\0')
	{
		return static_cast<Gesture>(number);
	}
	std::string upper;
	for (const char c : text)
	{
		upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
	}
	for (size_t g = 0; g < k_Names.size(); ++g)
	{
		if (upper == k_Names[g])
		{
			return static_cast<Gesture>(g);
		}
	}
	return k_None;
}

glm::vec2 Window()
{
	if (!Locator::windowing::has_value())
	{
		return {1600.0f, 900.0f};
	}
	const auto size = Locator::windowing::value().GetSize();
	return {static_cast<float>(size.x), static_cast<float>(size.y)};
}

/// The polyline sampled like mouse messages: about 50 samples, at least 5 px apart
std::vector<glm::ivec2> Sample(const std::vector<glm::vec2>& points)
{
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

std::vector<glm::vec2> TemplateStroke(Gesture gesture, bool mirror, float size, glm::vec2 centre)
{
	std::vector<glm::vec2> points;
	for (const auto& tpl : Templates())
	{
		if (tpl.gesture != gesture)
		{
			continue;
		}
		float maxX = 0.0f;
		float maxZ = 0.0f;
		for (int k = 0; k < tpl.count; ++k)
		{
			maxX = std::max(maxX, tpl.samples[k].x);
			maxZ = std::max(maxZ, tpl.samples[k].z);
		}
		// the template tool multiplied z by the screen's width / height
		const float ratio = sampling::ScreenRatio();
		for (int k = 0; k < tpl.count; ++k)
		{
			float x = (tpl.samples[k].x - 0.5f * maxX) * size;
			const float z = (tpl.samples[k].z - 0.5f * maxZ) * size / ratio;
			if (mirror)
			{
				x = -x;
			}
			points.emplace_back(centre.x + x, centre.y + z);
		}
		break;
	}
	return points;
}

void Parse()
{
	GestureDebugHooksData().parsed = true;
	const char* value = std::getenv("OPENBLACK_TEST_GESTURE");
	if (value == nullptr)
	{
		return;
	}
	std::string text(value);
	if (text.size() > 4 && text.substr(text.size() - 4) == ".txt")
	{
		std::ifstream file(text.front() == '=' ? text.substr(1) : text);
		PlannedStroke stroke;
		stroke.label = text;
		std::vector<glm::vec2> points;
		float x = 0.0f;
		float y = 0.0f;
		while (file >> x >> y)
		{
			points.emplace_back(x, y);
		}
		if (points.size() >= 2)
		{
			stroke.pixels = Sample(points);
			GestureDebugHooksData().planned.push_back(std::move(stroke));
		}
		return;
	}
	size_t start = 0;
	while (start < text.size())
	{
		const size_t end = std::min(text.find(';', start), text.size());
		const auto item = text.substr(start, end - start);
		start = end + 1;
		std::array<char, 64> name = {};
		float size = 200.0f;
		glm::vec2 centre(0.5f);
		float time = 0.5f;
		if (std::sscanf(item.c_str(), "%63[^,],%f,%f,%f,%f", name.data(), &size, &centre.x, &centre.y, &time) < 1)
		{
			continue;
		}
		std::string label(name.data());
		const bool mirror = !label.empty() && label.front() == '-';
		const auto gesture = GestureFromText(mirror ? label.substr(1) : label);
		const auto points = TemplateStroke(gesture, mirror, size, centre * Window());
		if (gesture == k_None || points.size() < 2)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Gesture test: no template for \"{}\"", label);
			continue;
		}
		GestureDebugHooksData().planned.push_back({time, Sample(points), label});
	}
}
} // namespace

void openblack::magic::gestures::ResetDebugHooks()
{
	GestureDebugHooksData().parsed = false;
	GestureDebugHooksData().planned.clear();
	GestureDebugHooksData().time = 0.0f;
	GestureDebugHooksData().reported = true;
}

void openblack::magic::gestures::RunDebugHooks(float seconds)
{
	if (!Locator::terrainSystem::has_value() || Templates().empty())
	{
		return;
	}
	if (!GestureDebugHooksData().parsed)
	{
		Parse();
	}
	GestureDebugHooksData().time += seconds;
	// once a played stroke is in: what the whole buffer matches now (the log only)
	if (!GestureDebugHooksData().reported && !sampling::PlayingStroke())
	{
		GestureDebugHooksData().reported = true;
		std::string matches;
		auto data = BuildFromSystem(State().system, sampling::ScreenRatio());
		for (Gesture g = 1; g < k_GestureCount; ++g)
		{
			Result result;
			if (MatchGesture(Templates(), g, data, result, sampling::ScreenRatio()))
			{
				matches += fmt::format(" {}{}", k_Names[g], result.reversed ? "(mirrored)" : "");
			}
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gesture test: the buffer has {} samples, {} keypoints; it matches:{}",
		                   State().system.Count(), data.count, matches.empty() ? " nothing" : matches);
	}
	if (GestureDebugHooksData().planned.empty() || sampling::PlayingStroke() ||
	    GestureDebugHooksData().time < GestureDebugHooksData().planned.front().time)
	{
		return;
	}
	auto stroke = std::move(GestureDebugHooksData().planned.front());
	GestureDebugHooksData().planned.erase(GestureDebugHooksData().planned.begin());
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gesture test: playing {} ({} mouse messages) at {:.2f} s", stroke.label,
	                   stroke.pixels.size(), GestureDebugHooksData().time);
	sampling::PlayStroke(std::move(stroke.pixels));
	GestureDebugHooksData().reported = false;
}
