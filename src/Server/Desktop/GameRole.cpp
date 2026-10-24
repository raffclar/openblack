/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameRole.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iterator>
#include <random>
#include <thread>
#include <utility>

#include <JoinTicket.h>
#include <spdlog/spdlog.h>

#include "Online/Desktop/CurlHttpClient.h"
#include "Server/Desktop/SocketWebSocketServer.h"
#include "Server/Ed25519.h"
#include "Server/TicketCheck.h"

namespace openblack::server
{
namespace
{

/// The server asks the lobby for its keys again, for a ticket with a key that it does not know, at most this often
constexpr auto k_KeysAgainAfter = std::chrono::seconds(60);
/// The first fetch of the keys waits this long at most
constexpr auto k_FirstKeysLimit = std::chrono::seconds(15);

int64_t SteadyMicroseconds()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int64_t UnixSeconds()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string MakeSecret()
{
	std::random_device device;
	std::array<uint8_t, 16> bytes {};
	for (auto& byte : bytes)
	{
		byte = static_cast<uint8_t>(device());
	}
	return join_ticket::EncodeBase64Url(bytes);
}

bool IsAddress(std::string_view text)
{
	return text.starts_with("https://");
}

} // namespace

struct GameRole::Parts
{
	SocketWebSocketServer sockets;
	TicketCheck tickets {std::string(), ed25519::Verify};
	std::unique_ptr<online::CurlHttpClient> http;
	std::unique_ptr<LockstepHost> host;
	std::string keysAddress;
	std::chrono::steady_clock::time_point lastFetch {};
	bool fetching {false};

	void FetchKeys()
	{
		if (!http || fetching)
		{
			return;
		}
		fetching = true;
		lastFetch = std::chrono::steady_clock::now();
		http->Send(online::HttpRequest {.method = online::HttpMethod::Get, .url = keysAddress, .headers = {}, .body = {}},
		           [this](online::HttpResponse&& response) {
			           fetching = false;
			           const auto keys = response.status == 200 ? join_ticket::ParseTicketKeys(response.Text()) : std::nullopt;
			           if (!keys.has_value())
			           {
				           SPDLOG_LOGGER_WARN(spdlog::get("game"), "The keys of the tickets did not come (status {})",
				                              response.status);
				           return;
			           }
			           SPDLOG_LOGGER_INFO(spdlog::get("game"), "The server has {} keys of the tickets", keys->size());
			           tickets.SetKeys(*keys);
		           });
	}
};

GameRole::GameRole()
    : _parts(std::make_unique<Parts>())
{
}

GameRole::~GameRole() = default;

LockstepHost& GameRole::Host()
{
	return *_parts->host;
}

uint16_t GameRole::Port() const
{
	return _parts->sockets.Port();
}

std::optional<std::string> GameRole::Start(const NetworkSettings& settings, game_socket::Start start)
{
	auto& parts = *_parts;
	if (settings.serverId.empty())
	{
		return "the game role needs the server's id (--server-id)";
	}
	parts.tickets = TicketCheck(settings.serverId, ed25519::Verify);
	if (IsAddress(settings.ticketKeys))
	{
		parts.http = std::make_unique<online::CurlHttpClient>();
		parts.keysAddress = settings.ticketKeys;
		parts.FetchKeys();
		const auto until = std::chrono::steady_clock::now() + k_FirstKeysLimit;
		while (parts.fetching && std::chrono::steady_clock::now() < until)
		{
			parts.http->Poll();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	else if (!settings.ticketKeys.empty())
	{
		std::ifstream file(settings.ticketKeys, std::ios::binary);
		const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const auto keys = join_ticket::ParseTicketKeys(text);
		if (!keys.has_value())
		{
			return "the file of the ticket keys does not read: " + settings.ticketKeys;
		}
		parts.tickets.SetKeys(*keys);
	}
	if (parts.tickets.KeyCount() == 0)
	{
		return "the server has no key to check the tickets (--ticket-keys)";
	}

	ListenSettings listen {
	    .address = settings.address, .port = settings.port, .certificate = settings.certificate, .key = settings.key};
	if (auto error = parts.sockets.Listen(listen); error.has_value())
	{
		return error;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game server listens on {}:{} ({})", settings.address, parts.sockets.Port(),
	                   settings.certificate.has_value() ? "wss" : "ws behind a proxy");

	auto check = [&parts](std::string_view token) {
		auto result = parts.tickets.Check(token, UnixSeconds());
		if (result.problem == TicketProblem::UnknownKey && parts.http &&
		    std::chrono::steady_clock::now() - parts.lastFetch > k_KeysAgainAfter)
		{
			parts.FetchKeys();
		}
		return result;
	};
	parts.host = std::make_unique<LockstepHost>(
	    parts.sockets, std::move(check), SteadyMicroseconds, MakeSecret,
	    HostSettings {.start = std::move(start),
	                  .godsToStart = settings.godsToStart,
	                  .godSlots = 7,
	                  .comeBackMicroseconds = static_cast<int64_t>(settings.comeBackSeconds) * 1'000'000,
	                  .keptTurns = settings.comeBackSeconds * 10 + 200});
	return std::nullopt;
}

void GameRole::Update()
{
	auto& parts = *_parts;
	if (parts.http)
	{
		parts.http->Poll();
	}
	parts.sockets.Poll();
	if (parts.host)
	{
		parts.host->Update();
	}
}

} // namespace openblack::server
