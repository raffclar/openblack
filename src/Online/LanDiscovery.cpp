/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LanDiscovery.h"

#include <charconv>

#include <algorithm>
#include <string_view>
#include <system_error>
#include <utility>

#include <LobbyApiJson.h>
#include <fmt/format.h>

namespace openblack::online::lan_discovery
{
namespace
{

constexpr std::string_view k_SearchText = "openblack-lan-search ";
constexpr std::string_view k_AnswerText = "openblack-lan-room ";
/// The 16 bits of the group of the search, after ff12:: (a temporary group of link-local scope)
constexpr uint16_t k_SearchGroupId = 0x2613;
constexpr size_t k_HexDigits = 16;

/// The text of the bytes. Gives a view, so the bytes must stay.
std::string_view TextOf(std::span<const uint8_t> bytes)
{
	return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

std::vector<uint8_t> BytesOf(std::string_view text)
{
	return {text.begin(), text.end()};
}

/// Reads an unsigned number in this base from the start of the text, and removes it from the text
template <typename T>
std::optional<T> TakeNumber(std::string_view& text, int base, size_t digits)
{
	if (text.size() < digits)
	{
		return std::nullopt;
	}
	T value {};
	const auto [end, error] = std::from_chars(text.data(), text.data() + digits, value, base);
	if (error != std::errc {} || end != text.data() + digits)
	{
		return std::nullopt;
	}
	text.remove_prefix(digits);
	return value;
}

/// Reads the version and a space after it, and gives true for this version only
bool TakeVersion(std::string_view& text)
{
	const auto space = text.find(' ');
	if (space == 0 || space == std::string_view::npos || space > 9)
	{
		return false;
	}
	const auto version = TakeNumber<uint32_t>(text, 10, space);
	if (!version.has_value() || *version != k_Version)
	{
		return false;
	}
	text.remove_prefix(1);
	return true;
}

bool TakeChar(std::string_view& text, char letter)
{
	if (text.empty() || text.front() != letter)
	{
		return false;
	}
	text.remove_prefix(1);
	return true;
}

/// True when the first bits of the two addresses are the same
bool SamePrefix(const Address& a, const Address& b, uint8_t bits) noexcept
{
	const size_t whole = bits / 8;
	if (!std::equal(a.bytes.begin(), a.bytes.begin() + static_cast<std::ptrdiff_t>(whole), b.bytes.begin()))
	{
		return false;
	}
	const auto rest = bits % 8;
	if (rest == 0)
	{
		return true;
	}
	const auto mask = static_cast<uint8_t>(0xFF << (8 - rest));
	return (a.bytes[whole] & mask) == (b.bytes[whole] & mask);
}

/// The size of the address in bits
uint8_t BitsOf(Family family) noexcept
{
	return family == Family::V4 ? 32 : 128;
}

/// Lower is better: IPv4 on the network, IPv4 on this computer, IPv6 on the network, IPv6 link-local, IPv6 on this
/// computer
int RankOf(const Address& address) noexcept
{
	if (address.family == Family::V4)
	{
		return IsLoopback(address) ? 1 : 0;
	}
	if (IsLoopback(address))
	{
		return 4;
	}
	return IsLinkLocal(address) ? 3 : 2;
}

} // namespace

Address V4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept
{
	Address address {.family = Family::V4};
	address.bytes[0] = a;
	address.bytes[1] = b;
	address.bytes[2] = c;
	address.bytes[3] = d;
	return address;
}

Address V6(std::array<uint16_t, 8> groups, uint32_t scope) noexcept
{
	Address address {.family = Family::V6, .scope = scope};
	for (size_t i = 0; i < groups.size(); ++i)
	{
		address.bytes[i * 2] = static_cast<uint8_t>(groups[i] >> 8);
		address.bytes[i * 2 + 1] = static_cast<uint8_t>(groups[i] & 0xFF);
	}
	return address;
}

std::string ToText(const Address& address)
{
	const auto& b = address.bytes;
	if (address.family == Family::V4)
	{
		return fmt::format("{}.{}.{}.{}", b[0], b[1], b[2], b[3]);
	}
	std::array<uint16_t, 8> groups {};
	for (size_t i = 0; i < groups.size(); ++i)
	{
		groups[i] = static_cast<uint16_t>((b[i * 2] << 8) | b[i * 2 + 1]);
	}
	// The longest sequence of two or more zero groups becomes "::"
	size_t bestStart = groups.size();
	size_t bestLength = 1;
	for (size_t i = 0; i < groups.size();)
	{
		if (groups[i] != 0)
		{
			++i;
			continue;
		}
		const size_t start = i;
		while (i < groups.size() && groups[i] == 0)
		{
			++i;
		}
		if (i - start > bestLength)
		{
			bestStart = start;
			bestLength = i - start;
		}
	}
	std::string text;
	for (size_t i = 0; i < groups.size(); ++i)
	{
		if (i == bestStart)
		{
			text += "::";
			i += bestLength - 1;
			continue;
		}
		if (!text.empty() && !text.ends_with(':'))
		{
			text += ':';
		}
		text += fmt::format("{:x}", groups[i]);
	}
	return text;
}

bool IsLoopback(const Address& address) noexcept
{
	if (address.family == Family::V4)
	{
		return address.bytes[0] == 127;
	}
	return std::all_of(address.bytes.begin(), address.bytes.end() - 1, [](uint8_t byte) { return byte == 0; }) &&
	       address.bytes[15] == 1;
}

bool IsLinkLocal(const Address& address) noexcept
{
	return address.family == Family::V6 && address.bytes[0] == 0xFE && (address.bytes[1] & 0xC0) == 0x80;
}

Address SearchGroup(uint32_t interfaceIndex) noexcept
{
	return V6({0xFF12, 0, 0, 0, 0, 0, 0, k_SearchGroupId}, interfaceIndex);
}

bool IsOnLocalNetwork(const Address& sender, std::span<const NetworkInterface> interfaces) noexcept
{
	if (IsLoopback(sender) || IsLinkLocal(sender))
	{
		return true;
	}
	return std::ranges::any_of(interfaces, [&sender](const NetworkInterface& network) {
		return network.address.family == sender.family && network.prefixLength > 0 &&
		       network.prefixLength <= BitsOf(sender.family) && SamePrefix(network.address, sender, network.prefixLength);
	});
}

std::vector<Address> SearchTargets(Family family, std::span<const NetworkInterface> interfaces)
{
	std::vector<Address> targets;
	const auto add = [&targets](const Address& address) {
		if (std::ranges::find(targets, address) == targets.end())
		{
			targets.push_back(address);
		}
	};
	if (family == Family::V4)
	{
		add(V4(127, 0, 0, 1));
		bool network = false;
		for (const auto& item : interfaces)
		{
			// A network of one or two addresses has no broadcast address
			if (item.address.family != Family::V4 || item.loopback || item.prefixLength == 0 || item.prefixLength > 30)
			{
				continue;
			}
			auto broadcast = item.address;
			for (size_t bit = item.prefixLength; bit < 32; ++bit)
			{
				broadcast.bytes[bit / 8] = static_cast<uint8_t>(broadcast.bytes[bit / 8] | (0x80 >> (bit % 8)));
			}
			add(broadcast);
			network = true;
		}
		if (!network)
		{
			add(V4(255, 255, 255, 255));
		}
		return targets;
	}
	add(V6({0, 0, 0, 0, 0, 0, 0, 1}));
	for (const auto& item : interfaces)
	{
		if (item.address.family == Family::V6 && !item.loopback && item.multicast && item.index != 0)
		{
			add(SearchGroup(item.index));
		}
	}
	return targets;
}

int64_t SearchDelay(int index, uint64_t random) noexcept
{
	if (index <= 0)
	{
		const auto range = static_cast<uint64_t>(k_MostStartDelayMicroseconds - k_LeastStartDelayMicroseconds + 1);
		return k_LeastStartDelayMicroseconds + static_cast<int64_t>(random % range);
	}
	auto gap = k_FirstSearchGapMicroseconds;
	for (int i = 1; i < index && gap < k_LongestSearchGapMicroseconds; ++i)
	{
		gap *= 2;
	}
	gap = std::min(gap, k_LongestSearchGapMicroseconds);
	const auto jitter = gap * k_SearchGapJitterPercent / 100;
	return gap - jitter + static_cast<int64_t>(random % static_cast<uint64_t>(jitter * 2 + 1));
}

int64_t AnswerDelay(uint64_t random) noexcept
{
	const auto range = static_cast<uint64_t>(k_MostAnswerDelayMicroseconds - k_LeastAnswerDelayMicroseconds + 1);
	return k_LeastAnswerDelayMicroseconds + static_cast<int64_t>(random % range);
}

std::vector<uint8_t> SearchBytes(uint64_t session)
{
	return BytesOf(fmt::format("{}{} {:016x}\n", k_SearchText, k_Version, session));
}

std::optional<uint64_t> ParseSearch(std::span<const uint8_t> bytes)
{
	auto text = TextOf(bytes);
	if (!text.starts_with(k_SearchText))
	{
		return std::nullopt;
	}
	text.remove_prefix(k_SearchText.size());
	if (!TakeVersion(text))
	{
		return std::nullopt;
	}
	const auto session = TakeNumber<uint64_t>(text, 16, k_HexDigits);
	if (!session.has_value() || text != "\n")
	{
		return std::nullopt;
	}
	return session;
}

std::vector<uint8_t> AnswerBytes(uint64_t session, uint64_t server, const lobby_api::LanAnswer& answer)
{
	return BytesOf(
	    fmt::format("{}{} {:016x} {:016x}\n{}", k_AnswerText, k_Version, session, server, lobby_api::ToJson(answer)));
}

std::optional<HeardAnswer> ParseAnswer(std::span<const uint8_t> bytes)
{
	if (bytes.size() > k_MostDatagramBytes)
	{
		return std::nullopt;
	}
	auto text = TextOf(bytes);
	if (!text.starts_with(k_AnswerText))
	{
		return std::nullopt;
	}
	text.remove_prefix(k_AnswerText.size());
	if (!TakeVersion(text))
	{
		return std::nullopt;
	}
	const auto session = TakeNumber<uint64_t>(text, 16, k_HexDigits);
	if (!session.has_value() || !TakeChar(text, ' '))
	{
		return std::nullopt;
	}
	const auto server = TakeNumber<uint64_t>(text, 16, k_HexDigits);
	if (!server.has_value() || !TakeChar(text, '\n'))
	{
		return std::nullopt;
	}
	auto room = lobby_api::ParseLanAnswer(text);
	if (!room.has_value())
	{
		return std::nullopt;
	}
	return HeardAnswer {.session = *session, .server = *server, .room = std::move(*room)};
}

lobby_api::ServerEntry EntryOf(const lobby_api::LanAnswer& answer, uint64_t server, const Address& address)
{
	std::string host = answer.host;
	if (host.empty())
	{
		if (address.family == Family::V4)
		{
			host = ToText(address);
		}
		else if (IsLinkLocal(address) && address.scope != 0)
		{
			// The scope of a link-local address goes after "%25" in an address of the web
			host = fmt::format("[{}%25{}]", ToText(address), address.scope);
		}
		else
		{
			host = fmt::format("[{}]", ToText(address));
		}
	}
	return lobby_api::ServerEntry {
	    .id = fmt::format("lan-{:016x}", server),
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

void ServerList::Heard(const HeardAnswer& answer, const Address& from, int64_t now)
{
	const auto rank = RankOf(from);
	const auto found = std::ranges::find(_rooms, answer.server, &Room::server);
	if (found == _rooms.end())
	{
		if (_rooms.size() >= k_MostRooms)
		{
			return;
		}
		_servers.push_back(EntryOf(answer.room, answer.server, from));
		_rooms.push_back({.server = answer.server, .rank = rank, .heard = now, .missed = 0});
		return;
	}
	const auto index = static_cast<size_t>(found - _rooms.begin());
	auto& entry = _servers[index];
	if (rank <= found->rank)
	{
		entry = EntryOf(answer.room, answer.server, from);
		found->rank = rank;
	}
	else
	{
		// Keep the better address, and take the room as it is now
		const auto url = std::move(entry.url);
		entry = EntryOf(answer.room, answer.server, from);
		entry.url = url;
	}
	found->heard = now;
	found->missed = 0;
}

void ServerList::Searched(int64_t now)
{
	if (_lastSearch.has_value())
	{
		for (size_t i = _rooms.size(); i-- > 0;)
		{
			auto& room = _rooms[i];
			room.missed = room.heard >= *_lastSearch ? 0 : room.missed + 1;
			if (room.missed >= k_MostMissedAnswers)
			{
				_rooms.erase(_rooms.begin() + static_cast<std::ptrdiff_t>(i));
				_servers.erase(_servers.begin() + static_cast<std::ptrdiff_t>(i));
			}
		}
	}
	_lastSearch = now;
}

void ServerList::Clear() noexcept
{
	_servers.clear();
	_rooms.clear();
	_lastSearch.reset();
}

} // namespace openblack::online::lan_discovery
