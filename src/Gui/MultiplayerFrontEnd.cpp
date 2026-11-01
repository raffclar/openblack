/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MultiplayerFrontEnd.h"

#include <algorithm>
#include <utility>

#include <LobbyApiJson.h>
#include <spdlog/spdlog.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/MapDownload.h"
#include "Online/NetworkSession.h"
#include "Online/OnlineMapStoreInterface.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using namespace openblack::gui::multiplayer;
using openblack::ecs::systems::LobbyErrorKind;

namespace
{

/// The path of a wss:// address, for the browser, which opens a socket only on the page's own host
std::string PathOf(std::string_view url)
{
	const auto scheme = url.find("://");
	if (scheme == std::string_view::npos)
	{
		return std::string(url);
	}
	const auto path = url.find('/', scheme + 3);
	return path == std::string_view::npos ? std::string("/") : std::string(url.substr(path));
}

std::string ChannelOf(std::string_view serverId)
{
	return std::string(openblack::lobby_api::k_ServerChannelPrefix) + std::string(serverId);
}

} // namespace

MultiplayerFrontEnd::MultiplayerFrontEnd(Services services, Settings settings)
    : _services(std::move(services))
    , _settings(std::move(settings))
    , _maps(ParseLocalMaps(_settings.localMaps))
{
	if (_services.lobby != nullptr)
	{
		_listener = _services.lobby->AddEventListener([this](const lobby_api::PushEvent& event) { HandleEvent(event); });
	}
	// The default connection is the internet one, else the local network
	const auto connections = GetConnections();
	if (!connections.empty())
	{
		_connection = connections.size() - 1;
	}
}

MultiplayerFrontEnd::~MultiplayerFrontEnd()
{
	LeaveRoom();
	if (_services.lobby != nullptr)
	{
		_services.lobby->WatchServers(false);
		if (_listener.has_value())
		{
			_services.lobby->RemoveEventListener(*_listener);
		}
	}
	if (_services.lan != nullptr)
	{
		_services.lan->Stop();
	}
}

void MultiplayerFrontEnd::Update()
{
	if (_services.poll)
	{
		_services.poll(*this);
	}
	const bool inLanRoom = _room.has_value() && (_flow.box == Box::Room || _flow.box == Box::Conditions);
	if (_services.lan != nullptr && _flow.kind == LobbyKind::Lan && (_flow.box == Box::GameList || inLanRoom))
	{
		_services.lan->Poll();
		if (inLanRoom)
		{
			FollowLanRoom();
		}
	}
	FollowSession();
	FollowLanChat();
	FollowStart();
	FollowMapDownload();
	if (_flow.box == Box::GameList)
	{
		FollowSelectedGame();
	}
	// The lobby ended the session, for example after a ban: the game list and the room need it
	if (_loggedIn && _flow.kind == LobbyKind::Internet && _services.lobby != nullptr &&
	    _services.lobby->GetState() == ecs::systems::LobbySessionState::LoggedOut && !IsStarted())
	{
		_loggedIn = false;
		if (_flow.box == Box::GameList || _flow.box == Box::Room || _flow.box == Box::Conditions)
		{
			++_generation;
			_busy = false;
			LeaveRoom();
			_services.lobby->WatchServers(false);
			_flow.box = Box::Login;
			_message = Message::ConnectionFailed;
		}
	}
}

const openblack::online::NetworkSession* MultiplayerFrontEnd::Session() const
{
	return _services.network != nullptr ? _services.network->Session() : nullptr;
}

std::optional<uint64_t> MultiplayerFrontEnd::GetRoundTrip() const
{
	const auto* session = Session();
	return _joined && session != nullptr ? session->RoundTrip() : std::nullopt;
}

bool MultiplayerFrontEnd::IsStarted() const
{
	const auto* session = Session();
	return _joined && session != nullptr && session->GetStart().has_value();
}

void MultiplayerFrontEnd::FollowSession()
{
	if (!_joining)
	{
		return;
	}
	const auto* session = Session();
	if (session == nullptr)
	{
		return;
	}
	using State = online::NetworkSession::State;
	const auto state = session->GetState();
	if (!_joined && session->GetJoined().has_value() && state != State::Ended)
	{
		// The server let the player in: the room opens
		_busy = false;
		_joined = true;
		_observer = session->GetJoined()->role == game_socket::Role::Observer;
		TakeHostPicks(session->GetJoined()->start);
		_flow.box = Box::Room;
		_tab = RoomTab::Chat;
		_chat.clear();
		_ready = false;
		UpdateRoom();
		return;
	}
	if (state != State::Ended || IsStarted())
	{
		return;
	}
	_joining = false;
	_busy = false;
	if (_joined)
	{
		// The server removed the player, the room's server went, or the player could not come back to it
		const bool kicked = session->GetEnd() == online::NetworkSession::End::Refused && session->RefusedReason() == "kicked";
		_joined = false;
		if (kicked)
		{
			ShowKicked();
			return;
		}
		Show(Message::HostLeft);
		return;
	}
	const auto refusal = session->GetEnd() == online::NetworkSession::End::Refused ? session->RefusedReason() : std::string();
	Show(JoinFailureMessage(refusal, _flow.kind));
}

void MultiplayerFrontEnd::FollowStart()
{
	if (_services.network == nullptr)
	{
		return;
	}
	while (const auto event = _services.network->TakeStartEvent())
	{
		if (!_joined || IsStarted())
		{
			continue;
		}
		// The boxes of the start have no button: they go when the next step comes
		const bool waiting = _message == Message::WaitingForTransfer || _message == Message::SendingStartData;
		switch (*event)
		{
		case online::StartEvent::HasLand:
			Show(Message::WaitingForTransfer);
			break;
		case online::StartEvent::LacksLand:
			// The server took the player's Ready
			_ready = false;
			Show(Message::StillDownloading);
			break;
		case online::StartEvent::Transfer:
			Show(Message::SendingStartData);
			break;
		case online::StartEvent::Cancelled:
			// A player who does not have the land keeps its own box
			if (waiting)
			{
				Show(Message::OthersDownloading);
			}
			break;
		case online::StartEvent::Stopped:
			// A player left during the start: the boxes of the start go, with no message
			if (waiting)
			{
				_message = Message::None;
			}
			break;
		}
	}
}

void MultiplayerFrontEnd::ShowKicked()
{
	_messageName = _room.has_value() ? ToUtf16(_room->hostname) : std::u16string();
	Show(Message::KickedFromGame);
}

bool MultiplayerFrontEnd::TakeLeft()
{
	return std::exchange(_left, false);
}

bool MultiplayerFrontEnd::HasLanConnection() const
{
	return _services.lan != nullptr && _services.lan->IsAvailable();
}

bool MultiplayerFrontEnd::HasInternetConnection() const
{
	return _services.lobby != nullptr && _services.lobby->IsConfigured();
}

std::vector<LobbyKind> MultiplayerFrontEnd::GetConnections() const
{
	std::vector<LobbyKind> kinds;
	if (HasLanConnection())
	{
		kinds.push_back(LobbyKind::Lan);
	}
	if (HasInternetConnection())
	{
		kinds.push_back(LobbyKind::Internet);
	}
	return kinds;
}

void MultiplayerFrontEnd::SelectConnection(std::optional<size_t> line)
{
	_connection = line.has_value() && *line < GetConnections().size() ? line : std::nullopt;
}

void MultiplayerFrontEnd::Next()
{
	if (_busy || _message != Message::None)
	{
		return;
	}
	switch (_flow.box)
	{
	case Box::ChooseConnection:
		if (GetConnections().empty())
		{
			Show(Message::NoConnections);
			return;
		}
		if (!_connection.has_value())
		{
			return;
		}
		multiplayer::Next(_flow);
		break;
	case Box::FindGames:
		multiplayer::Next(_flow);
		if (_flow.box == Box::GameList)
		{
			EnterGameList();
		}
		break;
	case Box::Room:
		multiplayer::Next(_flow);
		break;
	case Box::Login:
	case Box::GameList:
	case Box::Conditions:
		break;
	}
}

void MultiplayerFrontEnd::Back()
{
	if (_message != Message::None)
	{
		return;
	}
	// A call in progress is dropped: its answer comes to a box that moved on
	++_generation;
	_busy = false;
	switch (_flow.box)
	{
	case Box::GameList:
		if (_flow.kind == LobbyKind::Internet && _services.lobby != nullptr)
		{
			_services.lobby->WatchServers(false);
		}
		if (_flow.kind == LobbyKind::Lan && _services.lan != nullptr)
		{
			_services.lan->Stop();
		}
		multiplayer::Back(_flow);
		break;
	case Box::Room:
	case Box::Conditions:
		LeaveRoom();
		multiplayer::Back(_flow);
		EnterGameList();
		break;
	default:
		multiplayer::Back(_flow);
		_left = _flow.left;
		break;
	}
}

void MultiplayerFrontEnd::AnswerMessage(Answer answer)
{
	const auto message = std::exchange(_message, Message::None);
	switch (message)
	{
	case Message::NotRegistered:
		// Yes opens the registration page of the lobby's site
		if (answer == Answer::Yes && _services.openPage && !_settings.registerPage.empty())
		{
			_services.openPage(_settings.registerPage);
		}
		break;
	case Message::EmailRequired:
		// Yes opens the account page of the lobby's site, where the player adds an email address. The player then logs
		// in again.
		if (answer == Answer::Yes && _services.openPage && !_settings.accountPage.empty())
		{
			_services.openPage(_settings.accountPage);
		}
		break;
	case Message::HostLeft:
	case Message::KickedFromGame:
		LeaveRoom();
		_flow.box = Box::GameList;
		EnterGameList();
		break;
	default:
		break;
	}
}

void MultiplayerFrontEnd::Login(std::u16string name, std::u16string password)
{
	if (_busy || _message != Message::None)
	{
		return;
	}
	_loginName = std::move(name);
	switch (CheckLogin(_loginName, password))
	{
	case LoginProblem::InvalidDetails:
		Show(Message::InvalidDetails);
		return;
	case LoginProblem::IncorrectPassword:
		Show(Message::IncorrectPassword);
		return;
	case LoginProblem::None:
		break;
	}
	if (_services.lobby == nullptr)
	{
		Show(Message::IncorrectPassword);
		return;
	}
	_busy = true;
	const auto generation = ++_generation;
	auto nameUtf8 = ToUtf8(_loginName);
	_services.lobby->Login(
	    nameUtf8, ToUtf8(password), [this, generation, nameUtf8](ecs::systems::LobbyResult<lobby_api::AccountRef>&& result) {
		    if (generation != _generation)
		    {
			    return;
		    }
		    if (result.Ok())
		    {
			    _busy = false;
			    // The lobby refuses the server list until the account has an email address
			    if (_services.lobby->IsEmailRequired())
			    {
				    Show(Message::EmailRequired);
				    return;
			    }
			    _loggedIn = true;
			    EnterGameList();
			    return;
		    }
		    const auto kind = result.error.kind;
		    if (!AsksAboutName(kind))
		    {
			    _busy = false;
			    Show(LoginFailureMessage(kind, std::nullopt));
			    return;
		    }
		    // The lobby gives one answer for an unknown name and a wrong password: a name that is free
		    // has no account
		    _services.lobby->CheckAccountName(
		        nameUtf8, [this, generation, kind](ecs::systems::LobbyResult<lobby_api::AccountNameCheck>&& check) {
			        if (generation != _generation)
			        {
				        return;
			        }
			        _busy = false;
			        const auto available = check.Ok() ? std::optional(check.value->available) : std::nullopt;
			        Show(LoginFailureMessage(kind, available));
		        });
	    });
}

std::span<const openblack::lobby_api::ServerEntry> MultiplayerFrontEnd::GetGames() const
{
	if (_flow.kind == LobbyKind::Lan)
	{
		return _services.lan != nullptr ? _services.lan->GetServers() : std::span<const lobby_api::ServerEntry> {};
	}
	return _services.lobby != nullptr ? _services.lobby->GetKnownServers() : std::span<const lobby_api::ServerEntry> {};
}

std::vector<size_t> MultiplayerFrontEnd::GetShownGames() const
{
	return FilteredGames(GetGames(), _filter, !_selectedGame.has_value());
}

void MultiplayerFrontEnd::SelectGame(std::optional<size_t> game)
{
	const auto games = GetGames();
	_selectedGame = game.has_value() && *game < games.size() ? game : std::nullopt;
	_selectedId = _selectedGame.has_value() ? games[*_selectedGame].id : std::string();
}

void MultiplayerFrontEnd::FollowSelectedGame()
{
	if (!_selectedGame.has_value() || _selectedId.empty())
	{
		return;
	}
	const auto games = GetGames();
	if (*_selectedGame < games.size() && games[*_selectedGame].id == _selectedId)
	{
		return;
	}
	const auto found = std::ranges::find(games, _selectedId, &lobby_api::ServerEntry::id);
	if (found == games.end())
	{
		_selectedGame.reset();
		_selectedId.clear();
		return;
	}
	_selectedGame = static_cast<size_t>(found - games.begin());
}

void MultiplayerFrontEnd::FollowLanRoom()
{
	const auto games = _services.lan->GetServers();
	const auto found = std::ranges::find(games, _room->id, &lobby_api::ServerEntry::id);
	if (found == games.end())
	{
		return;
	}
	const auto same = [](const lobby_api::RoomPlayer& a, const lobby_api::RoomPlayer& b) {
		return a.account.name == b.account.name && a.role == b.role;
	};
	if (std::ranges::equal(found->players, _room->players, same) && found->gamemode == _room->gamemode)
	{
		return;
	}
	_room->players = found->players;
	_room->numplayers = found->numplayers;
	_room->gamemode = found->gamemode;
	_seats = Seats(_room->players);
}

void MultiplayerFrontEnd::FollowLanChat()
{
	if (_flow.kind != LobbyKind::Lan || _services.network == nullptr)
	{
		return;
	}
	// The game server of the room sends the lines of the other players
	auto lines = _services.network->TakeRoomChat();
	if (!_joined || !_room.has_value())
	{
		return;
	}
	const auto ownName = ToUtf8(_settings.profileName);
	for (const auto& line : lines)
	{
		const auto seat = std::ranges::find(_seats, line.from, [](const Seat& s) { return s.account.name; });
		const auto god = seat != _seats.end() ? seat->god : 0;
		_chat.push_back(ChatLine(ToUtf16(line.from), ClampChat(ToUtf16(line.text)), line.from == ownName, god));
		// The log keeps who spoke, never the words
		if (auto logger = spdlog::get("game"))
		{
			logger->info("A chat line from {} came on the room's game server", line.from);
		}
	}
}

void MultiplayerFrontEnd::EnterGameList()
{
	_flow.box = Box::GameList;
	_selectedGame.reset();
	_selectedId.clear();
	_filter.clear();
	if (_flow.kind == LobbyKind::Lan)
	{
		if (_services.lan != nullptr)
		{
			_services.lan->Start();
		}
		return;
	}
	RefreshGames();
}

void MultiplayerFrontEnd::RefreshGames()
{
	if (_flow.kind == LobbyKind::Lan)
	{
		if (_services.lan != nullptr)
		{
			_services.lan->Start();
		}
		return;
	}
	if (_services.lobby == nullptr)
	{
		return;
	}
	_services.lobby->WatchServers(true);
	_busy = true;
	const auto generation = ++_generation;
	_services.lobby->GetServers([this, generation](ecs::systems::LobbyResult<lobby_api::ServerList>&& result) {
		if (generation != _generation)
		{
			return;
		}
		_busy = false;
		if (!result.Ok())
		{
			Show(Message::ConnectionFailed);
		}
		// A list that shrank loses the pick of a game that went
		FollowSelectedGame();
	});
	// The names of the lobby's lands go in the list of the lands, for the lands of the games
	GetLobbyMaps({});
}

void MultiplayerFrontEnd::GetLobbyMaps(std::function<void(bool ok)> then)
{
	if (_flow.kind != LobbyKind::Internet || _services.lobby == nullptr)
	{
		if (then)
		{
			then(false);
		}
		return;
	}
	_services.lobby->GetMaps([this, then = std::move(then)](ecs::systems::LobbyResult<lobby_api::MapList>&& result) {
		if (result.Ok())
		{
			_lobbyMaps = std::move(result.value->maps);
			for (const auto& map : _lobbyMaps)
			{
				if (!FindLand(_maps, map.file).has_value())
				{
					_maps.push_back(LocalMap {.name = ToUtf16(map.name), .file = map.file, .players = map.players});
				}
			}
		}
		if (then)
		{
			then(result.Ok());
		}
	});
}

void MultiplayerFrontEnd::FetchRoomMap(std::string sha256)
{
	_roomMapHash = sha256;
	_mapFailed = false;
	if (_mapDownload != nullptr)
	{
		_mapDownload->Stop();
	}
	if (_services.maps == nullptr || _services.lobby == nullptr)
	{
		return;
	}
	GetLobbyMaps([this, sha256 = std::move(sha256)](bool ok) {
		// The player left the room, or the room has a different land now
		if (_roomMapHash != sha256)
		{
			return;
		}
		const auto map = ok ? online::MapDownload::MapOfRoom(_lobbyMaps, sha256) : std::nullopt;
		if (!map.has_value())
		{
			_mapFailed = true;
			return;
		}
		if (_mapDownload == nullptr)
		{
			auto* lobby = _services.lobby;
			_mapDownload = std::make_unique<online::MapDownload>(*_services.maps, [lobby](const lobby_api::OnlineMap& wanted,
			                                                                              auto done) {
				lobby->DownloadMap(wanted, [done = std::move(done)](ecs::systems::LobbyResult<std::vector<uint8_t>>&& bytes) {
					done(std::move(bytes.value));
				});
			});
		}
		_mapDownload->Start(*map);
	});
}

void MultiplayerFrontEnd::FollowMapDownload()
{
	if (_flow.kind != LobbyKind::Internet || !_room.has_value() || !_joining || IsStarted())
	{
		return;
	}
	// The room's game server gives the lobby its land after it opens, so the game list can have a newer hash
	const auto games = GetGames();
	const auto current = std::ranges::find(games, _room->id, &lobby_api::ServerEntry::id);
	const auto& hash = current != games.end() ? current->map : _room->map;
	if (!hash.empty() && hash != _roomMapHash)
	{
		FetchRoomMap(hash);
	}
	const bool failed =
	    _mapFailed || (_mapDownload != nullptr && _mapDownload->GetState() == online::MapDownload::State::Failed);
	if (failed && _flow.box == Box::Room && _message == Message::None)
	{
		_mapFailed = false;
		if (_mapDownload != nullptr)
		{
			_mapDownload->Stop();
		}
		Show(Message::CouldNotDownloadMap);
	}
}

void MultiplayerFrontEnd::Join(std::u16string password)
{
	if (_busy || _message != Message::None || !_selectedGame.has_value() || _services.network == nullptr)
	{
		return;
	}
	const auto games = GetGames();
	if (*_selectedGame >= games.size())
	{
		return;
	}
	const auto server = games[*_selectedGame];
	_busy = true;
	++_generation;
	if (_flow.kind == LobbyKind::Lan)
	{
		_room = server;
		RoomOpened({}, std::move(password));
		return;
	}
	if (_services.lobby == nullptr)
	{
		_busy = false;
		Show(Message::ConnectionFailed);
		return;
	}
	RequestTicket(server, game_socket::Role::God, std::move(password));
}

void MultiplayerFrontEnd::RequestTicket(const lobby_api::ServerEntry& server, game_socket::Role role, std::u16string password)
{
	const auto generation = _generation;
	_services.lobby->RequestJoinTicket(
	    server.id, role,
	    [this, generation, server, role,
	     password = std::move(password)](ecs::systems::LobbyResult<lobby_api::JoinTicket>&& result) mutable {
		    if (generation != _generation)
		    {
			    return;
		    }
		    if (!result.Ok())
		    {
			    // The gods fill the room, or the game started and the account has no god in it: the player joins as an
			    // observer (MP-Q12)
			    if (role == game_socket::Role::God && (result.error.code == lobby_api::problems::k_ServerFull ||
			                                           result.error.code == lobby_api::problems::k_GameStarted))
			    {
				    RequestTicket(server, game_socket::Role::Observer, std::move(password));
				    return;
			    }
			    _busy = false;
			    Show(TicketFailureMessage(result.error.kind, result.error.code));
			    return;
		    }
		    _room = server;
		    _room->url = result.value->url;
		    RoomOpened(std::move(result.value->ticket), std::move(password));
	    });
}

void MultiplayerFrontEnd::RoomOpened(std::string ticket, std::u16string password)
{
	const auto lanName = _flow.kind == LobbyKind::Lan ? ToUtf8(_settings.profileName) : std::string();
	_joined = false;
	_observer = false;
	// The browser opens a game socket only on the page's own host
	_joining = _services.network->Join(ecs::systems::NetworkJoin {
	    .url = _settings.browser ? PathOf(_room->url) : _room->url,
	    .ticket = std::move(ticket),
	    .gamePassword = ToUtf8(password),
	    .name = lanName,
	    .testAuthorities = std::nullopt,
	    .fromCommandLine = false,
	    .partAt = std::nullopt,
	    .partAgain = false,
	    .testCreatureFile = std::nullopt,
	    .origin = _flow.kind == LobbyKind::Lan ? online::GameOrigin::Lan : online::GameOrigin::InternetLobby,
	});
	if (!_joining)
	{
		_busy = false;
		Show(JoinFailureMessage({}, _flow.kind));
	}
}

void MultiplayerFrontEnd::TakeHostPicks(const std::optional<game_socket::Start>& start)
{
	_timeLimit = multiplayer::k_DefaultTimeLimitMinutes;
	_conditionPicks.clear();
	if (!start.has_value())
	{
		return;
	}
	_timeLimit = start->timeLimit.value_or(multiplayer::k_DefaultTimeLimitMinutes);
	for (const auto& each : start->conditions)
	{
		_conditionPicks.push_back(
		    {.condition = static_cast<game_end::Condition>(each.condition), .value = static_cast<int32_t>(each.value)});
	}
}

void MultiplayerFrontEnd::UpdateRoom()
{
	_seats = Seats(_room->players);
	_shownSeat.reset();
	// The lines of a player who left the room go. The game asks again when that player comes back.
	std::erase_if(_details, [this](const auto& entry) {
		return std::ranges::none_of(_seats, [&entry](const Seat& seat) { return seat.account.id == entry.first; });
	});
	_shownClan.reset();
	_shownLogo.reset();
	KeepUsedClan(std::nullopt);
	_myClans.clear();
	// The player's own clan in the room, from the server's list
	if (const auto* own = GetOwnSeat(); own != nullptr)
	{
		KeepUsedClan(own->clan);
	}
	const auto land = GetServerLand();
	_shownLand = land;
	_conditions = land.has_value() ? FindConditions(_settings.conditionTemplate, _maps.at(*land).file) : std::nullopt;
	if (_flow.kind != LobbyKind::Internet || _services.lobby == nullptr)
	{
		return;
	}
	_services.lobby->WatchChannel(ChannelOf(_room->id), true);
	const auto generation = _generation;
	_services.lobby->GetMyClans([this, generation](ecs::systems::LobbyResult<lobby_api::ClanList>&& result) {
		if (generation == _generation && result.Ok())
		{
			_myClans = std::move(result.value->clans);
		}
	});
}

void MultiplayerFrontEnd::LeaveRoom()
{
	if (_room.has_value() && _flow.kind == LobbyKind::Internet && _services.lobby != nullptr)
	{
		_services.lobby->WatchChannel(ChannelOf(_room->id), false);
	}
	// After the start the network game keeps the session for the game
	if (_joining && !IsStarted() && _services.network != nullptr)
	{
		_services.network->Leave();
	}
	_joining = false;
	_room.reset();
	_roomMapHash.clear();
	_mapFailed = false;
	if (_mapDownload != nullptr)
	{
		_mapDownload->Stop();
	}
	_joined = false;
	_observer = false;
	_ready = false;
	_seats.clear();
	_details.clear();
	_chat.clear();
}

const openblack::lobby_api::ServerEntry* MultiplayerFrontEnd::GetRoomServer() const
{
	return _room.has_value() ? &*_room : nullptr;
}

const Seat* MultiplayerFrontEnd::GetOwnSeat() const
{
	if (_flow.kind == LobbyKind::Lan)
	{
		const auto name = ToUtf8(_settings.profileName);
		const auto found = std::ranges::find(_seats, name, [](const Seat& seat) { return seat.account.name; });
		return found != _seats.end() ? &*found : nullptr;
	}
	if (_services.lobby == nullptr)
	{
		return nullptr;
	}
	const auto account = _services.lobby->GetAccount();
	if (!account.has_value())
	{
		return nullptr;
	}
	const auto found = std::ranges::find(_seats, account->id, [](const Seat& seat) { return seat.account.id; });
	return found != _seats.end() ? &*found : nullptr;
}

std::u16string MultiplayerFrontEnd::GetPlayerName() const
{
	if (_flow.kind == LobbyKind::Internet && _services.lobby != nullptr)
	{
		if (const auto account = _services.lobby->GetAccount(); account.has_value())
		{
			return ToUtf16(account->name);
		}
		return _loginName;
	}
	return _settings.profileName;
}

void MultiplayerFrontEnd::SendChat(std::u16string_view text)
{
	const auto line = ClampChat(text);
	// The game sends no line of only spaces
	if (!HasChatText(line) || !_joined || !_room.has_value())
	{
		return;
	}
	// On the internet the line comes back on the room's channel of the lobby to every player, the sender too
	if (_flow.kind == LobbyKind::Internet && _services.lobby != nullptr)
	{
		_services.lobby->SendChatLine(ChannelOf(_room->id), ToUtf8(line),
		                              [](ecs::systems::LobbyResult<ecs::systems::LobbyDone>&&) {});
	}
	// On the local network the game shows its own line at once, and the room's game server sends it to the other
	// players, as the host's lobby does
	else if (_flow.kind == LobbyKind::Lan && _services.network != nullptr)
	{
		const auto* own = GetOwnSeat();
		_chat.push_back(ChatLine(_settings.profileName, line, true, own != nullptr ? own->god : 0));
		_services.network->SendRoomChat(ToUtf8(line));
	}
}

void MultiplayerFrontEnd::HandleEvent(const lobby_api::PushEvent& event)
{
	if (!_room.has_value())
	{
		return;
	}
	const auto channel = ChannelOf(_room->id);
	if (event.type == lobby_api::events::k_ChatLine)
	{
		const auto line = lobby_api::ParseChatLine(event.data);
		if (!line.has_value() || line->channel != channel)
		{
			return;
		}
		const auto account = _services.lobby != nullptr ? _services.lobby->GetAccount() : std::nullopt;
		const bool own = account.has_value() && account->id == line->from.id;
		const auto seat = std::ranges::find(_seats, line->from.id, [](const Seat& s) { return s.account.id; });
		const auto god = seat != _seats.end() ? seat->god : 0;
		_chat.push_back(ChatLine(ToUtf16(line->from.name), ClampChat(ToUtf16(line->text)), own, god));
		// The log keeps who spoke, never the words
		if (auto logger = spdlog::get("game"))
		{
			logger->info("A chat line from {} came on the room's channel", line->from.name);
		}
		return;
	}
	if (event.type == lobby_api::events::k_RoomChanged)
	{
		const auto change = lobby_api::ParseRoomChange(event.data);
		if (!change.has_value() || change->server != _room->id)
		{
			return;
		}
		_room->players = change->players;
		_room->numplayers =
		    static_cast<int32_t>(std::ranges::count(change->players, std::string("god"), &lobby_api::RoomPlayer::role));
		if (!change->gamemode.empty())
		{
			_room->gamemode = change->gamemode;
		}
		_seats = Seats(_room->players);
		return;
	}
	if (event.type == lobby_api::events::k_ServerBanned)
	{
		// A ban of the room's server takes the player out of the room before the start, as a kick from the game does
		const auto ban = lobby_api::ParseServerBanned(event.data);
		if (!ban.has_value() || ban->server != _room->id || IsStarted())
		{
			return;
		}
		++_generation;
		_busy = false;
		ShowKicked();
		LeaveRoom();
		return;
	}
	if (event.type == lobby_api::events::k_RoomClanChosen)
	{
		const auto choice = lobby_api::ParseRoomClanChoice(event.data);
		if (!choice.has_value() || choice->server != _room->id)
		{
			return;
		}
		for (auto& seat : _seats)
		{
			if (seat.account.id == choice->account.id)
			{
				seat.clan = choice->clan.has_value() ? std::optional(choice->clan->id) : std::nullopt;
			}
		}
		if (const auto* own = GetOwnSeat(); own != nullptr && own->account.id == choice->account.id)
		{
			KeepUsedClan(own->clan);
		}
	}
}

std::optional<size_t> MultiplayerFrontEnd::GetServerLand() const
{
	return _room.has_value() ? FindLand(_maps, _room->land) : std::nullopt;
}

void MultiplayerFrontEnd::ShowSeat(std::optional<size_t> seat)
{
	_shownSeat = seat.has_value() && *seat < _seats.size() ? seat : std::nullopt;
	// The game asks for the lines of a player when the player picks that player on the Player Details tab, one time
	if (!_shownSeat.has_value() || _tab != RoomTab::PlayerDetails || _flow.kind != LobbyKind::Internet ||
	    _services.lobby == nullptr || !_room.has_value())
	{
		return;
	}
	const auto account = _seats[*_shownSeat].account.id;
	if (_details.contains(account))
	{
		return;
	}
	_details.emplace(account, std::vector<PlayerDetail> {});
	_services.lobby->GetPlayerDetailLines(
	    account, _settings.gameVersion,
	    [this, account, room = _room->id](ecs::systems::LobbyResult<lobby_api::PlayerDetailLines>&& result) {
		    // An answer that comes after the player left the room has no place to go. A failed request keeps no lines.
		    if (!_room.has_value() || _room->id != room || !result.Ok())
		    {
			    return;
		    }
		    std::vector<PlayerDetail> lines;
		    lines.reserve(result.value->lines.size());
		    for (const auto& line : result.value->lines)
		    {
			    lines.push_back({.label = ToUtf16(line.label), .value = ToUtf16(line.value)});
		    }
		    _details[account] = std::move(lines);
	    });
}

std::vector<PlayerDetail> MultiplayerFrontEnd::GetShownDetails() const
{
	const auto* seat = _shownSeat.has_value() && *_shownSeat < _seats.size() ? &_seats[*_shownSeat] : GetOwnSeat();
	if (seat == nullptr)
	{
		return {};
	}
	const auto found = _details.find(seat->account.id);
	return found != _details.end() ? found->second : std::vector<PlayerDetail> {};
}

std::optional<int64_t> MultiplayerFrontEnd::GetUsedClan() const
{
	return _usedClan;
}

void MultiplayerFrontEnd::KeepUsedClan(std::optional<int64_t> clan)
{
	_usedClan = clan;
	// At the start, the network game tells whether the god of this computer plays for a clan
	if (_services.network != nullptr)
	{
		_services.network->SetPlaysForClan(clan.has_value());
	}
}

void MultiplayerFrontEnd::ShowClan(std::optional<size_t> clan)
{
	_shownClan.reset();
	_shownLogo.reset();
	_logoChanged = true;
	if (!clan.has_value() || *clan >= _myClans.size() || _services.lobby == nullptr)
	{
		return;
	}
	const auto id = _myClans[*clan].id;
	const auto generation = _generation;
	_services.lobby->GetClan(id, [this, generation](ecs::systems::LobbyResult<lobby_api::ClanDetails>&& result) {
		if (generation == _generation && result.Ok())
		{
			_shownClan = std::move(*result.value);
		}
	});
	_services.lobby->GetClanLogo(id, [this, generation, id](ecs::systems::LobbyResult<std::vector<uint8_t>>&& result) {
		if (generation != _generation || !result.Ok() || (_shownClan.has_value() && _shownClan->id != id))
		{
			return;
		}
		_shownLogo = DecodeClanLogo(*result.value);
		_logoChanged = true;
	});
}

bool MultiplayerFrontEnd::TakeLogoChanged()
{
	return std::exchange(_logoChanged, false);
}

void MultiplayerFrontEnd::UseShownClan()
{
	if (!_room.has_value() || !_shownClan.has_value() || _services.lobby == nullptr)
	{
		return;
	}
	const auto generation = _generation;
	_services.lobby->UseClan(
	    _room->id, _shownClan->id, [this, generation](ecs::systems::LobbyResult<lobby_api::UsedClan>&& result) {
		    if (generation == _generation && result.Ok())
		    {
			    KeepUsedClan(result.value->clan.has_value() ? std::optional(result.value->clan->id) : std::nullopt);
		    }
	    });
}

void MultiplayerFrontEnd::UseNoClan()
{
	if (!_room.has_value() || _services.lobby == nullptr)
	{
		return;
	}
	const auto generation = _generation;
	_services.lobby->UseClan(
	    _room->id, std::nullopt, [this, generation](ecs::systems::LobbyResult<lobby_api::UsedClan>&& result) {
		    if (generation == _generation && result.Ok())
		    {
			    KeepUsedClan(result.value->clan.has_value() ? std::optional(result.value->clan->id) : std::nullopt);
		    }
	    });
}

void MultiplayerFrontEnd::SetReady(bool ready)
{
	if (!_joined || _services.network == nullptr)
	{
		return;
	}
	_ready = ready;
	_services.network->SetReady(ready);
	// The server starts the game when its gods are ready. The room shows no box while the player waits, and the tick
	// of the Ready box takes the Ready away.
}

void MultiplayerFrontEnd::ShowBox(Box box, LobbyKind kind)
{
	LeaveRoom();
	_message = Message::None;
	_busy = false;
	++_generation;
	_flow = {.box = box, .kind = kind};
	if (box == Box::GameList)
	{
		EnterGameList();
	}
}
