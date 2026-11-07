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
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

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

uint32_t speech_bubble::BoxColour() noexcept
{
	constexpr uint32_t k_DefaultColour = 0xFF3BBDEDu;
	return ((k_DefaultColour >> 1u) & 0x7F7F7Fu) | 0xFF000000u;
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

people::Category& people::List::Of(Group group) noexcept
{
	switch (group)
	{
	case Group::Friends:
		return friends;
	case Group::Others:
		return others;
	case Group::Players:
		break;
	}
	return players;
}

const people::Category& people::List::Of(Group group) const noexcept
{
	switch (group)
	{
	case Group::Friends:
		return friends;
	case Group::Others:
		return others;
	case Group::Players:
		break;
	}
	return players;
}

people::Category people::MakePlayers(std::vector<Person> players)
{
	std::ranges::stable_sort(players, {}, &Person::slot);
	for (auto& player : players)
	{
		player.selected = true;
	}
	return {.expanded = true, .messagesOn = true, .people = std::move(players)};
}

namespace
{

constexpr std::array<people::Group, 3> k_GroupOrder {people::Group::Players, people::Group::Friends, people::Group::Others};

/// The first person of a group with the number, if any
people::Person* FindUser(people::Category& category, const std::optional<people::UserId>& user)
{
	const auto found = std::ranges::find(category.people, user, &people::Person::user);
	return found != category.people.end() ? &*found : nullptr;
}

/// Removes the first person of a group with the number
void RemoveUser(people::Category& category, const std::optional<people::UserId>& user)
{
	if (const auto found = std::ranges::find(category.people, user, &people::Person::user); found != category.people.end())
	{
		category.people.erase(found);
	}
}

void ClearSelection(people::List& list)
{
	for (const auto group : k_GroupOrder)
	{
		for (auto& person : list.Of(group).people)
		{
			person.selected = false;
		}
	}
	list.lastSelected.reset();
}

} // namespace

std::vector<people::Row> people::Rows(const List& list)
{
	std::vector<Row> rows;
	for (const auto group : k_GroupOrder)
	{
		const auto& category = list.Of(group);
		if (category.people.empty())
		{
			continue;
		}
		rows.push_back({.group = group, .person = std::nullopt});
		if (category.expanded)
		{
			for (size_t i = 0; i < category.people.size(); ++i)
			{
				rows.push_back({.group = group, .person = i});
			}
		}
	}
	return rows;
}

size_t people::RowCount(const List& list)
{
	return Rows(list).size();
}

void people::Apply(List& list, const Click& click)
{
	const auto rows = Rows(list);
	if (click.row >= rows.size())
	{
		return;
	}
	const auto& row = rows[click.row];
	if (!row.person.has_value())
	{
		ClearSelection(list);
		auto& category = list.Of(row.group);
		if (click.onSpeechButton)
		{
			category.messagesOn = !category.messagesOn;
			category.expanded = category.messagesOn;
		}
		else
		{
			category.expanded = !category.expanded;
		}
		return;
	}
	const auto personAt = [&list, &rows](size_t index) -> Person* {
		if (index >= rows.size() || !rows[index].person.has_value())
		{
			return nullptr;
		}
		return &list.Of(rows[index].group).people.at(*rows[index].person);
	};
	const auto last = list.lastSelected;
	const bool modifier = click.shift || click.ctrl || click.otherModifier;
	if (!modifier && personAt(click.row)->selected)
	{
		ClearSelection(list);
		list.lastSelected = click.row;
		return;
	}
	if (!click.ctrl)
	{
		ClearSelection(list);
	}
	// The people of the rows change to the opposite of the clicked person. The headings do not change.
	const auto select = [&personAt](size_t first, size_t lastRow) {
		const bool selected = !personAt(lastRow)->selected;
		for (size_t index = std::min(first, lastRow); index <= std::max(first, lastRow); ++index)
		{
			if (auto* person = personAt(index); person != nullptr)
			{
				person->selected = selected;
			}
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
	listed.reserve(list.players.people.size());
	for (const auto& player : list.players.people)
	{
		listed.push_back({.slot = player.slot, .selected = player.selected});
	}
	return listed;
}

void people::MarkFriends(List& list)
{
	for (auto& friendPerson : list.friends.people)
	{
		for (auto* category : {&list.others, &list.players})
		{
			if (auto* person = FindUser(*category, friendPerson.user); person != nullptr)
			{
				person->isFriend = true;
			}
		}
		friendPerson.isFriend = true;
	}
}

people::FriendButton people::FriendButtonOf(const List& list)
{
	FriendButton button;
	bool found = false;
	for (const auto group : k_GroupOrder)
	{
		for (const auto& person : list.Of(group).people)
		{
			if (!person.selected)
			{
				continue;
			}
			if (!found)
			{
				found = true;
				button = {.usable = true, .removes = person.isFriend};
			}
			else if (person.isFriend != button.removes)
			{
				button.usable = false;
			}
		}
	}
	return button;
}

void people::MakeFriends(List& list)
{
	auto& friends = list.friends.people;
	// The selected players.
	for (const auto& player : list.players.people)
	{
		if (!player.selected)
		{
			continue;
		}
		if (player.isFriend)
		{
			if (auto* friendPerson = FindUser(list.friends, player.user); friendPerson != nullptr)
			{
				friendPerson->selected = true;
			}
		}
		else
		{
			auto copy = player;
			copy.selected = false;
			copy.isFriend = true;
			friends.insert(friends.begin(), std::move(copy));
		}
	}
	// The selected others. One that becomes a friend leaves the others, and the search starts again.
	for (bool again = true; again;)
	{
		again = false;
		for (const auto& other : list.others.people)
		{
			if (!other.selected)
			{
				continue;
			}
			if (other.isFriend)
			{
				if (auto* friendPerson = FindUser(list.friends, other.user); friendPerson != nullptr)
				{
					friendPerson->selected = true;
				}
				continue;
			}
			auto moved = other;
			moved.selected = false;
			friends.insert(friends.begin(), moved);
			RemoveUser(list.others, moved.user);
			again = true;
			break;
		}
	}
	// The selected friends go
	for (auto found = std::ranges::find_if(friends, &Person::selected); found != friends.end();
	     found = std::ranges::find_if(friends, &Person::selected))
	{
		const auto user = found->user;
		RemoveUser(list.friends, user);
		if (auto* player = FindUser(list.players, user); player != nullptr)
		{
			player->selected = false;
			player->isFriend = false;
		}
	}
	MarkFriends(list);
}

std::vector<people::FriendRecord> people::KeptFriends(const List& list)
{
	std::vector<FriendRecord> records;
	for (const auto& friendPerson : list.friends.people)
	{
		if (records.size() == k_MostKeptFriends)
		{
			break;
		}
		records.push_back({.user = friendPerson.user.value_or(0), .name = friendPerson.name});
	}
	return records;
}

void people::LoadFriends(List& list, std::span<const FriendRecord> records)
{
	list.friends.people.clear();
	for (const auto& record : records.first(std::min(records.size(), k_MostKeptFriends)))
	{
		list.friends.people.push_back({.slot = PlayerNames::PLAYER_ONE,
		                               .user = record.user,
		                               .name = record.name,
		                               .selected = false,
		                               .isFriend = false,
		                               .online = false,
		                               .lastSeen = 0,
		                               .colour = k_NoGodColour});
	}
	MarkFriends(list);
}

void people::AddOther(List& list, std::u16string_view name, UserId user, std::optional<UserId> own)
{
	if (own == user || FindUser(list.friends, user) != nullptr)
	{
		return;
	}
	const bool staff = user > k_BeforeFirstStaff && user < k_AfterLastStaff;
	list.others.people.insert(list.others.people.begin(), {.slot = PlayerNames::PLAYER_ONE,
	                                                       .user = user,
	                                                       .name = std::u16string(name),
	                                                       .selected = false,
	                                                       .isFriend = false,
	                                                       .online = false,
	                                                       .lastSeen = 0,
	                                                       .colour = staff ? k_StaffColour : k_NoGodColour});
	MarkFriends(list);
}

void people::UpdateOnline(List& list, std::span<const UserId> seen, uint32_t now)
{
	for (const auto user : seen)
	{
		for (auto& friendPerson : list.friends.people)
		{
			if (friendPerson.user == user)
			{
				friendPerson.lastSeen = now;
			}
		}
	}
	for (auto& friendPerson : list.friends.people)
	{
		if (friendPerson.lastSeen > now)
		{
			friendPerson.lastSeen = 0;
		}
		friendPerson.online =
		    FindUser(list.players, friendPerson.user) != nullptr || now - friendPerson.lastSeen < k_OnlineTime;
	}
}

std::vector<people::UserId> people::FriendNumbers(const List& list)
{
	std::vector<UserId> numbers;
	numbers.reserve(list.friends.people.size());
	for (const auto& friendPerson : list.friends.people)
	{
		numbers.push_back(friendPerson.user.value_or(0));
	}
	return numbers;
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
