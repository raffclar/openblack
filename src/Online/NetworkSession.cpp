/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NetworkSession.h"

#include <cstddef>

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <spdlog/spdlog.h>

#include "Online/EditorWorldUpload.h"
#include "Online/GameDataPrint.h"
#include "Online/JoinRetryRule.h"
#include "Online/RoomClock.h"
#include "Online/Sha256.h"
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
	_mindFiles.Clear();
	_restarted = false;
	_landOfRestartDue = false;
	_scriptFiles.Clear();
	_scriptParts = {};
	_ownGodPlaysForClan = false;
	_turns.clear();
	_nextLands.clear();
	_waitingCommands = 0;
	_playout.Reset();
	_turnsDone = 0;
	_loaded = false;
	_loadedSent = false;
	_loadedDueAt.reset();
	_loadWait.reset();
	_commandsInLoadWait = 0;
	_roomClock.reset();
	_timeNotices.clear();
	// A new session counts its blocks of commands from 0
	_sent.Reset();
	_joinStartedAt = _clock();
	_joinRetryAt.reset();
	_backoff.Reset();
	_attempts = 0;
	_partedTurn.reset();
	_pingsWarned = false;
	_heardAt.reset();
	_silence = {};
	_commandsInSilence = 0;
	Open(false);
}

void NetworkSession::Open(bool resume)
{
	++_generation;
	const auto generation = _generation;
	_stream = std::make_unique<TurnStream>(_sockets, [this] { return static_cast<uint64_t>(_clock()); });
	_state = resume ? State::Reconnecting : State::Connecting;
	_connectAt = _clock();
	// The log of a socket that closes before it opened must not give the time of the last socket
	_openedAt = 0;
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
			                                                                     .build = _settings.build,
			                                                                     .map = _settings.map}));
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

bool NetworkSession::SendEditorScriptFile(game_socket::EditorScriptFile::Kind kind, std::vector<uint8_t> file)
{
	if (!_start.has_value() || !_stream || !_stream->IsOpen() || file.empty() ||
	    file.size() > game_socket::k_MostEditorScriptFileBytes)
	{
		return false;
	}
	const auto parts = editor::script_files::SplitFile(kind, file);
	_log->Write(spdlog::level::info, fmt::format("This computer sends a script file of the editor of {} bytes in {} parts",
	                                             file.size(), parts.size()));
	return std::ranges::all_of(parts, [this](const auto& part) { return _stream->SendText(game_socket::ToJson(part)); });
}

bool NetworkSession::SendEditorMindFile(std::vector<uint8_t> file)
{
	if (!_start.has_value() || !_stream || !_stream->IsOpen() || file.empty() ||
	    file.size() > game_socket::k_MostEditorMindFileBytes)
	{
		return false;
	}
	_log->Write(spdlog::level::info, fmt::format("This computer sends a mind file of the editor of {} bytes", file.size()));
	return _stream->SendText(game_socket::ToJson(game_socket::EditorMindFile {.file = std::move(file)}));
}

bool NetworkSession::SendEditorWorld(std::span<const uint8_t> file)
{
	if (!_start.has_value() || !_stream || !_stream->IsOpen())
	{
		return false;
	}
	const auto parts = editor_world::Split(file);
	if (parts.empty())
	{
		return false;
	}
	_log->Write(spdlog::level::info, fmt::format("This computer sends a world file of the editor of {} bytes in {} parts",
	                                             file.size(), parts.size()));
	// The parts go in order on the one socket. A part that does not go leaves the server with a broken file.
	return std::ranges::all_of(parts, [this](const auto& part) { return _stream->SendText(game_socket::ToJson(part)); });
}

bool NetworkSession::SendEditorScenario(std::string_view id)
{
	if (!_start.has_value() || !_stream || !_stream->IsOpen() || id.empty() || id.size() > game_socket::k_MostScenarioIdBytes)
	{
		return false;
	}
	_log->Write(spdlog::level::info, fmt::format("This computer asks the server to run the testbed scenario {}", id));
	return _stream->SendText(game_socket::ToJson(game_socket::EditorScenario {.id = std::string(id)}));
}

bool NetworkSession::SendEditorLand(std::string_view land, bool testbed)
{
	// A request names a land or the testbed, not both
	if (!_start.has_value() || !_stream || !_stream->IsOpen() || land.empty() == !testbed ||
	    land.size() > game_socket::k_MostLandNameBytes)
	{
		return false;
	}
	_log->Write(spdlog::level::info, fmt::format("This computer asks the server to load {}",
	                                             testbed ? std::string("the testbed") : std::string(land)));
	return _stream->SendText(game_socket::ToJson(game_socket::EditorLand {.land = std::string(land), .testbed = testbed}));
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

void NetworkSession::RequestTime(const game_socket::EditorTime& request)
{
	// A request that cannot go now does not wait: the editor sends it again
	if (_stream && _stream->IsOpen())
	{
		_stream->SendText(game_socket::ToJson(request));
	}
}

bool NetworkSession::GoAwayForTest(int64_t microseconds)
{
	if (_state != State::Playing)
	{
		return false;
	}
	_log->Write(spdlog::level::warn,
	            fmt::format("Tests: the client drops its socket and comes back after {} ms", microseconds / 1000));
	DropStream();
	_lostAt = _clock();
	_state = State::Reconnecting;
	_nextAttempt = _lostAt + microseconds;
	return true;
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
	for (const auto& land : _nextLands)
	{
		for (const auto& [number, turn] : land.turns)
		{
			_waitingCommands += CommandsIn(turn);
		}
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
		// The server names the files of the game data that its world read: the client sends the print of its own files
		if (!joined->dataFiles.empty() && _settings.printData && _stream)
		{
			const auto print = _settings.printData(joined->dataFiles);
			std::vector<game_data::FilePrint> files;
			files.reserve(print.files.size());
			for (const auto& file : print.files)
			{
				files.push_back({.path = file.path, .present = file.present, .size = file.size, .hash = file.hash});
			}
			_dataHash = game_data::HashOfPrint(files);
			_dataPatch = print.patch;
			_dataLanguage = print.language;
			_log->Info(
			    fmt::format("The client sends the print of {} files of its game data (hash {:016x})", files.size(), _dataHash));
			_stream->SendText(game_socket::ToJson(print));
		}
		if (_ready)
		{
			_stream->SendText(game_socket::ToJson(game_socket::Ready {.ready = true}));
		}
	}
	else if (const auto* start = std::get_if<game_socket::Start>(&*frame))
	{
		// A start in the game: the editor of a player started the room again on a scenario or a land. The turns, the checksums
		// and the stop of the old game are of no use.
		if (_start.has_value())
		{
			_restarted = true;
			_landOfRestartDue = true;
			// The server takes the checksums of the new game after the report that the land of the new start loaded
			_loaded = false;
			_loadedSent = false;
			_turns.clear();
			_nextLands.clear();
			// The commands of the old game go to no turn of the new game, also after a come back
			_sent.DropKept();
			_waitingCommands = 0;
			_playout.Reset();
			_turnsDone = start->turn;
			_localChecksums.clear();
			_serverChecksums.clear();
			_stoppedAfter.reset();
			_worldTurn.reset();
			_worldParts.clear();
			_world.reset();
			// The test tools find the words "on the scenario" and "on the land"
			std::string on;
			if (start->scenario.has_value())
			{
				on = fmt::format(" on the scenario {}", *start->scenario);
			}
			else if (start->testbed)
			{
				on = " on the land of the testbed";
			}
			else if (!start->land.empty())
			{
				on = fmt::format(" on the land {}", start->land);
			}
			_log->Write(spdlog::level::info, fmt::format("The game server started the game again{}", on));
		}
		_start = *start;
		_state = State::Playing;
		_log->Write(spdlog::level::info, fmt::format("The network game starts at turn {}", start->turn));
		_firstTurnDue = true;
		_firstTurnFrom = _clock();
		SendWaiting();
		SendLoaded();
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
		// The server may not have the report of the load from before the loss. It takes a second report as one.
		_loadedSent = false;
		SendLoaded();
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
			// After a world file of the editor, the server's world can be at an earlier turn: no checksum of the old world
			// stays to compare
			_serverChecksums.clear();
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
	else if (const auto* again = std::get_if<game_socket::TurnsAgain>(&*frame))
	{
		// The turns and the checksums that come next are of the new land. This computer loads the land before the same turn.
		_nextLands.push_back(NextLand {.after = again->after, .turn = again->turn});
		_log->Write(spdlog::level::info,
		            fmt::format("Before turn {}, the world of the game server loaded a land: its turns start again at turn {}",
		                        again->after, again->turn));
	}
	else if (const auto* wait = std::get_if<game_socket::LoadWait>(&*frame))
	{
		if (wait->loading.empty())
		{
			_loadWait.reset();
			_log->Write(spdlog::level::info, "Each player loaded the world: the turns start");
		}
		else
		{
			_loadWait = *wait;
			_loadWaitAt = _clock();
			_log->Write(spdlog::level::info, fmt::format("The game is paused: {} players load the world, {} seconds left",
			                                             wait->loading.size(), wait->secondsLeft));
		}
	}
	else if (const auto* left = std::get_if<game_socket::ObserverLeft>(&*frame))
	{
		_observersLeft.push_back(left->name);
		_log->Write(spdlog::level::info, fmt::format("The observer {} left the game", left->name));
	}
	else if (const auto* mind = std::get_if<game_socket::EditorMindFile>(&*frame))
	{
		// The file comes before the turn of the command that uses it, on the same socket
		const auto size = mind->file.size();
		const auto [added, hash] = _mindFiles.Add(mind->file);
		if (added == editor::mind_loads::MindFileStore::Added::New)
		{
			_log->Write(spdlog::level::info,
			            fmt::format("The server sent a mind file of the editor of {} bytes (hash {:016x})", size, hash));
		}
		else if (added == editor::mind_loads::MindFileStore::Added::Refused)
		{
			_log->Write(spdlog::level::warn,
			            fmt::format("The session keeps the most mind files and does not keep the file {:016x}", hash));
		}
	}
	else if (const auto* time = std::get_if<game_socket::GameTime>(&*frame))
	{
		_roomClock = *time;
		++_roomClockChanges;
		if (auto notice = online::room_clock::NoticeOf(*time); notice.has_value())
		{
			_timeNotices.push_back(std::move(*notice));
		}
		_log->Write(spdlog::level::info,
		            fmt::format("The clock of the room after turn {}: {}, the time of a turn {:.2f}{}", time->turn,
		                        time->paused ? "paused" : "turns", time->speed,
		                        time->by.empty()
		                            ? std::string()
		                            : fmt::format(" ({} by {})", game_socket::TimeActName(time->act), Printable(time->by))));
	}
	else if (const auto* part = std::get_if<game_socket::EditorScriptFile>(&*frame))
	{
		// The parts come in order before the turn of the command that uses the file, on the same socket
		const auto step = _scriptParts.Add(*part);
		if (step == editor::script_files::PartAssembly::Step::Bad)
		{
			_log->Write(spdlog::level::warn,
			            fmt::format("A part of the script file {:016x} of the editor does not continue the file", part->hash));
		}
		else if (step == editor::script_files::PartAssembly::Step::Done)
		{
			auto file = _scriptParts.Take();
			const auto size = file->bytes.size();
			const auto [added, hash] = _scriptFiles.Add(file->kind, std::move(file->bytes));
			if (added == editor::script_files::FileStore::Added::New)
			{
				_log->Write(spdlog::level::info,
				            fmt::format("The server sent a script file of the editor of {} bytes (hash {:016x})", size, hash));
			}
			else if (added == editor::script_files::FileStore::Added::Refused)
			{
				_log->Write(spdlog::level::warn,
				            fmt::format("The session keeps the most script files and does not keep the file {:016x}", hash));
			}
		}
	}
	else if (const auto* right = std::get_if<game_socket::EditorRight>(&*frame))
	{
		// The lobby changed the editor right of the account while the game runs. The game follows the new right at once.
		if (_joined.has_value() && _joined->editor != right->editor)
		{
			_joined->editor = right->editor;
			// The test tools find the words "the editor right of this player"
			_log->Write(spdlog::level::info,
			            fmt::format("The game server {} the editor right of this player", right->editor ? "gave" : "removed"));
		}
	}
	else if (const auto* debugger = std::get_if<game_socket::DebuggerRight>(&*frame))
	{
		// The lobby changed the debugger right of the account while the game runs. The debug menu bar shows or hides at once.
		if (_joined.has_value() && _joined->debugger != debugger->debugger)
		{
			_joined->debugger = debugger->debugger;
			// The test tools find the words "the debugger right of this player"
			_log->Write(spdlog::level::info, fmt::format("The game server {} the debugger right of this player",
			                                             debugger->debugger ? "gave" : "removed"));
		}
	}
	else if (const auto* commands = std::get_if<game_socket::CommandRefused>(&*frame))
	{
		// The game shows a line to the player. A refusal for another reason shows the same line, as no other reason exists.
		++_editorRefusals;
		_log->Write(spdlog::level::warn, fmt::format("The game server refused {} commands of this player: {}",
		                                             commands->commands, Printable(commands->reason)));
	}
	else if (const auto* refused = std::get_if<game_socket::Refused>(&*frame))
	{
		_refusedReason = refused->reason;
		_refusedPath = refused->path;
		_refusedBuild = refused->build;
		// A refusal for the protocol or the version gives the build of the server: the log names the two builds
		const auto builds =
		    refused->build.empty()
		        ? std::string()
		        : fmt::format("; {}, the server has the build \"{}\"",
		                      _settings.build.empty() ? std::string("the client has no build id")
		                                              : fmt::format("the client has the build \"{}\"", _settings.build),
		                      Printable(refused->build));
		_log->Write(spdlog::level::warn,
		            fmt::format("The game server refused the client: {} (state {}, {} ms after the socket opened){}{}",
		                        refused->reason, StateName(_state), MillisecondsSince(_openedAt), builds,
		                        refused->file.empty() ? std::string() : fmt::format("; {}", Printable(refused->file))));
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
			const auto differences = DifferentParts(local, parts->parts);
			_log->Warn(
			    fmt::format("The worlds parted at turn {}: {}", parts->turn, DescribeParts(differences, "this", "server")));
			if (_settings.partedParts)
			{
				std::vector<std::string> names;
				names.reserve(differences.size());
				std::ranges::transform(differences, std::back_inserter(names), &PartDifference::name);
				_settings.partedParts(names);
			}
		}
	}
	else if (std::holds_alternative<game_socket::SyncCapture>(*frame) || std::holds_alternative<game_socket::SyncAsk>(*frame))
	{
		CaptureFrame(*frame);
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
	// Only a pool server of the lobby can listen after the client has its ticket. A server on the local network listens
	// before the search finds it.
	if (_state == State::Connecting && _settings.origin == GameOrigin::InternetLobby &&
	    join_retry::TryAgain(closed, _clock() - _joinStartedAt, _settings.joinRetryMicroseconds))
	{
		const auto wait = std::chrono::duration_cast<std::chrono::microseconds>(_backoff.Next()).count();
		_joinRetryAt = _clock() + wait;
		_log->Info(fmt::format("The game server does not listen yet: the client tries again in {} ms", wait / 1000));
		DropStream();
		return;
	}
	_log->Warn(fmt::format("The socket closed before the server accepted the client (state {})", StateName(_state)));
	FinishWith(End::NoConnection);
}

game_socket::SyncFacts NetworkSession::OwnFacts(uint32_t turn) const
{
	game_socket::SyncFacts facts {
	    .turn = turn,
	    .build = _settings.build,
	    .land = {},
	    .landSha256 = {},
	    .map = _settings.map,
	    .seed = 0,
	    .data = _dataHash,
	    .patch = _dataPatch,
	    .language = _dataLanguage,
	};
	const auto& start = _start.has_value() ? _start : (_joined.has_value() ? _joined->start : std::nullopt);
	if (start.has_value())
	{
		facts.land = start->land;
		facts.seed = start->seed;
		if (!start->land.empty() && _settings.readLand && start_rules::IsLandPath(start->land))
		{
			if (const auto file = _settings.readLand(start->land); file.has_value())
			{
				facts.landSha256 = sha256::HexOf(*file);
			}
		}
	}
	return facts;
}

void NetworkSession::CaptureFrame(const game_socket::Frame& frame)
{
	if (!_capture.has_value() || !_stream)
	{
		return;
	}
	if (const auto* capture = std::get_if<game_socket::SyncCapture>(&frame))
	{
		// A second frame with no parts tells that the server has all of the capture
		if (_capture->turn.has_value())
		{
			if (capture->parts.empty() && capture->turn == *_capture->turn)
			{
				ResyncAfterCapture("the server has the capture");
			}
			return;
		}
		// The world must do the turn of the capture after the frame came
		if (capture->turn < _turnsDone)
		{
			ResyncAfterCapture(fmt::format("the turn {} of the capture is past", capture->turn));
			return;
		}
		_capture->turn = capture->turn;
		_capture->parts = capture->parts;
		_log->Info(fmt::format("The server captures the desync at turn {} ({} parts)", capture->turn, capture->parts.size()));
	}
	else if (const auto* ask = std::get_if<game_socket::SyncAsk>(&frame))
	{
		if (!_capture->sent || !_capture->turn.has_value() || ask->turn != *_capture->turn)
		{
			return;
		}
		game_socket::SyncEntities entities {.turn = ask->turn, .parts = {}};
		for (const auto& part : ask->parts)
		{
			const auto own = std::ranges::find(_capture->entities, part.part, &synced_state::PartEntities::part);
			if (own == _capture->entities.end())
			{
				continue;
			}
			auto& out = entities.parts.emplace_back(game_socket::PartEntityHashes {.part = part.part, .entities = {}});
			for (auto& entity : synced_state::EntitiesOfBlocks(own->entities, part.blocks))
			{
				if (out.entities.size() >= game_socket::k_MostSyncEntities)
				{
					break;
				}
				out.entities.push_back({.entity = entity.entity,
				                        .hash = entity.hash,
				                        .kind = std::move(entity.kind),
				                        .position = entity.position});
			}
		}
		if (!entities.parts.empty())
		{
			_stream->SendText(game_socket::ToJson(entities));
		}
		// The client resyncs when the server has all of the capture: a resync now closes the socket before the world goes
		_log->Info(fmt::format("The client sent the entities of {} parts and waits for the end of the capture",
		                       entities.parts.size()));
	}
}

void NetworkSession::Capture(uint32_t turn)
{
	if (!_capture.has_value() || !_capture->turn.has_value() || _capture->sent || !_stream || !_stream->IsOpen())
	{
		return;
	}
	if (turn > *_capture->turn)
	{
		ResyncAfterCapture(fmt::format("the world did the turn {} of the capture before the frame came", *_capture->turn));
		return;
	}
	if (turn != *_capture->turn)
	{
		return;
	}
	_capture->sent = true;
	_capture->entities = _settings.entities ? _settings.entities(_capture->parts) : std::vector<synced_state::PartEntities> {};
	game_socket::SyncBlocks blocks {.turn = turn, .parts = {}};
	for (const auto& part : _capture->entities)
	{
		blocks.parts.push_back({.part = part.part,
		                        .entities = static_cast<uint32_t>(part.entities.size()),
		                        .blocks = synced_state::BlocksOf(part.entities)});
	}
	_stream->SendText(game_socket::ToJson(blocks));
	// The world goes in parts, so that no frame is large
	const auto world = _settings.saveWorld ? _settings.saveWorld() : std::vector<uint8_t> {};
	if (!world.empty() && world.size() <= game_socket::k_MostSyncWorldBytes)
	{
		const auto count = static_cast<uint32_t>((world.size() + game_socket::k_MostSyncWorldChunkBytes - 1) /
		                                         game_socket::k_MostSyncWorldChunkBytes);
		for (uint32_t index = 0; index < count; ++index)
		{
			const auto first = static_cast<size_t>(index) * game_socket::k_MostSyncWorldChunkBytes;
			const auto size = std::min(game_socket::k_MostSyncWorldChunkBytes, world.size() - first);
			_stream->SendText(game_socket::ToJson(game_socket::SyncWorld {
			    .turn = turn,
			    .index = index,
			    .count = count,
			    .size = static_cast<uint32_t>(world.size()),
			    .bytes = {world.begin() + static_cast<std::ptrdiff_t>(first),
			              world.begin() + static_cast<std::ptrdiff_t>(first + size)},
			}));
		}
	}
	_log->Info(fmt::format("The client sends the capture of turn {}: {} parts, a world of {} bytes", turn, blocks.parts.size(),
	                       world.size() <= game_socket::k_MostSyncWorldBytes ? world.size() : 0));
}

void NetworkSession::ResyncAfterCapture(std::string_view why)
{
	if (!_resyncAt.has_value())
	{
		return;
	}
	_resyncAt.reset();
	_capture.reset();
	_log->Info(fmt::format("The resync starts: {}", why));
	if (_state == State::Playing)
	{
		ComeBack(true);
	}
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
	const auto now = _clock();
	// The watch reads the socket before a drop of the socket, and can drop a silent socket in this update
	WatchSilence(now);
	if (std::exchange(_dropStream, false) && _stream)
	{
		_stream->Close();
		_stream.reset();
	}
	if (_state == State::Connecting && !_stream && _joinRetryAt.has_value())
	{
		if (now >= *_joinRetryAt)
		{
			_joinRetryAt.reset();
			Open(false);
		}
		return;
	}
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
			// The server knows no turn of the old land after it loaded a new land, so it sends its world
			_nextLands.clear();
			CountWaitingCommands();
			_playout.Reset();
			Open(true);
		}
		return;
	}
	// A report of the load that the tests delay goes when its time comes
	SendLoaded();
	if (_resyncAt.has_value() && now >= *_resyncAt)
	{
		ResyncAfterCapture("the capture did not end in time");
	}
	if (_stream && _stream->IsOpen() && now >= _nextPing)
	{
		_stream->SendPing();
		_nextPing = now + k_PingMicroseconds;
	}
	LogHealth(now);
}

bool NetworkSession::WaitsForTurns() const
{
	const bool plays = _state == State::Playing || (_state == State::Reconnecting && _start.has_value());
	const bool paused = _roomClock.has_value() && _roomClock->paused;
	return plays && !paused && !_loadWait.has_value() && !_stoppedAfter.has_value();
}

void NetworkSession::WatchSilence(int64_t now)
{
	if (_stream && _stream->LastReceived().has_value())
	{
		const auto received = static_cast<int64_t>(*_stream->LastReceived());
		_heardAt = _heardAt.has_value() ? std::max(*_heardAt, received) : received;
	}
	const auto speed = _roomClock.has_value() ? _roomClock->speed : 1.0f;
	const auto silenceBefore = _silence.Shown();
	if (_silence.Update({.now = now, .waits = WaitsForTurns(), .heardAt = _heardAt, .speed = speed}))
	{
		if (_silence.Shown().has_value())
		{
			_commandsInSilence = 0;
			_log->Write(spdlog::level::warn, fmt::format("No message from the server for {} ms: the hand and the camera stop",
			                                             _silence.Silence() / 1'000));
		}
		else
		{
			_log->Write(spdlog::level::info,
			            fmt::format("The silence of the server ended after {} ms ({} commands of the player in it)",
			                        silenceBefore.value_or(0) / 1'000, _commandsInSilence));
		}
	}
	// A socket that stays open but gives nothing for a long time is dead, for example after the network of the server
	// went away with no close. The client then tries to come back, as after a loss.
	if (_state == State::Playing && _stream && _stream->IsOpen())
	{
		const auto from = _heardAt.has_value() ? std::max(*_heardAt, _openedAt) : _openedAt;
		if (now - from > server_silence::k_LimitMicroseconds)
		{
			_log->Write(spdlog::level::warn,
			            fmt::format("No message from the server for {} s on an open socket: the client tries to come back",
			                        server_silence::k_LimitMicroseconds / 1'000'000));
			ComeBack(false);
		}
	}
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

void NetworkSession::ReportLoaded()
{
	_loaded = true;
	_landOfRestartDue = false;
	SendLoaded();
}

void NetworkSession::SendLoaded()
{
	if (!_loaded || _loadedSent || !_start.has_value())
	{
		return;
	}
	if (!_loadedDueAt.has_value())
	{
		_loadedDueAt = _clock() + _settings.testLoadDelayMicroseconds;
		if (_settings.testLoadDelayMicroseconds > 0)
		{
			_log->Write(spdlog::level::info, fmt::format("Tests: the session tells the server about the load {} ms late",
			                                             _settings.testLoadDelayMicroseconds / 1000));
		}
	}
	if (_clock() >= *_loadedDueAt && _stream && _stream->IsOpen())
	{
		_loadedSent = true;
		_stream->SendText(game_socket::ToJson(game_socket::Loaded {}));
		_log->Write(spdlog::level::info, "The world of the network game loaded: the session tells the server");
	}
}

std::optional<NetworkSession::LoadWaitView> NetworkSession::LoadWait() const
{
	if (!_loadWait.has_value())
	{
		return std::nullopt;
	}
	const auto passed = std::max<int64_t>(_clock() - _loadWaitAt, 0) / 1'000'000;
	const auto left = static_cast<int64_t>(_loadWait->secondsLeft) - passed;
	return LoadWaitView {.loading = _loadWait->loading, .secondsLeft = static_cast<uint32_t>(std::max<int64_t>(left, 0))};
}

void NetworkSession::Send(const player_commands::Action& action)
{
	if (_silence.Shown().has_value())
	{
		++_commandsInSilence;
	}
	// While the turns wait for the loads, the world does not change: the session sends only what an observer sends (the
	// reports of the hand and the camera, and the chat)
	if (_loadWait.has_value() && !player_commands::ObserverSends(action))
	{
		if (_commandsInLoadWait++ == 0)
		{
			_log->Write(spdlog::level::info,
			            "The game is paused for the loads: the session does not send the commands of the player");
		}
		return;
	}
	// An observer sends only the report of its hand and camera and the lines of the chat. An observer with the editor claim
	// also sends the edits of the editor. An observer with no number of an observer sends no commands.
	if (_joined.has_value() && (!SendsCommands() || (_joined->role == game_socket::Role::Observer &&
	                                                 !player_commands::ServerTakesFromObserver(action, _joined->editor))))
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
	// The world of the old game does no turn of the game that the server started again
	if ((_state != State::Playing && _state != State::Reconnecting) || _worldTurn.has_value() || _landOfRestartDue)
	{
		return false;
	}
	if (_stoppedAfter.has_value() && turn > *_stoppedAfter)
	{
		return false;
	}
	// Before this turn, the world of the server loaded a land. The turn starts when the first turn of the new land is here.
	if (!_nextLands.empty() && _turnsDone == _nextLands.front().after)
	{
		const auto& next = _nextLands.front();
		return turn == next.after && next.turns.contains(next.turn);
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

void NetworkSession::TurnsStartAgain(uint32_t after, uint32_t turn)
{
	if (_nextLands.empty() || _nextLands.front().after != after || _nextLands.front().turn != turn)
	{
		_log->Write(spdlog::level::warn,
		            fmt::format("Before turn {}, the world loaded a land, but the world of the game server did not", after));
		return;
	}
	auto next = std::move(_nextLands.front());
	_nextLands.pop_front();
	_turns = std::move(next.turns);
	_serverChecksums = std::move(next.serverChecksums);
	_localChecksums.clear();
	_turnsDone = turn;
	_playout.Reset();
	for (const auto& [number, at] : next.arrivals)
	{
		_playout.Arrived(number, at);
	}
	CountWaitingCommands();
	_log->Write(
	    spdlog::level::info,
	    fmt::format("Before turn {}, the world loaded the land of the game server: the turns go on from turn {}", after, turn));
	// The server compares the checksums of this computer again after this report
	if (_stream && _stream->IsOpen())
	{
		_stream->SendText(game_socket::ToJson(game_socket::Loaded {}));
	}
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
	// After a game.turns_again frame, the turns are of the land that the server loaded last. While the world of the server
	// comes, the session keeps the turns from the turn of that world. After a restart of the server, that turn can come
	// before the turns that this computer did.
	auto* next = _nextLands.empty() ? nullptr : &_nextLands.back();
	auto& turns = next != nullptr ? next->turns : _turns;
	const auto first = next != nullptr ? next->turn : _worldTurn.value_or(_turnsDone);
	if (turn.turn < first || turns.contains(turn.turn))
	{
		return;
	}
	// A server that sends more turns than the client can keep breaks a limit: the client leaves the game
	const auto commands = CommandsIn(turn);
	if (turn.turn - first >= k_MostWaitingTurns || turns.size() >= k_MostWaitingTurns ||
	    _waitingCommands + commands > k_MostWaitingCommands)
	{
		BreaksLimit(fmt::format("turn {} goes over the turns that the client keeps", turn.turn));
		return;
	}
	if (next != nullptr)
	{
		next->arrivals[turn.turn] = _clock();
	}
	else
	{
		_playout.Arrived(turn.turn, _clock());
	}
	turns.emplace(turn.turn, turn);
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
	// The last checksum of a world that the server's world or a new start replaces is of no use to the server
	if (_worldTurn.has_value() || _landOfRestartDue)
	{
		return;
	}
	if (_stream && _stream->IsOpen())
	{
		_stream->SendChecksum(turn, checksum);
	}
	_localChecksums[turn] = checksum;
	Capture(turn);
	Compare(turn);
}

void NetworkSession::ServerChecksum(const turn_codec::TurnChecksum& checksum)
{
	// A checksum after a game.turns_again frame is of the new land: it waits until this computer loads that land
	if (!_nextLands.empty())
	{
		_nextLands.back().serverChecksums[checksum.turn] = checksum.checksum;
		return;
	}
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
		const auto action = _resync.Differ(turn);
		if (action != ResyncRule::Action::None && _settings.desynced)
		{
			_settings.desynced(turn, localChecksum, serverChecksum);
		}
		switch (action)
		{
		case ResyncRule::Action::Rejoin:
			// The first desync of a game waits for the capture of the server, which compares the worlds at a later turn.
			// The client sends its facts and its parts at once, and resyncs after the capture.
			if (_joined.has_value() && _joined->capture && !_captured && _stream && _stream->IsOpen())
			{
				_captured = true;
				_capture = CaptureState {};
				_resyncAt = _clock() + k_CaptureWaitMicroseconds;
				_log->Info(fmt::format("The client sends its facts and waits up to {} s for the capture of the desync",
				                       k_CaptureWaitMicroseconds / 1'000'000));
				_stream->SendText(game_socket::ToJson(OwnFacts(turn)));
				SendPartedParts();
			}
			else
			{
				ComeBack(true);
			}
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
