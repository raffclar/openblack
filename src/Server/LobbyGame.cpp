/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyGame.h"

#include <algorithm>

#include <fmt/format.h>

#include "PlayerSlots.h"

namespace openblack::server::lobby_game
{

size_t Lobby::Gods() const noexcept
{
	return static_cast<size_t>(std::ranges::count(members, game_socket::Role::God, &lobby_members::Member::role));
}

const lobby_members::Member* Lobby::MemberOf(std::string_view account) const noexcept
{
	const auto found = std::ranges::find(members, account, &lobby_members::Member::account);
	return found != members.end() ? &*found : nullptr;
}

Admission Admit(const Lobby& lobby, std::string_view ticketLobby, std::string_view account, game_socket::Role role) noexcept
{
	if (ticketLobby != lobby.id)
	{
		return Admission::OtherLobby;
	}
	const auto* member = lobby.MemberOf(account);
	if (member == nullptr)
	{
		return Admission::NotMember;
	}
	return member->role == role ? Admission::Member : Admission::OtherRole;
}

std::string_view AdmissionName(Admission admission) noexcept
{
	switch (admission)
	{
	case Admission::Member:
		return "a member of the lobby";
	case Admission::OtherLobby:
		return "the ticket is for another lobby";
	case Admission::NotMember:
		return "the account is not a member of the lobby";
	case Admission::OtherRole:
		return "the ticket gives another role than the lobby";
	}
	return "unknown";
}

std::optional<std::string_view> BuildRefusal(std::string_view serverBuild, std::string_view clientBuild,
                                             bool buildNeeded) noexcept
{
	if (serverBuild.empty())
	{
		return std::nullopt;
	}
	if (clientBuild.empty())
	{
		return buildNeeded ? std::optional(k_VersionRefusal) : std::nullopt;
	}
	return clientBuild != serverBuild ? std::optional(k_VersionRefusal) : std::nullopt;
}

std::optional<std::string_view> MapRefusal(std::string_view serverMap, std::string_view clientMap) noexcept
{
	if (clientMap.empty() || serverMap.empty())
	{
		return std::nullopt;
	}
	return clientMap != serverMap ? std::optional(k_MapRefusal) : std::nullopt;
}

std::variant<std::vector<lobby_api::SlotKind>, std::string> ParseSlotKinds(std::string_view text)
{
	std::vector<lobby_api::SlotKind> kinds;
	if (text.empty())
	{
		return kinds;
	}
	size_t from = 0;
	while (true)
	{
		const auto comma = text.find(',', from);
		const auto word = text.substr(from, comma == std::string_view::npos ? std::string_view::npos : comma - from);
		const auto kind = lobby_api::SlotKindOf(word);
		if (!kind.has_value())
		{
			return fmt::format("the slot kind \"{}\" is not open, computer or closed", word);
		}
		if (kinds.size() == player_slots::k_MaxPlayers)
		{
			return fmt::format("a room has {} god slots at most", player_slots::k_MaxPlayers);
		}
		kinds.push_back(*kind);
		if (comma == std::string_view::npos)
		{
			return kinds;
		}
		from = comma + 1;
	}
}

std::vector<uint8_t> ClosedSlotsOf(std::span<const lobby_api::SlotKind> kinds)
{
	std::vector<uint8_t> closed;
	for (size_t slot = 0; slot < kinds.size(); ++slot)
	{
		if (kinds[slot] == lobby_api::SlotKind::Closed)
		{
			closed.push_back(static_cast<uint8_t>(slot));
		}
	}
	return closed;
}

const lobby_members::Member* MemberInShutSlot(const Lobby& lobby, std::span<const lobby_api::SlotKind> kinds) noexcept
{
	const auto found = std::ranges::find_if(lobby.members, [kinds](const lobby_members::Member& member) {
		return member.role == game_socket::Role::God && member.slot.has_value() && *member.slot < kinds.size() &&
		       kinds[*member.slot] != lobby_api::SlotKind::Open;
	});
	return found != lobby.members.end() ? &*found : nullptr;
}

} // namespace openblack::server::lobby_game
