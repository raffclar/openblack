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
/// The frame of an operation on the push channel. The argument is the channel for Subscribe and Unsubscribe, and the
/// last number of an event for Resume.
[[nodiscard]] std::string PushFrame(PushOp op, std::string_view channel = {}, int64_t lastSeq = 0);

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
[[nodiscard]] std::optional<ServerChange> ParseServerChange(std::string_view text);
[[nodiscard]] std::optional<PointsChange> ParsePointsChange(std::string_view text);
[[nodiscard]] std::optional<Ban> ParseBan(std::string_view text);

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
