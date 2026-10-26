/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChatRules.h"

#include <algorithm>
#include <array>
#include <utility>

namespace openblack::chat
{

namespace
{

/// The characters of the Western Windows code page from 0x80 to 0x9F. A zero has no character.
constexpr std::array<char16_t, 32> k_HighCharacters {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
};

/// The byte of a character in the Western Windows code page, '?' when the code page does not have it
char CodePageByte(char16_t character)
{
	if (character < 0x80 || (character >= 0xA0 && character <= 0xFF))
	{
		return static_cast<char>(character);
	}
	for (size_t i = 0; i < k_HighCharacters.size(); ++i)
	{
		if (k_HighCharacters.at(i) != 0 && k_HighCharacters.at(i) == character)
		{
			return static_cast<char>(0x80 + i);
		}
	}
	return '?';
}

} // namespace

std::u16string_view WithoutStaffCode(std::u16string_view text) noexcept
{
	constexpr size_t k_MostCodeCharacters = 19;
	size_t at = 0;
	const auto next = [&text, &at]() { return at < text.size() ? text[at] : u'\0'; };
	for (const auto letter : {u'!', u'S', u'G', u'R', u'B', u'O'})
	{
		if (next() == letter)
		{
			++at;
		}
	}
	if (next() == u'N')
	{
		++at;
		for (size_t count = 0; next() != u'#' && count < k_MostCodeCharacters && next() != u'\0'; ++count)
		{
			++at;
		}
	}
	if (next() == u'#')
	{
		++at;
	}
	return text.substr(at);
}

std::optional<uint32_t> TauntText(uint32_t taunt) noexcept
{
	if (taunt > k_LastTaunt)
	{
		return std::nullopt;
	}
	return k_FirstTauntText + taunt;
}

std::vector<PlayerNames> Recipients(std::span<const ListedPlayer> players)
{
	const bool anySelected = std::ranges::any_of(players, &ListedPlayer::selected);
	std::vector<PlayerNames> recipients;
	for (const auto& player : players)
	{
		if (!anySelected || player.selected)
		{
			recipients.push_back(player.slot);
		}
	}
	return recipients;
}

bool ShowsHere(PlayerNames sender, std::optional<PlayerNames> here, std::span<const PlayerNames> recipients)
{
	if (here.has_value() && *here == sender)
	{
		return false;
	}
	if (recipients.empty())
	{
		return true;
	}
	return here.has_value() && std::ranges::find(recipients, *here) != recipients.end();
}

bool CreatureSays(PlayerNames player, PlayerNames sender, std::span<const PlayerNames> recipients)
{
	if (player == sender)
	{
		return false;
	}
	return recipients.empty() || std::ranges::find(recipients, player) != recipients.end();
}

std::u16string NamedLine(std::u16string_view name, std::u16string_view text)
{
	std::u16string line(name);
	line += u": ";
	line += text;
	return line;
}

void speech_bubble::Say(Bubble& bubble, Line line, bool ofSender)
{
	bubble.lines.insert(bubble.lines.begin(), std::move(line));
	if (ofSender)
	{
		bubble.displayTime += k_LineTime;
	}
	bubble.scroll = 0.0f;
	if (bubble.displayTime < k_LeastTime)
	{
		bubble.displayTime = k_LeastTime;
	}
	bubble.displayTime += k_LineTime;
}

void speech_bubble::Update(Bubble& bubble, float gameMilliseconds) noexcept
{
	bubble.displayTime -= gameMilliseconds * k_TimePerHundredth;
	if (bubble.displayTime < 0.0f)
	{
		bubble.displayTime = 0.0f;
	}
}

uint32_t speech_bubble::LineColour(std::optional<uint32_t> godColour) noexcept
{
	if (!godColour.has_value() || *godColour == 0)
	{
		return 0xFFFFFFFFu;
	}
	return (*godColour >> 1u) | 0xFF808080u;
}

uint32_t speech_bubble::BoxColour(std::optional<uint32_t> godColour) noexcept
{
	constexpr uint32_t k_DefaultColour = 0xFF3BBDEDu;
	return ((godColour.value_or(k_DefaultColour) >> 1u) & 0x7F7F7Fu) | 0xFF000000u;
}

speech_bubble::Font speech_bubble::FontOf(float alignment) noexcept
{
	if (alignment > k_GoodFontAlignment)
	{
		return Font::Good;
	}
	if (alignment < k_EvilFontAlignment)
	{
		return Font::Evil;
	}
	return Font::Neutral;
}

std::string LogLine(std::u16string_view name, std::u16string_view text)
{
	std::string line;
	line.reserve(name.size() + text.size() + 3);
	for (const auto character : name)
	{
		line.push_back(CodePageByte(character));
	}
	line += ": ";
	for (const auto character : text)
	{
		line.push_back(CodePageByte(character));
	}
	line.push_back('\n');
	return line;
}

people::List people::MakeList(std::vector<Person> players)
{
	std::ranges::stable_sort(players, {}, &Person::slot);
	for (auto& player : players)
	{
		player.selected = true;
	}
	return {.expanded = true, .messagesOn = true, .players = std::move(players), .lastSelected = std::nullopt};
}

size_t people::RowCount(const List& list) noexcept
{
	return 1 + (list.expanded ? list.players.size() : 0);
}

void people::Apply(List& list, const Click& click)
{
	const auto clearAll = [&list]() {
		for (auto& player : list.players)
		{
			player.selected = false;
		}
		list.lastSelected.reset();
	};
	if (click.row == 0)
	{
		clearAll();
		if (click.onSpeechButton)
		{
			list.messagesOn = !list.messagesOn;
			list.expanded = list.messagesOn;
		}
		else
		{
			list.expanded = !list.expanded;
		}
		return;
	}
	if (!list.expanded || click.row > list.players.size())
	{
		return;
	}
	const auto last = list.lastSelected;
	const bool modifier = click.shift || click.ctrl || click.otherModifier;
	if (!modifier && list.players.at(click.row - 1).selected)
	{
		clearAll();
		list.lastSelected = click.row;
		return;
	}
	if (!click.ctrl)
	{
		clearAll();
	}
	// The players of the rows change to the opposite of the clicked player
	const auto select = [&list](size_t first, size_t lastRow) {
		const bool selected = !list.players.at(lastRow - 1).selected;
		for (size_t row = std::min(first, lastRow); row <= std::max(first, lastRow); ++row)
		{
			list.players.at(row - 1).selected = selected;
		}
	};
	if (click.shift)
	{
		select(last.value_or(click.row), click.row);
		list.lastSelected = last;
	}
	else
	{
		select(click.row, click.row);
		list.lastSelected = click.row;
	}
}

std::vector<ListedPlayer> people::Listed(const List& list)
{
	std::vector<ListedPlayer> listed;
	listed.reserve(list.players.size());
	for (const auto& player : list.players)
	{
		listed.push_back({.slot = player.slot, .selected = player.selected});
	}
	return listed;
}

int EditWidth(float textWidth, int layoutWidth) noexcept
{
	constexpr int k_Extra = 20;
	constexpr int k_Least = 90;
	const int width = static_cast<int>(textWidth + static_cast<float>(k_Extra));
	const int most = layoutWidth / 2;
	return width > k_Least ? std::min(width, most) : k_Least;
}

} // namespace openblack::chat
