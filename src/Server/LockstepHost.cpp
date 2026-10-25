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
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Online/StartRules.h"

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
		_sockets.Close(connection, server_close::k_Normal);
		_peers.erase(connection);
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
	// A god or an observer who joins after the start needs the live state of the world (plan step 9). The room also
	// takes nobody new during the start.
	if (_started || Starting())
	{
		Refuse(connection, "started");
		return;
	}
	game_socket::Joined joined {
	    .role = claims.role, .slot = std::nullopt, .session = {}, .name = claims.name, .start = _settings.start};
	if (claims.role == game_socket::Role::God)
	{
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
	if (peer.joined || found == _seats.end() || resume.turn > _turnsDone || !kept)
	{
		Refuse(connection, "resume");
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
	if (const auto state = world ? _worldState() : std::vector<uint8_t> {}; !state.empty())
	{
		const auto parts = turn_codec::SplitWorld(_turnsDone, state);
		SendTo(connection, game_socket::Resumed {.turn = _turnsDone, .world = true});
		for (const auto& part : parts)
		{
			_sockets.SendBinary(connection, turn_codec::Encode(part));
		}
		// The checksums that the client sent for the turns of its parted world are no longer of use
		std::erase_if(_early, [seat](const auto& early) { return early.first.first == seat; });
		Log(spdlog::level::info, fmt::format("The player of slot {} is back from turn {} with the world of turn {} ({} bytes)",
		                                     seat, resume.turn, _turnsDone, state.size()));
		if (wasAway)
		{
			Broadcast(game_socket::PlayerState {.slot = seat, .state = game_socket::PlayerState::State::Back});
		}
		return;
	}
	SendTo(connection, game_socket::Resumed {.turn = _turnsDone});
	for (auto turn = _history.lower_bound(resume.turn); turn != _history.end(); ++turn)
	{
		_sockets.SendBinary(connection, turn->second);
		if (const auto checksum = _checksums.find(turn->first + 1); checksum != _checksums.end())
		{
			_sockets.SendBinary(connection, turn_codec::Encode(turn_codec::TurnChecksum {.turn = checksum->first,
			                                                                             .checksum = checksum->second}));
		}
	}
	Log(spdlog::level::info,
	    fmt::format("The player of slot {} is back from turn {}; the server is at turn {}", seat, resume.turn, _turnsDone));
	if (wasAway)
	{
		Broadcast(game_socket::PlayerState {.slot = seat, .state = game_socket::PlayerState::State::Back});
	}
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
		std::erase_if(block->actions, [](const auto& action) {
			return std::holds_alternative<player_commands::PlayerLeft>(action) ||
			       std::holds_alternative<player_commands::PlayerJoined>(action);
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
	if (!_started)
	{
		const auto ready =
		    std::ranges::count_if(_seats, [](const Seat& seat) { return seat.state == SeatState::Taken && seat.ready; });
		if (!Starting() && ready > 0 && static_cast<uint32_t>(ready) >= _settings.godsToStart)
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
	// The server keeps no file: each player gets the creature file of each god at once
	Log(spdlog::level::info,
	    fmt::format("The god of slot {} sends a creature file of {} bytes", *peer.seat, creature.file.size()));
	Broadcast(game_socket::StartCreature {.slot = *peer.seat, .file = creature.file});
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
	const auto gods = std::ranges::count(_seats, SeatState::Taken, &Seat::state);
	Log(spdlog::level::info, fmt::format("The network game starts with {} gods", gods));
	// The first turn gives the gods of the other slots their hands, on every computer
	for (size_t i = 1; i < _seats.size(); ++i)
	{
		if (_seats[i].state == SeatState::Taken)
		{
			_joining.push_back(static_cast<uint8_t>(i));
		}
	}
	Broadcast(start);
}

void LockstepHost::StopStart(const game_socket::Frame& why)
{
	_startStep = StartStep::None;
	_awaited.clear();
	Broadcast(why);
}

void LockstepHost::Send(const player_commands::Action& /*action*/) {}

bool LockstepHost::AnyGodConnected() const
{
	return std::ranges::any_of(_seats, [](const Seat& seat) { return seat.state == SeatState::Taken; });
}

bool LockstepHost::Paused() const
{
	return _started && !AnyGodConnected();
}

bool LockstepHost::GameOver() const
{
	return _started && _leaving.empty() && std::ranges::none_of(_seats, [](const Seat& seat) {
		       return seat.state == SeatState::Taken || seat.state == SeatState::Away;
	       });
}

bool LockstepHost::HasTurn(uint32_t /*turn*/) const
{
	// The defeat of a god that left still comes at a turn, so a world with a god leaving turns once more
	return _started && (AnyGodConnected() || !_leaving.empty());
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
	if (own->second == checksum)
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
