/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LobbyClientSystem.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <string_view>

#include <LobbyApiJson.h>
#include <fmt/format.h>

namespace openblack::ecs::systems
{

using online::HttpMethod;
using online::HttpResponse;

namespace
{

constexpr std::string_view k_JsonType = "application/json";

/// The text of the body of an answer
std::string_view TextOf(const HttpResponse& response)
{
	return response.Text();
}

std::vector<uint8_t> BytesOf(std::string_view text)
{
	return {text.begin(), text.end()};
}

template <typename Value>
void Fail(const LobbyCallback<Value>& callback, LobbyError error)
{
	if (callback)
	{
		callback(LobbyResult<Value> {.value = std::nullopt, .error = std::move(error)});
	}
}

template <typename Value>
void Succeed(const LobbyCallback<Value>& callback, Value value)
{
	if (callback)
	{
		callback(LobbyResult<Value> {.value = std::move(value), .error = {}});
	}
}

LobbyError ErrorOfKind(LobbyErrorKind kind)
{
	return LobbyError {.kind = kind};
}

bool IsSuccess(int32_t status)
{
	return status >= 200 && status < 300;
}

/// The answer that gives the same error as a failed refresh of the tokens, for a call that waited for the refresh
HttpResponse ResponseOf(const LobbyError& error)
{
	switch (error.kind)
	{
	case LobbyErrorKind::AddressRefused:
		return {.error = online::HttpError::AddressRefused};
	case LobbyErrorKind::Network:
		return {.error = online::HttpError::Network};
	case LobbyErrorKind::Tls:
		return {.error = online::HttpError::Tls};
	case LobbyErrorKind::Timeout:
		return {.error = online::HttpError::Timeout};
	case LobbyErrorKind::Cancelled:
		return {.error = online::HttpError::Cancelled};
	default:
		// Each other failure of the refresh means that the session cannot continue
		return {.error = online::HttpError::None, .status = error.status >= 500 ? error.status : 401};
	}
}

} // namespace

LobbyError LobbyErrorOf(const online::HttpResponse& response)
{
	switch (response.error)
	{
	case online::HttpError::None:
		break;
	case online::HttpError::AddressRefused:
		return ErrorOfKind(LobbyErrorKind::AddressRefused);
	case online::HttpError::Network:
		return ErrorOfKind(LobbyErrorKind::Network);
	case online::HttpError::Tls:
		return ErrorOfKind(LobbyErrorKind::Tls);
	case online::HttpError::Timeout:
		return ErrorOfKind(LobbyErrorKind::Timeout);
	case online::HttpError::Cancelled:
		return ErrorOfKind(LobbyErrorKind::Cancelled);
	}
	if (IsSuccess(response.status))
	{
		return {};
	}
	LobbyError error {.status = response.status};
	if (const auto problem = lobby_api::ParseProblem(TextOf(response)); problem.has_value())
	{
		error.detail = !problem->detail.empty() ? problem->detail : problem->title;
		error.code = problem->code;
	}
	if (response.status == 401)
	{
		error.kind = LobbyErrorKind::NotLoggedIn;
	}
	else if (response.status == 429)
	{
		error.kind = LobbyErrorKind::RateLimited;
		if (const auto wait = response.Header("retry-after"); wait.has_value())
		{
			int32_t seconds = 0;
			const auto* end = wait->data() + wait->size();
			if (std::from_chars(wait->data(), end, seconds).ptr == end)
			{
				error.retryAfterSeconds = seconds;
			}
		}
	}
	else if (response.status >= 500)
	{
		error.kind = LobbyErrorKind::ServerFault;
	}
	else if (response.status >= 400)
	{
		error.kind = LobbyErrorKind::Refused;
	}
	else
	{
		error.kind = LobbyErrorKind::BadAnswer;
	}
	return error;
}

LobbyClientSystem::LobbyClientSystem(std::optional<LobbyClientConfig> config, online::HttpClientInterface& http,
                                     std::unique_ptr<online::PushChannelInterface> push,
                                     std::unique_ptr<online::TokenStoreInterface> tokens, Clock clock)
    : _config(std::move(config))
    , _http(http)
    , _push(std::move(push))
    , _tokens(std::move(tokens))
    , _clock(std::move(clock))
{
}

LobbyClientSystem::~LobbyClientSystem()
{
	_push->Close();
}

std::string LobbyClientSystem::Address(std::string_view apiPath) const
{
	return fmt::format("{}{}{}", _config.has_value() ? _config->origin : std::string {}, lobby_api::k_ApiPrefix, apiPath);
}

bool LobbyClientSystem::AccessTokenValid() const
{
	return !_accessToken.empty() && _clock() + lobby_times::k_RefreshMargin < _accessExpiry;
}

void LobbyClientSystem::Update()
{
	_push->Poll();
	if (_reconnectAt.has_value() && _clock() >= *_reconnectAt)
	{
		_reconnectAt.reset();
		if (_state == LobbySessionState::LoggedIn && _pushWanted)
		{
			OpenPush();
		}
	}
}

void LobbyClientSystem::Login(std::string name, std::string password, LobbyCallback<lobby_api::AccountRef> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	if (_state == LobbySessionState::LoggingIn || _loggingOut)
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::Busy));
		return;
	}
	if (!lobby_api::IsValidAccountName(name))
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::InvalidName));
		return;
	}
	if (password.empty() || password.size() > lobby_api::k_MostPasswordCharacters)
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::InvalidPassword));
		return;
	}
	// A new login replaces the session before it
	if (_state == LobbySessionState::LoggedIn)
	{
		EndSession();
	}
	_state = LobbySessionState::LoggingIn;
	const auto generation = ++_generation;
	const auto body = lobby_api::ToJson(lobby_api::LoginRequest {
	    .name = std::move(name),
	    .password = std::move(password),
	    .cookie = _config->client == lobby_api::ClientKind::Browser,
	});
	online::HttpRequest request {
	    .method = HttpMethod::Post,
	    .url = Address("/sessions"),
	    .headers = {{"Content-Type", std::string(k_JsonType)}, {"Accept", std::string(k_JsonType)}},
	    .body = BytesOf(body),
	};
	_http.Send(std::move(request), [this, generation, callback = std::move(callback)](HttpResponse&& response) {
		if (generation != _generation)
		{
			Fail(callback, ErrorOfKind(LobbyErrorKind::Cancelled));
			return;
		}
		_state = LobbySessionState::LoggedOut;
		if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		{
			// The lobby gives the same answer for an unknown name and a wrong password
			if (error.kind == LobbyErrorKind::NotLoggedIn)
			{
				error.kind = LobbyErrorKind::Refused;
			}
			Fail(callback, std::move(error));
			return;
		}
		const auto session = lobby_api::ParseSession(TextOf(response));
		if (!session.has_value())
		{
			Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			return;
		}
		ApplySession(*session);
		OpenPush();
		Succeed(callback, session->account);
	});
}

void LobbyClientSystem::ResumeSession(LobbyCallback<lobby_api::AccountRef> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	if (_state == LobbySessionState::LoggedIn && _account.has_value())
	{
		Succeed(callback, *_account);
		return;
	}
	if (_state == LobbySessionState::LoggingIn || _loggingOut)
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::Busy));
		return;
	}
	_state = LobbySessionState::LoggingIn;
	Refresh([this, callback = std::move(callback)](const LobbyError& error) {
		if (error.kind != LobbyErrorKind::None || !_account.has_value())
		{
			if (_state == LobbySessionState::LoggingIn)
			{
				_state = LobbySessionState::LoggedOut;
			}
			Fail(callback, error.kind != LobbyErrorKind::None ? error : ErrorOfKind(LobbyErrorKind::NotLoggedIn));
			return;
		}
		OpenPush();
		Succeed(callback, *_account);
	});
}

void LobbyClientSystem::Refresh(std::function<void(const LobbyError&)> after)
{
	_afterRefresh.push_back(std::move(after));
	if (_refreshing)
	{
		return;
	}
	const auto finish = [this](const LobbyError& error) {
		_refreshing = false;
		auto waiting = std::move(_afterRefresh);
		_afterRefresh.clear();
		for (auto& function : waiting)
		{
			function(error);
		}
	};

	// The desktop sends the refresh token. The browser sends none, because the cookie holds it.
	lobby_api::RefreshRequest refresh;
	if (_config->client == lobby_api::ClientKind::Desktop)
	{
		if (!_refreshToken.has_value())
		{
			_refreshToken = _tokens->Load();
		}
		if (!_refreshToken.has_value())
		{
			finish(ErrorOfKind(LobbyErrorKind::NotLoggedIn));
			return;
		}
		refresh.refreshToken = _refreshToken;
	}
	_refreshing = true;
	const auto generation = _generation;
	online::HttpRequest request {
	    .method = HttpMethod::Post,
	    .url = Address("/sessions/refresh"),
	    .headers = {{"Content-Type", std::string(k_JsonType)}, {"Accept", std::string(k_JsonType)}},
	    .body = BytesOf(lobby_api::ToJson(refresh)),
	};
	_http.Send(std::move(request), [this, generation, finish](HttpResponse&& response) {
		if (generation != _generation)
		{
			finish(ErrorOfKind(LobbyErrorKind::Cancelled));
			return;
		}
		auto error = LobbyErrorOf(response);
		if (error.kind == LobbyErrorKind::NotLoggedIn || error.kind == LobbyErrorKind::Refused)
		{
			// The lobby refused the refresh token: it is old, used, or the session ended. The player must log in again.
			EndSession();
			finish(ErrorOfKind(LobbyErrorKind::NotLoggedIn));
			return;
		}
		if (error.kind != LobbyErrorKind::None)
		{
			finish(error);
			return;
		}
		const auto session = lobby_api::ParseSession(TextOf(response));
		if (!session.has_value())
		{
			finish(LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			return;
		}
		ApplySession(*session);
		finish({});
	});
}

void LobbyClientSystem::ApplySession(const lobby_api::Session& session)
{
	_accessToken = session.accessToken;
	_accessExpiry = std::chrono::system_clock::time_point(std::chrono::milliseconds(session.accessTokenExpires));
	_account = session.account;
	_emailRequired = session.emailRequired;
	_state = LobbySessionState::LoggedIn;
	if (session.refreshToken.has_value())
	{
		// Each refresh token works one time, so the new token replaces the old token at once
		_refreshToken = session.refreshToken;
		_tokens->Save(*session.refreshToken);
	}
}

void LobbyClientSystem::EndSession()
{
	++_generation;
	_state = LobbySessionState::LoggedOut;
	_account.reset();
	_emailRequired = false;
	_accessToken.clear();
	_accessExpiry = {};
	_refreshToken.reset();
	_tokens->Clear();
	_pushWanted = false;
	_reconnectAt.reset();
	_push->Close();
}

void LobbyClientSystem::Logout(bool everywhere, LobbyCallback<LobbyDone> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	if (_state != LobbySessionState::LoggedIn)
	{
		// Nothing to end at the lobby, but a token that the store keeps goes
		EndSession();
		Succeed(callback, LobbyDone {});
		return;
	}
	if (_loggingOut)
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::Busy));
		return;
	}
	_loggingOut = true;
	Authorized(Call {
	    .method = HttpMethod::Delete,
	    .path = everywhere ? "/sessions" : "/sessions/current",
	    .done =
	        [this, callback = std::move(callback)](HttpResponse&& response) {
		        _loggingOut = false;
		        // The client forgets the tokens also when the lobby did not answer
		        EndSession();
		        const auto error = LobbyErrorOf(response);
		        if (error.kind != LobbyErrorKind::None && error.kind != LobbyErrorKind::NotLoggedIn)
		        {
			        Fail(callback, error);
			        return;
		        }
		        Succeed(callback, LobbyDone {});
	        },
	});
}

void LobbyClientSystem::Authorized(Call call)
{
	if (!_config.has_value())
	{
		if (call.done)
		{
			call.done(HttpResponse {.error = online::HttpError::None, .status = 401});
		}
		return;
	}
	if (AccessTokenValid())
	{
		Dispatch(std::move(call));
		return;
	}
	if (_state != LobbySessionState::LoggedIn)
	{
		if (call.done)
		{
			// The client has no session: the call fails as the lobby would refuse it
			call.done(HttpResponse {.error = online::HttpError::None, .status = 401});
		}
		return;
	}
	Refresh([this, call = std::move(call)](const LobbyError& error) mutable {
		if (error.kind != LobbyErrorKind::None)
		{
			if (call.done)
			{
				call.done(ResponseOf(error));
			}
			return;
		}
		Dispatch(std::move(call));
	});
}

void LobbyClientSystem::Dispatch(Call call)
{
	online::HttpRequest request {
	    .method = call.method,
	    .url = Address(call.path),
	    .headers = call.headers,
	    .body = call.body,
	};
	request.headers.emplace_back("Authorization", "Bearer " + _accessToken);
	const auto generation = _generation;
	_http.Send(std::move(request), [this, generation, call = std::move(call)](HttpResponse&& response) mutable {
		if (response.status == 401 && !call.retried && generation == _generation && _state == LobbySessionState::LoggedIn)
		{
			// The lobby refused the access token before its time: the client gets a new pair one time and calls again
			call.retried = true;
			_accessToken.clear();
			Authorized(std::move(call));
			return;
		}
		if (call.done)
		{
			call.done(std::move(response));
		}
	});
}

template <typename Value, typename Parser>
void LobbyClientSystem::CallFor(HttpMethod method, std::string path, LobbyCallback<Value> callback, Parser parse)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	Authorized(Call {
	    .method = method,
	    .path = std::move(path),
	    .headers = {{"Accept", std::string(k_JsonType)}},
	    .done =
	        [callback = std::move(callback), parse = std::move(parse)](HttpResponse&& response) {
		        if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		        {
			        Fail(callback, std::move(error));
			        return;
		        }
		        auto value = parse(response);
		        if (!value.has_value())
		        {
			        Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			        return;
		        }
		        Succeed(callback, std::move(*value));
	        },
	});
}

void LobbyClientSystem::GetMe(LobbyCallback<lobby_api::Me> callback)
{
	CallFor<lobby_api::Me>(HttpMethod::Get, "/me", std::move(callback),
	                       [](const HttpResponse& response) { return lobby_api::ParseMe(TextOf(response)); });
}

void LobbyClientSystem::GetServers(LobbyCallback<lobby_api::ServerList> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	Call call {
	    .method = HttpMethod::Get,
	    .path = "/servers",
	    .headers = {{"Accept", std::string(k_JsonType)}},
	};
	if (_serversTag.has_value())
	{
		call.headers.emplace_back("If-None-Match", *_serversTag);
	}
	call.done = [this, callback = std::move(callback)](HttpResponse&& response) {
		if (response.error == online::HttpError::None && response.status == 304 && _serversTag.has_value())
		{
			// The list did not change
			Succeed(callback, lobby_api::ServerList {.servers = _servers});
			return;
		}
		if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		{
			Fail(callback, std::move(error));
			return;
		}
		auto list = lobby_api::ParseServerList(TextOf(response));
		if (!list.has_value())
		{
			Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			return;
		}
		_servers = list->servers;
		if (const auto tag = response.Header("etag"); tag.has_value())
		{
			_serversTag = std::string(*tag);
		}
		else
		{
			_serversTag.reset();
		}
		Succeed(callback, std::move(*list));
	};
	Authorized(std::move(call));
}

void LobbyClientSystem::WatchServers(bool watch)
{
	WatchChannel(std::string(lobby_api::k_ServersChannel), watch);
}

void LobbyClientSystem::WatchChannel(std::string channel, bool watch)
{
	const auto found = std::ranges::find(_channels, channel);
	if ((found != _channels.end()) == watch)
	{
		return;
	}
	if (watch)
	{
		_channels.push_back(channel);
	}
	else
	{
		_channels.erase(found);
	}
	if (_push->IsOpen())
	{
		_push->Send(lobby_api::SubscribeFrame(watch, channel));
	}
}

void LobbyClientSystem::RequestJoinTicket(std::string serverId, game_socket::Role role,
                                          LobbyCallback<lobby_api::JoinTicket> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	const lobby_api::TicketRequest ticketRequest {.role = std::string(game_socket::RoleName(role))};
	Authorized(Call {
	    .method = HttpMethod::Post,
	    .path = fmt::format("/servers/{}/tickets", serverId),
	    .headers = {{"Content-Type", std::string(k_JsonType)}, {"Accept", std::string(k_JsonType)}},
	    .body = BytesOf(lobby_api::ToJson(ticketRequest)),
	    .done =
	        [callback = std::move(callback)](HttpResponse&& response) {
		        if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		        {
			        Fail(callback, std::move(error));
			        return;
		        }
		        auto ticket = lobby_api::ParseJoinTicket(TextOf(response));
		        if (!ticket.has_value())
		        {
			        Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			        return;
		        }
		        // The game socket is encrypted too
		        if (!online::http_policy::IsWssAddress(ticket->url))
		        {
			        Fail(callback, LobbyError {.kind = LobbyErrorKind::AddressRefused, .status = response.status});
			        return;
		        }
		        Succeed(callback, std::move(*ticket));
	        },
	});
}

void LobbyClientSystem::SendChatLine(std::string channel, std::string text, LobbyCallback<LobbyDone> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	// The lobby refuses an empty line and a line of more than 254 characters
	if (text.empty() || text.size() > lobby_api::k_MostChatCharacters)
	{
		Fail(callback, LobbyError {.kind = LobbyErrorKind::Refused, .status = 0});
		return;
	}
	Authorized(Call {
	    .method = HttpMethod::Post,
	    .path = fmt::format("/chat/channels/{}/messages", channel),
	    .headers = {{"Content-Type", std::string(k_JsonType)}, {"Accept", std::string(k_JsonType)}},
	    .body = BytesOf(lobby_api::ToJson(lobby_api::ChatLineRequest {.text = std::move(text)})),
	    .done =
	        [callback = std::move(callback)](HttpResponse&& response) {
		        if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		        {
			        Fail(callback, std::move(error));
			        return;
		        }
		        Succeed(callback, LobbyDone {});
	        },
	});
}

void LobbyClientSystem::UseClan(std::string serverId, std::optional<int64_t> clanId,
                                LobbyCallback<lobby_api::UsedClan> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	Authorized(Call {
	    .method = HttpMethod::Put,
	    .path = fmt::format("/servers/{}/clan", serverId),
	    .headers = {{"Content-Type", std::string(k_JsonType)}, {"Accept", std::string(k_JsonType)}},
	    .body = BytesOf(lobby_api::ToJson(lobby_api::UseClanRequest {.clan = clanId})),
	    .done =
	        [callback = std::move(callback)](HttpResponse&& response) {
		        if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		        {
			        Fail(callback, std::move(error));
			        return;
		        }
		        auto used = lobby_api::ParseUsedClan(TextOf(response));
		        if (!used.has_value())
		        {
			        Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			        return;
		        }
		        Succeed(callback, std::move(*used));
	        },
	});
}

void LobbyClientSystem::CheckAccountName(std::string name, LobbyCallback<lobby_api::AccountNameCheck> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	// Only letters and digits can be in a name, so the name needs no escape in the path
	if (!lobby_api::IsValidAccountName(name))
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::InvalidName));
		return;
	}
	online::HttpRequest request {
	    .method = HttpMethod::Get,
	    .url = Address(fmt::format("/account-names/{}", name)),
	    .headers = {{"Accept", std::string(k_JsonType)}},
	};
	_http.Send(std::move(request), [callback = std::move(callback)](HttpResponse&& response) {
		if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		{
			Fail(callback, std::move(error));
			return;
		}
		auto check = lobby_api::ParseAccountNameCheck(TextOf(response));
		if (!check.has_value())
		{
			Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			return;
		}
		Succeed(callback, std::move(*check));
	});
}

void LobbyClientSystem::GetMyClans(LobbyCallback<lobby_api::ClanList> callback)
{
	CallFor<lobby_api::ClanList>(HttpMethod::Get, "/me/clans", std::move(callback),
	                             [](const HttpResponse& response) { return lobby_api::ParseClanList(TextOf(response)); });
}

void LobbyClientSystem::GetClan(int64_t clanId, LobbyCallback<lobby_api::ClanDetails> callback)
{
	CallFor<lobby_api::ClanDetails>(HttpMethod::Get, fmt::format("/clans/{}", clanId), std::move(callback),
	                                [](const HttpResponse& response) { return lobby_api::ParseClanDetails(TextOf(response)); });
}

void LobbyClientSystem::GetClanLogo(int64_t clanId, LobbyCallback<std::vector<uint8_t>> callback)
{
	CallFor<std::vector<uint8_t>>(HttpMethod::Get, fmt::format("/clans/{}/logo", clanId), std::move(callback),
	                              [](const HttpResponse& response) -> std::optional<std::vector<uint8_t>> {
		                              // The logo is a BMP file, which starts with "BM"
		                              if (response.body.size() < 2 || response.body[0] != 'B' || response.body[1] != 'M')
		                              {
			                              return std::nullopt;
		                              }
		                              return response.body;
	                              });
}

void LobbyClientSystem::GetPlayerRanking(int32_t page, LobbyCallback<lobby_api::RankingPage> callback)
{
	CallFor<lobby_api::RankingPage>(HttpMethod::Get, fmt::format("/rankings/players?page={}", page), std::move(callback),
	                                [](const HttpResponse& response) { return lobby_api::ParseRankingPage(TextOf(response)); });
}

void LobbyClientSystem::GetClanRanking(int32_t page, LobbyCallback<lobby_api::RankingPage> callback)
{
	CallFor<lobby_api::RankingPage>(HttpMethod::Get, fmt::format("/rankings/clans?page={}", page), std::move(callback),
	                                [](const HttpResponse& response) { return lobby_api::ParseRankingPage(TextOf(response)); });
}

void LobbyClientSystem::GetPlayer(int64_t playerId, LobbyCallback<lobby_api::PlayerDetails> callback)
{
	CallFor<lobby_api::PlayerDetails>(
	    HttpMethod::Get, fmt::format("/players/{}", playerId), std::move(callback),
	    [](const HttpResponse& response) { return lobby_api::ParsePlayerDetails(TextOf(response)); });
}

void LobbyClientSystem::GetWeather(lobby_api::Date date, LobbyCallback<lobby_api::RealWeather> callback)
{
	CallFor<lobby_api::RealWeather>(HttpMethod::Get, fmt::format("/me/weather?date={}", lobby_api::DateText(date)),
	                                std::move(callback),
	                                [](const HttpResponse& response) { return lobby_api::ParseRealWeather(TextOf(response)); });
}

void LobbyClientSystem::UploadCreature(lobby_api::CreatureUpload upload, LobbyCallback<lobby_api::CreatureUploadReply> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	constexpr std::string_view k_Binary = "application/octet-stream";
	const std::array parts {
	    lobby_api::FormPart {.name = std::string(lobby_api::k_UploadPackageField),
	                         .fileName = std::string(lobby_api::k_UploadPackageFileName),
	                         .contentType = std::string(k_Binary),
	                         .bytes = std::move(upload.package)},
	    lobby_api::FormPart {.name = std::string(lobby_api::k_UploadMindField),
	                         .fileName = std::move(upload.mindFileName),
	                         .contentType = std::string(k_Binary),
	                         .bytes = std::move(upload.mind)},
	    lobby_api::FormPart {.name = std::string(lobby_api::k_UploadPhysiqueField),
	                         .fileName = std::move(upload.physiqueFileName),
	                         .contentType = std::string(k_Binary),
	                         .bytes = std::move(upload.physique)},
	};
	const auto seed = static_cast<uint64_t>(_clock().time_since_epoch().count());
	const auto boundary = lobby_api::MultipartBoundary(parts, seed);
	Authorized(Call {
	    .method = HttpMethod::Post,
	    .path = "/me/creatures",
	    .headers = {{"Content-Type", lobby_api::MultipartContentType(boundary)}, {"Accept", std::string(k_JsonType)}},
	    .body = lobby_api::MultipartBody(boundary, parts),
	    .done =
	        [callback = std::move(callback)](HttpResponse&& response) {
		        if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		        {
			        Fail(callback, std::move(error));
			        return;
		        }
		        auto reply = lobby_api::ParseCreatureUploadReply(TextOf(response));
		        if (!reply.has_value())
		        {
			        Fail(callback, LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status});
			        return;
		        }
		        Succeed(callback, std::move(*reply));
	        },
	});
}

void LobbyClientSystem::SendSkirmishPing(LobbyCallback<LobbyDone> callback)
{
	if (!_config.has_value())
	{
		Fail(callback, ErrorOfKind(LobbyErrorKind::NotConfigured));
		return;
	}
	Authorized(Call {
	    .method = HttpMethod::Post,
	    .path = "/me/skirmish-pings",
	    .headers = {{"Accept", std::string(k_JsonType)}},
	    .body = {},
	    .done =
	        [callback = std::move(callback)](HttpResponse&& response) {
		        if (auto error = LobbyErrorOf(response); error.kind != LobbyErrorKind::None)
		        {
			        Fail(callback, std::move(error));
			        return;
		        }
		        Succeed(callback, LobbyDone {});
	        },
	});
}

uint32_t LobbyClientSystem::AddEventListener(LobbyEventListener listener)
{
	const auto id = _nextListener++;
	_listeners.emplace_back(id, std::move(listener));
	return id;
}

void LobbyClientSystem::RemoveEventListener(uint32_t id)
{
	std::erase_if(_listeners, [id](const auto& entry) { return entry.first == id; });
}

void LobbyClientSystem::OpenPush()
{
	_pushWanted = true;
	if (_pushUnavailable || _push->IsOpen() || !_config.has_value())
	{
		return;
	}
	if (!AccessTokenValid())
	{
		// The handshake needs a new access token first
		Refresh([this](const LobbyError& error) {
			if (error.kind == LobbyErrorKind::None && _state == LobbySessionState::LoggedIn && AccessTokenValid())
			{
				OpenPush();
			}
			else if (_state == LobbySessionState::LoggedIn)
			{
				_reconnectAt = _clock() + _reconnectWait.Current();
			}
		});
		return;
	}
	std::string url;
	if (_config->client == lobby_api::ClientKind::Browser)
	{
		url = std::string(lobby_api::k_EventsPath);
	}
	else
	{
		// The origin is an https:// address, and the channel is at the same host over WSS
		constexpr std::string_view k_Https = "https://";
		url = fmt::format("wss://{}{}", std::string_view(_config->origin).substr(k_Https.size()), lobby_api::k_EventsPath);
	}
	const auto generation = _generation;
	_push->Open(std::move(url), _accessToken,
	            online::PushHandlers {
	                .opened =
	                    [this, generation]() {
		                    if (generation != _generation)
		                    {
			                    return;
		                    }
		                    // The lobby sends "hello" first; the client then asks for its channels
		                    _reconnectWait.Reset();
	                    },
	                .message =
	                    [this, generation](std::string_view text) {
		                    if (generation == _generation)
		                    {
			                    HandlePush(text);
		                    }
	                    },
	                .closed =
	                    [this, generation](online::PushClose reason) {
		                    if (generation == _generation)
		                    {
			                    PushClosed(reason);
		                    }
	                    },
	            });
}

void LobbyClientSystem::PushClosed(online::PushClose reason)
{
	switch (reason)
	{
	case online::PushClose::Closed:
		return;
	case online::PushClose::Unavailable:
		// This build has no transport for the channel. The REST calls still work.
		_pushUnavailable = true;
		return;
	case online::PushClose::SignedOut:
	case online::PushClose::Banned:
		// The lobby ended the session, or banned the account
		EndSession();
		return;
	case online::PushClose::Refused:
		// The access token is old: the next open gets a new one first
		_accessToken.clear();
		[[fallthrough]];
	case online::PushClose::Lost:
		if (_state == LobbySessionState::LoggedIn && _pushWanted)
		{
			_reconnectAt = _clock() + _reconnectWait.Next();
		}
		return;
	}
}

void LobbyClientSystem::HandleControl(const lobby_api::PushEvent& frame)
{
	if (frame.type == lobby_api::events::k_Hello)
	{
		const auto position = lobby_api::ParsePushPosition(frame.data);
		if (!position.has_value())
		{
			return;
		}
		if (_epoch.empty())
		{
			// The first connection: the client wants the events from now
			_epoch = position->epoch;
			_lastSeq = position->seq;
			for (const auto& channel : _channels)
			{
				_push->Send(lobby_api::SubscribeFrame(true, channel));
			}
			return;
		}
		// A connection after a loss: the lobby sends again the events after the last one that the client got, and
		// subscribes the channels again
		_push->Send(lobby_api::ResumeFrame(_epoch, _lastSeq, _channels));
		return;
	}
	if (frame.type == lobby_api::events::k_Resumed || frame.type == lobby_api::events::k_Resync)
	{
		const auto position = lobby_api::ParsePushPosition(frame.data);
		if (!position.has_value())
		{
			return;
		}
		_epoch = position->epoch;
		if (frame.type == lobby_api::events::k_Resync)
		{
			// The lobby could not send the events that the client missed: the client continues from now, and the
			// next server list comes whole
			_lastSeq = position->seq;
			_serversTag.reset();
		}
	}
}

void LobbyClientSystem::HandlePush(std::string_view text)
{
	const auto event = lobby_api::ParsePushEvent(text);
	if (!event.has_value())
	{
		return;
	}
	if (!event->seq.has_value())
	{
		HandleControl(*event);
	}
	else
	{
		// An event that the client got before comes again after a resume
		if (*event->seq <= _lastSeq)
		{
			return;
		}
		_lastSeq = *event->seq;
	}
	if (event->type == lobby_api::events::k_ServersChanged || event->type == lobby_api::events::k_ServersRemoved)
	{
		if (const auto change = lobby_api::ParseServerChange(event->type, event->data); change.has_value())
		{
			const auto found =
			    std::ranges::find_if(_servers, [&change](const auto& server) { return server.id == change->server.id; });
			if (change->removed)
			{
				if (found != _servers.end())
				{
					_servers.erase(found);
				}
			}
			else if (found != _servers.end())
			{
				*found = change->server;
			}
			else
			{
				_servers.push_back(change->server);
			}
		}
	}
	// The listeners get the events and the control frames. They can add or remove listeners, so they run from a copy.
	const auto listeners = _listeners;
	for (const auto& [id, listener] : listeners)
	{
		if (listener)
		{
			listener(*event);
		}
	}
	if (event->type == lobby_api::events::k_AccountBanned)
	{
		// The lobby ended every session of the account and closes the channel
		EndSession();
	}
}

} // namespace openblack::ecs::systems
