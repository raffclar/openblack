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
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <spdlog/spdlog.h>

#include "Online/StartRules.h"
#include "PlayerSlots.h"

namespace openblack::online
{
namespace
{

/// The checksums that the client keeps for a compare, by turn
constexpr uint32_t k_KeptChecksums = 600;
/// The client writes a line of the compares to the log after this many turns
constexpr uint32_t k_CompareLogEvery = 100;

/// The build id that a server gave, for the log: at most 64 characters. Each character that the log cannot show becomes
/// '?'.
[[nodiscard]] std::string Printable(std::string_view build)
{
	constexpr size_t k_MostCharacters = 64;
	std::string printable;
	for (const char c : build.substr(0, k_MostCharacters))
	{
		printable += c >= 0x20 && c < 0x7F ? c : '?';
	}
	return printable;
}

/// The name of the land of a start, when the start loads a land of a file. The testbed and the map for 32 gods have no
/// file.
[[nodiscard]] std::optional<std::string_view> LandFileOf(const game_socket::Start& start)
{
	// A start with no land name opens no file
	if (start.testbed || start.godsRingMap || start.land.empty())
	{
		return std::nullopt;
	}
	return start.land;
}

/// The name of the land that a message of the server gives, when it gives a land of a file
[[nodiscard]] std::optional<std::string_view> LandFileOf(const game_socket::Frame& frame)
{
	if (const auto* joined = std::get_if<game_socket::Joined>(&frame); joined != nullptr && joined->start.has_value())
	{
		return LandFileOf(*joined->start);
	}
	if (const auto* start = std::get_if<game_socket::Start>(&frame))
	{
		return LandFileOf(*start);
	}
	if (const auto* query = std::get_if<game_socket::StartQuery>(&frame); query != nullptr && !query->testbed)
	{
		return query->land;
	}
	return std::nullopt;
}

/// The god slot of a player, as the key of the start files. The neutral player has no slot.
[[nodiscard]] std::optional<uint8_t> SlotKey(PlayerNames player)
{
	const auto slot = player_slots::SlotOfPlayer(player);
	return slot.has_value() ? std::optional(static_cast<uint8_t>(*slot)) : std::nullopt;
}

} // namespace

NetworkSession::NetworkSession(WebSocketClientInterface& sockets, Clock clock, std::shared_ptr<ConnectionLog> log)
    : _sockets(sockets)
    , _clock(std::move(clock))
    , _log(log != nullptr ? std::move(log) : std::make_shared<ConnectionLog>(_clock))
{
}

int64_t NetworkSession::MillisecondsSince(int64_t time) const
{
	return (_clock() - time) / 1000;
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
	_roomChat.clear();
	_startCreatures.clear();
	_startAlignments.clear();
	_ownGodPlaysForClan = false;
	_turns.clear();
	_waitingCommands = 0;
	_playout.Reset();
	_turnsDone = 0;
	// A new session counts its blocks of commands from 0
	_sent.Reset();
	_attempts = 0;
	_partedTurn.reset();
	_pingsWarned = false;
	Open(false);
}

void NetworkSession::Open(bool resume)
{
	++_generation;
	const auto generation = _generation;
	_stream = std::make_unique<TurnStream>(_sockets, [this] { return static_cast<uint64_t>(_clock()); });
	_state = resume ? State::Reconnecting : State::Connecting;
	_connectAt = _clock();
	++_attempts;
	_log->Info(fmt::format("The game socket opens to {} for a {} (attempt {})", _settings.request.url,
	                       resume ? "resume" : "join", _attempts));
	_stream->Open(
	    _settings.request,
	    TurnStreamHandlers {
	        .opened =
	            [this, generation, resume] {
		            if (generation != _generation)
		            {
			            return;
		            }
		            _openedAt = _clock();
		            _log->Info(fmt::format("The game socket is open after {} ms", MillisecondsSince(_connectAt)));
		            if (resume && _joined.has_value())
		            {
			            _stream->SendText(game_socket::ToJson(
			                game_socket::Resume {.session = _joined->session, .turn = _turnsDone, .world = _wantsWorld}));
			            _log->Info(fmt::format("The client asks to resume from turn {}{}", _turnsDone,
			                                   _wantsWorld ? ", with the server's world" : ""));
		            }
		            else
		            {
			            _stream->SendText(game_socket::ToJson(game_socket::Join {.ticket = _settings.ticket,
			                                                                     .gamePassword = _settings.gamePassword,
			                                                                     .name = _settings.name,
			                                                                     .build = _settings.build}));
			            _state = State::Joining;
			            _log->Info(fmt::format("The client sends its join ({}{})",
			                                   _settings.ticket.empty() ? "no ticket" : "with a ticket",
			                                   _settings.gamePassword.empty() ? "" : " and a game password"));
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

void NetworkSession::SendRoomChat(std::string_view text, std::string_view to, bool team)
{
	if (_state == State::Waiting && _stream && _stream->IsOpen())
	{
		_stream->SendText(game_socket::ToJson(game_socket::RoomChat {
		    .from = {}, .text = std::string(text), .to = std::string(to), .privateLine = false, .team = team}));
	}
}

std::vector<game_socket::RoomChat> NetworkSession::TakeRoomChat()
{
	return std::exchange(_roomChat, {});
}

void NetworkSession::Leave()
{
	if (_stream && _stream->IsOpen())
	{
		_stream->SendText(game_socket::ToJson(game_socket::Leave {}));
	}
	FinishWith(End::Left);
}

void NetworkSession::StopGame()
{
	// The request goes once. A request that cannot go now waits for no socket: another computer stops the turns, or the
	// player leaves.
	if (_stopAsked || _stoppedAfter.has_value())
	{
		return;
	}
	if (_stream && _stream->IsOpen())
	{
		_stopAsked = true;
		_stream->SendText(game_socket::ToJson(game_socket::StopRequest {}));
	}
}

void NetworkSession::DropStream()
{
	// The handlers of the old socket do nothing more. The socket closes in the next update, never inside its own handler.
	++_generation;
	_dropStream = true;
}

void NetworkSession::BreaksLimit(std::string_view what)
{
	_log->Write(spdlog::level::err,
	            fmt::format("The game server breaks a limit of the client: {}. The client leaves the game.", what));
	FinishWith(End::ServerLimit);
}

size_t NetworkSession::CommandsIn(const turn_codec::TurnBundle& turn)
{
	size_t commands = 0;
	for (const auto& block : turn.blocks)
	{
		commands += block.actions.size();
	}
	return commands;
}

void NetworkSession::CountWaitingCommands()
{
	_waitingCommands = 0;
	for (const auto& [number, turn] : _turns)
	{
		_waitingCommands += CommandsIn(turn);
	}
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
	// A server names a land only by a plain name of a land of the game. The client opens no file for another name.
	if (const auto land = LandFileOf(*frame); land.has_value() && !start_rules::IsLandPath(*land))
	{
		BreaksLimit("the land of the game is not a plain name of a land of the game");
		return;
	}
	if (const auto* joined = std::get_if<game_socket::Joined>(&*frame))
	{
		_joined = *joined;
		_state = State::Waiting;
		_backoff.Reset();
		_attempts = 0;
		_log->Info(fmt::format("The server accepted the join after {} ms", MillisecondsSince(_openedAt)));
		// The test tools find the words "Joined the game server as", and a line that ends with the slot
		_log->Info(fmt::format("Joined the game server as {}{}{}", game_socket::RoleName(joined->role),
		                       joined->observer != 0 ? fmt::format(" (observer {})", joined->observer) : "",
		                       joined->slot.has_value() ? fmt::format(" in slot {}", *joined->slot) : ""));
		if (_ready)
		{
			_stream->SendText(game_socket::ToJson(game_socket::Ready {.ready = true}));
		}
	}
	else if (const auto* start = std::get_if<game_socket::Start>(&*frame))
	{
		_start = *start;
		_state = State::Playing;
		_log->Write(spdlog::level::info, fmt::format("The network game starts at turn {}", start->turn));
		_firstTurnDue = true;
		_firstTurnFrom = _clock();
		SendWaiting();
	}
	else if (const auto* resumed = std::get_if<game_socket::Resumed>(&*frame))
	{
		_state = _start.has_value() ? State::Playing : State::Waiting;
		_backoff.Reset();
		++_resumes;
		_attempts = 0;
		_firstTurnDue = true;
		_firstTurnFrom = _clock();
		if (_resync.GetState() == ResyncRule::State::Resyncing)
		{
			_resync.Rejoined();
		}
		_log->Write(spdlog::level::info,
		            fmt::format("Back in the game at turn {}; the server is at turn {} ({} ms after the loss)", _turnsDone,
		                        resumed->turn, MillisecondsSince(_lostAt)));
		if (resumed->world)
		{
			// The server's world state of its turn comes first, and the turns from there
			_wantsWorld = false;
			_worldTurn = resumed->turn;
			_worldParts.clear();
			_worldBytes = 0;
			_worldQuartersLogged = 0;
			_worldStartedAt = _clock();
			_world.reset();
			_turns.clear();
			_waitingCommands = 0;
			_playout.Reset();
		}
		SendKeptAgain();
		SendWaiting();
		SendPartedParts();
	}
	else if (const auto* stop = std::get_if<game_socket::GameStop>(&*frame))
	{
		_stoppedAfter = stop->turn;
		_log->Write(spdlog::level::info,
		            fmt::format("The game server stopped the turns of the game after turn {}", stop->turn));
	}
	else if (const auto* left = std::get_if<game_socket::ObserverLeft>(&*frame))
	{
		_observersLeft.push_back(left->name);
		_log->Write(spdlog::level::info, fmt::format("The observer {} left the game", left->name));
	}
	else if (const auto* refused = std::get_if<game_socket::Refused>(&*frame))
	{
		_refusedReason = refused->reason;
		// A refusal for the protocol or the version gives the build of the server: the log names the two builds
		const auto builds =
		    refused->build.empty()
		        ? std::string()
		        : fmt::format("; {}, the server has the build \"{}\"",
		                      _settings.build.empty() ? std::string("the client has no build id")
		                                              : fmt::format("the client has the build \"{}\"", _settings.build),
		                      Printable(refused->build));
		_log->Write(spdlog::level::warn,
		            fmt::format("The game server refused the client: {} (state {}, {} ms after the socket opened){}",
		                        refused->reason, StateName(_state), MillisecondsSince(_openedAt), builds));
		FinishWith(End::Refused);
	}
	else if (const auto* parts = std::get_if<game_socket::SyncParts>(&*frame))
	{
		// The server's parts of the turn at which the worlds parted: the log names the parts that differ
		const auto local = _settings.localParts ? _settings.localParts(parts->turn) : PartHashes {};
		if (local.empty())
		{
			_log->Warn(
			    fmt::format("The server sent its parts of turn {}; this computer no longer keeps that turn", parts->turn));
		}
		else
		{
			_log->Warn(fmt::format("The worlds parted at turn {}: {}", parts->turn,
			                       DescribeParts(DifferentParts(local, parts->parts), "this", "server")));
		}
	}
	else if (const auto* chat = std::get_if<game_socket::RoomChat>(&*frame))
	{
		// The chat of the room ends when the game starts
		if (_state == State::Waiting)
		{
			_roomChat.push_back(*chat);
		}
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
		_log->Write(spdlog::level::info, reply.have ? fmt::format("This computer has the land of the game ({})", query->land)
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
		_startAlignments.clear();
		// An observer has no god, so it has no creature to send
		if (_joined.has_value() && _joined->role == game_socket::Role::God)
		{
			auto file = _settings.readCreature ? _settings.readCreature() : std::vector<uint8_t> {};
			if (file.size() > game_socket::k_MostCreatureFileBytes)
			{
				_log->Write(spdlog::level::warn,
				            fmt::format("The creature file of {} bytes is too large to send", file.size()));
				file.clear();
			}
			// The god starts the game at the alignment of the player's profile
			const auto alignment = _settings.readAlignment ? _settings.readAlignment() : 0.0f;
			_ownGodPlaysForClan = _settings.readPlaysForClan && _settings.readPlaysForClan();
			_log->Write(spdlog::level::info, fmt::format("This computer sends a creature file of {} bytes and the alignment {}",
			                                             file.size(), alignment));
			_stream->SendText(game_socket::ToJson(
			    game_socket::StartCreature {.slot = std::nullopt, .file = std::move(file), .alignment = alignment}));
		}
		_startEvents.push_back(StartEvent::Transfer);
	}
	else if (const auto* creature = std::get_if<game_socket::StartCreature>(&frame))
	{
		if (creature->slot.has_value())
		{
			_startCreatures[*creature->slot] = creature->file;
			_startAlignments[*creature->slot] = creature->alignment;
		}
	}
	else if (std::holds_alternative<game_socket::StartCancel>(frame))
	{
		_log->Write(spdlog::level::info, "A player does not have the land: the start stops");
		_startCreatures.clear();
		_startAlignments.clear();
		_startEvents.push_back(StartEvent::Cancelled);
	}
	else if (std::holds_alternative<game_socket::StartStop>(frame))
	{
		_log->Write(spdlog::level::info, "A player left during the start: the start stops");
		_startCreatures.clear();
		_startAlignments.clear();
		_startEvents.push_back(StartEvent::Stopped);
	}
}

std::span<const uint8_t> NetworkSession::StartCreatureFile(PlayerNames god) const
{
	const auto slot = SlotKey(god);
	const auto file = slot.has_value() ? _startCreatures.find(*slot) : _startCreatures.end();
	return file != _startCreatures.end() ? std::span<const uint8_t>(file->second) : std::span<const uint8_t> {};
}

std::optional<float> NetworkSession::StartAlignment(PlayerNames god) const
{
	const auto slot = SlotKey(god);
	const auto alignment = slot.has_value() ? _startAlignments.find(*slot) : _startAlignments.end();
	return alignment != _startAlignments.end() ? std::optional<float>(alignment->second) : std::nullopt;
}

std::optional<uint8_t> NetworkSession::OwnObserver() const
{
	if (!_joined.has_value() || _joined->role != game_socket::Role::Observer || _joined->observer == 0)
	{
		return std::nullopt;
	}
	return _joined->observer;
}

bool NetworkSession::SendsCommands() const
{
	return _joined.has_value() && (_joined->role == game_socket::Role::God || OwnObserver().has_value());
}

std::optional<PlayerNames> NetworkSession::OwnGod() const
{
	if (!_joined.has_value() || _joined->role != game_socket::Role::God || !_joined->slot.has_value())
	{
		return std::nullopt;
	}
	return player_slots::PlayerOfSlot(*_joined->slot);
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
	const auto lastFrame = _stream ? _stream->LastReceived() : std::nullopt;
	const auto unanswered = _stream ? _stream->UnansweredPings() : 0;
	_log->Write(_state == State::Playing ? spdlog::level::warn : spdlog::level::info,
	            fmt::format("The game socket closed: {}. State {}, open for {} s{}{}", DescribeClosed(closed),
	                        StateName(_state), _openedAt != 0 ? MillisecondsSince(_openedAt) / 1000 : 0,
	                        lastFrame.has_value()
	                            ? fmt::format(", last frame {} ms before", MillisecondsSince(static_cast<int64_t>(*lastFrame)))
	                            : std::string(", no frame came"),
	                        unanswered > 1 ? fmt::format(", {} pings with no answer", unanswered - 1) : std::string()));
	if (_joined.has_value() && _state != State::Ended)
	{
		// The client keeps its world and tries to come back
		if (_state != State::Reconnecting)
		{
			_lostAt = _clock();
		}
		_state = State::Reconnecting;
		const auto wait = std::chrono::duration_cast<std::chrono::microseconds>(_backoff.Next()).count();
		_nextAttempt = _clock() + wait;
		_log->Info(fmt::format("The client tries to come back in {} ms", wait / 1000));
		DropStream();
		return;
	}
	_log->Warn(fmt::format("The socket closed before the server accepted the client (state {})", StateName(_state)));
	FinishWith(End::NoConnection);
}

void NetworkSession::ComeBack(bool resync)
{
	if (resync)
	{
		_log->Write(spdlog::level::warn,
		            fmt::format("Resynchronising... (try {} of {})", _resync.Tries(), ResyncRule::k_MostTries));
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
			_log->Write(spdlog::level::warn,
			            fmt::format("The client did not come back to the game server in {} s ({} attempts)",
			                        _settings.comeBackMicroseconds / 1'000'000, _attempts));
			FinishWith(End::Lost);
			return;
		}
		if (now >= _nextAttempt)
		{
			// The server sends again the turns after the last turn that the world did
			std::erase_if(_turns, [this](const auto& entry) { return entry.first >= _turnsDone; });
			CountWaitingCommands();
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
	LogHealth(now);
}

void NetworkSession::LogHealth(int64_t now)
{
	if (_state != State::Playing || !_stream || !_stream->IsOpen())
	{
		return;
	}
	// The ping that went last has had no time for its answer
	const auto unanswered = _stream->UnansweredPings();
	const auto missed = unanswered > 0 ? unanswered - 1 : 0;
	if (missed >= k_MissedPingsWarning && !_pingsWarned)
	{
		_pingsWarned = true;
		_log->Warn(fmt::format("{} pings in a row have no answer from the server", missed));
	}
	else if (missed == 0)
	{
		_pingsWarned = false;
	}
	if (now < _nextHealthLog)
	{
		return;
	}
	_nextHealthLog = now + k_HealthLogMicroseconds;
	const auto received = LastTurnReceived();
	const auto roundTrip = RoundTrip();
	const auto lag = received.has_value() ? static_cast<int64_t>(*received) + 1 - static_cast<int64_t>(_turnsDone) : 0;
	_log->Info(
	    fmt::format("Health: turn {} done, server turn {} (lag {}), {} turns wait, round trip {}, {} pings with no answer",
	                _turnsDone, received.has_value() ? std::to_string(*received) : std::string("none"), lag, _turns.size(),
	                roundTrip.has_value() ? fmt::format("{} ms", *roundTrip / 1000) : std::string("none"), missed));
}

void NetworkSession::SendPartedParts()
{
	if (!_partedTurn.has_value() || !_stream || !_stream->IsOpen())
	{
		return;
	}
	const auto turn = *std::exchange(_partedTurn, std::nullopt);
	auto parts = _settings.localParts ? _settings.localParts(turn) : PartHashes {};
	if (parts.empty())
	{
		_log->Info(fmt::format("This computer does not keep the parts of turn {}: it sends no parts", turn));
		return;
	}
	if (parts.size() > game_socket::k_MostSyncParts)
	{
		parts.resize(game_socket::k_MostSyncParts);
	}
	_log->Info(fmt::format("The client sends the {} parts of its world at turn {} to the server", parts.size(), turn));
	_stream->SendText(game_socket::ToJson(game_socket::SyncParts {.turn = turn, .parts = std::move(parts)}));
}

void NetworkSession::SendWaiting()
{
	if (!_stream || !_stream->IsOpen() || !SendsCommands())
	{
		return;
	}
	for (size_t i = 0; i < _waiting.size(); i += turn_codec::k_MostBlockCommands)
	{
		const auto count = std::min(turn_codec::k_MostBlockCommands, _waiting.size() - i);
		SendNewBlock(std::span(_waiting).subspan(i, count));
	}
	_waiting.clear();
}

bool NetworkSession::SendNewBlock(std::span<const player_commands::Action> actions)
{
	auto block = _sent.Make(_turnsDone, actions);
	if (!_stream || !_stream->SendBlock(block))
	{
		return false;
	}
	// The server refuses the commands before the start, so the client keeps only the blocks of the game
	_sent.Sent(std::move(block), _state == State::Playing);
	return true;
}

void NetworkSession::SendKeptAgain()
{
	const auto& kept = _sent.Kept();
	if (kept.empty() || !_stream || !_stream->IsOpen() || !SendsCommands())
	{
		return;
	}
	size_t sent = 0;
	for (const auto& block : kept)
	{
		if (_stream->SendBlock(block))
		{
			++sent;
		}
	}
	_log->Write(spdlog::level::info,
	            fmt::format("The client sends again {} blocks of commands that no turn holds (blocks {} to {})", sent,
	                        kept.front().sequence, kept.back().sequence));
}

void NetworkSession::Send(const player_commands::Action& action)
{
	// An observer sends only the report of its hand and camera and the lines of the chat. An observer with no number of
	// an observer sends no commands.
	if (_joined.has_value() &&
	    (!SendsCommands() || (_joined->role == game_socket::Role::Observer && !player_commands::ObserverSends(action))))
	{
		return;
	}
	if ((_state == State::Playing || _state == State::Waiting) && _stream && _stream->IsOpen())
	{
		const std::array actions {action};
		if (SendNewBlock(actions))
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
	if (_stoppedAfter.has_value() && turn > *_stoppedAfter)
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
			commands.push_back(
			    player_commands::Command {.player = block.sender, .action = std::move(action), .observer = block.observer});
		}
	}
	for (auto done = _turns.begin(); done != std::next(found); ++done)
	{
		_waitingCommands -= std::min(_waitingCommands, CommandsIn(done->second));
	}
	_turns.erase(_turns.begin(), std::next(found));
	_playout.Done(turn);
	_turnsDone = turn + 1;
	if (std::exchange(_firstTurnDue, false))
	{
		_log->Info(fmt::format("The world did turn {}, the first turn {} ms after the {}", turn,
		                       MillisecondsSince(_firstTurnFrom), _resumes > 0 ? "resume" : "start"));
	}
	return commands;
}

void NetworkSession::Turn(const turn_codec::TurnBundle& turn)
{
	// The blocks of this player that the turn holds do not go again. A turn that the client had still tells this.
	if (const auto god = OwnGod(); god.has_value())
	{
		_sent.HeldIn(turn, *god);
	}
	else if (const auto observer = OwnObserver(); observer.has_value())
	{
		_sent.HeldInOfObserver(turn, *observer);
	}
	// While the world of the server comes, the session keeps the turns from the turn of that world. After a restart of the
	// server, that turn can come before the turns that this computer did.
	const auto first = _worldTurn.value_or(_turnsDone);
	if (turn.turn < first || _turns.contains(turn.turn))
	{
		return;
	}
	// A server that sends more turns than the client can keep breaks a limit: the client leaves the game
	const auto commands = CommandsIn(turn);
	if (turn.turn - first >= k_MostWaitingTurns || _turns.size() >= k_MostWaitingTurns ||
	    _waitingCommands + commands > k_MostWaitingCommands)
	{
		BreaksLimit(fmt::format("turn {} goes over the turns that the client keeps", turn.turn));
		return;
	}
	_playout.Arrived(turn.turn, _clock());
	_turns.emplace(turn.turn, turn);
	_waitingCommands += commands;
}

void NetworkSession::WorldPart(const turn_codec::WorldPart& part)
{
	if (!_worldTurn.has_value() || part.turn != *_worldTurn || part.part != _worldParts.size())
	{
		return;
	}
	// Each part of a world state gives the same count of parts, and the whole state has a size that the client can keep
	if (part.part == 0)
	{
		_worldPartCount = part.parts;
		_log->Info(fmt::format("The world state of turn {} starts to come: {} parts, {} ms after the resume", part.turn,
		                       part.parts, MillisecondsSince(_worldStartedAt)));
		_worldStartedAt = _clock();
	}
	if (part.parts != _worldPartCount || static_cast<size_t>(part.parts) * turn_codec::k_WorldPartBytes > k_MostWorldBytes)
	{
		_worldParts.clear();
		BreaksLimit(fmt::format("the world state of turn {} has {} parts", part.turn, part.parts));
		return;
	}
	_worldParts.push_back(part.bytes);
	_worldBytes += part.bytes.size();
	// A line at each quarter of the parts, so that a stop in the middle of the world shows where it stopped
	if (const auto quarter = _worldParts.size() * 4 / part.parts; quarter > _worldQuartersLogged && quarter < 4)
	{
		_worldQuartersLogged = quarter;
		_log->Info(fmt::format("World state: {} of {} parts, {} bytes, {} ms", _worldParts.size(), part.parts, _worldBytes,
		                       MillisecondsSince(_worldStartedAt)));
	}
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
	_log->Write(spdlog::level::info, fmt::format("The world state of turn {} came from the game server ({} bytes in {} ms)",
	                                             world.turn, world.bytes.size(), MillisecondsSince(_worldStartedAt)));
	_worldCameAt = _clock();
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
	CountWaitingCommands();
	// The checksums of the parted world are of no use
	_localChecksums.clear();
	std::erase_if(_serverChecksums, [turn](const auto& entry) { return entry.first <= turn; });
	_log->Write(spdlog::level::info, fmt::format("The world goes on from the server's world of turn {} ({} ms after it came)",
	                                             turn, MillisecondsSince(_worldCameAt)));
	_firstTurnDue = true;
	_firstTurnFrom = _clock();
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
	const auto localChecksum = local->second;
	const auto serverChecksum = server->second;
	const bool same = localChecksum == serverChecksum;
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
		_log->Write(spdlog::level::warn,
		            fmt::format("The world parts from the server's world at turn {} (this {:016x}, server {:016x})", turn,
		                        localChecksum, serverChecksum));
		_partedTurn = turn;
		if (_settings.parts)
		{
			_settings.parts(turn);
		}
		switch (_resync.Differ(turn))
		{
		case ResyncRule::Action::Rejoin:
			ComeBack(true);
			break;
		case ResyncRule::Action::GiveUp:
			_log->Write(spdlog::level::err, "The resyncs did not work: the client leaves the game (out of sync)");
			FinishWith(End::OutOfSync);
			break;
		case ResyncRule::Action::None:
			break;
		}
	}
	if (_compares.compared % k_CompareLogEvery == 0)
	{
		_log->Write(spdlog::level::info, fmt::format("Network game: {} turns compared with the server, {} the same",
		                                             _compares.compared, _compares.same));
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
	case NetworkSession::End::ServerLimit:
		return "server broke a limit";
	}
	return "unknown";
}

} // namespace openblack::online
