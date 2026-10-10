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
	const auto lost =
	    static_cast<int32_t>(static_cast<float>(screenHeight) - static_cast<float>(screenWidth) * k_SixteenByNine);
	const int32_t bar = lost / 2;
	return {.x = 0, .y = bar, .width = screenWidth + 1, .height = screenHeight - 2 * bar + 1};
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
