/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>
#include <LobbyMembers.h>

/// The rules of the join of a game server of the pool, which plays the game of one lobby (deviation 236). The server
/// takes only the members of the lobby, each in the role and the god slot that the lobby gave, and only a client of
/// its own build (deviation 248). Pure, tested with fakes.
namespace openblack::server::lobby_game
{

/// The lobby of the game: its id, which each join ticket names, and its members
struct Lobby
{
	std::string id;
	std::vector<lobby_members::Member> members;

	/// The members that play a god
	[[nodiscard]] size_t Gods() const noexcept;
	/// The member of the account, or none
	[[nodiscard]] const lobby_members::Member* MemberOf(std::string_view account) const noexcept;

	[[nodiscard]] bool operator==(const Lobby&) const = default;
};

/// What the server finds about the ticket of a join
enum class Admission : uint8_t
{
	/// A member of the lobby in the role of the ticket
	Member,
	/// The ticket names another lobby, or no lobby
	OtherLobby,
	/// The account of the ticket is not a member of the lobby
	NotMember,
	/// The ticket gives another role than the lobby gave the member
	OtherRole,
};

/// Checks the lobby, the account and the role of a ticket against the lobby of the game
[[nodiscard]] Admission Admit(const Lobby& lobby, std::string_view ticketLobby, std::string_view account,
                              game_socket::Role role) noexcept;

/// The words of the log for each admission
[[nodiscard]] std::string_view AdmissionName(Admission admission) noexcept;

/// The word of the refusal of a client of another build
inline constexpr std::string_view k_VersionRefusal = "version";
/// The word of the refusal of a client with another file of the land
inline constexpr std::string_view k_MapRefusal = "map";
/// The word of the refusal that each client gets when the server ends its room before the start
inline constexpr std::string_view k_EndedRefusal = "ended";

/// Why the server refuses the build of a client, or none. A client that gives no build is refused only by a server that
/// needs one: the server of a lobby game. Each other server takes it, as an older client gives no build. A server with
/// no build id of its own (a test) takes every client.
[[nodiscard]] std::optional<std::string_view> BuildRefusal(std::string_view serverBuild, std::string_view clientBuild,
                                                           bool buildNeeded) noexcept;

/// Why the server refuses the SHA-256 of the client's file of the land, or none. A client that gives no SHA-256 is not
/// refused here: the check of the land before the start still compares the files.
[[nodiscard]] std::optional<std::string_view> MapRefusal(std::string_view serverMap, std::string_view clientMap) noexcept;

} // namespace openblack::server::lobby_game
