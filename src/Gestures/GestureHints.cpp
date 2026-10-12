/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureHints.h"

#include <cmath>

#include <algorithm>
#include <map>

#include <glm/gtc/matrix_transform.hpp>

#include "Gui/ScreenFit.h"

using namespace openblack;
using namespace openblack::gesture_hints;

namespace
{
/// Turns two of a collar's axes towards each other by an angle
void Turn(glm::vec3& a, glm::vec3& b, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	const auto oldA = a;
	a = (c * a) - (s * b);
	b = (c * b) + (s * oldA);
}

/// The width of the row, from its share of the screen
float Share(float share, float rowWidth)
{
	return share * rowWidth;
}
} // namespace

std::vector<Hint> gesture_hints::HintsFor(std::span<const gesture::Request> requests, const ShapeOf& shapeOf)
{
	// One hint a gesture, the last asked for it standing, taken in the order of the gestures' numbers
	std::map<GestureType, const gesture::Request*> byGesture;
	for (const auto& request : requests)
	{
		if (request.purpose == gesture::Purpose::PickLeash && request.gesture != GestureType::None)
		{
			byGesture[request.gesture] = &request;
		}
	}
	const auto circle = shapeOf ? shapeOf(GestureType::Circle) : std::nullopt;
	std::vector<Hint> hints;
	for (const auto& [gesture, request] : byGesture)
	{
		Hint hint {
		    .gesture = gesture,
		    .order = static_cast<int>(request->leash) + k_LeashOrderBase,
		    .leash = request->leash,
		};
		auto shape = shapeOf ? shapeOf(gesture) : std::nullopt;
		if (!shape.has_value())
		{
			shape = circle;
		}
		if (shape.has_value())
		{
			hint.path = shape->points;
			if (circle.has_value() && circle->length > 0.0f)
			{
				hint.period = k_CirclePeriod * shape->length / circle->length;
			}
		}
		// Each goes before the first already in the row whose order is no greater
		const auto at = std::ranges::find_if(hints, [&hint](const Hint& shown) { return shown.order <= hint.order; });
		hints.insert(at, std::move(hint));
	}
	return hints;
}

bool gesture_hints::Differs(std::span<const Hint> shown, std::span<const Hint> next)
{
	return !std::ranges::equal(shown, next, [](const Hint& a, const Hint& b) {
		return a.gesture == b.gesture && a.order == b.order && a.leash == b.leash;
	});
}

float gesture_hints::RowWidth(glm::vec2 screen)
{
	return gui::ReferenceScale(static_cast<int32_t>(screen.x), static_cast<int32_t>(screen.y)) *
	       static_cast<float>(gui::k_ReferenceWidth);
}

Box gesture_hints::Square(size_t slot, glm::vec2 screen)
{
	const float width = RowWidth(screen);
	const float side = Share(k_SquareShare, width);
	const float step = Share(k_GapShare + k_SquareShare, width) * static_cast<float>(slot);
	const float margin = Share(k_MarginShare, width);
	return {
	    .min = {screen.x - (step + side + margin), screen.y - side},
	    .max = {screen.x - (step + margin), screen.y},
	};
}

Box gesture_hints::PictureSquare(const Box& square)
{
	const float side = square.max.y - square.min.y;
	return {.min = {square.min.x, square.min.y - side}, .max = {square.max.x, square.min.y}};
}

glm::vec2 gesture_hints::PixelOf(const Box& square, glm::vec2 point)
{
	return glm::trunc(square.min + ((square.max - square.min) * point));
}

float gesture_hints::SpriteSide(float halfSize, float rowWidth, float horizontalFov)
{
	return halfSize / k_SpriteDepth / std::tan(horizontalFov * 0.5f) * rowWidth;
}

float gesture_hints::Fade(float alpha, float seconds, bool wordsShowing)
{
	if (alpha > 0.0f && wordsShowing)
	{
		return std::max(alpha - (k_FadeRate * seconds), 0.0f);
	}
	if (alpha < 1.0f && !wordsShowing)
	{
		return std::min(alpha + (k_FadeRate * seconds), 1.0f);
	}
	return alpha;
}

float gesture_hints::StepDot(float dot, float seconds, float period)
{
	if (!(period > 0.0f))
	{
		return dot;
	}
	dot += seconds / period;
	if (dot > 1.0f)
	{
		dot -= 1.0f - k_DotOverlap;
	}
	return std::fmod(dot, 1.0f);
}

std::vector<float> gesture_hints::DotsAt(float dot)
{
	std::vector<float> dots {dot};
	if (dot > 1.0f - k_DotOverlap)
	{
		dots.push_back(dot - (1.0f - k_DotOverlap));
	}
	return dots;
}

float gesture_hints::DotBrightness(float at)
{
	const float fromMiddle = (at * 2.0f) - 1.0f;
	return std::min(1.0f - (fromMiddle * fromMiddle), 1.0f);
}

glm::vec3 gesture_hints::PointAlong(std::span<const glm::vec3> path, float at)
{
	if (path.empty())
	{
		return glm::vec3(0.0f);
	}
	if (at >= 1.0f)
	{
		return path.back();
	}
	at = std::max(at, 0.0f);
	const float place = static_cast<float>(path.size() - 1) * at;
	const auto index = static_cast<size_t>(place);
	if (index + 1 >= path.size())
	{
		return path.back();
	}
	const float t = place - static_cast<float>(index);
	return path[index] + ((path[index + 1] - path[index]) * t);
}

uint8_t gesture_hints::DotShade(float at, float alpha)
{
	constexpr float k_Full = 255.0f;
	const auto level = static_cast<uint32_t>(std::max(std::nearbyint(DotBrightness(at) * alpha * k_Full), 0.0f));
	return static_cast<uint8_t>((255u * level) >> 8u);
}

std::array<uint8_t, 4> gesture_hints::PictureColour(uint32_t rgb, float alpha)
{
	constexpr float k_Full = 255.0f;
	const auto level = static_cast<uint32_t>(std::max(alpha * k_Full, 0.0f));
	const auto scale = [level](uint32_t channel) { return static_cast<uint8_t>(((channel & 0xFFu) * level) >> 8u); };
	return {scale(rgb >> 16u), scale(rgb >> 8u), scale(rgb), scale(0xFFu)};
}

Axes gesture_hints::Tumble(const Axes& axes, LeashType leash, float seconds)
{
	// Each leash turns at its own pace about each axis: the higher its number, the faster about the second axis and
	// the slower about the other two
	constexpr float k_FirstRate = 0.65823f;
	constexpr float k_ThirdRate = 0.79543f;
	constexpr float k_FirstSlowing = 0.05f;
	constexpr float k_SecondQuickening = 0.05f;
	constexpr float k_ThirdSlowing = 0.06f;
	const auto number = static_cast<float>(leash);
	const float second = (number * k_SecondQuickening) + 1.0f;
	auto turned = axes;
	Turn(turned.y, turned.z, (1.0f - (number * k_FirstSlowing)) * k_FirstRate * seconds);
	Turn(turned.z, turned.x, seconds * second);
	Turn(turned.x, turned.y, (1.0f - (number * k_ThirdSlowing)) * second * k_ThirdRate * seconds);
	return turned;
}

glm::mat4 gesture_hints::CollarModel(const Axes& axes)
{
	glm::mat4 model(1.0f);
	model[0] = glm::vec4(axes.x * k_CollarScale, 0.0f);
	model[1] = glm::vec4(axes.y * k_CollarScale, 0.0f);
	model[2] = glm::vec4(axes.z * k_CollarScale, 0.0f);
	model[3] = glm::vec4(k_CollarAt, 1.0f);
	return model;
}

glm::mat4 gesture_hints::CollarView()
{
	return glm::lookAt(k_CollarEye, k_CollarAt, glm::vec3(0.0f, 1.0f, 0.0f));
}

float gesture_hints::FocalPixels(float rowWidth, float horizontalFov)
{
	return rowWidth * 0.5f / std::tan(horizontalFov * 0.5f);
}

PixelRect gesture_hints::PixelsOf(const Box& square)
{
	const auto min = glm::ivec2(glm::round(square.min));
	const auto max = glm::ivec2(glm::round(square.max));
	return {.x = min.x, .y = min.y, .width = max.x - min.x, .height = max.y - min.y};
}

glm::mat4 gesture_hints::CollarProjection(const PixelRect& rect, glm::vec2 centre, float focalPixels, float nearClip)
{
	// The rectangle's edges on the near plane, up the screen being up
	const float scale = nearClip / focalPixels;
	const float left = (static_cast<float>(rect.x) - centre.x) * scale;
	const float right = (static_cast<float>(rect.x + rect.width) - centre.x) * scale;
	const float top = (centre.y - static_cast<float>(rect.y)) * scale;
	const float bottom = (centre.y - static_cast<float>(rect.y + rect.height)) * scale;
	glm::mat4 projection(0.0f);
	projection[0][0] = 2.0f * nearClip / (right - left);
	projection[1][1] = 2.0f * nearClip / (top - bottom);
	projection[2][0] = -(right + left) / (right - left);
	projection[2][1] = -(top + bottom) / (top - bottom);
	projection[2][2] = 1.0f;
	projection[2][3] = 1.0f;
	projection[3][2] = -2.0f * nearClip;
	return projection;
}

void gesture_hints::Step(Row& row, float seconds, bool wordsShowing)
{
	row.alpha = Fade(row.alpha, seconds, wordsShowing);
	if (row.alpha <= 0.0f)
	{
		return;
	}
	for (auto& hint : row.hints)
	{
		if (hint.path.empty())
		{
			continue;
		}
		const auto leash = static_cast<size_t>(hint.leash);
		if (hint.leash != LeashType::None && leash < row.collars.size())
		{
			row.collars.at(leash) = Tumble(row.collars.at(leash), hint.leash, seconds);
		}
		hint.dot = StepDot(hint.dot, seconds, hint.period);
	}
}
