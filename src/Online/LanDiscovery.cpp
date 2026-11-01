/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanDiscovery.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <LobbyApiJson.h>
#include <fmt/format.h>

namespace openblack::online::lan_discovery
{
namespace
{

constexpr std::string_view k_SearchText = "openblack-lan-search 1\n";
constexpr std::string_view k_AnswerText = "openblack-lan-room 1\n";

/// True when the bytes start with the text
bool StartsWith(std::span<const uint8_t> bytes, std::string_view text)
{
	return bytes.size() >= text.size() && std::ranges::equal(bytes.first(text.size()), text, [](uint8_t byte, char letter) {
		       return byte == static_cast<uint8_t>(letter);
	       });
}

} // namespace

std::span<const uint8_t> Search() noexcept
{
	static constexpr auto k_Bytes = [] {
		std::array<uint8_t, k_SearchText.size()> bytes {};
		std::ranges::transform(k_SearchText, bytes.begin(), [](char letter) { return static_cast<uint8_t>(letter); });
		return bytes;
	}();
	return k_Bytes;
}

bool IsSearch(std::span<const uint8_t> bytes) noexcept
{
	return bytes.size() == k_SearchText.size() && StartsWith(bytes, k_SearchText);
}

std::vector<uint8_t> AnswerBytes(const lobby_api::LanAnswer& answer)
{
	const auto text = std::string(k_AnswerText) + lobby_api::ToJson(answer);
	return {text.begin(), text.end()};
}

std::optional<lobby_api::LanAnswer> ParseAnswer(std::span<const uint8_t> bytes)
{
	if (bytes.size() > k_MostDatagramBytes || !StartsWith(bytes, k_AnswerText))
	{
		return std::nullopt;
	}
	const auto rest = bytes.subspan(k_AnswerText.size());
	const std::string text(rest.begin(), rest.end());
	return lobby_api::ParseLanAnswer(text);
}

lobby_api::ServerEntry EntryOf(const lobby_api::LanAnswer& answer, std::string_view address)
{
	const std::string host = answer.host.empty() ? std::string(address) : answer.host;
	return lobby_api::ServerEntry {
	    .id = fmt::format("lan-{}:{}", address, answer.port),
	    .hostname = answer.hostname,
	    .land = answer.land,
	    .gamemode = answer.gamemode,
	    .locked = false,
	    .password = answer.password,
	    .numplayers = answer.numplayers,
	    .maxplayers = answer.maxplayers,
	    .gamever = answer.gamever,
	    .infoversion = 0,
	    .creatureversion = 0,
	    .oosversion = 0,
	    .players = answer.players,
	    .url = fmt::format("wss://{}:{}/game", host, answer.port),
	    .updated = 0,
	    .map = {},
	};
}

void ServerList::Heard(lobby_api::ServerEntry entry, int64_t now)
{
	const auto found = std::ranges::find(_servers, entry.id, &lobby_api::ServerEntry::id);
	if (found == _servers.end())
	{
		_servers.push_back(std::move(entry));
		_heard.push_back(now);
		return;
	}
	const auto index = static_cast<size_t>(found - _servers.begin());
	*found = std::move(entry);
	_heard[index] = now;
}

void ServerList::Forget(int64_t now)
{
	for (size_t i = _servers.size(); i-- > 0;)
	{
		if (now - _heard[i] > k_ForgetAfterMicroseconds)
		{
			_servers.erase(_servers.begin() + static_cast<std::ptrdiff_t>(i));
			_heard.erase(_heard.begin() + static_cast<std::ptrdiff_t>(i));
		}
	}
}

void ServerList::Clear() noexcept
{
	_servers.clear();
	_heard.clear();
}

} // namespace openblack::online::lan_discovery
