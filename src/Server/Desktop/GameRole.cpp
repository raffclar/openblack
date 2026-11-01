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
#include <memory>
#include <random>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <JoinTicket.h>
#include <LobbyApiJson.h>
#include <spdlog/spdlog.h>

#include "Common/BuildVersion.h"
#include "Online/Desktop/CurlHttpClient.h"
#include "Online/Desktop/UdpLanDiscovery.h"
#include "Online/LanDiscovery.h"
#include "Server/Desktop/Sha256SessionHasher.h"
#include "Server/Desktop/SocketWebSocketServer.h"
#include "Server/Ed25519.h"
#include "Server/LanRoom.h"
#include "Server/LobbyBans.h"
#include "Server/LobbyStatus.h"
#include "Server/TicketCheck.h"

namespace openblack::server
{
namespace
{

/// The server asks the lobby for its keys again, for a ticket with a key that it does not know, at most this often
constexpr auto k_KeysAgainAfter = std::chrono::seconds(60);
/// The first fetch of the keys waits this long at most
constexpr auto k_FirstKeysLimit = std::chrono::seconds(15);
/// The last report, when the server stops, waits this long at most
constexpr auto k_LastReportLimit = std::chrono::seconds(3);

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
	/// The status reports to the lobby, over mutual TLS
	std::unique_ptr<online::CurlHttpClient> lobby;
	std::string statusAddress;
	lobby_status::Room room;
	std::chrono::steady_clock::time_point nextReport {};
	bool reporting {false};
	bool listed {false};
	/// The bans of the server, which the server reads from the lobby with each status report
	std::unique_ptr<lobby_bans::Reader> bans;
	/// The land that the players do not have, which goes to the lobby once (deviation 169)
	std::optional<lobby_status::CustomMap> customMap;
	std::string mapAddress;
	std::chrono::steady_clock::time_point nextMapSend {};
	bool mapSending {false};
	bool mapSent {false};
	/// The answers to the search of the clients on the local network
	std::unique_ptr<online::LanResponder> lan;
	std::string lanHost;

	void AnswerSearches()
	{
		if (!lan || !host)
		{
			return;
		}
		lan->Poll([this] { return lan_room::AnswerOf(room, host->Started(), host->Seats(), sockets.Port(), lanHost); });
	}

	void SendMap()
	{
		if (!lobby || !customMap.has_value() || mapSending || mapSent || std::chrono::steady_clock::now() < nextMapSend)
		{
			return;
		}
		mapSending = true;
		online::HttpRequest request {.method = online::HttpMethod::Put,
		                             .url = mapAddress,
		                             .headers = {{"Content-Type", "application/octet-stream"}},
		                             .body = customMap->bytes,
		                             .large = true};
		lobby->Send(std::move(request), [this](online::HttpResponse&& response) {
			mapSending = false;
			mapSent = response.status == 200 || response.status == 201;
			if (mapSent)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby took the map {} of the room", customMap->file);
				return;
			}
			nextMapSend = std::chrono::steady_clock::now() + lobby_status::k_MapAgainAfter;
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The lobby did not take the map {} (status {}): {}", customMap->file,
			                   response.status, response.Text().substr(0, 200));
		});
	}

	void Report(lobby_status::Phase phase)
	{
		if (!lobby || reporting || !host)
		{
			return;
		}
		reporting = true;
		nextReport = std::chrono::steady_clock::now() + lobby_status::k_ReportInterval;
		const auto text = lobby_api::ToJson(lobby_status::MakeStatus(room, phase, host->Seats()));
		online::HttpRequest request {.method = online::HttpMethod::Put,
		                             .url = statusAddress,
		                             .headers = {{"Content-Type", "application/json"}},
		                             .body = std::vector<uint8_t>(text.begin(), text.end())};
		if (bans && phase != lobby_status::Phase::Exiting)
		{
			bans->Fetch();
		}
		lobby->Send(std::move(request), [this](online::HttpResponse&& response) {
			reporting = false;
			const bool ok = response.status == 200;
			if (ok != listed)
			{
				if (ok)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby lists the server");
				}
				else
				{
					SPDLOG_LOGGER_WARN(spdlog::get("game"), "The lobby did not take the status of the server (status {}): {}",
					                   response.status, response.Text().substr(0, 200));
				}
			}
			listed = ok;
		});
	}

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

std::optional<std::string> GameRole::Start(const NetworkSettings& settings, game_socket::Start start, uint64_t landChecksum,
                                           uint8_t godSlots, std::optional<lobby_status::CustomMap> customMap)
{
	auto& parts = *_parts;
	if (settings.serverId.empty() && !settings.lan)
	{
		return "the game role needs the server's id (--server-id)";
	}
	parts.tickets = TicketCheck(settings.serverId, ed25519::Verify);
	// The test authorities are only for a lobby on this computer
	const auto testAuthorities = [&settings](std::string_view address) -> std::optional<std::filesystem::path> {
		if (settings.lobbyTestAuthorities.has_value() && online::http_policy::IsLoopbackAddress(address))
		{
			return settings.lobbyTestAuthorities;
		}
		return std::nullopt;
	};
	if (settings.lobbyTestAuthorities.has_value() && !online::http_policy::IsLoopbackAddress(settings.lobbyApi) &&
	    !online::http_policy::IsLoopbackAddress(settings.ticketKeys))
	{
		return "the test authorities (--lobby-test-ca) are only for a lobby on a loopback address";
	}
	if (IsAddress(settings.ticketKeys))
	{
		const auto authorities = testAuthorities(settings.ticketKeys);
		parts.http = authorities.has_value() ? std::make_unique<online::CurlHttpClient>(std::nullopt, *authorities)
		                                     : std::make_unique<online::CurlHttpClient>();
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
	if (parts.tickets.KeyCount() == 0 && !settings.lan)
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
	parts.room = lobby_status::Room {.hostname = settings.roomName,
	                                 .land = start.land,
	                                 .maxGods = godSlots,
	                                 .version = std::string(build_version::Semantic())};
	if (settings.lan)
	{
		parts.lan = online::MakeUdpLanResponder([] {
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
			    .count();
		});
		parts.lanHost = settings.lanHost;
		if (parts.lan == nullptr)
		{
			return "the UDP port of the search on the local network does not open";
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game server answers the search of the local network on UDP port {}",
		                   online::lan_discovery::k_Port);
	}
	if (!settings.lobbyApi.empty())
	{
		if (!IsAddress(settings.lobbyApi) || !settings.clientCertificate.has_value() || !settings.clientKey.has_value())
		{
			return "the status reports need an https:// address of the lobby and a client certificate and key";
		}
		const online::ClientIdentity identity {.certificate = *settings.clientCertificate, .key = *settings.clientKey};
		const auto authorities = testAuthorities(settings.lobbyApi);
		parts.lobby = authorities.has_value() ? std::make_unique<online::CurlHttpClient>(identity, *authorities)
		                                      : std::make_unique<online::CurlHttpClient>(identity);
		auto base = settings.lobbyApi;
		while (base.ends_with('/'))
		{
			base.pop_back();
		}
		parts.statusAddress = base + "/servers/" + settings.serverId + "/status";
		if (customMap.has_value())
		{
			parts.mapAddress = lobby_status::MapAddress(base, settings.serverId, *customMap);
			parts.customMap = std::move(customMap);
		}
		parts.bans = std::make_unique<lobby_bans::Reader>(
		    *parts.lobby, lobby_bans::BansAddress(settings.lobbyApi, settings.serverId), [&parts](std::set<int64_t> banned) {
			    if (!parts.host)
			    {
				    return;
			    }
			    if (banned != parts.host->Bans())
			    {
				    SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby bans {} accounts from the server", banned.size());
			    }
			    parts.host->SetBans(std::move(banned));
		    });
	}

	parts.host = std::make_unique<LockstepHost>(
	    parts.sockets, std::move(check), SteadyMicroseconds, MakeSecret,
	    HostSettings {.start = std::move(start),
	                  .landChecksum = landChecksum,
	                  .minGods = settings.minGods,
	                  .godSlots = godSlots,
	                  .comeBackMicroseconds = static_cast<int64_t>(settings.comeBackSeconds) * 1'000'000,
	                  .keptTurns = settings.comeBackSeconds * 10 + 200,
	                  .testCommands = settings.testCommands,
	                  .sessionHasher = std::make_shared<Sha256SessionHasher>(),
	                  .lanNames = settings.lan});
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
	parts.AnswerSearches();
	if (parts.lobby)
	{
		if (std::chrono::steady_clock::now() >= parts.nextReport)
		{
			parts.Report(lobby_status::PhaseOf(parts.host && parts.host->Started(), false));
		}
		parts.SendMap();
		parts.lobby->Poll();
	}
}

void GameRole::Close()
{
	auto& parts = *_parts;
	if (!parts.lobby || !parts.host)
	{
		return;
	}
	// The last report goes after the report that did not come back yet
	const auto until = std::chrono::steady_clock::now() + k_LastReportLimit;
	const auto wait = [&parts, until] {
		while (parts.reporting && std::chrono::steady_clock::now() < until)
		{
			parts.lobby->Poll();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	};
	wait();
	parts.Report(lobby_status::Phase::Exiting);
	wait();
}

} // namespace openblack::server
