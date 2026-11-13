/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Lobbies.h"

/// The JSON text of the shapes of the lobby rooms (Lobbies.h). The rules are the same as in LobbyApiJson.h: nothing
/// throws, a parse gives none for text that does not have the shape, and a parse ignores the members that it does not
/// know.
namespace openblack::lobby_api
{

[[nodiscard]] std::string ToJson(const LobbyCreateRequest& request);
[[nodiscard]] std::string ToJson(const LobbyChangeRequest& request);
[[nodiscard]] std::string ToJson(const LobbyJoinRequest& request);
[[nodiscard]] std::string ToJson(const MemberChangeRequest& request);
[[nodiscard]] std::string ToJson(const QuickJoinRequest& request);

[[nodiscard]] std::optional<LobbyList> ParseLobbyList(std::string_view text);
[[nodiscard]] std::optional<Lobby> ParseLobby(std::string_view text);
[[nodiscard]] std::optional<LobbyMember> ParseLobbyMember(std::string_view text);
[[nodiscard]] std::optional<StartReply> ParseStartReply(std::string_view text);
[[nodiscard]] std::optional<LobbyCodeReply> ParseLobbyCodeReply(std::string_view text);
[[nodiscard]] std::optional<LobbyTicket> ParseLobbyTicket(std::string_view text);

/// The data of "lobbies.changed" or of "lobbies.removed"
[[nodiscard]] std::optional<LobbyListChange> ParseLobbyListChange(std::string_view type, std::string_view data);
/// The data of "pool.changed"
[[nodiscard]] std::optional<PoolState> ParsePoolState(std::string_view data);
/// The data of "member.*" and of "host.changed"
[[nodiscard]] std::optional<MemberEvent> ParseMemberEvent(std::string_view data);
/// The data of "lobby.changed"
[[nodiscard]] std::optional<LobbyChange> ParseLobbyChange(std::string_view data);
/// The data of "start.*", "game.*" and "lobby.closed"
[[nodiscard]] std::optional<LobbyStateEvent> ParseLobbyStateEvent(std::string_view data);
/// The data of "lobby.kicked"
[[nodiscard]] std::optional<LobbyKick> ParseLobbyKick(std::string_view data);

/// The code of a private lobby as the player types it: the dash and the spaces go, and the letters become capitals.
/// None when the code does not have 8 characters of Crockford base32.
[[nodiscard]] std::optional<std::string> NormalLobbyCode(std::string_view text);
/// The code as the room shows it: XXXX-XXXX
[[nodiscard]] std::string LobbyCodeText(std::string_view code);

} // namespace openblack::lobby_api
