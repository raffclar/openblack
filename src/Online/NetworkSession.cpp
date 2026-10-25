/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NetworkSession.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <span>
#include <type_traits>
#include <utility>

#include <spdlog/spdlog.h>

#include "Online/StartRules.h"

namespace openblack::online
{
namespace
{

/// The checksums that the client keeps for a compare, by turn
constexpr uint32_t k_KeptChecksums = 600;
/// The client writes a line of the compares to the log after this many turns
constexpr uint32_t k_CompareLogEvery = 100;

void Log(spdlog::level::level_enum level, std::string_view text)
{
	if (auto logger = spdlog::get("game"))
	{
		logger->log(level, "{}", text);
	}
}

} // namespace

NetworkSession::NetworkSession(WebSocketClientInterface& sockets, Clock clock)
    : _sockets(sockets)
    , _clock(std::move(clock))
{
}

NetworkSession::~NetworkSession()
{
	if (_stream)
	{
		_stream->Close();
	}
}

void NetworkSession::Join(NetworkSessionSettings settings)
{
	if (_stream)
	{
		_stream->Close();
		_stream.reset();
	}
	_dropStream = false;
	_settings = std::move(settings);
	_end = End::None;
	_joined.reset();
	_start.reset();
	_startEvents.clear();
	_startCreatures.clear();
	_turns.clear();
	_playout.Reset();
	_turnsDone = 0;
	Open(false);
}

void NetworkSession::Open(bool resume)
{
	++_generation;
	const auto generation = _generation;
	_stream = std::make_unique<TurnStream>(_sockets, [this] { return static_cast<uint64_t>(_clock()); });
	_state = resume ? State::Reconnecting : State::Connecting;
	_stream->Open(
	    _settings.request,
	    TurnStreamHandlers {
	        .opened =
	            [this, generation, resume] {
		            if (generation != _generation)
		            {
			            return;
		            }
		            if (resume && _joined.has_value())
		            {
			            _stream->SendText(game_socket::ToJson(
			                game_socket::Resume {.session = _joined->session, .turn = _turnsDone, .world = _wantsWorld}));
		            }
		            else
		            {
			            _stream->SendText(game_socket::ToJson(game_socket::Join {
			                .ticket = _settings.ticket, .gamePassword = _settings.gamePassword, .name = _settings.name}));
			            _state = State::Joining;
		            }
	            },
	        .turn =
	            [this, generation](const turn_codec::TurnBundle& turn) {
		            if (generation == _generation)
		            {
			            Turn(turn);
		            }
	            },
	        .checksum =
	            [this, generation](const turn_codec::TurnChecksum& checksum) {
		            if (generation == _generation)
		            {
			            ServerChecksum(checksum);
		            }
	            },
	        .worldPart =
	            [this, generation](const turn_codec::WorldPart& part) {
		            if (generation == _generation)
		            {
			            WorldPart(part);
		            }
	            },
	        .text =
	            [this, generation](std::string_view text) {
		            if (generation == _generation)
		            {
			            Text(text);
		            }
	            },
	        .closed =
	            [this, generation](const WebSocketClosed& closed) {
		            if (generation == _generation)
		            {
			            Closed(closed);
		            }
	            },
	    });
	_nextPing = _clock() + k_PingMicroseconds;
}

void NetworkSession::SetReady(bool ready)
{
	_ready = ready;
	if (_state == State::Waiting && _stream && _stream->IsOpen())
	{
		_stream->SendText(game_socket::ToJson(game_socket::Ready {.ready = ready}));
	}
}

void NetworkSession::Leave()
{
	if (_stream && _stream->IsOpen())
	{
		_stream->SendText(game_socket::ToJson(game_socket::Leave {}));
	}
	FinishWith(End::Left);
}

void NetworkSession::DropStream()
{
	// The handlers of the old socket do nothing more. The socket closes in the next update, never inside its own handler.
	++_generation;
	_dropStream = true;
}

void NetworkSession::FinishWith(End end)
{
	DropStream();
	_state = State::Ended;
	_end = end;
}

void NetworkSession::Text(std::string_view text)
{
	const auto frame = game_socket::Parse(text);
	if (!frame.has_value())
	{
		return;
	}
	if (const auto* joined = std::get_if<game_socket::Joined>(&*frame))
	{
		_joined = *joined;
		_state = State::Waiting;
		_backoff.Reset();
		Log(spdlog::level::info, fmt::format("Joined the game server as {} {}", game_socket::RoleName(joined->role),
		                                     joined->slot.has_value() ? fmt::format("in slot {}", *joined->slot) : ""));
		if (_ready)
		{
			_stream->SendText(game_socket::ToJson(game_socket::Ready {.ready = true}));
		}
	}
	else if (const auto* start = std::get_if<game_socket::Start>(&*frame))
	{
		_start = *start;
		_state = State::Playing;
		Log(spdlog::level::info, fmt::format("The network game starts at turn {}", start->turn));
		SendWaiting();
	}
	else if (const auto* resumed = std::get_if<game_socket::Resumed>(&*frame))
	{
		_state = _start.has_value() ? State::Playing : State::Waiting;
		_backoff.Reset();
		++_resumes;
		if (_resync.GetState() == ResyncRule::State::Resyncing)
		{
			_resync.Rejoined();
		}
		Log(spdlog::level::info,
		    fmt::format("Back in the game at turn {}; the server is at turn {}", _turnsDone, resumed->turn));
		if (resumed->world)
		{
			// The server's world state of its turn comes first, and the turns from there
			_wantsWorld = false;
			_worldTurn = resumed->turn;
			_worldParts.clear();
			_world.reset();
			_turns.clear();
			_playout.Reset();
		}
		SendWaiting();
	}
	else if (const auto* refused = std::get_if<game_socket::Refused>(&*frame))
	{
		_refusedReason = refused->reason;
		Log(spdlog::level::warn, fmt::format("The game server refused the client: {}", refused->reason));
		FinishWith(End::Refused);
	}
	else if (_state == State::Waiting)
	{
		StartFrame(*frame);
	}
}

void NetworkSession::StartFrame(const game_socket::Frame& frame)
{
	if (const auto* query = std::get_if<game_socket::StartQuery>(&frame))
	{
		// The server names a land in the Scripts folder of the game, never a path out of it
		std::optional<std::vector<uint8_t>> landFile;
		if (!query->testbed && _settings.readLand && start_rules::IsLandPath(query->land))
		{
			landFile = _settings.readLand(query->land);
		}
		const auto reply = start_rules::Reply(*query, landFile);
		Log(spdlog::level::info, reply.have ? fmt::format("This computer has the land of the game ({})", query->land)
		                                    : fmt::format("This computer does not have the land {}", query->land));
		_stream->SendText(game_socket::ToJson(reply));
		if (!reply.have)
		{
			// The server takes the Ready of a player who cannot play the land
			_ready = false;
		}
		_startEvents.push_back(reply.have ? StartEvent::HasLand : StartEvent::LacksLand);
	}
	else if (std::holds_alternative<game_socket::StartTransfer>(frame))
	{
		_startCreatures.clear();
		// An observer has no god, so it has no creature to send
		if (_joined.has_value() && _joined->role == game_socket::Role::God)
		{
			auto file = _settings.readCreature ? _settings.readCreature() : std::vector<uint8_t> {};
			if (file.size() > game_socket::k_MostCreatureFileBytes)
			{
				Log(spdlog::level::warn, fmt::format("The creature file of {} bytes is too large to send", file.size()));
				file.clear();
			}
			Log(spdlog::level::info, fmt::format("This computer sends a creature file of {} bytes", file.size()));
			_stream->SendText(game_socket::ToJson(game_socket::StartCreature {.slot = std::nullopt, .file = std::move(file)}));
		}
		_startEvents.push_back(StartEvent::Transfer);
	}
	else if (const auto* creature = std::get_if<game_socket::StartCreature>(&frame))
	{
		if (creature->slot.has_value())
		{
			_startCreatures[*creature->slot] = creature->file;
		}
	}
	else if (std::holds_alternative<game_socket::StartCancel>(frame))
	{
		Log(spdlog::level::info, "A player does not have the land: the start stops");
		_startCreatures.clear();
		_startEvents.push_back(StartEvent::Cancelled);
	}
	else if (std::holds_alternative<game_socket::StartStop>(frame))
	{
		Log(spdlog::level::info, "A player left during the start: the start stops");
		_startCreatures.clear();
		_startEvents.push_back(StartEvent::Stopped);
	}
}

std::optional<StartEvent> NetworkSession::TakeStartEvent()
{
	if (_startEvents.empty())
	{
		return std::nullopt;
	}
	const auto event = _startEvents.front();
	_startEvents.erase(_startEvents.begin());
	return event;
}

void NetworkSession::Closed(const WebSocketClosed& closed)
{
	Log(spdlog::level::info, fmt::format("The game socket closed (code {}, HTTP status {})", closed.code, closed.httpStatus));
	if (_joined.has_value() && _state != State::Ended)
	{
		// The client keeps its world and tries to come back
		if (_state != State::Reconnecting)
		{
			_lostAt = _clock();
		}
		_state = State::Reconnecting;
		_nextAttempt = _clock() + std::chrono::duration_cast<std::chrono::microseconds>(_backoff.Next()).count();
		DropStream();
		return;
	}
	FinishWith(End::NoConnection);
}

void NetworkSession::ComeBack(bool resync)
{
	if (resync)
	{
		Log(spdlog::level::warn, fmt::format("Resynchronising... (try {} of {})", _resync.Tries(), ResyncRule::k_MostTries));
		_wantsWorld = true;
	}
	DropStream();
	_lostAt = _clock();
	_state = State::Reconnecting;
	_nextAttempt = _lostAt;
}

void NetworkSession::Update()
{
	if (std::exchange(_dropStream, false) && _stream)
	{
		_stream->Close();
		_stream.reset();
	}
	const auto now = _clock();
	if (_state == State::Reconnecting && !_stream)
	{
		if (now - _lostAt > _settings.comeBackMicroseconds)
		{
			Log(spdlog::level::warn, "The client did not come back to the game server in time");
			FinishWith(End::Lost);
			return;
		}
		if (now >= _nextAttempt)
		{
			// The server sends again the turns after the last turn that the world did
			std::erase_if(_turns, [this](const auto& entry) { return entry.first >= _turnsDone; });
			_playout.Reset();
			Open(true);
		}
		return;
	}
	if (_stream && _stream->IsOpen() && now >= _nextPing)
	{
		_stream->SendPing();
		_nextPing = now + k_PingMicroseconds;
	}
}

void NetworkSession::SendWaiting()
{
	if (!_stream || !_stream->IsOpen() || _joined.has_value() == false || _joined->role != game_socket::Role::God)
	{
		return;
	}
	for (size_t i = 0; i < _waiting.size(); i += turn_codec::k_MostBlockCommands)
	{
		const auto count = std::min(turn_codec::k_MostBlockCommands, _waiting.size() - i);
		_stream->SendCommands(_turnsDone, std::span(_waiting).subspan(i, count));
	}
	_waiting.clear();
}

void NetworkSession::Send(const player_commands::Action& action)
{
	// An observer sends no commands
	if (_joined.has_value() && _joined->role != game_socket::Role::God)
	{
		return;
	}
	if ((_state == State::Playing || _state == State::Waiting) && _stream && _stream->IsOpen())
	{
		const std::array actions {action};
		if (_stream->SendCommands(_turnsDone, actions))
		{
			return;
		}
	}
	// The commands wait for the socket, and are not lost (MI-6)
	if (_state != State::Ended)
	{
		_waiting.push_back(action);
	}
}

bool NetworkSession::HasTurn(uint32_t turn) const
{
	if ((_state != State::Playing && _state != State::Reconnecting) || _worldTurn.has_value())
	{
		return false;
	}
	return _turns.contains(turn) && _playout.Due(turn, _clock());
}

std::vector<player_commands::Command> NetworkSession::TakeTurn(uint32_t turn)
{
	std::vector<player_commands::Command> commands;
	const auto found = _turns.find(turn);
	if (found == _turns.end())
	{
		return commands;
	}
	for (auto& block : found->second.blocks)
	{
		for (auto& action : block.actions)
		{
			commands.push_back(player_commands::Command {.player = block.sender, .action = std::move(action)});
		}
	}
	_turns.erase(_turns.begin(), std::next(found));
	_playout.Done(turn);
	_turnsDone = turn + 1;
	return commands;
}

void NetworkSession::Turn(const turn_codec::TurnBundle& turn)
{
	if (turn.turn < _turnsDone || _turns.contains(turn.turn))
	{
		return;
	}
	_playout.Arrived(turn.turn, _clock());
	_turns.emplace(turn.turn, turn);
}

void NetworkSession::WorldPart(const turn_codec::WorldPart& part)
{
	if (!_worldTurn.has_value() || part.turn != *_worldTurn || part.part != _worldParts.size())
	{
		return;
	}
	_worldParts.push_back(part.bytes);
	if (_worldParts.size() < part.parts)
	{
		return;
	}
	WorldState world {.turn = part.turn, .bytes = {}};
	for (const auto& bytes : _worldParts)
	{
		world.bytes.insert(world.bytes.end(), bytes.begin(), bytes.end());
	}
	_worldParts.clear();
	Log(spdlog::level::info,
	    fmt::format("The world state of turn {} came from the game server ({} bytes)", world.turn, world.bytes.size()));
	_world = std::move(world);
}

std::optional<NetworkSession::WorldState> NetworkSession::TakeWorld()
{
	return std::exchange(_world, std::nullopt);
}

void NetworkSession::WorldLoaded(uint32_t turn)
{
	_worldTurn.reset();
	_turnsDone = turn;
	std::erase_if(_turns, [turn](const auto& entry) { return entry.first < turn; });
	// The checksums of the parted world are of no use
	_localChecksums.clear();
	std::erase_if(_serverChecksums, [turn](const auto& entry) { return entry.first <= turn; });
	Log(spdlog::level::info, fmt::format("The world goes on from the server's world of turn {}", turn));
}

void NetworkSession::LocalChecksum(uint32_t turn, uint64_t checksum)
{
	if (_stream && _stream->IsOpen())
	{
		_stream->SendChecksum(turn, checksum);
	}
	_localChecksums[turn] = checksum;
	Compare(turn);
}

void NetworkSession::ServerChecksum(const turn_codec::TurnChecksum& checksum)
{
	_serverChecksums[checksum.turn] = checksum.checksum;
	Compare(checksum.turn);
}

void NetworkSession::Compare(uint32_t turn)
{
	const auto local = _localChecksums.find(turn);
	const auto server = _serverChecksums.find(turn);
	if (local == _localChecksums.end() || server == _serverChecksums.end())
	{
		return;
	}
	const bool same = local->second == server->second;
	++_compares.compared;
	_localChecksums.erase(_localChecksums.begin(), std::next(local));
	_serverChecksums.erase(_serverChecksums.begin(), std::next(server));
	if (turn > k_KeptChecksums)
	{
		const auto oldest = turn - k_KeptChecksums;
		std::erase_if(_serverChecksums, [oldest](const auto& entry) { return entry.first < oldest; });
	}
	if (same)
	{
		++_compares.same;
		_resync.Same();
	}
	else
	{
		_compares.firstDifferent = _compares.firstDifferent.value_or(turn);
		Log(spdlog::level::warn, fmt::format("The world parts from the server's world at turn {}", turn));
		switch (_resync.Differ(turn))
		{
		case ResyncRule::Action::Rejoin:
			ComeBack(true);
			break;
		case ResyncRule::Action::GiveUp:
			Log(spdlog::level::err, "The resyncs did not work: the client leaves the game (out of sync)");
			FinishWith(End::OutOfSync);
			break;
		case ResyncRule::Action::None:
			break;
		}
	}
	if (_compares.compared % k_CompareLogEvery == 0)
	{
		Log(spdlog::level::info,
		    fmt::format("Network game: {} turns compared with the server, {} the same", _compares.compared, _compares.same));
	}
}

std::optional<uint64_t> NetworkSession::RoundTrip() const
{
	return _stream ? _stream->RoundTrip() : std::nullopt;
}

std::optional<uint32_t> NetworkSession::LastTurnReceived() const
{
	if (_turns.empty())
	{
		return _turnsDone > 0 ? std::optional(_turnsDone - 1) : std::nullopt;
	}
	return _turns.rbegin()->first;
}

std::string_view StateName(NetworkSession::State state) noexcept
{
	switch (state)
	{
	case NetworkSession::State::Idle:
		return "idle";
	case NetworkSession::State::Connecting:
		return "connecting";
	case NetworkSession::State::Joining:
		return "joining";
	case NetworkSession::State::Waiting:
		return "waiting";
	case NetworkSession::State::Playing:
		return "playing";
	case NetworkSession::State::Reconnecting:
		return "reconnecting";
	case NetworkSession::State::Ended:
		return "ended";
	}
	return "unknown";
}

std::string_view EndName(NetworkSession::End end) noexcept
{
	switch (end)
	{
	case NetworkSession::End::None:
		return "none";
	case NetworkSession::End::Refused:
		return "refused";
	case NetworkSession::End::Left:
		return "left";
	case NetworkSession::End::OutOfSync:
		return "out of sync";
	case NetworkSession::End::Lost:
		return "lost";
	case NetworkSession::End::NoConnection:
		return "no connection";
	}
	return "unknown";
}

} // namespace openblack::online
