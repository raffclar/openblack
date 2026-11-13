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

} // namespace openblack::server::lobby_game
