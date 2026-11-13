/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyMembers.h"

#include <algorithm>
#include <array>

#include <nlohmann/json.hpp>

namespace openblack::lobby_members
{

namespace
{
using Json = nlohmann::json;
} // namespace

std::variant<std::vector<Member>, std::string> ParseMembers(std::string_view text)
{
	const auto value = Json::parse(text.begin(), text.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::string("the file of the members is not a JSON object");
	}
	const auto list = value.find("members");
	if (list == value.end() || !list->is_array())
	{
		return std::string("the file of the members has no \"members\" array");
	}
	if (list->size() > k_MostMembers)
	{
		return std::string("the file of the members names more than 64 members");
	}
	std::vector<Member> members;
	std::array<bool, k_MostSlots> slotsTaken {};
	bool anyGod = false;
	for (const auto& entry : *list)
	{
		if (!entry.is_object())
		{
			return std::string("a member is not a JSON object");
		}
		const auto account = entry.find("account");
		if (account == entry.end() || !account->is_string() || account->get_ref<const std::string&>().empty() ||
		    account->get_ref<const std::string&>().size() > k_MostAccountBytes)
		{
			return std::string("a member has no account, or its account is too long");
		}
		Member member {.account = account->get<std::string>(), .role = game_socket::Role::God, .slot = std::nullopt};
		if (std::ranges::any_of(members, [&member](const Member& other) { return other.account == member.account; }))
		{
			return "the account " + member.account + " is a member two times";
		}
		if (const auto role = entry.find("role"); role != entry.end())
		{
			const auto known = role->is_string() ? game_socket::RoleFrom(role->get_ref<const std::string&>()) : std::nullopt;
			if (!known.has_value())
			{
				return "the member " + member.account + " has a role that is not god or observer";
			}
			member.role = *known;
		}
		if (const auto slot = entry.find("slot"); slot != entry.end() && !slot->is_null())
		{
			if (member.role != game_socket::Role::God)
			{
				return "the observer " + member.account + " has a slot";
			}
			if (!slot->is_number_unsigned() || slot->get<uint64_t>() >= k_MostSlots)
			{
				return "the slot of the member " + member.account + " is not a number from 0 to 31";
			}
			const auto index = static_cast<uint8_t>(slot->get<uint64_t>());
			if (slotsTaken.at(index))
			{
				return "two gods have the slot " + std::to_string(index);
			}
			slotsTaken.at(index) = true;
			member.slot = index;
		}
		anyGod = anyGod || member.role == game_socket::Role::God;
		members.push_back(std::move(member));
	}
	if (!anyGod)
	{
		return std::string("the file of the members names no god");
	}
	return members;
}

} // namespace openblack::lobby_members
