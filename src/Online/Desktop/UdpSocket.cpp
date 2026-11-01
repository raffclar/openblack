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

#include <array>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace openblack::online
{
namespace
{

/// The most bytes of a datagram that the socket reads
constexpr size_t k_MostBytes = 65536;

#if defined(_WIN32)
using Socket = SOCKET;
using Length = int;
Socket ToSocket(intptr_t value)
{
	return static_cast<Socket>(value);
}
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
#else
using Socket = int;
using Length = socklen_t;
Socket ToSocket(intptr_t value)
{
	return static_cast<Socket>(value);
}
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
#endif

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

bool UdpSocket::Open(uint16_t port, bool broadcast)
{
	if (_socket != -1)
	{
		return true;
	}
	const auto socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!IsValid(socket))
	{
		return false;
	}
	int on = 1;
	if (broadcast)
	{
		setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
	}
	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);
	if (!MakeNonBlocking(socket) || bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		CloseSocket(socket);
		return false;
	}
	_socket = static_cast<intptr_t>(socket);
	return true;
}

bool UdpSocket::IsOpen() const noexcept
{
	return _socket != -1;
}

bool UdpSocket::SendTo(std::string_view address, uint16_t port, std::span<const uint8_t> bytes)
{
	if (_socket == -1)
	{
		return false;
	}
	sockaddr_in to {};
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	const std::string text(address);
	if (inet_pton(AF_INET, text.c_str(), &to.sin_addr) != 1)
	{
		return false;
	}
	const auto sent = sendto(ToSocket(_socket), reinterpret_cast<const char*>(bytes.data()), static_cast<Length>(bytes.size()),
	                         0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
	return sent == static_cast<decltype(sent)>(bytes.size());
}

std::optional<UdpSocket::Datagram> UdpSocket::Receive()
{
	if (_socket == -1)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> buffer(k_MostBytes);
	sockaddr_in from {};
	Length fromLength = sizeof(from);
	const auto received = recvfrom(ToSocket(_socket), reinterpret_cast<char*>(buffer.data()),
	                               static_cast<Length>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
	if (received < 0)
	{
		return std::nullopt;
	}
	buffer.resize(static_cast<size_t>(received));
	std::array<char, INET_ADDRSTRLEN> text {};
	if (inet_ntop(AF_INET, &from.sin_addr, text.data(), text.size()) == nullptr)
	{
		return std::nullopt;
	}
	return Datagram {.bytes = std::move(buffer), .address = std::string(text.data()), .port = ntohs(from.sin_port)};
}

} // namespace openblack::online
