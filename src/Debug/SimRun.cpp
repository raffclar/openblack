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
#include "PlayerSlots.h"

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

bool EndsHeldInTemple(const Settings& settings, bool clockHeld, bool networkGame, bool scenarioGaveAll)
{
	return settings.untilTurn.has_value() && clockHeld && !networkGame && scenarioGaveAll;
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
	// A number that fills the whole text
	const auto number = [](std::string_view part) -> std::optional<uint32_t> {
		uint32_t value = 0;
		const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
		if (part.empty() || error != std::errc() || end != part.data() + part.size())
		{
			return std::nullopt;
		}
		return value;
	};
	const auto playerText = text.substr(0, colon);
	const auto dash = playerText.find('-');
	const auto player = number(playerText.substr(0, dash));
	const auto lastPlayer = dash == std::string_view::npos ? player : number(playerText.substr(dash + 1));
	const auto turn = number(text.substr(colon + 1));
	constexpr uint32_t k_LastPlayer = player_slots::k_PlayerNumbers - 1;
	if (!player.has_value() || !lastPlayer.has_value() || !turn.has_value() || *lastPlayer > k_LastPlayer ||
	    *player > *lastPlayer)
	{
		return std::nullopt;
	}
	return TempleLoss {.player = static_cast<uint8_t>(*player), .turn = *turn, .lastPlayer = static_cast<uint8_t>(*lastPlayer)};
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
		if (index >= static_cast<size_t>(game_end::Condition::_Count))
		{
			return std::nullopt;
		}
		std::optional<int32_t> value;
		if (equals != std::string_view::npos)
		{
			int32_t number = 0;
			if (!readNumber(item.substr(equals + 1), number))
			{
				return std::nullopt;
			}
			value = number;
		}
		const auto condition = static_cast<game_end::Condition>(index);
		if (std::ranges::find(request.conditions, condition, &game_end::SwitchedCondition::condition) !=
		    request.conditions.end())
		{
			return std::nullopt;
		}
		request.conditions.push_back({.condition = condition, .value = value});
	}
	if (request.conditions.empty())
	{
		return std::nullopt;
	}
	return request;
}

std::optional<std::vector<game_end::ConditionPick>> PicksWithValues(const ConditionsRequest& request)
{
	std::vector<game_end::ConditionPick> picks;
	for (const auto& each : request.conditions)
	{
		if (!each.value.has_value())
		{
			return std::nullopt;
		}
		picks.push_back({.condition = each.condition, .value = *each.value});
	}
	return picks;
}

} // namespace openblack::sim_run
