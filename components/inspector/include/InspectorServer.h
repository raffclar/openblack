/*******************************************************************************
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

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// The inspector's transport: a TCP server on the local machine's loopback address only, taking lines of text and
/// answering each with a line. It never blocks: the game polls it once a frame, and it answers what came in since.
namespace openblack::inspector
{

/// One end of a socket, closed when it goes
class Socket
{
public:
	Socket() = default;
	explicit Socket(std::intptr_t handle)
	    : _handle(handle)
	{
	}
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;
	Socket(Socket&& other) noexcept;
	Socket& operator=(Socket&& other) noexcept;
	~Socket();

	[[nodiscard]] bool Valid() const;
	[[nodiscard]] std::intptr_t Handle() const { return _handle; }

private:
	std::intptr_t _handle {-1};
};

class Server
{
public:
	/// Lines longer than this drop their client
	static constexpr size_t k_LongestLine = 1024 * 1024;

	using Handler = std::function<std::string(std::string_view line)>;
	/// Whether a line takes control of the game: a client that has only sent lines that don't (a ping, say) is
	/// looking, not driving
	using ControlFilter = std::function<bool(std::string_view line)>;

	/// Listens on the loopback address at a port, any free one for 0; none, with why in the error, if it can't
	[[nodiscard]] static std::unique_ptr<Server> Listen(uint16_t port, std::string& error);
	/// A server on a socket already listening, as Listen makes it
	Server(Socket listener, uint16_t port);
	~Server();
	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;

	/// The port it listens on, the one chosen when asked for any
	[[nodiscard]] uint16_t Port() const { return _port; }
	[[nodiscard]] size_t ClientCount() const;
	/// The clients that have sent a line taking control; every line does unless a filter says otherwise
	[[nodiscard]] size_t ControllingClientCount() const;
	void SetControlFilter(ControlFilter filter) { _takesControl = std::move(filter); }

	/// Takes new clients, reads what they sent, and answers each whole line through the handler. Never waits. The
	/// number of lines answered.
	///
	/// Safe to call from two threads at once (the game's frame, and a helper answering while the game loads): the
	/// handler runs outside the server's lock, so a handler that takes long (a land loading) doesn't stop the other
	/// thread answering other lines meanwhile. Each client's answers go out whole, in the order they were made.
	size_t Poll(const Handler& handler);

private:
	struct Client
	{
		Socket socket;
		std::string received;
		std::string sending;
		bool controlling {false};
	};

	void Accept();
	/// False once the client has gone
	static bool Receive(Client& client);
	static bool Send(Client& client);

	Socket _listener;
	uint16_t _port;
	/// Shared so that a client a handler is answering outlives its removal by the other thread
	std::vector<std::shared_ptr<Client>> _clients;
	ControlFilter _takesControl;
	mutable std::mutex _mutex;
};

/// A blocking client of the server, as the tests and tools use it
class Client
{
public:
	[[nodiscard]] static std::optional<Client> Connect(uint16_t port);

	bool SendLine(std::string_view line);
	/// The next line, none if none came within the time or the server went
	[[nodiscard]] std::optional<std::string> ReceiveLine(std::chrono::milliseconds timeout);

private:
	explicit Client(Socket socket)
	    : _socket(std::move(socket))
	{
	}

	Socket _socket;
	std::string _received;
};

} // namespace openblack::inspector
