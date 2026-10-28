/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SimRun.h"

#include <charconv>
#include <cmath>

#include <algorithm>
#include <system_error>

namespace openblack::sim_run
{

bool DrawsFrame(const Settings& settings, uint64_t frame, bool pictureWanted)
{
	if (!settings.fast || pictureWanted)
	{
		return true;
	}
	return settings.drawEvery != 0 && frame % settings.drawEvery == 0;
}

bool Finished(const Settings& settings, uint32_t turn)
{
	return settings.untilTurn.has_value() && turn >= *settings.untilTurn;
}

std::chrono::microseconds PaceWait(const Settings& settings, std::optional<std::chrono::microseconds> frameTime,
                                   std::chrono::microseconds frameTook)
{
	if (settings.fast || !settings.paceToWallClock || !frameTime.has_value())
	{
		return std::chrono::microseconds {0};
	}
	return std::max(*frameTime - frameTook, std::chrono::microseconds {0});
}

std::optional<std::chrono::microseconds> FrameTimeFromMs(double milliseconds)
{
	constexpr double k_MicrosecondsPerMs = 1000.0;
	if (!(milliseconds >= 1.0 && milliseconds <= static_cast<double>(k_LongestFrameMs)))
	{
		return std::nullopt;
	}
	return std::chrono::microseconds(std::llround(milliseconds * k_MicrosecondsPerMs));
}

void Timings::AddTurn(float ms)
{
	++turns;
	turnTotalMs += ms;
	turnLongestMs = std::max(turnLongestMs, ms);
}

void Timings::AddFrame(float ms)
{
	++frames;
	frameTotalMs += ms;
	frameLongestMs = std::max(frameLongestMs, ms);
}

void Timings::AddStages(std::span<const float> ms)
{
	if (stageTotalMs.size() < ms.size())
	{
		stageTotalMs.resize(ms.size(), 0.0f);
	}
	for (size_t i = 0; i < ms.size(); ++i)
	{
		stageTotalMs[i] += ms[i];
	}
}

float Timings::TurnAverageMs() const
{
	return turns == 0 ? 0.0f : turnTotalMs / static_cast<float>(turns);
}

float Timings::FrameAverageMs() const
{
	return frames == 0 ? 0.0f : frameTotalMs / static_cast<float>(frames);
}

std::optional<TempleLoss> ParseTempleLoss(std::string_view text)
{
	const auto colon = text.find(':');
	if (colon == std::string_view::npos)
	{
		return std::nullopt;
	}
	unsigned player = 0;
	uint32_t turn = 0;
	const auto playerText = text.substr(0, colon);
	const auto turnText = text.substr(colon + 1);
	const auto [playerEnd, playerError] = std::from_chars(playerText.data(), playerText.data() + playerText.size(), player);
	const auto [turnEnd, turnError] = std::from_chars(turnText.data(), turnText.data() + turnText.size(), turn);
	constexpr unsigned k_LastPlayer = 6;
	if (playerError != std::errc() || turnError != std::errc() || playerEnd != playerText.data() + playerText.size() ||
	    turnEnd != turnText.data() + turnText.size() || player > k_LastPlayer)
	{
		return std::nullopt;
	}
	return TempleLoss {.player = static_cast<uint8_t>(player), .turn = turn};
}

} // namespace openblack::sim_run
