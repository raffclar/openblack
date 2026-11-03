/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GatheringBoxRules.h"

#include <algorithm>
#include <ranges>

using namespace openblack::gathering_box;
using openblack::chat::people::Category;
using openblack::chat::people::List;
using openblack::chat::people::Person;

namespace
{

Person* FindUser(Category& category, UserId user)
{
	const auto found = std::ranges::find(category.people, std::optional<UserId>(user), &Person::user);
	return found != category.people.end() ? &*found : nullptr;
}

bool HasUser(const Category& category, UserId user)
{
	return std::ranges::find(category.people, std::optional<UserId>(user), &Person::user) != category.people.end();
}

void AppendNumber(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
	{
		bytes.push_back(static_cast<uint8_t>(value >> shift));
	}
}

} // namespace

bool PingClock::Due(uint32_t now) noexcept
{
	// The clock does not run before the first check. The time wraps as the time of the computer wraps.
	if (_start.has_value() && now - *_start < k_PingTime)
	{
		return false;
	}
	_start = now;
	return true;
}

std::optional<question::Question> question::Parse(std::u16string_view name, std::u16string_view text, UserId sender,
                                                  bool plainChat)
{
	if (plainChat || text.size() <= k_SecretWord.size() || !text.starts_with(k_SecretWord))
	{
		return std::nullopt;
	}
	auto words = text.substr(k_SecretWord.size());
	const bool line = words.front() == k_LineMark;
	if (line)
	{
		words.remove_prefix(1);
	}
	std::u16string shown(name);
	shown += u' ';
	shown += words;
	return Question {.kind = line ? Kind::Line : Kind::YesNo, .text = std::move(shown), .sender = sender};
}

question::Push question::PushOf(Key key, bool lineHasFocus) noexcept
{
	if (key == Key::Escape)
	{
		return Push::No;
	}
	return lineHasFocus ? Push::FocusFirstButton : Push::First;
}

std::u16string question::AnswerOf(Kind kind, Push push, std::u16string_view line)
{
	if (kind == Kind::Line)
	{
		return std::u16string(line.substr(0, std::min(line.size(), k_MostAnswerCharacters)));
	}
	switch (push)
	{
	case Push::First:
		return std::u16string(k_YesAnswer);
	case Push::No:
		return std::u16string(k_NoAnswer);
	default:
		return {};
	}
}

std::u16string_view openblack::gathering_box::ClippedLine(std::u16string_view text) noexcept
{
	return text.substr(0, std::min(text.size(), k_MostLineCharacters));
}

bool openblack::gathering_box::SeePerson(List& list, std::u16string_view name, UserId user, std::optional<UserId> own,
                                         uint32_t now)
{
	const auto kept = name.substr(0, std::min(name.size(), k_MostNameCharacters));
	if (auto* player = FindUser(list.players, user); player != nullptr)
	{
		player->name = kept;
	}
	if (auto* friendPerson = FindUser(list.friends, user); friendPerson != nullptr)
	{
		friendPerson->lastSeen = now;
		friendPerson->online = true;
		friendPerson->name = kept;
	}
	if (auto* other = FindUser(list.others, user); other != nullptr)
	{
		other->name = kept;
	}
	else
	{
		chat::people::AddOther(list, kept, user, own);
	}
	if (user != k_AdminUser)
	{
		return false;
	}
	list.others.expanded = true;
	return true;
}

bool openblack::gathering_box::ShowsLine(const List& list, UserId user)
{
	return std::ranges::any_of(std::array {&list.players, &list.others, &list.friends},
	                           [user](const Category* category) { return category->messagesOn && HasUser(*category, user); });
}

std::vector<UserId> openblack::gathering_box::PickedFriendsAndOthers(const List& list)
{
	const auto picked = [](const Category& category) { return std::ranges::any_of(category.people, &Person::selected); };
	std::vector<UserId> numbers;
	if (!picked(list.players) && !picked(list.friends) && !picked(list.others))
	{
		return numbers;
	}
	for (const auto* category : {&list.friends, &list.others})
	{
		for (const auto& person : category->people)
		{
			if (person.selected && person.user.has_value())
			{
				numbers.push_back(*person.user);
			}
		}
	}
	return numbers;
}

std::vector<uint8_t> host_report::Encode(const Facts& facts)
{
	std::vector<uint8_t> bytes(facts.game.begin(), facts.game.end());
	AppendNumber(bytes, facts.valueIsOne ? 1 : 0);
	uint32_t mask = 0;
	for (size_t i = 0; i < k_ConditionCount; ++i)
	{
		if (facts.conditions[i].has_value())
		{
			mask |= 1u << i;
		}
	}
	AppendNumber(bytes, mask);
	for (const auto& condition : facts.conditions)
	{
		if (!condition.has_value())
		{
			continue;
		}
		AppendNumber(bytes, static_cast<uint32_t>(condition->aim));
		for (const auto& share : condition->players)
		{
			AppendNumber(bytes, static_cast<uint32_t>(share.met ? condition->aim : share.current));
		}
	}
	return bytes;
}
