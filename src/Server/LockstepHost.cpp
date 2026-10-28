/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LockstepHost.h"

#include <algorithm>
#include <limits>
#include <ranges>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Online/StartRules.h"
#include "Server/LobbyBans.h"

namespace openblack::server
{
namespace
{

/// The server writes a line of each god's compares to the log after this many compares
constexpr uint32_t k_CompareLogEvery = 100;
/// The clients' checksums that wait for the server's own, at most, for each god
constexpr size_t k_MostEarlyChecksums = 64;

void Log(spdlog::level::level_enum level, std::string_view text)
{
	if (auto logger = spdlog::get("game"))
	{
		logger->log(level, "{}", text);
	}
}

} // namespace

uint8_t GodSlotsOfLand(std::span<const bool> slotHasCitadel) noexcept
{
	const auto reversed = slotHasCitadel | std::views::reverse;
	const auto last = std::ranges::find(reversed, true);
	if (last == reversed.end())
	{
		return k_DefaultGodSlots;
	}
	const auto slots = static_cast<size_t>(std::ranges::distance(last, reversed.end()));
	return static_cast<uint8_t>(std::min<size_t>(slots, std::numeric_limits<uint8_t>::max()));
}

bool RoomStarts(size_t godsInRoom, size_t readyGods, uint32_t minGods) noexcept
{
	return godsInRoom > 0 && readyGods == godsInRoom && readyGods >= minGods;
}

LockstepHost::LockstepHost(WebSocketServerInterface& sockets, CheckTicket checkTicket, Clock clock, MakeSecret makeSecret,
                           HostSettings settings)
    : _sockets(sockets)
    , _checkTicket(std::move(checkTicket))
    , _clock(std::move(clock))
    , _makeSecret(std::move(makeSecret))
    , _settings(std::move(settings))
    , _seats(_settings.godSlots)
{
	_sockets.SetHandlers(ServerSocketHandlers {
	    .opened = [this](ConnectionId connection) { Opened(connection); },
	    .text = [this](ConnectionId connection, std::string_view text) { Text(connection, text); },
	    .binary = [this](ConnectionId connection, std::span<const uint8_t> bytes) { Binary(connection, bytes); },
	    .closed = [this](ConnectionId connection, uint16_t /*code*/) { Closed(connection); },
	});
}

LockstepHost::~LockstepHost()
{
	_sockets.SetHandlers({});
}

void LockstepHost::Opened(ConnectionId connection)
{
	_peers[connection] = Peer {};
}

void LockstepHost::SendTo(ConnectionId connection, const game_socket::Frame& frame)
{
	_sockets.SendText(connection, game_socket::ToJson(frame));
}

void LockstepHost::Broadcast(const game_socket::Frame& frame)
{
	const auto text = game_socket::ToJson(frame);
	for (const auto& [connection, peer] : _peers)
	{
		if (peer.joined)
		{
			_sockets.SendText(connection, text);
		}
	}
}

void LockstepHost::BroadcastBinary(std::span<const uint8_t> bytes)
{
	for (const auto& [connection, peer] : _peers)
	{
		if (peer.joined)
		{
			_sockets.SendBinary(connection, bytes);
		}
	}
}

void LockstepHost::Refuse(ConnectionId connection, std::string_view reason)
{
	Log(spdlog::level::info, fmt::format("The server refuses connection {}: {}", connection, reason));
	SendTo(connection, game_socket::Refused {.reason = std::string(reason)});
	_sockets.Close(connection, server_close::k_Refused);
	_peers.erase(connection);
	// A refused observer cannot come back with its session
	std::erase_if(_watchers, [connection](const Watcher& watcher) { return watcher.connection == connection; });
}

void LockstepHost::Text(ConnectionId connection, std::string_view text)
{
	const auto found = _peers.find(connection);
	if (found == _peers.end())
	{
		return;
	}
	auto& peer = found->second;
	const auto frame = game_socket::Parse(text);
	if (!frame.has_value())
	{
		// The game server closes a connection that sends a message that it does not know
		Refuse(connection, "protocol");
		return;
	}
	if (const auto* join = std::get_if<game_socket::Join>(&*frame))
	{
		Join(connection, peer, *join);
	}
	else if (const auto* resume = std::get_if<game_socket::Resume>(&*frame))
	{
		Resume(connection, peer, *resume);
	}
	else if (const auto* ready = std::get_if<game_socket::Ready>(&*frame))
	{
		if (peer.seat.has_value())
		{
			_seats[*peer.seat].ready = ready->ready;
		}
	}
	else if (std::holds_alternative<game_socket::Leave>(*frame))
	{
		if (peer.seat.has_value())
		{
			const auto seat = *peer.seat;
			Log(spdlog::level::info, fmt::format("The player of slot {} leaves the game", seat));
			peer.seat.reset();
			_seats[seat].connection.reset();
			CloseSeat(seat);
			// A god who leaves during the start stops the start
			if (Starting())
			{
				StopStart(game_socket::StartStop {});
			}
		}
		// An observer that leaves cannot come back with its session
		std::erase_if(_watchers, [connection](const Watcher& watcher) { return watcher.connection == connection; });
		_sockets.Close(connection, server_close::k_Normal);
		_peers.erase(connection);
	}
	else if (std::holds_alternative<game_socket::StopRequest>(*frame))
	{
		if (peer.joined)
		{
			StopTurns();
		}
	}
	else if (const auto* reply = std::get_if<game_socket::StartReply>(&*frame))
	{
		StartReply(connection, peer, *reply);
	}
	else if (const auto* creature = std::get_if<game_socket::StartCreature>(&*frame))
	{
		StartCreature(connection, peer, *creature);
	}
	else
	{
		Refuse(connection, "protocol");
	}
}

void LockstepHost::Join(ConnectionId connection, Peer& peer, const game_socket::Join& join)
{
	if (peer.joined)
	{
		Refuse(connection, "protocol");
		return;
	}
	const auto result = _checkTicket ? _checkTicket(join.ticket) : TicketCheck::Result {};
	if (result.problem != TicketProblem::None || !result.claims.has_value())
	{
		Refuse(connection, "ticket");
		Log(spdlog::level::info,
		    fmt::format("The ticket of connection {} is not valid: {}", connection, ProblemName(result.problem)));
		return;
	}
	const auto& claims = *result.claims;
	// The room takes nobody new during the start
	if (Starting())
	{
		Refuse(connection, "started");
		return;
	}
	if (_started)
	{
		JoinRunning(connection, peer, claims);
		return;
	}
	game_socket::Joined joined {
	    .role = claims.role, .slot = std::nullopt, .session = {}, .name = claims.name, .start = _settings.start};
	if (claims.role == game_socket::Role::God)
	{
		// The room refuses a god that the lobby banned, as the host's channel refuses a banned player
		if (lobby_bans::IsBanned(claims.subject, _bans))
		{
			Refuse(connection, "kicked");
			return;
		}
		const auto same = std::ranges::find_if(
		    _seats, [&claims](const Seat& seat) { return seat.state != SeatState::Open && seat.account == claims.subject; });
		if (same != _seats.end())
		{
			Refuse(connection, "already");
			return;
		}
		const auto free = std::ranges::find(_seats, SeatState::Open, &Seat::state);
		if (free == _seats.end())
		{
			Refuse(connection, "full");
			return;
		}
		const auto seat = static_cast<uint8_t>(free - _seats.begin());
		*free = Seat {.state = SeatState::Taken,
		              .connection = connection,
		              .session = _makeSecret ? _makeSecret() : std::string(),
		              .account = claims.subject,
		              .name = claims.name,
		              .ready = false,
		              .awaySince = 0,
		              .compares = {},
		              .resumes = 0,
		              .commands = {}};
		peer.seat = seat;
		joined.slot = seat;
		joined.session = free->session;
		Log(spdlog::level::info, fmt::format("{} joins as the god of slot {}", claims.name, seat));
	}
	else
	{
		joined.session = NewWatcher(connection);
		Log(spdlog::level::info, fmt::format("{} joins as an observer", claims.name));
	}
	peer.joined = true;
	peer.role = claims.role;
	SendTo(connection, joined);
}

void LockstepHost::Resume(ConnectionId connection, Peer& peer, const game_socket::Resume& resume)
{
	const auto found = std::ranges::find_if(_seats, [&resume](const Seat& seat) {
		return !seat.session.empty() && seat.session == resume.session &&
		       (seat.state == SeatState::Taken || seat.state == SeatState::Away);
	});
	// The server sends again only the turns that it kept, unless it sends its world
	const bool world = resume.world && _worldState;
	const bool kept = world || resume.turn == _turnsDone || _history.contains(resume.turn);
	if (peer.joined || resume.turn > _turnsDone || !kept)
	{
		Refuse(connection, "resume");
		return;
	}
	if (found == _seats.end())
	{
		// A session of no god can be the session of an observer
		if (!ResumeObserver(connection, peer, resume, world))
		{
			Refuse(connection, "resume");
		}
		return;
	}
	const auto seat = static_cast<uint8_t>(found - _seats.begin());
	if (found->connection.has_value() && *found->connection != connection)
	{
		// A new socket of the same player replaces the old one
		_sockets.Close(*found->connection, server_close::k_GoingAway);
		_peers.erase(*found->connection);
	}
	const bool wasAway = found->state == SeatState::Away;
	found->state = SeatState::Taken;
	found->connection = connection;
	++found->resumes;
	peer.joined = true;
	peer.role = game_socket::Role::God;
	peer.seat = seat;
	// A client whose world parted gets the server's world at the start of the next turn, and then the turns from there
	if (world && SendWorld(connection))
	{
		// The checksums that the client sent for the turns of its parted world are no longer of use
		std::erase_if(_early, [seat](const auto& early) { return early.first.first == seat; });
		Log(spdlog::level::info,
		    fmt::format("The player of slot {} is back from turn {} with the world of turn {}", seat, resume.turn, _turnsDone));
		if (wasAway)
		{
			Broadcast(game_socket::PlayerState {.slot = seat, .state = game_socket::PlayerState::State::Back});
		}
		return;
	}
	SendTo(connection, game_socket::Resumed {.turn = _turnsDone});
	SendKeptTurns(connection, resume.turn);
	Log(spdlog::level::info,
	    fmt::format("The player of slot {} is back from turn {}; the server is at turn {}", seat, resume.turn, _turnsDone));
	if (wasAway)
	{
		Broadcast(game_socket::PlayerState {.slot = seat, .state = game_socket::PlayerState::State::Back});
	}
}

bool LockstepHost::ResumeObserver(ConnectionId connection, Peer& peer, const game_socket::Resume& resume, bool world)
{
	const auto found = std::ranges::find_if(
	    _watchers, [&resume](const Watcher& watcher) { return !watcher.session.empty() && watcher.session == resume.session; });
	if (found == _watchers.end())
	{
		return false;
	}
	if (found->connection.has_value() && *found->connection != connection)
	{
		// A new socket of the same observer replaces the old one
		_sockets.Close(*found->connection, server_close::k_GoingAway);
		_peers.erase(*found->connection);
	}
	found->connection = connection;
	found->awaySince = 0;
	++found->resumes;
	peer.joined = true;
	peer.role = game_socket::Role::Observer;
	peer.seat.reset();
	// An observer comes back as a god does: the world of the server when its world parted, else the turns that it missed.
	// An observer has no god, so the other players get no message.
	if (world && SendWorld(connection))
	{
		Log(spdlog::level::info,
		    fmt::format("An observer is back from turn {} with the world of turn {}", resume.turn, _turnsDone));
		return true;
	}
	SendTo(connection, game_socket::Resumed {.turn = _turnsDone});
	SendKeptTurns(connection, resume.turn);
	Log(spdlog::level::info,
	    fmt::format("An observer is back from turn {}; the server is at turn {}", resume.turn, _turnsDone));
	return true;
}

void LockstepHost::SendKeptTurns(ConnectionId connection, uint32_t from)
{
	for (auto turn = _history.lower_bound(from); turn != _history.end(); ++turn)
	{
		_sockets.SendBinary(connection, turn->second);
		if (const auto checksum = _checksums.find(turn->first + 1); checksum != _checksums.end())
		{
			_sockets.SendBinary(connection, turn_codec::Encode(turn_codec::TurnChecksum {.turn = checksum->first,
			                                                                             .checksum = checksum->second}));
		}
	}
	if (_stoppedAfter.has_value())
	{
		SendTo(connection, game_socket::GameStop {.turn = *_stoppedAfter});
	}
}

std::string LockstepHost::NewWatcher(ConnectionId connection)
{
	if (!_makeSecret)
	{
		return {};
	}
	auto session = _makeSecret();
	_watchers.push_back(Watcher {.session = session, .connection = connection, .awaySince = 0, .resumes = 0});
	return session;
}

bool LockstepHost::SendWorld(ConnectionId connection)
{
	const auto state = _worldState ? _worldState() : std::vector<uint8_t> {};
	if (state.empty())
	{
		return false;
	}
	SendTo(connection, game_socket::Resumed {.turn = _turnsDone, .world = true});
	for (const auto& part : turn_codec::SplitWorld(_turnsDone, state))
	{
		_sockets.SendBinary(connection, turn_codec::Encode(part));
	}
	Log(spdlog::level::info,
	    fmt::format("The server sends its world of turn {} ({} bytes) to connection {}", _turnsDone, state.size(), connection));
	return true;
}

void LockstepHost::JoinRunning(ConnectionId connection, Peer& peer, const join_ticket::Claims& claims)
{
	// A player who joins the game that turns needs the server's world. The testbed scenario of the tools keeps its state
	// out of the world and plays from the first turn, so nobody can join its game as it turns.
	if (!_worldState || _settings.start.scenario.has_value())
	{
		Refuse(connection, "started");
		return;
	}
	// The player of a god that waits, or whose old socket is not closed yet, comes back into the god (MP-Q6). The ticket
	// of the lobby shows the account, so a new process of the player needs no secret of the old session.
	const auto own = std::ranges::find_if(_seats, [&claims](const Seat& seat) {
		return claims.role == game_socket::Role::God && !seat.account.empty() && seat.account == claims.subject &&
		       (seat.state == SeatState::Taken || seat.state == SeatState::Away);
	});
	auto start = _settings.start;
	start.turn = _turnsDone;
	start.gods = Gods();
	game_socket::Joined joined {
	    .role = game_socket::Role::Observer, .slot = std::nullopt, .session = {}, .name = claims.name, .start = start};
	if (own != _seats.end())
	{
		const auto seat = static_cast<uint8_t>(own - _seats.begin());
		if (own->connection.has_value() && *own->connection != connection)
		{
			// The new socket of the player replaces the old one
			_sockets.Close(*own->connection, server_close::k_GoingAway);
			_peers.erase(*own->connection);
		}
		const bool wasAway = own->state == SeatState::Away;
		own->state = SeatState::Taken;
		own->connection = connection;
		// A new secret, so that the old process of the player cannot take the god back
		own->session = _makeSecret ? _makeSecret() : std::string();
		++own->resumes;
		std::erase_if(_early, [seat](const auto& early) { return early.first.first == seat; });
		peer.role = game_socket::Role::God;
		peer.seat = seat;
		joined.role = game_socket::Role::God;
		joined.slot = seat;
		joined.session = own->session;
		Log(spdlog::level::info,
		    fmt::format("{} comes back into the god of slot {} at turn {}", claims.name, seat, _turnsDone));
		if (wasAway)
		{
			Broadcast(game_socket::PlayerState {.slot = seat, .state = game_socket::PlayerState::State::Back});
		}
	}
	else
	{
		// Any other player who joins after the start is an observer (MP-Q12)
		peer.role = game_socket::Role::Observer;
		joined.session = NewWatcher(connection);
		Log(spdlog::level::info, fmt::format("{} joins the game at turn {} as an observer", claims.name, _turnsDone));
	}
	peer.joined = true;
	SendTo(connection, joined);
	SendTo(connection, start);
	SendWorld(connection);
}

void LockstepHost::Binary(ConnectionId connection, std::span<const uint8_t> bytes)
{
	const auto found = _peers.find(connection);
	if (found == _peers.end() || !found->second.joined)
	{
		return;
	}
	const auto& peer = found->second;
	auto decoded = turn_codec::Decode(bytes);
	if (!decoded.frame.has_value())
	{
		++_refusedBlocks;
		return;
	}
	if (auto* block = std::get_if<turn_codec::CommandBlock>(&*decoded.frame))
	{
		// Only a god sends commands, and only during the game. A command that only the server sends is left out.
		if (peer.role != game_socket::Role::God || !peer.seat.has_value() || !_started)
		{
			++_refusedBlocks;
			return;
		}
		const auto before = block->actions.size();
		// A command of the test tools goes in only when the server's test commands are on
		std::erase_if(block->actions, [this](const auto& action) {
			return std::holds_alternative<player_commands::PlayerLeft>(action) ||
			       std::holds_alternative<player_commands::PlayerJoined>(action) ||
			       (!_settings.testCommands && std::holds_alternative<player_commands::CreatureFightCheat>(action));
		});
		if (block->actions.size() != before)
		{
			++_refusedBlocks;
		}
		if (!block->actions.empty())
		{
			for (const auto& action : block->actions)
			{
				++_seats[*peer.seat].commands[static_cast<uint8_t>(turn_codec::TagOf(action))];
			}
			_pending.push_back(turn_codec::StampedBlock {.sender = static_cast<PlayerNames>(*peer.seat),
			                                             .sequence = block->sequence,
			                                             .actions = std::move(block->actions)});
		}
	}
	else if (const auto* checksum = std::get_if<turn_codec::TurnChecksum>(&*decoded.frame))
	{
		if (peer.seat.has_value())
		{
			CompareClient(*peer.seat, checksum->turn, checksum->checksum);
		}
	}
	else if (const auto* ping = std::get_if<turn_codec::Ping>(&*decoded.frame))
	{
		_sockets.SendBinary(connection, turn_codec::Encode(turn_codec::Pong {.id = ping->id, .time = ping->time}));
	}
}

void LockstepHost::Closed(ConnectionId connection)
{
	const auto found = _peers.find(connection);
	if (found == _peers.end())
	{
		return;
	}
	const auto seat = found->second.seat;
	_peers.erase(found);
	if (const auto watcher = std::ranges::find(_watchers, std::optional(connection), &Watcher::connection);
	    watcher != _watchers.end())
	{
		// Before the start, the session of the observer ends. After the start, the observer can come back for a time.
		if (!_started)
		{
			_watchers.erase(watcher);
		}
		else
		{
			watcher->connection.reset();
			watcher->awaySince = _clock();
			Log(spdlog::level::info, "An observer lost the connection; it can come back");
		}
		return;
	}
	if (!seat.has_value() || _seats[*seat].connection != connection)
	{
		return;
	}
	auto& place = _seats[*seat];
	place.connection.reset();
	if (!_started)
	{
		// Before the start, the slot is free again. A god who leaves during the start stops the start.
		Log(spdlog::level::info, fmt::format("The player of slot {} left the room", *seat));
		place = Seat {};
		if (Starting())
		{
			StopStart(game_socket::StartStop {});
		}
		return;
	}
	place.state = SeatState::Away;
	place.awaySince = _clock();
	Log(spdlog::level::info, fmt::format("The player of slot {} lost the connection; the god waits", *seat));
	Broadcast(game_socket::PlayerState {.slot = *seat, .state = game_socket::PlayerState::State::Away});
}

void LockstepHost::CloseSeat(uint8_t seat)
{
	auto& place = _seats[seat];
	if (place.state == SeatState::Closed || place.state == SeatState::Open)
	{
		return;
	}
	place.state = SeatState::Closed;
	place.connection.reset();
	if (_started)
	{
		_leaving.push_back(seat);
		Broadcast(game_socket::PlayerState {.slot = seat, .state = game_socket::PlayerState::State::Left});
	}
	else
	{
		place = Seat {};
	}
}

void LockstepHost::Update()
{
	const auto now = _clock();
	// An observer that does not come back in the time to come back loses its session
	std::erase_if(_watchers, [this, now](const Watcher& watcher) {
		return !watcher.connection.has_value() && now - watcher.awaySince > _settings.comeBackMicroseconds;
	});
	if (AllComputerGods())
	{
		// A room with no human god starts at once. The observers join the running game and get its world.
		if (!_started)
		{
			StartGame();
		}
		const auto observers = Observers();
		_observerSeen = _observerSeen || observers > 0;
		if (!_observersJoined && observers >= _settings.minGods)
		{
			_observersJoined = true;
			Log(spdlog::level::info, fmt::format("{} observers joined: the world of computer gods turns", observers));
		}
		return;
	}
	if (!_started)
	{
		const auto inRoom = std::ranges::count(_seats, SeatState::Taken, &Seat::state);
		const auto ready =
		    std::ranges::count_if(_seats, [](const Seat& seat) { return seat.state == SeatState::Taken && seat.ready; });
		if (!Starting() && RoomStarts(static_cast<size_t>(inRoom), static_cast<size_t>(ready), _settings.minGods))
		{
			QueryLands();
		}
		return;
	}
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		auto& seat = _seats[i];
		if (seat.state == SeatState::Away && now - seat.awaySince > _settings.comeBackMicroseconds)
		{
			Log(spdlog::level::info, fmt::format("The player of slot {} did not come back: the god is defeated", i));
			CloseSeat(static_cast<uint8_t>(i));
		}
	}
	const bool paused = Paused();
	if (paused != _pausedBefore)
	{
		Log(spdlog::level::info,
		    paused ? "No god's player is connected: the world waits" : "A god's player is connected: the world turns again");
		_pausedBefore = paused;
	}
}

std::set<ConnectionId> LockstepHost::GodConnections() const
{
	std::set<ConnectionId> connections;
	for (const auto& seat : _seats)
	{
		if (seat.state == SeatState::Taken && seat.connection.has_value())
		{
			connections.insert(*seat.connection);
		}
	}
	return connections;
}

void LockstepHost::QueryLands()
{
	_startStep = StartStep::Query;
	_awaited = GodConnections();
	Log(spdlog::level::info, fmt::format("The gods are ready: the server asks {} gods if they have the land", _awaited.size()));
	const auto& start = _settings.start;
	// The testbed and the map for 32 gods have no land file, so the players need nothing to compare
	Broadcast(game_socket::StartQuery {
	    .land = start.land, .testbed = start.testbed || start.godsRingMap, .checksum = _settings.landChecksum});
}

void LockstepHost::StartReply(ConnectionId connection, const Peer& peer, const game_socket::StartReply& reply)
{
	// The server does not use a reply that comes after its step
	if (_startStep != StartStep::Query || !_awaited.contains(connection) || !peer.seat.has_value())
	{
		return;
	}
	_awaited.erase(connection);
	const auto seat = *peer.seat;
	switch (online::start_rules::Judge(_settings.landChecksum, reply))
	{
	case online::start_rules::Verdict::Same:
		break;
	case online::start_rules::Verdict::Missing:
		// The god cannot play the land: it is not ready, and the start stops for all
		Log(spdlog::level::info, fmt::format("The player of slot {} does not have the land: the start stops", seat));
		_seats[seat].ready = false;
		StopStart(game_socket::StartCancel {});
		return;
	case online::start_rules::Verdict::Different:
		// The god's land is not the server's: the start stops, and the player leaves the room
		Log(spdlog::level::info, fmt::format("The land of the player of slot {} is not the server's: the start stops", seat));
		StopStart(game_socket::StartCancel {});
		_seats[seat] = Seat {};
		Refuse(connection, "kicked");
		return;
	}
	if (_awaited.empty())
	{
		TransferCreatures();
	}
}

void LockstepHost::TransferCreatures()
{
	_startStep = StartStep::Transfer;
	_awaited = GodConnections();
	_startCreatures.clear();
	_startAlignments.clear();
	Log(spdlog::level::info, "Every god has the land: the gods send their creatures");
	Broadcast(game_socket::StartTransfer {});
}

void LockstepHost::StartCreature(ConnectionId connection, const Peer& peer, const game_socket::StartCreature& creature)
{
	if (_startStep != StartStep::Transfer || !_awaited.contains(connection) || !peer.seat.has_value())
	{
		return;
	}
	_awaited.erase(connection);
	// Each player gets the creature file and the alignment of each god at once. The server keeps them in memory for its own
	// world.
	Log(spdlog::level::info, fmt::format("The god of slot {} sends a creature file of {} bytes and the alignment {}",
	                                     *peer.seat, creature.file.size(), creature.alignment));
	_startCreatures[*peer.seat] = creature.file;
	_startAlignments[*peer.seat] = creature.alignment;
	Broadcast(game_socket::StartCreature {.slot = *peer.seat, .file = creature.file, .alignment = creature.alignment});
	if (_awaited.empty())
	{
		StartGame();
	}
}

void LockstepHost::StartGame()
{
	_startStep = StartStep::None;
	_awaited.clear();
	_started = true;
	auto start = _settings.start;
	start.turn = _turnsDone;
	// Every computer names the god of a slot by the name of its player
	start.names.clear();
	for (const auto& seat : _seats)
	{
		start.names.push_back(seat.state == SeatState::Taken ? seat.name : std::string());
	}
	start.gods = Gods();
	const auto gods = std::ranges::count(_seats, SeatState::Taken, &Seat::state);
	Log(spdlog::level::info, fmt::format("The network game starts with {} gods", gods));
	// The first turn names the humans of the game on every computer: each gets its player and its hands, and the slots
	// with no human fill
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		if (_seats[i].state == SeatState::Taken)
		{
			_joining.push_back(static_cast<uint8_t>(i));
		}
	}
	Broadcast(start);
}

std::vector<game_socket::God> LockstepHost::Gods() const
{
	std::vector<game_socket::God> gods;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		if (_seats[i].state == SeatState::Taken || _seats[i].state == SeatState::Away)
		{
			gods.push_back({.slot = static_cast<uint8_t>(i), .name = _seats[i].name});
		}
	}
	return gods;
}

void LockstepHost::SetBans(std::set<int64_t> accounts)
{
	_bans = std::move(accounts);
	if (_started)
	{
		return;
	}
	const auto banned = lobby_bans::BannedSeats(_seats, _bans);
	for (const auto seat : banned)
	{
		// The server takes the banned god out of the room, as the host kicks a player that it bans
		Log(spdlog::level::info, fmt::format("The lobby banned the player of slot {}: the player leaves the room", seat));
		const auto connection = _seats[seat].connection;
		_seats[seat] = Seat {};
		if (connection.has_value())
		{
			Refuse(*connection, "kicked");
		}
	}
	// A god who leaves during the start stops the start
	if (!banned.empty() && Starting())
	{
		StopStart(game_socket::StartStop {});
	}
}

void LockstepHost::StopStart(const game_socket::Frame& why)
{
	_startStep = StartStep::None;
	_awaited.clear();
	_startCreatures.clear();
	_startAlignments.clear();
	Broadcast(why);
}

void LockstepHost::Send(const player_commands::Action& /*action*/) {}

std::span<const uint8_t> LockstepHost::StartCreatureFile(PlayerNames god) const
{
	const auto file = _startCreatures.find(static_cast<uint8_t>(god));
	return file != _startCreatures.end() ? std::span<const uint8_t>(file->second) : std::span<const uint8_t> {};
}

std::optional<float> LockstepHost::StartAlignment(PlayerNames god) const
{
	const auto alignment = _startAlignments.find(static_cast<uint8_t>(god));
	return alignment != _startAlignments.end() ? std::optional<float>(alignment->second) : std::nullopt;
}

bool LockstepHost::AnyGodConnected() const
{
	return std::ranges::any_of(_seats, [](const Seat& seat) { return seat.state == SeatState::Taken; });
}

bool LockstepHost::Paused() const
{
	return _started && !AllComputerGods() && !AnyGodConnected();
}

void LockstepHost::StopTurns()
{
	// The first request stops the turns. The turns closed before it are the last turns of the game.
	if (!_started || _stoppedAfter.has_value() || _turnsDone == 0)
	{
		return;
	}
	_stoppedAfter = _turnsDone - 1;
	// The world gives each turn the number after the number of its commands, as the checksums and the records do. The
	// log uses the number of the world, so that it names the same turn as the line of the decision.
	Log(spdlog::level::info, fmt::format("The game is decided: the turns of the game stop after turn {}", *_stoppedAfter + 1));
	Broadcast(game_socket::GameStop {.turn = *_stoppedAfter});
}

bool LockstepHost::GameOver() const
{
	if (AllComputerGods())
	{
		// A room with no human god is over when the observers that joined all left
		return _started && _observerSeen && Observers() == 0;
	}
	// After the turns stop, the defeat of a god that left comes at no turn
	return _started && (_leaving.empty() || _stoppedAfter.has_value()) && std::ranges::none_of(_seats, [](const Seat& seat) {
		       return seat.state == SeatState::Taken || seat.state == SeatState::Away;
	       });
}

bool LockstepHost::HasTurn(uint32_t /*turn*/) const
{
	// The defeat of a god that left still comes at a turn, so a world with a god leaving turns once more. No turn comes
	// after the turns of the game stop.
	if (AllComputerGods())
	{
		return _started && _observersJoined && !_stoppedAfter.has_value();
	}
	return _started && !_stoppedAfter.has_value() && (AnyGodConnected() || !_leaving.empty());
}

std::vector<player_commands::Command> LockstepHost::TakeTurn(uint32_t turn)
{
	turn_codec::TurnBundle bundle {.turn = turn, .blocks = {}};
	for (const auto seat : _joining)
	{
		bundle.blocks.push_back(turn_codec::StampedBlock {
		    .sender = static_cast<PlayerNames>(seat), .sequence = 0, .actions = {player_commands::PlayerJoined {}}});
	}
	_joining.clear();
	for (const auto seat : _leaving)
	{
		bundle.blocks.push_back(turn_codec::StampedBlock {
		    .sender = static_cast<PlayerNames>(seat), .sequence = 0, .actions = {player_commands::PlayerLeft {}}});
	}
	_leaving.clear();
	for (auto& block : _pending)
	{
		bundle.blocks.push_back(std::move(block));
	}
	_pending.clear();
	auto bytes = turn_codec::Encode(bundle);
	if (bytes.empty())
	{
		// A turn larger than a frame drops the commands of the turn, but never the turn itself
		Log(spdlog::level::err, fmt::format("The commands of turn {} do not fit in a frame", turn));
		bundle.blocks.clear();
		bytes = turn_codec::Encode(bundle);
	}
	BroadcastBinary(bytes);
	_history[turn] = std::move(bytes);
	if (turn >= _settings.keptTurns)
	{
		_history.erase(_history.begin(), _history.lower_bound(turn - _settings.keptTurns));
	}
	_turnsDone = turn + 1;

	std::vector<player_commands::Command> commands;
	for (auto& block : bundle.blocks)
	{
		for (auto& action : block.actions)
		{
			commands.push_back(player_commands::Command {.player = block.sender, .action = std::move(action)});
		}
	}
	return commands;
}

void LockstepHost::TurnChecksum(uint32_t turn, uint64_t checksum)
{
	_checksums[turn] = checksum;
	if (turn > _settings.keptTurns)
	{
		_checksums.erase(_checksums.begin(), _checksums.lower_bound(turn - _settings.keptTurns));
	}
	BroadcastBinary(turn_codec::Encode(turn_codec::TurnChecksum {.turn = turn, .checksum = checksum}));
	for (auto early = _early.begin(); early != _early.end();)
	{
		if (early->first.second <= turn)
		{
			const auto [seat, clientTurn] = early->first;
			const auto value = early->second;
			early = _early.erase(early);
			CompareClient(seat, clientTurn, value);
		}
		else
		{
			++early;
		}
	}
}

void LockstepHost::CompareClient(uint8_t seat, uint32_t turn, uint64_t checksum)
{
	const auto own = _checksums.find(turn);
	if (own == _checksums.end())
	{
		if (turn > _turnsDone && _early.size() < k_MostEarlyChecksums * _seats.size())
		{
			_early[{seat, turn}] = checksum;
		}
		return;
	}
	auto& compares = _seats[seat].compares;
	++compares.compared;
	const bool differs = own->second != checksum;
	if (differs && !compares.differs && _partListener)
	{
		_partListener(seat, turn);
	}
	compares.differs = differs;
	if (!differs)
	{
		++compares.same;
	}
	else if (!compares.firstDifferent.has_value())
	{
		compares.firstDifferent = turn;
		Log(spdlog::level::warn, fmt::format("The world of slot {} parts from the server's world at turn {}", seat, turn));
	}
	if (compares.compared % k_CompareLogEvery == 0)
	{
		Log(spdlog::level::info,
		    fmt::format("Slot {}: {} checksums compared, {} the same", seat, compares.compared, compares.same));
	}
}

size_t LockstepHost::Observers() const
{
	return static_cast<size_t>(std::ranges::count_if(
	    _peers, [](const auto& entry) { return entry.second.joined && entry.second.role == game_socket::Role::Observer; }));
}

std::vector<std::string> LockstepHost::SummaryLines() const
{
	std::vector<std::string> lines;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		const auto& seat = _seats[i];
		if (seat.account.empty() && seat.compares.compared == 0)
		{
			continue;
		}
		std::string commands;
		for (const auto& [tag, count] : seat.commands)
		{
			commands += fmt::format("{}{}x{}", commands.empty() ? "" : " ", count, tag);
		}
		lines.push_back(fmt::format(
		    "slot {} {}: commands [{}], {} checksums compared, {} the same, first different {}, {} resumes", i, seat.name,
		    commands, seat.compares.compared, seat.compares.same,
		    seat.compares.firstDifferent.has_value() ? std::to_string(*seat.compares.firstDifferent) : std::string("none"),
		    seat.resumes));
	}
	return lines;
}

} // namespace openblack::server
