/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "LobbyApi.h"

/// The JSON text of the shapes of the lobby's API (LobbyApi.h). Nothing here throws. A parse gives none when the text is
/// not JSON, when a member that the shape needs is missing, or when a member has the wrong type. A parse ignores the
/// members that it does not know, so that the lobby can add members to a version of the API.
namespace openblack::lobby_api
{

[[nodiscard]] std::string ToJson(const LoginRequest& request);
[[nodiscard]] std::string ToJson(const RefreshRequest& request);
[[nodiscard]] std::string ToJson(const ClanCreature& creature);
[[nodiscard]] std::string ToJson(const ClanCreatureReply& reply);
[[nodiscard]] std::string ToJson(const ChatLineRequest& request);
[[nodiscard]] std::string ToJson(const UseClanRequest& request);
[[nodiscard]] std::string ToJson(const ServerStatus& status);
/// The frame that asks for the events of a channel, or stops them
[[nodiscard]] std::string SubscribeFrame(bool subscribe, std::string_view channel);
/// The frame that a client sends after it opens the channel again: the epoch and the number of the last event that it
/// got, and the channels that it wants
[[nodiscard]] std::string ResumeFrame(std::string_view epoch, int64_t seq, std::span<const std::string> channels);

/// The first text frame on the game socket of a server: {"type": "room.join", ...}. On the internet it holds the
/// lobby's ticket and the game's password, when the game needs one. On the local network there is no ticket, and it
/// holds the name of the player's profile. The game server owns the protocol of its room; these frames follow its plan.
[[nodiscard]] std::string RoomJoinFrame(std::string_view ticket, std::string_view gamePassword, std::string_view lanName);
/// The text frame of the Ready box: {"type": "room.ready", "ready": true}
[[nodiscard]] std::string RoomReadyFrame(bool ready);

[[nodiscard]] std::optional<Session> ParseSession(std::string_view text);
[[nodiscard]] std::optional<Me> ParseMe(std::string_view text);
[[nodiscard]] std::optional<ServerList> ParseServerList(std::string_view text);
[[nodiscard]] std::optional<JoinTicket> ParseJoinTicket(std::string_view text);
[[nodiscard]] std::optional<ClanList> ParseClanList(std::string_view text);
[[nodiscard]] std::optional<ClanDetails> ParseClanDetails(std::string_view text);
[[nodiscard]] std::optional<RankingPage> ParseRankingPage(std::string_view text);
[[nodiscard]] std::optional<PlayerDetails> ParsePlayerDetails(std::string_view text);
[[nodiscard]] std::optional<RealWeather> ParseRealWeather(std::string_view text);
[[nodiscard]] std::optional<CreatureUploadReply> ParseCreatureUploadReply(std::string_view text);
[[nodiscard]] std::optional<ClanCreature> ParseClanCreature(std::string_view text);
[[nodiscard]] std::optional<ClanCreatureReply> ParseClanCreatureReply(std::string_view text);
[[nodiscard]] std::optional<Problem> ParseProblem(std::string_view text);
[[nodiscard]] std::optional<PushEvent> ParsePushEvent(std::string_view text);
[[nodiscard]] std::optional<AccountNameCheck> ParseAccountNameCheck(std::string_view text);
[[nodiscard]] std::optional<UsedClan> ParseUsedClan(std::string_view text);
/// The data of "chat.line"
[[nodiscard]] std::optional<ChatLine> ParseChatLine(std::string_view data);
/// The data of "room.changed"
[[nodiscard]] std::optional<RoomChange> ParseRoomChange(std::string_view data);
/// The data of "room.clan_chosen"
[[nodiscard]] std::optional<RoomClanChoice> ParseRoomClanChoice(std::string_view data);
/// The data of "servers.changed" or of "servers.removed"
[[nodiscard]] std::optional<ServerChange> ParseServerChange(std::string_view type, std::string_view data);
[[nodiscard]] std::optional<PushPosition> ParsePushPosition(std::string_view data);
[[nodiscard]] std::optional<PushError> ParsePushError(std::string_view data);
[[nodiscard]] std::optional<PointsChange> ParsePointsChange(std::string_view text);
[[nodiscard]] std::optional<Ban> ParseBan(std::string_view text);

/// An ISO 8601 time of the lobby, such as "2026-10-22T10:15:00.123Z" or "2026-10-22T10:15:00+00:00", as milliseconds
/// since 1970 (UTC). None when the text is not such a time.
[[nodiscard]] std::optional<int64_t> ParseTime(std::string_view text);

/// The date as the query value of the weather call: YYYY-MM-DD
[[nodiscard]] std::string DateText(const Date& date);

/// Base64 with the standard alphabet and padding (RFC 4648)
[[nodiscard]] std::string Base64Encode(std::span<const uint8_t> bytes);
/// None when the text is not base64
[[nodiscard]] std::optional<std::vector<uint8_t>> Base64Decode(std::string_view text);

/// A part of a multipart/form-data body
struct FormPart
{
	/// The name of the field
	std::string name;
	/// The name of the file
	std::string fileName;
	std::string contentType;
	std::vector<uint8_t> bytes;
};

/// The content type of a multipart/form-data body with a boundary
[[nodiscard]] std::string MultipartContentType(std::string_view boundary);
/// A multipart/form-data body (RFC 7578). The boundary must not be in the bytes of a part.
[[nodiscard]] std::vector<uint8_t> MultipartBody(std::string_view boundary, std::span<const FormPart> parts);
/// A boundary that is not in the bytes of any part. The seed makes the boundary different between bodies.
[[nodiscard]] std::string MultipartBoundary(std::span<const FormPart> parts, uint64_t seed);

} // namespace openblack::lobby_api
