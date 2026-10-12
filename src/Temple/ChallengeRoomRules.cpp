/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChallengeRoomRules.h"

#include <cctype>
#include <charconv>

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "Gui/TextDatabase.h"

using namespace openblack::temple;

namespace
{
struct PartName
{
	std::string_view prefix;
	challenge_room::Part part;
};
/// The room's parts by the start of their names, each followed by its place from 01 to 21. The frames' names are tried
/// longest first, as the plain frame's is the start of the others'.
constexpr std::array k_PartNames {
    PartName {"LH_Picture_Challenge_", challenge_room::Part::Picture},
    PartName {"LH_FRAME_CHALLENGE_SILVER_", challenge_room::Part::SilverFrame},
    PartName {"LH_FRAME_CHALLENGE_GOLD_", challenge_room::Part::GoldFrame},
    PartName {"LH_FRAME_CHALLENGE_", challenge_room::Part::Frame},
    PartName {"LH_Button_Challenge_", challenge_room::Part::ReplayButton},
};

bool StartsWithIgnoringCase(std::string_view text, std::string_view prefix)
{
	return text.size() >= prefix.size() && std::ranges::equal(text.substr(0, prefix.size()), prefix, [](char a, char b) {
		       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	       });
}
} // namespace

std::optional<challenge_room::Control> challenge_room::ParseControl(std::string_view subMeshName)
{
	for (const auto& [prefix, part] : k_PartNames)
	{
		if (!StartsWithIgnoringCase(subMeshName, prefix))
		{
			continue;
		}
		const auto digits = subMeshName.substr(prefix.size());
		if (digits.size() != 2 || !std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; }))
		{
			return std::nullopt;
		}
		uint32_t number = 0;
		std::from_chars(digits.data(), digits.data() + digits.size(), number);
		if (number < 1 || number > k_Places)
		{
			return std::nullopt;
		}
		return Control {.part = part, .place = number - 1};
	}
	return std::nullopt;
}

bool challenge_room::IsShown(Control control, std::span<const Challenge> challenges)
{
	if (control.place >= challenges.size())
	{
		return false;
	}
	const auto& challenge = challenges[control.place];
	switch (control.part)
	{
	case Part::Picture:
	case Part::Frame:
		return true;
	case Part::SilverFrame:
		return !challenge.quest;
	case Part::GoldFrame:
		return challenge.quest;
	case Part::ReplayButton:
		return !challenge.done;
	}
	return false;
}

challenge_room::Click challenge_room::Clicked(Control control, bool closeUp, std::span<const Challenge> challenges)
{
	if (control.place >= challenges.size())
	{
		return Click::Nothing;
	}
	const bool replays = !challenges[control.place].done;
	switch (control.part)
	{
	case Part::Picture:
		if (!closeUp)
		{
			return Click::LookCloser;
		}
		return replays ? Click::Replay : Click::Nothing;
	case Part::ReplayButton:
		return replays ? Click::Replay : Click::Nothing;
	case Part::Frame:
	case Part::SilverFrame:
	case Part::GoldFrame:
		break;
	}
	return Click::Nothing;
}

challenge_room::Duration challenge_room::TimeTaken(uint32_t startedTurn, std::optional<uint32_t> doneTurn, uint32_t turnNow,
                                                   uint32_t millisecondsPerTurn)
{
	// Whole seconds of whole turns: the turns over the turns in a second, both rounded down
	const uint32_t turns = doneTurn.value_or(turnNow) - startedTurn;
	const uint32_t turnsPerSecond = std::max(1u, 1000u / std::max(1u, millisecondsPerTurn));
	const uint32_t seconds = turns / turnsPerSecond;
	return {.hours = seconds / 3600, .minutes = (seconds / 60) % 60, .seconds = seconds % 60};
}

std::u16string challenge_room::DetailsLine(std::u16string_view timeTaken, Duration duration, std::u16string_view success,
                                           float successShare)
{
	const auto percent = static_cast<int32_t>(successShare * 100.0f);
	const auto numbers = fmt::format("{:02}:{:02}:{:02}", duration.hours, duration.minutes, duration.seconds);
	std::u16string line(timeTaken);
	line += u' ';
	line += gui::ToUtf16(numbers);
	line += u"  ";
	line += success;
	line += u' ';
	line += gui::ToUtf16(fmt::format("{}%", percent));
	return line;
}

uint8_t challenge_room::TextAlpha(float closeness)
{
	return static_cast<uint8_t>(std::clamp(static_cast<int32_t>(closeness * 255.0f), 0, 255));
}
