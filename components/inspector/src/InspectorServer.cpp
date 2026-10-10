/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InspectorServer.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace openblack::inspector;

namespace
{

#if defined(_WIN32)
using NativeSocket = SOCKET;
constexpr NativeSocket k_NoSocket = INVALID_SOCKET;
using Length = int;

/// Windows' sockets are started for as long as a socket of the inspector's is open
class SocketLibrary
{
public:
	static bool Start()
	{
		WSADATA data {};
		return WSAStartup(MAKEWORD(2, 2), &data) == 0;
	}
	static void Stop() { WSACleanup(); }
};

void CloseNative(NativeSocket socket)
{
	closesocket(socket);
}

bool WouldBlock()
{
	return WSAGetLastError() == WSAEWOULDBLOCK;
}

bool SetNonBlocking(NativeSocket socket)
{
	u_long on = 1;
	return ioctlsocket(socket, FIONBIO, &on) == 0;
}
#else
using NativeSocket = int;
constexpr NativeSocket k_NoSocket = -1;
using Length = socklen_t;

class SocketLibrary
{
public:
	static bool Start() { return true; }
	static void Stop() {}
};

void CloseNative(NativeSocket socket)
{
	close(socket);
}

bool WouldBlock()
{
	return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool SetNonBlocking(NativeSocket socket)
{
#if defined(SO_NOSIGPIPE)
	// A client gone mid-answer is an error to handle, not a signal that ends the game
	int on = 1;
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
	const int flags = fcntl(socket, F_GETFL, 0);
	return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

NativeSocket Native(const Socket& socket)
{
	return static_cast<NativeSocket>(socket.Handle());
}

Socket Wrap(NativeSocket socket)
{
	return Socket(socket == k_NoSocket ? -1 : static_cast<std::intptr_t>(socket));
}

sockaddr_in Loopback(uint16_t port)
{
	sockaddr_in address {};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	return address;
}

/// Sends what it can of the text without waiting: how much went, none if the socket failed
std::optional<size_t> SendSome(const Socket& socket, std::string_view text)
{
#if defined(_WIN32)
	const int sent = send(Native(socket), text.data(), static_cast<int>(std::min<size_t>(text.size(), 1 << 20)), 0);
#else
#if defined(MSG_NOSIGNAL)
	constexpr int k_Flags = MSG_NOSIGNAL;
#else
	// Where there is no such flag (Apple's systems) the sockets are told not to signal instead
	constexpr int k_Flags = 0;
#endif
	const auto sent = ::send(Native(socket), text.data(), text.size(), k_Flags);
#endif
	if (sent < 0)
	{
		return WouldBlock() ? std::optional<size_t>(0) : std::nullopt;
	}
	return static_cast<size_t>(sent);
}

/// Reads what has come without waiting: false once the other end has closed or failed
bool ReceiveSome(const Socket& socket, std::string& into)
{
	std::array<char, 4096> buffer {};
	while (true)
	{
#if defined(_WIN32)
		const int got = recv(Native(socket), buffer.data(), static_cast<int>(buffer.size()), 0);
#else
		const auto got = ::recv(Native(socket), buffer.data(), buffer.size(), 0);
#endif
		if (got == 0)
		{
			return false;
		}
		if (got < 0)
		{
			return WouldBlock();
		}
		into.append(buffer.data(), static_cast<size_t>(got));
		if (static_cast<size_t>(got) < buffer.size())
		{
			return true;
		}
	}
}

/// Waits for the socket to have something to read, up to a time: false if nothing came
bool WaitReadable(const Socket& socket, std::chrono::milliseconds timeout)
{
#if defined(_WIN32)
	WSAPOLLFD entry {};
	entry.fd = Native(socket);
	entry.events = POLLRDNORM;
	return WSAPoll(&entry, 1, static_cast<INT>(timeout.count())) > 0;
#else
	pollfd entry {};
	entry.fd = Native(socket);
	entry.events = POLLIN;
	return ::poll(&entry, 1, static_cast<int>(timeout.count())) > 0;
#endif
}

} // namespace

Socket::Socket(Socket&& other) noexcept
    : _handle(std::exchange(other._handle, -1))
{
}

Socket& Socket::operator=(Socket&& other) noexcept
{
	if (this != &other)
	{
		Socket closing(std::exchange(_handle, std::exchange(other._handle, -1)));
	}
	return *this;
}

Socket::~Socket()
{
	if (Valid())
	{
		CloseNative(Native(*this));
		SocketLibrary::Stop();
	}
}

bool Socket::Valid() const
{
	return _handle != -1;
}

std::unique_ptr<Server> Server::Listen(uint16_t port, std::string& error)
{
	if (!SocketLibrary::Start())
	{
		error = "the system's sockets couldn't be started";
		return nullptr;
	}
	auto listener = Wrap(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	if (!listener.Valid())
	{
		SocketLibrary::Stop();
		error = "no socket could be made";
		return nullptr;
	}
	auto address = Loopback(port);
	if (::bind(Native(listener), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		error = "port " + std::to_string(port) + " on 127.0.0.1 is taken";
		return nullptr;
	}
	if (::listen(Native(listener), 8) != 0 || !SetNonBlocking(Native(listener)))
	{
		error = "the socket couldn't listen";
		return nullptr;
	}
	Length length = sizeof(address);
	if (::getsockname(Native(listener), reinterpret_cast<sockaddr*>(&address), &length) != 0)
	{
		error = "the socket's port couldn't be read";
		return nullptr;
	}
	return std::make_unique<Server>(std::move(listener), ntohs(address.sin_port));
}

Server::Server(Socket listener, uint16_t port)
    : _listener(std::move(listener))
    , _port(port)
{
}

Server::~Server() = default;

void Server::Accept()
{
	while (true)
	{
		const NativeSocket accepted = ::accept(Native(_listener), nullptr, nullptr);
		if (accepted == k_NoSocket)
		{
			return;
		}
		// Each accepted socket holds the library open as the listener does
		SocketLibrary::Start();
		auto socket = Wrap(accepted);
		if (!SetNonBlocking(accepted))
		{
			continue;
		}
		_clients.push_back({.socket = std::move(socket), .received = {}, .sending = {}});
	}
}

bool Server::Receive(Client& client)
{
	return ReceiveSome(client.socket, client.received) && client.received.size() <= k_LongestLine;
}

bool Server::Send(Client& client)
{
	while (!client.sending.empty())
	{
		const auto sent = SendSome(client.socket, client.sending);
		if (!sent.has_value())
		{
			return false;
		}
		if (*sent == 0)
		{
			// The rest goes next frame
			return true;
		}
		client.sending.erase(0, *sent);
	}
	return true;
}

size_t Server::Poll(const Handler& handler)
{
	Accept();
	size_t answered = 0;
	for (auto& client : _clients)
	{
		if (!Receive(client))
		{
			client.socket = Socket();
			continue;
		}
		size_t end = 0;
		while ((end = client.received.find('\n')) != std::string::npos)
		{
			std::string_view line(client.received.data(), end);
			if (!line.empty() && line.back() == '\r')
			{
				line.remove_suffix(1);
			}
			if (!line.empty())
			{
				client.controlling = client.controlling || !_takesControl || _takesControl(line);
				client.sending += handler(line);
				client.sending += '\n';
				++answered;
			}
			client.received.erase(0, end + 1);
		}
		if (!Send(client))
		{
			client.socket = Socket();
		}
	}
	std::erase_if(_clients, [](const Client& client) { return !client.socket.Valid(); });
	return answered;
}

size_t Server::ControllingClientCount() const
{
	return static_cast<size_t>(std::ranges::count_if(_clients, [](const Client& client) { return client.controlling; }));
}

std::optional<Client> Client::Connect(uint16_t port)
{
	if (!SocketLibrary::Start())
	{
		return std::nullopt;
	}
	auto socket = Wrap(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	if (!socket.Valid())
	{
		SocketLibrary::Stop();
		return std::nullopt;
	}
	const auto address = Loopback(port);
	if (::connect(Native(socket), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
	{
		return std::nullopt;
	}
	if (!SetNonBlocking(Native(socket)))
	{
		return std::nullopt;
	}
	return Client(std::move(socket));
}

bool Client::SendLine(std::string_view line)
{
	std::string text(line);
	text += '\n';
	std::string_view rest = text;
	while (!rest.empty())
	{
		const auto sent = SendSome(_socket, rest);
		if (!sent.has_value())
		{
			return false;
		}
		rest.remove_prefix(*sent);
	}
	return true;
}

std::optional<std::string> Client::ReceiveLine(std::chrono::milliseconds timeout)
{
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	while (true)
	{
		if (const auto end = _received.find('\n'); end != std::string::npos)
		{
			auto line = _received.substr(0, end);
			_received.erase(0, end + 1);
			return line;
		}
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if (left.count() <= 0 || !WaitReadable(_socket, left))
		{
			return std::nullopt;
		}
		if (!ReceiveSome(_socket, _received))
		{
			return std::nullopt;
		}
	}
}
