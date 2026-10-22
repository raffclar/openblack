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

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/TurnStream.h"
#include "Online/WebSocketInterface.h"
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
	if (_services.lan != nullptr && _flow.box == Box::GameList && _flow.kind == LobbyKind::Lan)
	{
		_services.lan->Poll();
	}
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
	case Message::WaitingForStart:
		// Cancel takes the tick from the Ready box
		SetReady(false);
		_message = Message::None;
		break;
	case Message::HostLeft:
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
	_selectedGame = game.has_value() && *game < GetGames().size() ? game : std::nullopt;
}

void MultiplayerFrontEnd::EnterGameList()
{
	_flow.box = Box::GameList;
	_selectedGame.reset();
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
		SelectGame(_selectedGame);
	});
}

void MultiplayerFrontEnd::Join(std::u16string password)
{
	if (_busy || _message != Message::None || !_selectedGame.has_value() || _services.sockets == nullptr)
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
	const auto generation = ++_generation;
	if (_flow.kind == LobbyKind::Lan)
	{
		_room = server;
		RoomOpened({}, std::move(password));
		return;
	}
	if (_services.lobby == nullptr)
	{
		_busy = false;
		Show(Message::CouldNotConnect);
		return;
	}
	_services.lobby->RequestJoinTicket(server.id, [this, generation, server, password = std::move(password)](
	                                                  ecs::systems::LobbyResult<lobby_api::JoinTicket>&& result) mutable {
		if (generation != _generation)
		{
			return;
		}
		if (!result.Ok())
		{
			_busy = false;
			Show(Message::CouldNotConnect);
			return;
		}
		_room = server;
		_room->url = result.value->url;
		RoomOpened(std::move(result.value->ticket), std::move(password));
	});
}

void MultiplayerFrontEnd::RoomOpened(std::string ticket, std::u16string password)
{
	_stream = std::make_unique<online::TurnStream>(
	    *_services.sockets, _services.clock ? _services.clock : [] { return uint64_t {0}; });
	online::WebSocketRequest request {.url = _settings.browser ? PathOf(_room->url) : _room->url};
	const auto generation = _generation;
	const auto lanName = _flow.kind == LobbyKind::Lan ? ToUtf8(_settings.profileName) : std::string();
	online::TurnStreamHandlers handlers {
	    .opened =
	        [this, generation, ticket = std::move(ticket), password = ToUtf8(password), lanName] {
		        if (generation != _generation || _stream == nullptr)
		        {
			        return;
		        }
		        _stream->SendText(lobby_api::RoomJoinFrame(ticket, password, lanName));
		        _busy = false;
		        _joined = true;
		        _flow.box = Box::Room;
		        _tab = RoomTab::Chat;
		        _chat.clear();
		        _ready = false;
		        UpdateRoom();
	        },
	    .closed =
	        [this, generation](const online::WebSocketClosed& /*closed*/) {
		        if (generation != _generation)
		        {
			        return;
		        }
		        _busy = false;
		        Show(_joined ? Message::HostLeft : Message::CouldNotConnect);
		        _joined = false;
	        },
	};
	_stream->Open(std::move(request), std::move(handlers));
}

void MultiplayerFrontEnd::UpdateRoom()
{
	_seats = Seats(_room->players);
	_shownSeat.reset();
	_shownClan.reset();
	_shownLogo.reset();
	_usedClan.reset();
	_myClans.clear();
	// The player's own clan in the room, from the server's list
	if (const auto* own = GetOwnSeat(); own != nullptr)
	{
		_usedClan = own->clan;
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
	if (_stream != nullptr)
	{
		_stream->Close();
		_stream.reset();
	}
	_room.reset();
	_joined = false;
	_ready = false;
	_seats.clear();
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
	if (line.empty() || !_joined || !_room.has_value())
	{
		return;
	}
	// The line comes back on the room's channel to every player, the sender too
	if (_flow.kind == LobbyKind::Internet && _services.lobby != nullptr)
	{
		_services.lobby->SendChatLine(ChannelOf(_room->id), ToUtf8(line),
		                              [](ecs::systems::LobbyResult<ecs::systems::LobbyDone>&&) {});
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
			_usedClan = own->clan;
		}
	}
}

std::optional<size_t> MultiplayerFrontEnd::GetServerLand() const
{
	return _room.has_value() ? FindLand(_maps, _room->land) : std::nullopt;
}

std::optional<int64_t> MultiplayerFrontEnd::GetUsedClan() const
{
	return _usedClan;
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
			    _usedClan = result.value->clan.has_value() ? std::optional(result.value->clan->id) : std::nullopt;
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
			    _usedClan = result.value->clan.has_value() ? std::optional(result.value->clan->id) : std::nullopt;
		    }
	    });
}

void MultiplayerFrontEnd::SetReady(bool ready)
{
	if (!_joined || _stream == nullptr)
	{
		return;
	}
	_ready = ready;
	_stream->SendText(lobby_api::RoomReadyFrame(ready));
	// The start of the game comes from the server, which does not make it yet: the box says that the player waits
	if (ready)
	{
		Show(Message::WaitingForStart);
	}
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
