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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <LobbyApi.h>

/// The search for game servers on the local network, with no lobby (deviations 113 and 194). A client sends a search on
/// IPv4 broadcast and on IPv6 link-local multicast. Each game server that gets the search sends back its room after a
/// short random time. The client lists each room in the shape of the lobby's list.
namespace openblack::online::lan_discovery
{

/// The UDP port of the search, the port of the game's own lobby on the local network
constexpr uint16_t k_Port = 2613;
/// The version of the datagrams. A game with a different version ignores the datagrams of this version.
constexpr uint32_t k_Version = 2;
/// The most bytes of one search or one answer
constexpr size_t k_MostDatagramBytes = 8192;

/// The first search goes after a random time in this range, so that many games that start together do not search
/// together
constexpr int64_t k_LeastStartDelayMicroseconds = 20'000;
constexpr int64_t k_MostStartDelayMicroseconds = 120'000;
/// The time between the first two searches. The time doubles after each search, until it gets to the longest time.
constexpr int64_t k_FirstSearchGapMicroseconds = 500'000;
constexpr int64_t k_LongestSearchGapMicroseconds = 2'000'000;
/// Each time between two searches changes by a random value of up to this percentage, up or down
constexpr int64_t k_SearchGapJitterPercent = 10;
/// The client removes a room from the list when the room did not answer this number of searches in a sequence
constexpr int k_MostMissedAnswers = 3;
/// The most rooms in the list
constexpr size_t k_MostRooms = 64;
/// The search and the server read the network interfaces of the computer again after this time
constexpr int64_t k_InterfaceRefreshMicroseconds = 30'000'000;
/// The most datagrams that one socket reads in one frame, so that a flood of datagrams does not stop the frame
constexpr size_t k_MostDatagramsInAFrame = 256;

/// A server sends its answer after a random time in this range, so that many servers do not answer together
constexpr int64_t k_LeastAnswerDelayMicroseconds = 20'000;
constexpr int64_t k_MostAnswerDelayMicroseconds = 120'000;
/// A server sends one answer at most in this time to one sender
constexpr int64_t k_LeastAnswerGapMicroseconds = 250'000;
/// The most answers that wait for their time in a server
constexpr size_t k_MostWaitingAnswers = 64;

/// The family of an IP address
enum class Family : uint8_t
{
	V4,
	V6,
};

/// An IPv4 or IPv6 address. An IPv4 address uses the first four bytes. The scope is the index of the network
/// interface of an IPv6 link-local address, else 0.
struct Address
{
	Family family {Family::V4};
	std::array<uint8_t, 16> bytes {};
	uint32_t scope {0};

	[[nodiscard]] bool operator==(const Address&) const = default;
};

[[nodiscard]] Address V4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept;
/// An IPv6 address from its eight groups of 16 bits
[[nodiscard]] Address V6(std::array<uint16_t, 8> groups, uint32_t scope = 0) noexcept;
/// The address as text: "192.168.1.2", or "fe80::1" in the short form of IPv6, with no scope
[[nodiscard]] std::string ToText(const Address& address);
[[nodiscard]] bool IsLoopback(const Address& address) noexcept;
/// True for an IPv6 address in fe80::/10. A router does not send such an address to another network.
[[nodiscard]] bool IsLinkLocal(const Address& address) noexcept;

/// The IPv6 link-local multicast group of the search, on one network interface
[[nodiscard]] Address SearchGroup(uint32_t interfaceIndex) noexcept;

/// An address of a network interface of this computer
struct NetworkInterface
{
	Address address;
	/// The number of bits of the network part of the address
	uint8_t prefixLength {0};
	/// The index of the interface in the system
	uint32_t index {0};
	bool loopback {false};
	/// The interface can send multicast datagrams
	bool multicast {false};
};

/// True when the sender is on the local network: this computer, an IPv6 link-local address, or an address in the
/// network of an interface
[[nodiscard]] bool IsOnLocalNetwork(const Address& sender, std::span<const NetworkInterface> interfaces) noexcept;

/// The addresses that get each search, for the interfaces of this computer:
/// - On IPv4, the loopback address and the broadcast address of each network. With no network, the broadcast address
///   of all networks.
/// - On IPv6, the loopback address and the search group on each interface that can send multicast datagrams.
[[nodiscard]] std::vector<Address> SearchTargets(Family family, std::span<const NetworkInterface> interfaces);

/// The time before the search of this number, 0 for the first search. The random value gives the random part.
[[nodiscard]] int64_t SearchDelay(int index, uint64_t random) noexcept;
/// The time before a server sends an answer. The random value gives the time.
[[nodiscard]] int64_t AnswerDelay(uint64_t random) noexcept;

/// The bytes of a search. The session marks the searches of one client, and the answers to them.
[[nodiscard]] std::vector<uint8_t> SearchBytes(uint64_t session);
/// The session of a search, none when the bytes are not a search of this version
[[nodiscard]] std::optional<uint64_t> ParseSearch(std::span<const uint8_t> bytes);

/// An answer of a game server, with the session of the search and the number that marks the server
struct HeardAnswer
{
	uint64_t session {0};
	uint64_t server {0};
	lobby_api::LanAnswer room;
};

/// The bytes of the answer of a game server
[[nodiscard]] std::vector<uint8_t> AnswerBytes(uint64_t session, uint64_t server, const lobby_api::LanAnswer& answer);
/// The answer in the bytes, none when the bytes are not an answer of this version
[[nodiscard]] std::optional<HeardAnswer> ParseAnswer(std::span<const uint8_t> bytes);

/// The line of the list for an answer from an address. The game socket is on the TCP port of the answer, at the
/// computer name of the answer, else at the address.
[[nodiscard]] lobby_api::ServerEntry EntryOf(const lobby_api::LanAnswer& answer, uint64_t server, const Address& address);

/// The rooms that answered the search, in the order of their first answer. A server that answers on more than one
/// address has one line, with its best address: IPv4 before IPv6, and the local network before this computer.
class ServerList
{
public:
	/// An answer came at this time: the line of the room starts or changes
	void Heard(const HeardAnswer& answer, const Address& from, int64_t now);
	/// A search goes at this time. A room that did not answer some searches in a sequence goes out of the list.
	void Searched(int64_t now);
	void Clear() noexcept;
	[[nodiscard]] std::span<const lobby_api::ServerEntry> Servers() const noexcept { return _servers; }

private:
	struct Room
	{
		uint64_t server {0};
		int rank {0};
		int64_t heard {0};
		int missed {0};
	};

	std::vector<lobby_api::ServerEntry> _servers;
	std::vector<Room> _rooms;
	std::optional<int64_t> _lastSearch;
};

} // namespace openblack::online::lan_discovery
