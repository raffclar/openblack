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
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <JoinTicket.h>
#include <LobbyApiJson.h>
#include <openssl/rand.h>
#include <spdlog/spdlog.h>

#include "Common/BuildVersion.h"
#include "Creature/CreatureFileLoad.h"
#include "Online/Desktop/CurlHttpClient.h"
#include "Online/Desktop/UdpLanDiscovery.h"
#include "Online/GameBuildId.h"
#include "Online/LanDiscovery.h"
#include "Online/ServerKeyPin.h"
#include "Server/Desktop/Sha256SessionHasher.h"
#include "Server/Desktop/SocketWebSocketServer.h"
#include "Server/Ed25519.h"
#include "Server/LanRoom.h"
#include "Server/LobbyBans.h"
#include "Server/LobbyHostReport.h"
#include "Server/LobbyResults.h"
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
	// The secret comes from the cryptographic random source of OpenSSL. A server with no such source cannot give a secret
	// that no client can guess, so it stops.
	std::array<uint8_t, 16> bytes {};
	if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
	{
		throw std::runtime_error("The random source of OpenSSL gives no bytes for the secret of a session");
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
	/// The end of the game for the lobby, signed with the key of the server
	std::optional<ed25519::Seed> resultSeed;
	std::string resultsAddress;
	/// The gods of the room that play for a clan, from the lobby's last answer to a status report
	std::vector<lobby_api::ServerStatusClan> clans;
	/// The body of the report and its signature. The server sends the same bytes each time, so that the lobby knows the
	/// report again.
	std::string resultBody;
	std::string resultSignature;
	bool resultMade {false};
	bool resultSending {false};
	bool resultDone {false};
	uint32_t resultSends {0};
	std::chrono::steady_clock::time_point nextResultSend {};
	/// The answers to the search of the clients on the local network
	std::unique_ptr<online::LanResponder> lan;
	/// The report of the host to the lobby: the god teams and the winning conditions
	std::string hostReportAddress;
	gathering_box::PingClock hostReportClock;
	bool hostReportDecided {false};
	/// The status of the last answer to a report of the host, so that the log tells each change only one time
	int32_t hostReportStatus {-1};
	std::string lanHost;
	/// The pin of the key of the server's own certificate as hex digits, empty for a certificate file
	std::string lanKey;

	void AnswerSearches()
	{
		if (!lan || !host)
		{
			return;
		}
		lan->Poll([this] { return lan_room::AnswerOf(room, host->Started(), host->Seats(), sockets.Port(), lanHost, lanKey); });
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

	void SendResult()
	{
		if (!lobby || !resultMade || resultDone || resultSending || resultSends >= lobby_results::k_MostSends ||
		    std::chrono::steady_clock::now() < nextResultSend)
		{
			return;
		}
		resultSending = true;
		++resultSends;
		online::HttpRequest request {
		    .method = online::HttpMethod::Post,
		    .url = resultsAddress,
		    .headers = {{"Content-Type", "application/json"}, {std::string(lobby_results::k_SignatureHeader), resultSignature}},
		    .body = std::vector<uint8_t>(resultBody.begin(), resultBody.end())};
		lobby->Send(std::move(request), [this](online::HttpResponse&& response) {
			resultSending = false;
			switch (lobby_results::NextAfter(response.status))
			{
			case lobby_results::Next::Done:
			{
				resultDone = true;
				const auto answer = lobby_api::ParseResultAnswer(response.Text());
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby took the result of the game (result {})",
				                   answer.has_value() ? std::to_string(answer->result) : std::string("not known"));
				break;
			}
			case lobby_results::Next::Stop:
				resultDone = true;
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "The lobby refused the result of the game (status {}): {}",
				                   response.status, response.Text().substr(0, 200));
				break;
			case lobby_results::Next::SendAgain:
				nextResultSend = std::chrono::steady_clock::now() + lobby_results::k_SendAgainAfter;
				SPDLOG_LOGGER_WARN(spdlog::get("game"),
				                   "The lobby did not take the result of the game (status {}), send {} of {}", response.status,
				                   resultSends, lobby_results::k_MostSends);
				break;
			}
		});
	}

	void Report(lobby_status::Phase phase)
	{
		if (!lobby || reporting || !host)
		{
			return;
		}
		reporting = true;
		const auto sentAt = std::chrono::steady_clock::now();
		nextReport = sentAt + lobby_status::k_ReportInterval;
		const auto text = lobby_api::ToJson(lobby_status::MakeStatus(room, phase, host->Seats()));
		online::HttpRequest request {.method = online::HttpMethod::Put,
		                             .url = statusAddress,
		                             .headers = {{"Content-Type", "application/json"}},
		                             .body = std::vector<uint8_t>(text.begin(), text.end())};
		if (bans && phase != lobby_status::Phase::Exiting)
		{
			bans->Fetch();
		}
		lobby->Send(std::move(request), [this, sentAt](online::HttpResponse&& response) {
			reporting = false;
			// The answer says whether the lobby lists the server, and when it wants the next report
			const auto reply = response.status == 200 ? lobby_api::ParseServerStatusReply(response.Text()) : std::nullopt;
			if (reply.has_value())
			{
				nextReport = sentAt + std::chrono::seconds(reply->nextStatusIn);
				clans = reply->clans;
			}
			const bool ok = reply.has_value() && reply->listed;
			if (ok != listed)
			{
				if (ok)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby lists the server");
				}
				else if (reply.has_value())
				{
					SPDLOG_LOGGER_WARN(spdlog::get("game"), "The lobby took the status of the server but does not list it");
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
                                           uint8_t godSlots, std::optional<lobby_status::CustomMap> customMap,
                                           std::string landSha256)
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

	// A room on the local network with no certificate file makes a certificate of its own, and its answers give the pin
	// of its key (deviation 113)
	const bool ownCertificate = settings.lan && !settings.certificate.has_value();
	ListenSettings listen {.address = settings.address,
	                       .port = settings.port,
	                       .certificate = settings.certificate,
	                       .key = settings.key,
	                       .ownCertificate = ownCertificate};
	if (auto error = parts.sockets.Listen(listen); error.has_value())
	{
		return error;
	}
	const auto keyPin = parts.sockets.OwnKeyPin();
	parts.lanKey = keyPin.has_value() ? online::server_key::HexOf(*keyPin) : std::string();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game server listens on {}:{} ({})", settings.address, parts.sockets.Port(),
	                   ownCertificate                     ? "wss with its own certificate, key " + parts.lanKey
	                   : settings.certificate.has_value() ? std::string("wss")
	                                                      : std::string("ws behind a proxy"));

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
	if (settings.resultKey.has_value() && settings.lobbyApi.empty())
	{
		return "the result key (--result-key) needs the address of the lobby (--lobby-api)";
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
		parts.hostReportAddress = lobby_host_report::HostReportAddress(base, settings.serverId);
		if (settings.resultKey.has_value())
		{
			std::ifstream file(*settings.resultKey, std::ios::binary);
			const std::string pem((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			parts.resultSeed = ed25519::SeedOfPem(pem);
			if (!parts.resultSeed.has_value())
			{
				return "the file of the result key is not an Ed25519 private key in PEM: " + settings.resultKey->string();
			}
			parts.resultsAddress = lobby_results::ResultsAddress(base, settings.serverId);
		}
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
	                  .allGoneMicroseconds = static_cast<int64_t>(settings.allGoneSeconds) * 1'000'000,
	                  .keptTurns = settings.comeBackSeconds * 10 + 200,
	                  .testCommands = settings.testCommands,
	                  .sessionHasher = std::make_shared<Sha256SessionHasher>(),
	                  .lanNames = settings.lan,
	                  .relaysCreature = creature_file_load::ServerRelays,
	                  .lobby = settings.lobby,
	                  .buildId = online::build_id::Own(),
	                  .mapSha256 = std::move(landSha256),
	                  .loadWindowMicroseconds = settings.joinWindowSeconds * 1'000'000});
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
		parts.SendResult();
		parts.lobby->Poll();
	}
}

bool GameRole::HostReportDue()
{
	auto& parts = *_parts;
	if (!parts.lobby || !parts.host || !parts.host->Started())
	{
		return false;
	}
	// The clock uses the time of the computer in milliseconds, which wraps as the clock of the game wraps
	const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
	return parts.hostReportClock.Due(static_cast<uint32_t>(now.count()));
}

void GameRole::SendHostReport(uint32_t turn, bool decided, const HostConditions& conditions)
{
	auto& parts = *_parts;
	if (!parts.lobby || !parts.host || (decided && parts.hostReportDecided))
	{
		return;
	}
	parts.hostReportDecided = parts.hostReportDecided || decided;
	const gathering_box::host_report::Facts facts {
	    .teams = lobby_host_report::TeamsOf(parts.host->Seats(), parts.clans),
	    .turn = turn,
	    .decided = decided,
	    .conditions = conditions,
	};
	const auto body = lobby_host_report::BodyOf(gathering_box::host_report::Encode(facts));
	online::HttpRequest request {.method = online::HttpMethod::Put,
	                             .url = parts.hostReportAddress,
	                             .headers = {{"Content-Type", "application/json"}},
	                             .body = std::vector<uint8_t>(body.begin(), body.end())};
	parts.lobby->Send(std::move(request), [&parts, turn](online::HttpResponse&& response) {
		const bool taken = response.status == 204 || response.status == 200;
		if (std::exchange(parts.hostReportStatus, response.status) == response.status)
		{
			return;
		}
		if (taken)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby takes the reports of the host (turn {})", turn);
			return;
		}
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The lobby did not take the report of the host of turn {} (status {}): {}",
		                   turn, response.status, response.Text().substr(0, 200));
	});
}

bool GameRole::WantsResult() const
{
	const auto& parts = *_parts;
	return parts.lobby && parts.resultSeed.has_value() && parts.host && !parts.resultMade;
}

void GameRole::ReportResult(const lobby_results::Game& game, std::span<const lobby_results::GodFigures> gods)
{
	auto& parts = *_parts;
	if (!WantsResult())
	{
		return;
	}
	parts.resultMade = true;
	const auto report = lobby_results::MakeReport(game, gods, parts.host->Seats(), parts.clans);
	if (!report.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game {} has fewer than 2 gods with an account: no result for the lobby",
		                   game.id);
		parts.resultDone = true;
		return;
	}
	parts.resultBody = lobby_api::ToJson(*report);
	const auto signature = ed25519::Sign(*parts.resultSeed, lobby_results::SignedBytes(parts.resultBody));
	if (!signature.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The server cannot sign the result of the game {}", game.id);
		parts.resultDone = true;
		return;
	}
	parts.resultSignature = join_ticket::EncodeBase64Url(*signature);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game {} is decided at turn {}: the server sends the result of {} gods",
	                   game.id, game.turn, report->gods.size());
	parts.SendResult();
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
		while ((parts.reporting || parts.resultSending) && std::chrono::steady_clock::now() < until)
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
