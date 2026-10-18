/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SingAlong.h"

#include <algorithm>

namespace openblack::help::sing_along
{
namespace
{
/// Milliseconds to seconds, as the game multiplies them
constexpr float k_SecondsPerMillisecond = 0.001f;
} // namespace

bool IsSungVerse(int32_t scriptMusicType)
{
	return scriptMusicType >= k_FirstVerse && scriptMusicType <= k_LastVerse;
}

int32_t StepMilliseconds(bool inTemple, int32_t realMs, int32_t gameMs)
{
	return std::clamp(inTemple ? realMs : gameMs, 0, k_MostStepMilliseconds);
}

glm::vec4 GlowColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	const auto half = static_cast<uint8_t>(a >> 1u);
	return glm::vec4(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(half)) / 255.0f;
}

Box Highlight::Step(glm::vec2 wordAt, float wordWidth, float lineHeight, int32_t stepMs)
{
	_left.SetDestination(wordAt.x, k_GlideSeconds);
	_top.SetDestination(wordAt.y, k_GlideSeconds);
	_width.SetDestination(wordWidth, k_GlideSeconds);
	const float seconds = static_cast<float>(stepMs) * k_SecondsPerMillisecond;
	_left.Update(seconds);
	_top.Update(seconds);
	_width.Update(seconds);
	// Each side is cut to a whole pixel towards zero
	const float left = _left.GetValue();
	const float top = _top.GetValue();
	const float right = _width.GetValue() + left;
	const float bottom = top + lineHeight;
	return {
	    .min = {static_cast<int>(left), static_cast<int>(top)},
	    .max = {static_cast<int>(right), static_cast<int>(bottom)},
	};
}

} // namespace openblack::help::sing_along
