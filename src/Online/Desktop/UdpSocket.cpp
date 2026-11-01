/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UdpSocket.h"

#include <cstddef>
#include <cstring>

#include <utility>

#if defined(_WIN32)
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mstcpip.h>
// clang-format on
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace openblack::online
{
namespace
{

using lan_discovery::Address;
using lan_discovery::Family;

/// The most bytes of a datagram that the socket reads
constexpr size_t k_MostBytes = 65536;
/// The datagrams of the search cross no router
constexpr int k_Hops = 1;

#if defined(_WIN32)
using Socket = SOCKET;
using Length = int;
void CloseSocket(Socket socket)
{
	closesocket(socket);
}
bool MakeNonBlocking(Socket socket)
{
	u_long on = 1;
	return ioctlsocket(socket, FIONBIO, &on) == 0;
}
bool IsValid(Socket socket)
{
	return socket != INVALID_SOCKET;
}
/// Windows stops the next read of a UDP socket when a datagram went to a port with no socket. This turns that off.
void IgnorePortErrors(Socket socket)
{
	BOOL off = FALSE;
	DWORD returned = 0;
	WSAIoctl(socket, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &returned, nullptr, nullptr);
}
#else
using Socket = int;
using Length = socklen_t;
void CloseSocket(Socket socket)
{
	close(socket);
}
bool MakeNonBlocking(Socket socket)
{
	const auto flags = fcntl(socket, F_GETFL, 0);
	return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
bool IsValid(Socket socket)
{
	return socket >= 0;
}
void IgnorePortErrors(Socket /*socket*/) {}

/// The number of bits that are on at the start of the mask
uint8_t PrefixOf(std::span<const uint8_t> mask)
{
	uint8_t bits = 0;
	for (const auto byte : mask)
	{
		for (int bit = 7; bit >= 0; --bit)
		{
			if ((byte & (1 << bit)) == 0)
			{
				return bits;
			}
			++bits;
		}
	}
	return bits;
}
#endif

Socket ToSocket(intptr_t value)
{
	return static_cast<Socket>(value);
}

void SetInteger(Socket socket, int level, int name, int value)
{
	setsockopt(socket, level, name, reinterpret_cast<const char*>(&value), sizeof(value));
}

Address FromIpv4(const in_addr& address)
{
	Address result {.family = Family::V4};
	std::memcpy(result.bytes.data(), &address, 4);
	return result;
}

Address FromIpv6(const in6_addr& address, uint32_t scope)
{
	Address result {.family = Family::V6, .scope = scope};
	std::memcpy(result.bytes.data(), &address, 16);
	return result;
}

} // namespace

UdpSocket::UdpSocket()
{
#if defined(_WIN32)
	WSADATA data {};
	WSAStartup(MAKEWORD(2, 2), &data);
#endif
}

UdpSocket::~UdpSocket()
{
	if (_socket != -1)
	{
		CloseSocket(ToSocket(_socket));
	}
#if defined(_WIN32)
	WSACleanup();
#endif
}

bool UdpSocket::Open(const Options& options)
{
	if (_socket != -1)
	{
		return true;
	}
	const bool v6 = options.family == Family::V6;
	const auto socket = ::socket(v6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!IsValid(socket))
	{
		return false;
	}
	IgnorePortErrors(socket);
	bool bound = false;
	if (v6)
	{
		SetInteger(socket, IPPROTO_IPV6, IPV6_V6ONLY, 1);
		SetInteger(socket, IPPROTO_IPV6, IPV6_UNICAST_HOPS, k_Hops);
		SetInteger(socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, k_Hops);
		SetInteger(socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, 1);
		sockaddr_in6 address {};
		address.sin6_family = AF_INET6;
		address.sin6_addr = in6addr_any;
		address.sin6_port = htons(options.port);
		bound = bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
		const auto group = lan_discovery::SearchGroup(0);
		for (const auto index : options.groups)
		{
			ipv6_mreq join {};
			std::memcpy(&join.ipv6mr_multiaddr, group.bytes.data(), 16);
			join.ipv6mr_interface = index;
			setsockopt(socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, reinterpret_cast<const char*>(&join), sizeof(join));
		}
	}
	else
	{
		SetInteger(socket, IPPROTO_IP, IP_TTL, k_Hops);
		if (options.broadcast)
		{
			SetInteger(socket, SOL_SOCKET, SO_BROADCAST, 1);
		}
		sockaddr_in address {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		address.sin_port = htons(options.port);
		bound = bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
	}
	if (!bound || !MakeNonBlocking(socket))
	{
		CloseSocket(socket);
		return false;
	}
	_family = options.family;
	_socket = static_cast<intptr_t>(socket);
	return true;
}

bool UdpSocket::IsOpen() const
{
	return _socket != -1;
}

bool UdpSocket::SendTo(const Address& to, uint16_t port, std::span<const uint8_t> bytes)
{
	if (_socket == -1 || to.family != _family)
	{
		return false;
	}
	sockaddr_storage storage {};
	Length length = 0;
	if (to.family == Family::V6)
	{
		auto* address = reinterpret_cast<sockaddr_in6*>(&storage);
		address->sin6_family = AF_INET6;
		address->sin6_port = htons(port);
		address->sin6_scope_id = to.scope;
		std::memcpy(&address->sin6_addr, to.bytes.data(), 16);
		length = sizeof(sockaddr_in6);
	}
	else
	{
		auto* address = reinterpret_cast<sockaddr_in*>(&storage);
		address->sin_family = AF_INET;
		address->sin_port = htons(port);
		std::memcpy(&address->sin_addr, to.bytes.data(), 4);
		length = sizeof(sockaddr_in);
	}
	const auto sent = sendto(ToSocket(_socket), reinterpret_cast<const char*>(bytes.data()), static_cast<Length>(bytes.size()),
	                         0, reinterpret_cast<const sockaddr*>(&storage), length);
	return sent == static_cast<decltype(sent)>(bytes.size());
}

std::optional<Datagram> UdpSocket::Receive()
{
	if (_socket == -1)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> buffer(k_MostBytes);
	sockaddr_storage from {};
	Length fromLength = sizeof(from);
	const auto received = recvfrom(ToSocket(_socket), reinterpret_cast<char*>(buffer.data()),
	                               static_cast<Length>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
	if (received < 0)
	{
		return std::nullopt;
	}
	buffer.resize(static_cast<size_t>(received));
	if (from.ss_family == AF_INET6)
	{
		const auto* address = reinterpret_cast<const sockaddr_in6*>(&from);
		return Datagram {.bytes = std::move(buffer),
		                 .from = FromIpv6(address->sin6_addr, address->sin6_scope_id),
		                 .port = ntohs(address->sin6_port)};
	}
	if (from.ss_family == AF_INET)
	{
		const auto* address = reinterpret_cast<const sockaddr_in*>(&from);
		return Datagram {.bytes = std::move(buffer), .from = FromIpv4(address->sin_addr), .port = ntohs(address->sin_port)};
	}
	return std::nullopt;
}

std::vector<lan_discovery::NetworkInterface> LocalInterfaces()
{
	std::vector<lan_discovery::NetworkInterface> interfaces;
#if defined(_WIN32)
	ULONG size = 16384;
	std::vector<uint8_t> buffer;
	ULONG result = ERROR_BUFFER_OVERFLOW;
	for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt)
	{
		buffer.resize(size);
		result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
		                              nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
	}
	if (result != NO_ERROR)
	{
		return interfaces;
	}
	for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter != nullptr; adapter = adapter->Next)
	{
		if (adapter->OperStatus != IfOperStatusUp)
		{
			continue;
		}
		const bool loopback = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
		const bool multicast = (adapter->Flags & IP_ADAPTER_NO_MULTICAST) == 0;
		for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
		{
			const auto* address = unicast->Address.lpSockaddr;
			if (address->sa_family == AF_INET)
			{
				interfaces.push_back({.address = FromIpv4(reinterpret_cast<const sockaddr_in*>(address)->sin_addr),
				                      .prefixLength = unicast->OnLinkPrefixLength,
				                      .index = adapter->IfIndex,
				                      .loopback = loopback,
				                      .multicast = multicast});
			}
			else if (address->sa_family == AF_INET6)
			{
				const auto* v6 = reinterpret_cast<const sockaddr_in6*>(address);
				interfaces.push_back({.address = FromIpv6(v6->sin6_addr, v6->sin6_scope_id),
				                      .prefixLength = unicast->OnLinkPrefixLength,
				                      .index = adapter->Ipv6IfIndex,
				                      .loopback = loopback,
				                      .multicast = multicast});
			}
		}
	}
#else
	ifaddrs* list = nullptr;
	if (getifaddrs(&list) != 0)
	{
		return interfaces;
	}
	for (const auto* item = list; item != nullptr; item = item->ifa_next)
	{
		if (item->ifa_addr == nullptr || item->ifa_netmask == nullptr || (item->ifa_flags & IFF_UP) == 0)
		{
			continue;
		}
		const bool loopback = (item->ifa_flags & IFF_LOOPBACK) != 0;
		const bool multicast = (item->ifa_flags & IFF_MULTICAST) != 0;
		const auto index = if_nametoindex(item->ifa_name);
		if (item->ifa_addr->sa_family == AF_INET)
		{
			const auto* mask = reinterpret_cast<const sockaddr_in*>(item->ifa_netmask);
			interfaces.push_back({.address = FromIpv4(reinterpret_cast<const sockaddr_in*>(item->ifa_addr)->sin_addr),
			                      .prefixLength = PrefixOf({reinterpret_cast<const uint8_t*>(&mask->sin_addr), 4}),
			                      .index = index,
			                      .loopback = loopback,
			                      .multicast = multicast});
		}
		else if (item->ifa_addr->sa_family == AF_INET6)
		{
			const auto* address = reinterpret_cast<const sockaddr_in6*>(item->ifa_addr);
			const auto* mask = reinterpret_cast<const sockaddr_in6*>(item->ifa_netmask);
			auto ipv6 = FromIpv6(address->sin6_addr, index);
			if (lan_discovery::IsLinkLocal(ipv6))
			{
				// Some systems keep the interface index in the third and fourth bytes of a link-local address
				ipv6.bytes[2] = 0;
				ipv6.bytes[3] = 0;
			}
			else
			{
				ipv6.scope = 0;
			}
			interfaces.push_back({.address = ipv6,
			                      .prefixLength = PrefixOf({reinterpret_cast<const uint8_t*>(&mask->sin6_addr), 16}),
			                      .index = index,
			                      .loopback = loopback,
			                      .multicast = multicast});
		}
	}
	freeifaddrs(list);
#endif
	return interfaces;
}

} // namespace openblack::online
