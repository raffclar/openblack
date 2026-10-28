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

#include "Gui/MultiplayerFrontEndRules.h"

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

bool SkipsHeldFrame(bool steppedByTool, bool paused, bool workWaits)
{
	return steppedByTool && paused && !workWaits;
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
	const auto turnMs = std::max(ms - recordSinceTurnMs, 0.0f);
	recordSinceTurnMs = 0.0f;
	++turns;
	turnTotalMs += turnMs;
	turnLongestMs = std::max(turnLongestMs, turnMs);
}

void Timings::AddRecord(float ms)
{
	++records;
	recordTotalMs += ms;
	recordLongestMs = std::max(recordLongestMs, ms);
	recordSinceTurnMs += ms;
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

float Timings::RecordAverageMs() const
{
	return records == 0 ? 0.0f : recordTotalMs / static_cast<float>(records);
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

std::optional<ConditionsRequest> ParseConditions(std::string_view text)
{
	ConditionsRequest request;
	if (const auto colon = text.find(':'); colon != std::string_view::npos)
	{
		request.limitsLand = std::string(text.substr(0, colon));
		text = text.substr(colon + 1);
	}
	const auto readNumber = [](std::string_view digits, auto& out) {
		const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), out);
		return error == std::errc() && end == digits.data() + digits.size() && !digits.empty();
	};
	while (!text.empty())
	{
		const auto comma = text.find(',');
		const auto item = text.substr(0, comma);
		text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
		const auto equals = item.find('=');
		if (equals == std::string_view::npos)
		{
			return std::nullopt;
		}
		const auto name = item.substr(0, equals);
		size_t index = 0;
		if (const auto named = gui::multiplayer::ConditionIndex(name); named.has_value())
		{
			index = *named;
		}
		else if (!readNumber(name, index))
		{
			return std::nullopt;
		}
		int32_t value = 0;
		if (index >= static_cast<size_t>(game_end::Condition::_Count) || !readNumber(item.substr(equals + 1), value))
		{
			return std::nullopt;
		}
		const auto condition = static_cast<game_end::Condition>(index);
		if (std::ranges::find(request.picks, condition, &game_end::ConditionPick::condition) != request.picks.end())
		{
			return std::nullopt;
		}
		request.picks.push_back({.condition = condition, .value = value});
	}
	if (request.picks.empty())
	{
		return std::nullopt;
	}
	return request;
}

} // namespace openblack::sim_run
