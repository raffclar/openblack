/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mists.h"

#include <cmath>

#include <glm/geometric.hpp>

#include "Common/X87.h"

using namespace openblack;

namespace
{
uint32_t LowByte(float value) noexcept
{
	return static_cast<uint32_t>(x87::ToInteger(value)) & 0xFFu;
}
} // namespace

float mists::EdgeOnSize(float size, float edgeShrink, const glm::vec3& toMist)
{
	const float lengthSquared = glm::dot(toMist, toMist);
	if (lengthSquared == 0.0f)
	{
		return size;
	}
	return size / (1.0f + ((edgeShrink - 1.0f) * (1.0f - (std::abs(toMist.y) / std::sqrt(lengthSquared)))));
}

void mists::Advance(int& counter, float& remainder, float milliseconds) noexcept
{
	remainder += milliseconds * k_Rate;
	const auto step = static_cast<int>(remainder);
	remainder -= static_cast<float>(step);
	counter += step;
	// Only once it passes the end
	if (counter > k_CounterWrap)
	{
		counter %= k_CounterWrap;
	}
}

mists::Fade mists::StartFade(float startSize, float endSize, float startAlpha, float endAlpha, float seconds) noexcept
{
	const float turns = seconds * k_FadeTurnsPerSecond;
	return {
	    .turns = turns,
	    .elapsed = 0.0f,
	    .sizeStep = (endSize - startSize) / turns,
	    .alphaStep = (endAlpha - startAlpha) / turns,
	    .alpha = startAlpha,
	};
}

uint8_t mists::AlphaByte(float alpha) noexcept
{
	return static_cast<uint8_t>(LowByte(alpha));
}

int32_t mists::WholeAlpha(float alpha) noexcept
{
	return x87::ToInteger(alpha);
}

uint32_t mists::ScriptColour(float red, float green, float blue, float transparency) noexcept
{
	constexpr float k_ByteMax = 255.0f;
	const float alpha = transparency * k_ByteMax;
	return (LowByte(alpha) << 24u) | (LowByte(red) << 16u) | (LowByte(green) << 8u) | LowByte(blue);
}

bool mists::StepFade(Fade& fade, float& size) noexcept
{
	const float elapsed = fade.elapsed + 1.0f;
	if (elapsed > fade.turns)
	{
		return false;
	}
	fade.elapsed = elapsed;
	size = fade.sizeStep + size;
	fade.alpha = fade.alphaStep + fade.alpha;
	if (fade.alpha < 0.0f)
	{
		fade.alpha = 0.0f;
	}
	if (fade.alpha > 255.0f)
	{
		fade.alpha = 255.0f;
	}
	return true;
}

float mists::FadeDone(float elapsed, float turns) noexcept
{
	return elapsed / turns;
}
