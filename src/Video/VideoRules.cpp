/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VideoRules.h"

#include <cstdlib>

#include <algorithm>

#include "Gui/ScreenPixels.h"

using namespace openblack::video;

namespace
{
/// The film times the timeline moves on after, in milliseconds
constexpr int32_t k_RumbleMs = 17450;
constexpr int32_t k_LaserMs = 19450;
constexpr int32_t k_CreedMs = 31650;
constexpr int32_t k_CitadelMs = 13450;
constexpr int32_t k_HealMs = 37750;
constexpr int32_t k_WhiteMs = 43900;
} // namespace

std::optional<Schedule> openblack::video::SkipSchedule(int32_t frame, Schedule schedule, int32_t frameCount) noexcept
{
	if (frame > schedule.fadeStart)
	{
		return std::nullopt;
	}
	return Schedule {.fadeStart = frame, .end = std::min(frame + k_SkipFadeFrames, frameCount)};
}

float openblack::video::VideoAlpha(int32_t frame, Schedule schedule) noexcept
{
	switch (PhaseAt(frame, schedule))
	{
	case Phase::Playing:
		return 1.0f;
	case Phase::Fading:
		return 1.0f - static_cast<float>(frame - schedule.fadeStart) /
		                  static_cast<float>(std::abs(schedule.end - schedule.fadeStart) + 1);
	case Phase::Ended:
		break;
	}
	return 0.0f;
}

uint8_t openblack::video::DrawAlpha(float alpha, bool fallingSpell) noexcept
{
	const float strength = fallingSpell ? k_FallingSpellStrength : k_OpaqueStrength;
	return static_cast<uint8_t>(static_cast<int32_t>(strength * alpha));
}

ScreenRect openblack::video::LetterboxRect(int32_t screenWidth, int32_t screenHeight) noexcept
{
	const auto fit = gui::FitInside(gui::k_SixteenByNine, screenWidth, screenHeight);
	return {.x = fit.x, .y = fit.y, .width = fit.width + 1, .height = fit.height + 1};
}

std::vector<PictureTile> openblack::video::PictureTiles(ScreenRect rect, uint32_t width, uint32_t height)
{
	std::vector<PictureTile> tiles;
	if (width == 0 || height == 0)
	{
		return tiles;
	}
	const auto texels = static_cast<uint32_t>(k_TileTexels);
	const uint32_t columns = (width + texels - 1) / texels;
	const uint32_t rows = (height + texels - 1) / texels;
	// The screen size of one texel, in single precision
	const float scaleX = static_cast<float>(rect.width) / static_cast<float>(width);
	const float scaleY = static_cast<float>(rect.height) / static_cast<float>(height);
	// The last column and row have the remaining texels, or a full tile when no texels remain
	const auto lastColumn = width % texels != 0 ? width % texels : texels;
	const auto lastRow = height % texels != 0 ? height % texels : texels;
	// A tile starts half a texel in and stops half a texel before its end
	constexpr float k_TexelStep = 1.0f / 256.0f;
	constexpr float k_HalfTexelStep = 1.0f / 512.0f;
	constexpr auto k_TileSize = static_cast<float>(k_TileTexels);
	const auto pixelCentre = glm::vec2(gui::screen_pixels::k_PixelCentre);
	tiles.reserve(static_cast<size_t>(columns) * rows);
	for (uint32_t row = 0; row < rows; ++row)
	{
		const auto rowTexels = row + 1 == rows ? lastRow : texels;
		const auto firstRowTexel = static_cast<float>(row * texels);
		const float top = (firstRowTexel * scaleY) + static_cast<float>(rect.y);
		const float bottom = (static_cast<float>(rowTexels) * scaleY) + top;
		const float vEnd = (static_cast<float>(rowTexels) * k_TexelStep) - k_HalfTexelStep;
		for (uint32_t column = 0; column < columns; ++column)
		{
			const auto columnTexels = column + 1 == columns ? lastColumn : texels;
			const auto firstColumnTexel = static_cast<float>(column * texels);
			const float left = (firstColumnTexel * scaleX) + static_cast<float>(rect.x);
			const float right = (static_cast<float>(columnTexels) * scaleX) + left;
			const float uEnd = (static_cast<float>(columnTexels) * k_TexelStep) - k_HalfTexelStep;
			const glm::vec2 first(firstColumnTexel, firstRowTexel);
			tiles.push_back({
			    .min = glm::vec2(left, top) + pixelCentre,
			    .max = glm::vec2(right, bottom) + pixelCentre,
			    .texelMin = first + (glm::vec2(k_HalfTexelStep) * k_TileSize),
			    .texelMax = first + (glm::vec2(uEnd, vEnd) * k_TileSize),
			});
		}
	}
	return tiles;
}

uint32_t openblack::video::FramesDue(std::chrono::microseconds elapsed, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept
{
	if (elapsed.count() < 0 || fpsDenominator == 0)
	{
		return 1;
	}
	const auto us = static_cast<uint64_t>(elapsed.count());
	return static_cast<uint32_t>(us * fpsNumerator / (uint64_t {1'000'000} * fpsDenominator)) + 1;
}

int32_t openblack::video::FilmMilliseconds(int32_t frame, int32_t fps) noexcept
{
	const auto scaled = static_cast<int32_t>(static_cast<uint32_t>(frame) * 1000u);
	return fps > 0 ? scaled / fps : 0;
}

void FallingSpellTimeline::Advance(int32_t filmMs, bool whiteReached, std::vector<FallingSpellCue>& cues)
{
	using enum FallingSpellCue;
	// The sounds first, each test after the one before, so one frame can pass several
	if (_soundState == 0 && filmMs > k_RumbleMs)
	{
		_soundState = 1;
		cues.push_back(Rumble);
	}
	if (_soundState == 1 && filmMs > k_LaserMs)
	{
		_soundState = 2;
		cues.insert(cues.end(), {LaserExplode, VolcanoStop});
	}
	if (_soundState == 2 && filmMs > k_CreedMs)
	{
		_soundState = 3;
		cues.push_back(Creed);
	}
	if (_state == 0 && filmMs > k_CitadelMs)
	{
		_state = 1;
		cues.insert(cues.end(), {CitadelExplode, Volcano});
	}
	if (_state == 1 && filmMs > k_HealMs)
	{
		_state = 2;
		cues.insert(cues.end(), {HealChakra, CreedHigh});
	}
	if (_state == 2 && filmMs > k_WhiteMs)
	{
		_state = 3;
		cues.insert(cues.end(), {WhiteFadeStart, CreedStop, CreedHighStop, MusicFadeOut, CitadelExplodeEnd});
		return;
	}
	if (_state == 3 && whiteReached)
	{
		_state = k_EndState;
		cues.push_back(WhiteFadeBack);
	}
}
