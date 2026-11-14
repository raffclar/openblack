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
#include <iterator>
#include <limits>
#include <ranges>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Common/DebugCheats.h"
#include "Gui/TextDatabase.h"
#include "Online/ConnectionLog.h"
#include "Online/GameBuildId.h"
#include "Online/StartRules.h"
#include "PlayerSlots.h"
#include "Server/ConnectionLimits.h"
#include "Server/GameLifetime.h"
#include "Server/LobbyBans.h"
#include "Server/LobbyStatus.h"

namespace openblack::server
{
namespace
{

/// The server writes a line of each god's compares to the log after this many compares
constexpr uint32_t k_CompareLogEvery = 100;
/// The clients' checksums that wait for the server's own, at most, for each god
constexpr size_t k_MostEarlyChecksums = 64;
/// The server writes a line about the refused blocks of a god after this many refused blocks
constexpr uint32_t k_RefusedLogEvery = 100;
/// The most bytes of the commands of one god in one turn: the commands of a turn that fills a frame
constexpr size_t k_MostGodBytesInTurn =
    turn_codec::k_MostFrameBytes - turn_codec::sizes::k_Header - turn_codec::sizes::k_TurnBody;

/// A text for the log, with at most a number of characters. Each character that the log cannot show becomes '?'.
[[nodiscard]] std::string Printable(std::string_view text, size_t mostCharacters)
{
	std::string printable;
	for (const char c : text.substr(0, mostCharacters))
	{
		printable += c >= 0x20 && c < 0x7F ? c : '?';
	}
	return printable;
}

/// The text of a string member of a JSON text that the server could not read, for the log. It has at most a number of
/// characters. Empty when the text has no such member.
[[nodiscard]] std::string StringMember(std::string_view text, std::string_view name, size_t mostCharacters)
{
	const auto key = fmt::format("\"{}\"", name);
	auto at = text.find(key);
	if (at == std::string_view::npos)
	{
		return {};
	}
	at = text.find('"', at + key.size());
	if (at == std::string_view::npos)
	{
		return {};
	}
	const auto end = text.find('"', at + 1);
	return Printable(text.substr(at + 1, end == std::string_view::npos ? std::string_view::npos : end - at - 1),
	                 mostCharacters);
}

/// The value of the "type" member of a JSON text frame, for the log. At most 32 characters; empty when the text has none.
[[nodiscard]] std::string FrameType(std::string_view text)
{
	constexpr size_t k_MostCharacters = 32;
	return StringMember(text, "type", k_MostCharacters);
}

/// The most characters of a build id of a client in the log
constexpr size_t k_MostBuildCharacters = 64;

/// The refusals that name the build of the client and of the server: the client cannot talk with this server
[[nodiscard]] bool NamesBuilds(std::string_view reason) noexcept
{
	return reason == "protocol" || reason == "version";
}

/// The builds of a client and of the server for the log of a refusal
[[nodiscard]] std::string BuildsText(std::string_view client, std::string_view server)
{
	const auto printable = Printable(client, k_MostBuildCharacters);
	return fmt::format("{}, the server has the build \"{}\"",
	                   printable.empty() ? std::string("the client gave no build id")
	                                     : fmt::format("the client has the build \"{}\"", printable),
	                   server);
}

void Log(spdlog::level::level_enum level, std::string_view text)
{
	if (auto logger = spdlog::get("game"))
	{
		logger->log(level, "{}", text);
	}
}

/// The player of a seat. The room has no more seats than the gods of a network game.
[[nodiscard]] PlayerNames SeatPlayer(size_t seat)
{
	return player_slots::PlayerOfSlot(seat).value_or(PlayerNames::PLAYER_ONE);
}

/// True when two secrets are the same. The time of the compare does not depend on where the texts differ, so that a
/// client cannot find a secret from the times of the answers.
[[nodiscard]] bool SameSecret(std::string_view a, std::string_view b) noexcept
{
	uint8_t difference = a.size() == b.size() ? 0 : 1;
	for (size_t i = 0; i < a.size(); ++i)
	{
		difference |= static_cast<uint8_t>(static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(i < b.size() ? b[i] : 0));
	}
	return difference == 0;
}

/// The sender with the most bytes of commands in the blocks. None when no block has a command.
[[nodiscard]] std::optional<PlayerNames> HeaviestSender(std::span<const turn_codec::StampedBlock> blocks)
{
	std::map<PlayerNames, size_t> bytes;
	for (const auto& block : blocks)
	{
		bytes[block.sender] += turn_codec::BlockSize(block);
	}
	const auto heaviest = std::ranges::max_element(bytes, {}, [](const auto& entry) { return entry.second; });
	return heaviest != bytes.end() ? std::optional(heaviest->first) : std::nullopt;
}

/// The seat of a player, as the key of the start files. The neutral player has no seat.
[[nodiscard]] std::optional<uint8_t> PlayerSeat(PlayerNames player)
{
	const auto slot = player_slots::SlotOfPlayer(player);
	return slot.has_value() ? std::optional(static_cast<uint8_t>(*slot)) : std::nullopt;
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

std::optional<join_ticket::Claims> LanClaims(std::string_view name)
{
	const bool control = std::ranges::any_of(name, [](char letter) {
		const auto byte = static_cast<uint8_t>(letter);
		return byte < 0x20 || byte == 0x7F;
	});
	if (name.empty() || name.size() > k_MostLanNameBytes || control)
	{
		return std::nullopt;
	}
	join_ticket::Claims claims;
	// The prefix keeps a name apart from the account ids of the lobby
	claims.subject = "lan:" + std::string(name);
	claims.name = std::string(name);
	claims.role = game_socket::Role::God;
	return claims;
}

bool LeavesOut(const player_commands::Action& action, bool testCommands) noexcept
{
	using namespace player_commands;
	return std::holds_alternative<PlayerLeft>(action) || std::holds_alternative<PlayerJoined>(action) ||
	       std::holds_alternative<ObserversJoined>(action) || std::holds_alternative<ObserversLeft>(action) ||
	       std::holds_alternative<QuickSave>(action) || std::holds_alternative<QuickLoad>(action) ||
	       std::holds_alternative<MusicMood>(action) || std::holds_alternative<CreatureTattoo>(action) ||
	       (!testCommands && std::holds_alternative<CreatureFightCheat>(action));
}

bool IsRoomChatLine(std::string_view text) noexcept
{
	size_t units = 0;
	bool visible = false;
	for (const char letter : text)
	{
		const auto byte = static_cast<uint8_t>(letter);
		// A continuation byte adds no code unit. A lead byte of four bytes gives two code units.
		if ((byte & 0xC0) == 0x80)
		{
			continue;
		}
		units += byte >= 0xF0 ? 2 : 1;
		visible = visible || byte > 0x20;
	}
	return visible && units <= game_socket::k_MostRoomChatCharacters;
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
    , _seats(std::min<size_t>(_settings.godSlots, player_slots::k_MaxPlayers))
    , _godsToWaitFor(_settings.lobby.has_value()
                         ? game_lifetime::GodsToWaitFor(_settings.lobby->Gods(), _settings.minGods, false)
                         : _settings.minGods)
    , _createdAt(_clock())
{
	_sockets.SetHandlers(ServerSocketHandlers {
	    .opened = [this](ConnectionId connection) { Opened(connection); },
	    .text = [this](ConnectionId connection, std::string_view text) { Text(connection, text); },
	    .binary = [this](ConnectionId connection, std::span<const uint8_t> bytes) { Binary(connection, bytes); },
	    .closed = [this](ConnectionId connection, uint16_t code) { Closed(connection, code); },
	});
}

LockstepHost::~LockstepHost()
{
	_sockets.SetHandlers({});
}

void LockstepHost::Opened(ConnectionId connection)
{
	_peers[connection] = Peer {.joined = false,
	                           .role = game_socket::Role::God,
	                           .seat = std::nullopt,
	                           .openedAt = _clock(),
	                           .name = {},
	                           .observer = 0,
	                           .account = {}};
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

std::string LockstepHost::WhoIs(ConnectionId connection) const
{
	const auto found = _peers.find(connection);
	if (found == _peers.end())
	{
		return "not known";
	}
	const auto name = PeerName(connection, found->second);
	if (!found->second.joined)
	{
		return name.empty() ? std::string("not joined") : fmt::format("{}, not joined", name);
	}
	if (found->second.seat.has_value())
	{
		return fmt::format("{}, slot {}", name, *found->second.seat);
	}
	return fmt::format("{}, observer {}", name, found->second.observer);
}

void LockstepHost::Refuse(ConnectionId connection, std::string_view reason, std::string_view detail, std::string_view path)
{
	// A refusal for the protocol or the version names the two builds, in the log and in the frame to the client
	std::string text(detail);
	std::string serverBuild;
	if (NamesBuilds(reason))
	{
		serverBuild = online::build_id::Own();
		const auto found = _peers.find(connection);
		const auto builds =
		    BuildsText(found != _peers.end() ? std::string_view(found->second.build) : std::string_view(), serverBuild);
		text = text.empty() ? builds : text + "; " + builds;
	}
	Log(spdlog::level::info, fmt::format("The server refuses connection {} ({}): {}{}{}", connection, WhoIs(connection), reason,
	                                     text.empty() ? "" : ": ", text));
	// A refusal for the game data names the file in the frame, so that the player sees which file differs
	SendTo(connection, game_socket::Refused {.reason = std::string(reason),
	                                         .build = std::move(serverBuild),
	                                         .file = reason == "data" ? std::string(detail) : std::string(),
	                                         .path = std::string(path)});
	_sockets.Close(connection, server_close::k_Refused);
	std::optional<std::string> observer;
	if (const auto found = _peers.find(connection);
	    found != _peers.end() && found->second.joined && found->second.role == game_socket::Role::Observer)
	{
		observer = found->second.name;
	}
	_peers.erase(connection);
	// A refused observer cannot come back with its session
	std::erase_if(_watchers, [connection](const Watcher& watcher) { return watcher.connection == connection; });
	if (observer.has_value())
	{
		ObserverLeft(*observer);
	}
}

void LockstepHost::EndRoom(std::string_view reason)
{
	std::vector<ConnectionId> connections;
	connections.reserve(_peers.size());
	std::ranges::copy(_peers | std::views::keys, std::back_inserter(connections));
	for (const auto connection : connections)
	{
		Refuse(connection, reason);
	}
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
		// A client of another build can send a join that this server cannot read. The log then names its build.
		if (peer.build.empty())
		{
			peer.build = StringMember(text, "build", k_MostBuildCharacters);
		}
		// The game server closes a connection that sends a message that it does not know
		Refuse(connection, "protocol",
		       fmt::format("a text frame that the server does not know ({} bytes, type \"{}\")", text.size(), FrameType(text)));
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
	else if (std::holds_alternative<game_socket::Loaded>(*frame))
	{
		if (_started && peer.seat.has_value() && !_seats[*peer.seat].loaded)
		{
			_seats[*peer.seat].loaded = true;
			Log(spdlog::level::info, fmt::format("The player of slot {} loaded the world", *peer.seat));
			UpdateLoadWait();
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
		if (std::erase_if(_watchers, [connection](const Watcher& watcher) { return watcher.connection == connection; }) > 0)
		{
			Log(spdlog::level::info, "An observer leaves the game");
		}
		const bool observer = peer.joined && peer.role == game_socket::Role::Observer;
		const auto name = peer.name;
		const auto number = peer.observer;
		_sockets.Close(connection, server_close::k_Normal);
		_peers.erase(connection);
		if (observer)
		{
			ObserverGone(number);
			ObserverLeft(name);
		}
	}
	else if (std::holds_alternative<game_socket::StopRequest>(*frame))
	{
		// Only the player of a god can stop the turns. The end box of an observer does not stop the game. A room with
		// no human god has only observers, so an observer stops its turns.
		const bool god = peer.role == game_socket::Role::God && peer.seat.has_value();
		if (peer.joined && (god || AllComputerGods()))
		{
			StopTurns();
		}
		else
		{
			Log(spdlog::level::warn,
			    fmt::format("Connection {} has no god: the server refuses its request to stop the turns", connection));
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
	else if (const auto* chat = std::get_if<game_socket::RoomChat>(&*frame))
	{
		ChatInRoom(connection, peer, *chat);
	}
	else if (const auto* parts = std::get_if<game_socket::SyncParts>(&*frame))
	{
		PartsFrom(connection, peer, *parts);
	}
	else if (const auto* print = std::get_if<game_socket::DataPrint>(&*frame))
	{
		DataPrintFrom(connection, peer, *print);
	}
	else if (std::holds_alternative<game_socket::SyncFacts>(*frame) ||
	         std::holds_alternative<game_socket::SyncBlocks>(*frame) ||
	         std::holds_alternative<game_socket::SyncEntities>(*frame) ||
	         std::holds_alternative<game_socket::SyncWorld>(*frame))
	{
		CaptureFrame(peer, *frame);
	}
	else
	{
		Refuse(connection, "protocol", fmt::format("a frame that only the server sends (type \"{}\")", FrameType(text)));
	}
}

void LockstepHost::PartsFrom(ConnectionId connection, Peer& peer, const game_socket::SyncParts& parts)
{
	if (!peer.joined || ++peer.partReports > k_MostPartReports)
	{
		return;
	}
	auto own = _partsOfTurn ? _partsOfTurn(parts.turn) : std::vector<std::pair<std::string, uint64_t>> {};
	if (own.empty())
	{
		Log(spdlog::level::warn, fmt::format("Connection {} ({}) sent its parts of turn {}; the server does not keep that turn",
		                                     connection, WhoIs(connection), parts.turn));
		return;
	}
	const auto differences = online::DifferentParts(own, parts.parts);
	Log(spdlog::level::warn, fmt::format("The world of connection {} ({}) parted at turn {}: {}", connection, WhoIs(connection),
	                                     parts.turn, online::DescribeParts(differences, "server", "client")));
	if ((_partedListener || _capture) && peer.seat.has_value())
	{
		std::vector<std::string> names;
		names.reserve(differences.size());
		std::ranges::transform(differences, std::back_inserter(names), &online::PartDifference::name);
		if (_partedListener)
		{
			_partedListener(*peer.seat, names);
		}
		if (_capture)
		{
			_capture->Parted(*peer.seat, connection, parts.turn, _turnsDone, names, _clock());
		}
	}
	if (own.size() > game_socket::k_MostSyncParts)
	{
		own.resize(game_socket::k_MostSyncParts);
	}
	SendTo(connection, game_socket::SyncParts {.turn = parts.turn, .parts = std::move(own)});
}

void LockstepHost::SetCapture(
    game_socket::SyncFacts facts,
    std::function<std::vector<synced_state::PartEntities>(std::span<const std::string> parts)> entities,
    std::function<void(CaptureRecord record)> finished)
{
	_capture = std::make_unique<DesyncCapture>(
	    std::move(facts),
	    DesyncCapture::Hooks {
	        .send = [this](ConnectionId connection, const game_socket::Frame& frame) { SendTo(connection, frame); },
	        .entities = std::move(entities),
	        .world = [this] { return _worldState ? _worldState() : std::vector<uint8_t> {}; },
	        .finished = std::move(finished),
	    });
}

void LockstepHost::FinishCaptures()
{
	if (_capture)
	{
		_capture->FinishAll();
	}
}

std::vector<std::string> LockstepHost::DataFiles() const
{
	std::vector<std::string> files;
	files.reserve(_dataPrint.size());
	std::ranges::transform(_dataPrint, std::back_inserter(files), &online::game_data::FilePrint::path);
	return files;
}

void LockstepHost::DataPrintFrom(ConnectionId connection, Peer& peer, const game_socket::DataPrint& print)
{
	if (!peer.joined || _dataPrint.empty() || std::exchange(peer.printed, true))
	{
		return;
	}
	std::vector<online::game_data::FilePrint> theirs;
	theirs.reserve(print.files.size());
	for (const auto& file : print.files)
	{
		theirs.push_back({.path = file.path, .present = file.present, .size = file.size, .hash = file.hash});
	}
	const auto copy = fmt::format("patch {}, language {}", print.patch.empty() ? "not known" : print.patch,
	                              print.language.empty() ? "not known" : print.language);
	const auto difference = online::game_data::FirstDifference(_dataPrint, theirs);
	if (!difference.has_value())
	{
		Log(spdlog::level::info, fmt::format("The game data of connection {} ({}) is the server's: {} files ({})", connection,
		                                     WhoIs(connection), _dataPrint.size(), copy));
		return;
	}
	const auto words = online::game_data::Describe(*difference);
	// A god of a game that started keeps its god: its world parts, and the capture of the desync names the file too
	const bool refuses =
	    online::game_data::RefusesJoin(difference->server.path) && (!_started || peer.role == game_socket::Role::Observer);
	Log(spdlog::level::warn, fmt::format("The game data of connection {} ({}) is not the server's: {} ({}){}", connection,
	                                     WhoIs(connection), words, copy, refuses ? "" : "; the player stays"));
	if (!refuses)
	{
		return;
	}
	if (peer.seat.has_value() && !_started)
	{
		const auto seat = *peer.seat;
		peer.seat.reset();
		_seats[seat] = Seat {};
		if (Starting())
		{
			StopStart(game_socket::StartCancel {});
		}
	}
	Refuse(connection, "data", words, difference->server.path);
}

void LockstepHost::CaptureFrame(const Peer& peer, const game_socket::Frame& frame)
{
	if (!_capture || !peer.joined || !peer.seat.has_value())
	{
		return;
	}
	const auto seat = *peer.seat;
	if (const auto* facts = std::get_if<game_socket::SyncFacts>(&frame))
	{
		_capture->Facts(seat, *facts);
	}
	else if (const auto* blocks = std::get_if<game_socket::SyncBlocks>(&frame))
	{
		_capture->Blocks(seat, *blocks);
	}
	else if (const auto* entities = std::get_if<game_socket::SyncEntities>(&frame))
	{
		_capture->Entities(seat, *entities);
	}
	else if (const auto* world = std::get_if<game_socket::SyncWorld>(&frame))
	{
		_capture->World(seat, *world);
	}
}

void LockstepHost::ChatInRoom(ConnectionId connection, const Peer& peer, const game_socket::RoomChat& chat)
{
	// Only a room on the local network has its chat on the game socket. On the internet the lobby's channel of the room
	// has it. The chat of the room ends when the game starts.
	if (!_settings.lanNames || !peer.joined || _started || !IsRoomChatLine(chat.text))
	{
		return;
	}
	auto from = PeerName(connection, peer);
	if (from.empty())
	{
		return;
	}
	// A line with an addressee goes only to the joined player with that name, also when that player is the sender. A
	// name that is not in the room gets no line.
	if (!chat.to.empty())
	{
		const auto target = std::ranges::find_if(_peers, [this, &chat](const auto& entry) {
			return entry.second.joined && PeerName(entry.first, entry.second) == chat.to;
		});
		if (target == _peers.end())
		{
			return;
		}
		const auto text = game_socket::ToJson(game_socket::RoomChat {
		    .from = std::move(from), .text = chat.text, .to = {}, .privateLine = true, .team = chat.team});
		_sockets.SendText(target->first, text);
		Log(spdlog::level::info,
		    fmt::format("A private chat line from connection {} goes to connection {}", connection, target->first));
		return;
	}
	// The line goes to every other player in the room, as the host's lobby sends it. The sender's game shows its own line
	// when it sends it. The log keeps who spoke, never the words.
	const auto text = game_socket::ToJson(game_socket::RoomChat {.from = std::move(from), .text = chat.text});
	for (const auto& [other, otherPeer] : _peers)
	{
		if (otherPeer.joined && other != connection)
		{
			_sockets.SendText(other, text);
		}
	}
	Log(spdlog::level::info, fmt::format("A chat line from connection {} goes to the room", connection));
}

std::string LockstepHost::PeerName(ConnectionId connection, const Peer& peer) const
{
	if (peer.seat.has_value())
	{
		return _seats[*peer.seat].name;
	}
	if (const auto watcher = std::ranges::find(_watchers, std::optional(connection), &Watcher::connection);
	    watcher != _watchers.end())
	{
		return watcher->name;
	}
	return peer.name;
}

void LockstepHost::Join(ConnectionId connection, Peer& peer, const game_socket::Join& join)
{
	if (peer.joined)
	{
		Refuse(connection, "protocol", "a second join on a connection that joined");
		return;
	}
	peer.build = Printable(join.build, k_MostBuildCharacters);
	// On the local network, a join with no ticket gives the name of the player's profile
	const bool lan = _settings.lanNames && join.ticket.empty();
	join_ticket::Claims claims;
	if (lan)
	{
		auto named = LanClaims(join.name);
		if (!named.has_value())
		{
			Refuse(connection, "ticket", "the name on the local network is not valid");
			return;
		}
		claims = std::move(*named);
	}
	else
	{
		auto result = _checkTicket ? _checkTicket(join.ticket) : TicketCheck::Result {};
		if (result.problem != TicketProblem::None || !result.claims.has_value())
		{
			Refuse(connection, "ticket", fmt::format("the ticket is not valid: {}", ProblemName(result.problem)));
			return;
		}
		claims = std::move(*result.claims);
	}
	// The room takes only a client of its own build, with the same file of the land (deviation 248)
	if (const auto refusal = lobby_game::BuildRefusal(_settings.buildId, join.build, _settings.lobby.has_value());
	    refusal.has_value())
	{
		Log(spdlog::level::info, fmt::format("{} has the build \"{}\", not the build \"{}\" of the server", claims.name,
		                                     join.build, _settings.buildId));
		Refuse(connection, *refusal);
		return;
	}
	if (const auto refusal = lobby_game::MapRefusal(_settings.mapSha256, join.map); refusal.has_value())
	{
		Log(spdlog::level::info, fmt::format("{} has another file of the land: SHA-256 {}", claims.name, join.map));
		Refuse(connection, *refusal);
		return;
	}
	// The game of a lobby takes only the members of the lobby, in the role that the lobby gave them (deviation 236)
	const auto* member = _settings.lobby.has_value() && !lan ? _settings.lobby->MemberOf(claims.subject) : nullptr;
	if (_settings.lobby.has_value())
	{
		const auto admission = lan ? lobby_game::Admission::NotMember
		                           : lobby_game::Admit(*_settings.lobby, claims.lobby, claims.subject, claims.role);
		if (admission != lobby_game::Admission::Member)
		{
			Log(spdlog::level::info,
			    fmt::format("The server refuses the account {}: {}", claims.subject, lobby_game::AdmissionName(admission)));
			Refuse(connection, "ticket");
			return;
		}
	}
	// The lobby's host can kick or ban an account also after the start: the server refuses the account in every role
	if (!lan && lobby_bans::IsBanned(claims.subject, _bans))
	{
		Log(spdlog::level::info, fmt::format("The lobby removed the account {}: the server refuses it", claims.subject));
		Refuse(connection, "kicked");
		return;
	}
	// The room takes nobody new during the start
	if (Starting())
	{
		Refuse(connection, "started", fmt::format("{} joins during the start of the game", claims.name));
		return;
	}
	if (_started)
	{
		JoinRunning(connection, peer, claims, lan);
		return;
	}
	game_socket::Joined joined {
	    .role = claims.role, .slot = std::nullopt, .session = {}, .name = claims.name, .start = _settings.start};
	joined.staff = claims.staff;
	joined.dataFiles = DataFiles();
	joined.capture = _capture != nullptr;
	if (claims.role == game_socket::Role::God)
	{
		// The room refuses a god that the lobby banned, as the host's channel refuses a banned player
		if (lobby_bans::IsBanned(claims.subject, _bans))
		{
			Refuse(connection, "kicked", fmt::format("the lobby banned {}", claims.name));
			return;
		}
		const auto same = std::ranges::find_if(
		    _seats, [&claims](const Seat& seat) { return seat.state != SeatState::Open && seat.account == claims.subject; });
		if (same != _seats.end())
		{
			Refuse(connection, "already", fmt::format("{} already has the god of slot {}", claims.name, same - _seats.begin()));
			return;
		}
		const auto free = std::ranges::find(_seats, SeatState::Open, &Seat::state);
		if (free == _seats.end() && !lan)
		{
			Refuse(connection, "full", fmt::format("{} finds each of the {} god slots taken", claims.name, _seats.size()));
			return;
		}
		// A member of the lobby with a slot takes that slot
		if (member != nullptr && member->slot.has_value() &&
		    (*member->slot >= _seats.size() || _seats.at(*member->slot).state != SeatState::Open))
		{
			Log(spdlog::level::info, fmt::format("The slot {} of {} is not free in the room", *member->slot, claims.name));
			Refuse(connection, "full");
			return;
		}
		// On the local network, a player who finds no free slot watches the game, as the game puts that player with the
		// observers
		if (free == _seats.end())
		{
			claims.role = game_socket::Role::Observer;
			joined.role = claims.role;
		}
	}
	if (claims.role == game_socket::Role::God)
	{
		const auto free = member != nullptr && member->slot.has_value()
		                      ? _seats.begin() + *member->slot
		                      : std::ranges::find(_seats, SeatState::Open, &Seat::state);
		const auto seat = static_cast<uint8_t>(free - _seats.begin());
		*free = Seat {.state = SeatState::Taken,
		              .connection = connection,
		              .session = _makeSecret ? _makeSecret() : std::string(),
		              .sessionHash = std::nullopt,
		              .account = claims.subject,
		              .name = claims.name,
		              .ready = false,
		              .awaySince = 0,
		              .compares = {},
		              .resumes = 0,
		              .commands = {},
		              .sessionNumber = ++_sessions,
		              .lastBlock = std::nullopt,
		              .lastBlockInTurn = std::nullopt,
		              .blocksAgain = 0,
		              .refusedBlocks = 0,
		              .staff = claims.staff};
		free->sessionHash = HashOf(free->session);
		peer.seat = seat;
		joined.slot = seat;
		joined.session = free->session;
		Log(spdlog::level::info, fmt::format("{} joins as the god of slot {}", claims.name, seat));
		WaitForGodsOfTicket(claims);
	}
	else
	{
		if (ObserversFull())
		{
			Refuse(connection, "full");
			return;
		}
		peer.observer = ObserverCame(claims.name);
		joined.observer = peer.observer;
		joined.session = NewWatcher(connection, claims.name, peer.observer, claims.staff);
		Log(spdlog::level::info, fmt::format("{} joins as observer {}", claims.name, peer.observer));
	}
	peer.joined = true;
	peer.role = claims.role;
	peer.name = claims.name;
	peer.account = lan ? std::string() : claims.subject;
	peer.staff = claims.staff;
	if (claims.staff)
	{
		Log(spdlog::level::info, fmt::format("{} has the staff claim: the server takes the edits and the debug cheats of "
		                                     "this player",
		                                     claims.name));
	}
	SendTo(connection, joined);
}

bool LockstepHost::ObserversFull() const
{
	return std::max(Observers(), _watchers.size()) >= _settings.mostObservers || !FreeObserverNumber().has_value();
}

void LockstepHost::Resume(ConnectionId connection, Peer& peer, const game_socket::Resume& resume)
{
	const auto found = std::ranges::find_if(_seats, [this, &resume](const Seat& seat) {
		return (seat.state == SeatState::Taken || seat.state == SeatState::Away) &&
		       SessionMatches(seat.session, seat.sessionHash, resume.session);
	});
	// The server sends again only the turns that it kept, unless it sends its world. After a restart from a snapshot, a
	// client that the kept turns cannot bring back gets the world too.
	const bool ahead = resume.turn > _turnsDone;
	const bool keptTurns = resume.turn == _turnsDone || _history.contains(resume.turn);
	const bool world = static_cast<bool>(_worldState) && (resume.world || (_restored && (ahead || !keptTurns)));
	const bool kept = world || keptTurns;
	if (peer.joined)
	{
		Refuse(connection, "resume", "a resume on a connection that joined");
		return;
	}
	if (ahead && !world)
	{
		Refuse(connection, "resume",
		       fmt::format("the client is at turn {}, after the server's turn {}, and the server has no world to send",
		                   resume.turn, _turnsDone));
		return;
	}
	if (!kept)
	{
		Refuse(connection, "resume",
		       fmt::format("the server does not keep turn {} (it keeps from turn {} to {}) and has no world to send",
		                   resume.turn, _history.empty() ? _turnsDone : _history.begin()->first, _turnsDone));
		return;
	}
	if (found == _seats.end())
	{
		// A session of no god can be the session of an observer
		if (!ResumeObserver(connection, peer, resume, world))
		{
			const bool awayGod = std::ranges::any_of(_seats, [](const Seat& seat) { return seat.state == SeatState::Away; });
			Refuse(connection, "resume",
			       fmt::format("no god or observer has the secret of the session (gods away: {}, observers that can come "
			                   "back: {}, restored game: {})",
			                   awayGod ? "yes" : "no", _watchers.size(), _restored ? "yes" : "no"));
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
	found->session = resume.session;
	++found->resumes;
	peer.joined = true;
	peer.role = game_socket::Role::God;
	peer.seat = seat;
	peer.staff = found->staff;
	// A client whose world parted gets the server's world at the start of the next turn, and then the turns from there
	if (world && SendWorld(connection))
	{
		// The checksums that the client sent for the turns of its parted world are no longer of use
		std::erase_if(_early, [seat](const auto& early) { return early.first.first == seat; });
		// The client loads the server's world, so the next checksum that differs starts a new desync
		found->compares.differs = false;
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
	const auto found = std::ranges::find_if(_watchers, [this, &resume](const Watcher& watcher) {
		return SessionMatches(watcher.session, watcher.sessionHash, resume.session);
	});
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
	found->session = resume.session;
	found->awaySince = 0;
	++found->resumes;
	peer.joined = true;
	peer.role = game_socket::Role::Observer;
	peer.name = found->name;
	peer.seat.reset();
	peer.observer = found->number;
	peer.staff = found->staff;
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
	SendLoadWait(connection);
}

void LockstepHost::SendLoadWait(ConnectionId connection)
{
	// A player who comes back or joins during the wait for the loads gets the wait, and counts down from the same end
	if (_loadWait && !_loadingSent.empty())
	{
		SendTo(connection, game_socket::LoadWait {.loading = _loadingSent, .secondsLeft = LoadSecondsLeft()});
	}
}

std::string LockstepHost::NewWatcher(ConnectionId connection, std::string name, uint8_t number, bool staff)
{
	if (!_makeSecret)
	{
		return {};
	}
	auto session = _makeSecret();
	_watchers.push_back(Watcher {.session = session,
	                             .sessionHash = HashOf(session),
	                             .connection = connection,
	                             .awaySince = 0,
	                             .resumes = 0,
	                             .name = std::move(name),
	                             .number = number,
	                             .staff = staff});
	return session;
}

std::optional<uint8_t> LockstepHost::FreeObserverNumber() const
{
	for (uint8_t number = 1; number <= turn_codec::k_MostObserverNumber; ++number)
	{
		if (!_observerBlocks.contains(number))
		{
			return number;
		}
	}
	return std::nullopt;
}

uint8_t LockstepHost::ObserverCame(std::string_view name)
{
	const auto number = FreeObserverNumber();
	if (!number.has_value())
	{
		return 0;
	}
	_observerBlocks.emplace(*number, ObserverBlocks {});
	_observerArrivals.emplace_back(*number, std::string(name));
	return *number;
}

void LockstepHost::ObserverGone(uint8_t number)
{
	if (number == 0 || !_observerBlocks.contains(number))
	{
		return;
	}
	// An observer that leaves before a turn told of its come is not told at all, and its number is free at once
	if (std::erase_if(_observerArrivals, [number](const auto& arrival) { return arrival.first == number; }) > 0)
	{
		_observerBlocks.erase(number);
		return;
	}
	_observerDepartures.push_back(number);
}

std::vector<uint8_t> LockstepHost::ObserverNumbers() const
{
	std::vector<uint8_t> numbers;
	for (const auto& [number, blocks] : _observerBlocks)
	{
		numbers.push_back(number);
	}
	return numbers;
}

void LockstepHost::RefuseStaffCommands(const Peer& peer, std::vector<player_commands::Action>& actions)
{
	// The server refuses the edits of the editor and the debug cheats of a player with no staff claim, and the log tells
	// each refusal
	const auto refused =
	    std::erase_if(actions, [&peer](const auto& action) { return player_commands::OnlyStaffSends(action) && !peer.staff; });
	if (refused > 0)
	{
		_refusedStaffCommands += static_cast<uint32_t>(refused);
		Log(spdlog::level::warn,
		    fmt::format("The server refuses {} edit or cheat commands of {}: the join ticket has no staff claim", refused,
		                peer.name));
	}
}

void LockstepHost::LogDebugCheats(const Peer& peer, const std::vector<player_commands::Action>& actions)
{
	for (const auto& action : actions)
	{
		if (const auto* cheat = std::get_if<player_commands::DebugCheat>(&action))
		{
			Log(spdlog::level::info, fmt::format("The server takes the debug cheat {} {} of the staff player {}",
			                                     debug_cheats::NameOf(static_cast<debug_cheats::Cheat>(cheat->cheat)),
			                                     cheat->on ? "on" : "off", peer.name));
		}
	}
}

void LockstepHost::ObserverBlock(const Peer& peer, turn_codec::CommandBlock& block)
{
	const auto found = _observerBlocks.find(peer.observer);
	if (found == _observerBlocks.end())
	{
		++_refusedBlocks;
		return;
	}
	// An observer sends only the report of its hand and camera and the lines of the chat. A staff observer also sends the
	// edits of the editor.
	RefuseStaffCommands(peer, block.actions);
	if (std::erase_if(block.actions, [&peer](const auto& action) {
		    return !player_commands::ServerTakesFromObserver(action, peer.staff);
	    }) > 0)
	{
		++_refusedBlocks;
	}
	auto& blocks = found->second;
	// An observer that came back sends again the blocks that no turn held, as the player of a god does
	if (blocks.lastBlock.has_value() && !turn_codec::SequenceAfter(block.sequence, *blocks.lastBlock))
	{
		return;
	}
	blocks.lastBlock = block.sequence;
	turn_codec::StampedBlock stamped {
	    .sender = ObserversGod(), .sequence = block.sequence, .actions = std::move(block.actions), .observer = peer.observer};
	auto& used = _pendingObserverBytes[peer.observer];
	const auto blockBytes = turn_codec::BlockSize(stamped);
	if (used + blockBytes > k_MostGodBytesInTurn)
	{
		++_refusedBlocks;
		stamped.actions.clear();
	}
	else
	{
		used += blockBytes;
	}
	_pending.push_back(PendingBlock {.block = std::move(stamped), .seat = 0, .session = 0, .observer = peer.observer});
}

void LockstepHost::AddObserverBlocks(turn_codec::TurnBundle& bundle, uint32_t turn)
{
	const auto god = ObserversGod();
	const bool in = ObserversInGame() > 0;
	// The god of the observers comes into the game with its first observer
	if (in && !_observersIn)
	{
		_observersIn = true;
		bundle.blocks.push_back(
		    turn_codec::StampedBlock {.sender = god, .sequence = 0, .actions = {player_commands::ObserversJoined {}}});
		Log(spdlog::level::info,
		    fmt::format("Turn {}: the god of player {} of the observers comes into the game", turn, static_cast<int>(god)));
	}
	for (const auto number : _observerDepartures)
	{
		bundle.blocks.push_back(turn_codec::StampedBlock {
		    .sender = god, .sequence = 0, .actions = {player_commands::ObserverOut {}}, .observer = number});
		_observerBlocks.erase(number);
	}
	_observerDepartures.clear();
	for (const auto& [number, name] : _observerArrivals)
	{
		auto text = gui::ToUtf16(name);
		text.resize(std::min(text.size(), turn_codec::k_MostObserverNameCharacters));
		bundle.blocks.push_back(turn_codec::StampedBlock {.sender = god,
		                                                  .sequence = 0,
		                                                  .actions = {player_commands::ObserverIn {.name = std::move(text)}},
		                                                  .observer = number});
	}
	_observerArrivals.clear();
	// The god goes when its last observer left
	if (!in && _observersIn)
	{
		_observersIn = false;
		bundle.blocks.push_back(
		    turn_codec::StampedBlock {.sender = god, .sequence = 0, .actions = {player_commands::ObserversLeft {}}});
		Log(spdlog::level::info,
		    fmt::format("Turn {}: the god of player {} of the observers leaves the game", turn, static_cast<int>(god)));
	}
}

void LockstepHost::ObserverLeft(std::string_view name)
{
	// Before the start, the room shows who is in it. A session that a restore gave has no name, so no player can be told.
	if (!_started || name.empty())
	{
		return;
	}
	Broadcast(game_socket::ObserverLeft {.name = std::string(name)});
}

std::optional<SessionHash> LockstepHost::HashOf(std::string_view secret) const
{
	if (secret.empty() || !_settings.sessionHasher)
	{
		return std::nullopt;
	}
	return _settings.sessionHasher->Hash(secret);
}

bool LockstepHost::SessionMatches(std::string_view session, const std::optional<SessionHash>& hash,
                                  std::string_view given) const
{
	if (given.empty())
	{
		return false;
	}
	if (!session.empty())
	{
		return SameSecret(session, given);
	}
	return hash.has_value() && _settings.sessionHasher && _settings.sessionHasher->Matches(*hash, given);
}

bool LockstepHost::SendWorld(ConnectionId connection)
{
	const auto state = _worldState ? _worldState() : std::vector<uint8_t> {};
	if (state.empty())
	{
		return false;
	}
	SendTo(connection, game_socket::Resumed {.turn = _turnsDone, .world = true});
	size_t parts = 0;
	size_t refused = 0;
	for (const auto& part : turn_codec::SplitWorld(_turnsDone, state))
	{
		++parts;
		if (!_sockets.SendBinary(connection, turn_codec::Encode(part)))
		{
			++refused;
		}
	}
	// The socket of the server writes a line when the client has all the bytes, with the time that it took
	Log(refused == 0 ? spdlog::level::info : spdlog::level::warn,
	    fmt::format("The server sends its world of turn {} ({} bytes in {} parts) to connection {} ({}){}", _turnsDone,
	                state.size(), parts, connection, WhoIs(connection),
	                refused == 0 ? std::string() : fmt::format("; the socket refused {} parts", refused)));
	// A client that was away when the turns stopped did not get the stop. No turn comes after this world.
	if (_stoppedAfter.has_value())
	{
		SendTo(connection, game_socket::GameStop {.turn = *_stoppedAfter});
	}
	SendLoadWait(connection);
	return true;
}

void LockstepHost::JoinRunning(ConnectionId connection, Peer& peer, const join_ticket::Claims& claims, bool byName)
{
	// A player who joins the game that turns needs the server's world. The testbed scenario of the tools keeps its state
	// out of the world and plays from the first turn, so nobody can join its game as it turns.
	if (!_worldState || _settings.start.scenario.has_value())
	{
		Refuse(connection, "started",
		       fmt::format("{} joins a game that turns, and {}", claims.name,
		                   _worldState ? "a scenario game takes nobody after the start" : "the server has no world to send"));
		return;
	}
	// The player of a god that waits, or whose old socket is not closed yet, comes back into the god (MP-Q6). The ticket
	// of the lobby shows the account, so a new process of the player needs no secret of the old session. A name on the
	// local network is only a label that any computer can give: a player who joins by name never takes a god. The player
	// of the god comes back only with the secret of the session.
	const auto own = std::ranges::find_if(_seats, [&claims, byName](const Seat& seat) {
		return !byName && claims.role == game_socket::Role::God && !seat.account.empty() && seat.account == claims.subject &&
		       (seat.state == SeatState::Taken || seat.state == SeatState::Away);
	});
	if (own == _seats.end() && ObserversFull())
	{
		Refuse(connection, "full",
		       fmt::format("{} finds the room full of observers ({})", claims.name, _settings.mostObservers));
		return;
	}
	auto start = _settings.start;
	start.turn = _turnsDone;
	start.gods = Gods();
	game_socket::Joined joined {
	    .role = game_socket::Role::Observer, .slot = std::nullopt, .session = {}, .name = claims.name, .start = start};
	joined.staff = claims.staff;
	joined.dataFiles = DataFiles();
	joined.capture = _capture != nullptr;
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
		own->sessionHash = HashOf(own->session);
		// The new process of the player counts its blocks of commands from 0
		own->sessionNumber = ++_sessions;
		own->lastBlock.reset();
		own->lastBlockInTurn.reset();
		++own->resumes;
		own->staff = claims.staff;
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
		peer.observer = ObserverCame(claims.name);
		joined.observer = peer.observer;
		joined.session = NewWatcher(connection, claims.name, peer.observer, claims.staff);
		Log(spdlog::level::info,
		    fmt::format("{} joins the game at turn {} as observer {}", claims.name, _turnsDone, peer.observer));
	}
	peer.joined = true;
	peer.name = claims.name;
	peer.account = byName ? std::string() : claims.subject;
	peer.staff = claims.staff;
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
	// A client with another version of the format of the turns cannot play in this game. The server closes it.
	if (decoded.error == turn_codec::DecodeError::Version)
	{
		++_refusedBlocks;
		Refuse(connection, "version",
		       fmt::format("a binary frame of version {} of the format of the turns, not version {}",
		                   static_cast<unsigned>(bytes.front()), static_cast<unsigned>(turn_codec::k_Version)));
		return;
	}
	if (!decoded.frame.has_value())
	{
		++_refusedBlocks;
		return;
	}
	if (auto* block = std::get_if<turn_codec::CommandBlock>(&*decoded.frame))
	{
		// A god and an observer send commands, only during the game. A command that only the server sends is left out.
		if (!_started)
		{
			++_refusedBlocks;
			return;
		}
		if (peer.role == game_socket::Role::Observer)
		{
			ObserverBlock(peer, *block);
			return;
		}
		if (peer.role != game_socket::Role::God || !peer.seat.has_value())
		{
			++_refusedBlocks;
			return;
		}
		const auto before = block->actions.size();
		RefuseStaffCommands(peer, block->actions);
		std::erase_if(block->actions, [this](const auto& action) { return LeavesOut(action, _settings.testCommands); });
		LogDebugCheats(peer, block->actions);
		if (block->actions.size() != before)
		{
			++_refusedBlocks;
		}
		auto& seat = _seats[*peer.seat];
		// A client that came back sends again the blocks that no turn held. The server takes each block one time only.
		if (seat.lastBlock.has_value() && !turn_codec::SequenceAfter(block->sequence, *seat.lastBlock))
		{
			++seat.blocksAgain;
			Log(spdlog::level::info, fmt::format("The server had block {} of slot {}: it leaves out the block that came again",
			                                     block->sequence, *peer.seat));
			return;
		}
		seat.lastBlock = block->sequence;
		// The commands of one god in a turn fit in one frame at most. The server refuses the commands of a block that go
		// over this, so that the flood of one god does not push the commands of the other gods out of the turn.
		auto& used = _pendingBytes[*peer.seat];
		const auto blockBytes =
		    turn_codec::BlockSize({.sender = SeatPlayer(*peer.seat), .sequence = block->sequence, .actions = block->actions});
		if (used + blockBytes > k_MostGodBytesInTurn)
		{
			++_refusedBlocks;
			// The log gets the first refused block of the god and then one line for each 100 blocks
			if (seat.refusedBlocks++ % k_RefusedLogEvery == 0)
			{
				Log(spdlog::level::warn,
				    fmt::format("The commands of slot {} do not fit in the turn: the server refuses block {}", *peer.seat,
				                block->sequence));
			}
			block->actions.clear();
		}
		else
		{
			used += blockBytes;
		}
		if (!block->actions.empty())
		{
			for (const auto& action : block->actions)
			{
				++seat.commands[static_cast<uint8_t>(turn_codec::TagOf(action))];
			}
		}
		// A block with no command left still goes to the next turn's count of blocks, so its number is in a turn
		_pending.push_back(PendingBlock {
		    .block = {.sender = SeatPlayer(*peer.seat), .sequence = block->sequence, .actions = std::move(block->actions)},
		    .seat = *peer.seat,
		    .session = seat.sessionNumber});
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

void LockstepHost::Closed(ConnectionId connection, uint16_t code)
{
	const auto found = _peers.find(connection);
	if (found == _peers.end())
	{
		return;
	}
	// The socket of the server writes the cause of a close with no close frame on the line before
	Log(spdlog::level::info,
	    fmt::format("Connection {} ({}) closed from the side of the client: code {}{}, open for {} s", connection,
	                WhoIs(connection), code, code == server_close::k_NoCloseFrame ? " (no close frame)" : "",
	                (_clock() - found->second.openedAt) / 1'000'000));
	const auto seat = found->second.seat;
	const bool observer = found->second.joined && found->second.role == game_socket::Role::Observer;
	const auto name = found->second.name;
	const auto number = found->second.observer;
	_peers.erase(found);
	if (_capture && seat.has_value())
	{
		_capture->Gone(*seat);
	}
	if (const auto watcher = std::ranges::find(_watchers, std::optional(connection), &Watcher::connection);
	    watcher != _watchers.end())
	{
		// Before the start, the session of the observer ends. After the start, the observer can come back for a time.
		if (!_started)
		{
			ObserverGone(watcher->number);
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
	// An observer with no session cannot come back: it left the game
	if (observer)
	{
		ObserverGone(number);
		ObserverLeft(name);
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
	if (_capture)
	{
		_capture->Update(now);
	}
	// A connection that does not join or come back in its time closes, so that a socket with no ticket does not stay
	std::vector<ConnectionId> late;
	for (const auto& [connection, peer] : _peers)
	{
		if (!peer.joined && now - peer.openedAt > connection_limits::k_JoinMicroseconds)
		{
			late.push_back(connection);
		}
	}
	for (const auto connection : late)
	{
		Refuse(connection, "protocol",
		       fmt::format("no join or resume in {} s after the socket opened",
		                   connection_limits::k_JoinMicroseconds / 1'000'000));
	}
	// An observer that does not come back in the time to come back loses its session, and so leaves the game
	std::vector<std::pair<std::string, uint8_t>> gone;
	if (std::erase_if(_watchers, [this, now, &gone](const Watcher& watcher) {
		    const bool ends = !watcher.connection.has_value() && now - watcher.awaySince > _settings.comeBackMicroseconds;
		    if (ends)
		    {
			    gone.emplace_back(watcher.name, watcher.number);
		    }
		    return ends;
	    }) > 0)
	{
		Log(spdlog::level::info, "An observer did not come back: its session ends");
	}
	for (const auto& [name, number] : gone)
	{
		ObserverGone(number);
		ObserverLeft(name);
	}
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
		if (inRoom == 0 && !_settings.lobby.has_value() && _godsToWaitFor != _settings.minGods)
		{
			// The gods of a lobby's game left the room before the start: the room waits for its own fewest gods again
			_godsToWaitFor = _settings.minGods;
			Log(spdlog::level::info, fmt::format("The room is empty: it starts with {} gods at least", _godsToWaitFor));
		}
		const auto ready =
		    std::ranges::count_if(_seats, [](const Seat& seat) { return seat.state == SeatState::Taken && seat.ready; });
		if (!Starting() && RoomStarts(static_cast<size_t>(inRoom), static_cast<size_t>(ready), _godsToWaitFor))
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
	// A god who left or did not come back no longer holds the wait for the loads. At the end of the time to load, each
	// god whose player did not load leaves the game.
	if (_loadWait && now >= _loadWaitEndsAt)
	{
		EndLoadWait();
	}
	UpdateLoadWait();
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
	// The testbed has no land file, so the players need nothing to compare. The map for 32 gods is a file that the
	// players got from the lobby: they compare it as a land of the game.
	Broadcast(game_socket::StartQuery {.land = start.land, .testbed = start.testbed, .checksum = _settings.landChecksum});
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
	auto file = creature.file;
	// No computer brings a creature from a file that does not read, so the server sends no file in its place
	if (_settings.relaysCreature && !_settings.relaysCreature(file))
	{
		Log(spdlog::level::warn,
		    fmt::format("The creature file of slot {} does not read: the server sends no creature file for it", *peer.seat));
		file.clear();
	}
	_startCreatures[*peer.seat] = file;
	_startAlignments[*peer.seat] = creature.alignment;
	Broadcast(game_socket::StartCreature {.slot = *peer.seat, .file = std::move(file), .alignment = creature.alignment});
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
	// A player who joins later, such as an observer, gets the same names
	_settings.start.names = start.names;
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
	// The turns of a lobby game wait until each god's player loaded the world (deviation 254)
	if (_settings.lobby.has_value() && !AllComputerGods())
	{
		_loadWait = true;
		// The time to load counts from the time when the room was made. A start after that time gets the whole time from
		// the start, so that the players can load.
		const auto now = _clock();
		const auto fromCreation = _createdAt + _settings.loadWindowMicroseconds;
		_loadWaitEndsAt = now < fromCreation ? fromCreation : now + _settings.loadWindowMicroseconds;
		UpdateLoadWait();
	}
}

std::vector<uint8_t> LockstepHost::LoadingSeats() const
{
	std::vector<uint8_t> loading;
	if (!_loadWait)
	{
		return loading;
	}
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		const auto& seat = _seats[i];
		if ((seat.state == SeatState::Taken || seat.state == SeatState::Away) && !seat.loaded)
		{
			loading.push_back(static_cast<uint8_t>(i));
		}
	}
	return loading;
}

uint32_t LockstepHost::LoadSecondsLeft() const
{
	if (!_loadWait)
	{
		return 0;
	}
	const auto left = std::max<int64_t>(_loadWaitEndsAt - _clock(), 0);
	return static_cast<uint32_t>((left + 999'999) / 1'000'000);
}

void LockstepHost::UpdateLoadWait()
{
	if (!_loadWait)
	{
		return;
	}
	std::vector<std::string> names;
	for (const auto seat : LoadingSeats())
	{
		names.push_back(_seats[seat].name);
	}
	if (names.empty())
	{
		_loadWait = false;
		Log(spdlog::level::info, "Each god's player loaded the world: the turns of the game start");
	}
	else if (names != _loadingSent)
	{
		Log(spdlog::level::info, fmt::format("The turns wait for {} players to load the world", names.size()));
	}
	if (names != _loadingSent || names.empty())
	{
		_loadingSent = names;
		Broadcast(game_socket::LoadWait {.loading = std::move(names), .secondsLeft = LoadSecondsLeft()});
	}
}

void LockstepHost::EndLoadWait()
{
	for (const auto seat : LoadingSeats())
	{
		Log(spdlog::level::info,
		    fmt::format("The player of slot {} did not load the world in time: the god leaves the game", seat));
		const auto connection = _seats[seat].connection;
		if (const auto peer = connection.has_value() ? _peers.find(*connection) : _peers.end(); peer != _peers.end())
		{
			peer->second.seat.reset();
		}
		CloseSeat(seat);
		if (connection.has_value())
		{
			Refuse(*connection, "load");
		}
	}
	UpdateLoadWait();
}

std::vector<game_socket::God> LockstepHost::Gods() const
{
	std::vector<game_socket::God> gods;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		if (_seats[i].state == SeatState::Taken || _seats[i].state == SeatState::Away)
		{
			gods.push_back({.slot = static_cast<uint8_t>(i),
			                .name = _seats[i].name,
			                .account = lobby_status::AccountId(_seats[i].account),
			                .staff = _seats[i].staff});
		}
	}
	return gods;
}

void LockstepHost::SetBans(std::set<int64_t> accounts)
{
	_bans = std::move(accounts);
	if (_started)
	{
		RemoveBannedFromGame();
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

void LockstepHost::RemoveBannedFromGame()
{
	// The lobby's host kicked or banned a player after the start: the server closes the player's socket at once with
	// "kicked", and the god leaves the game as a player who leaves. The closed seat takes no resume and no join.
	for (const auto seat : lobby_bans::BannedSeats(_seats, _bans))
	{
		Log(spdlog::level::info,
		    fmt::format("The lobby removed the player of slot {} from the game: the god leaves the game", seat));
		const auto connection = _seats[seat].connection;
		if (const auto peer = connection.has_value() ? _peers.find(*connection) : _peers.end(); peer != _peers.end())
		{
			peer->second.seat.reset();
		}
		CloseSeat(seat);
		if (connection.has_value())
		{
			Refuse(*connection, "kicked");
		}
	}
	// An observer of a removed account leaves the game too
	std::vector<ConnectionId> observers;
	for (const auto& [connection, peer] : _peers)
	{
		if (peer.joined && peer.role == game_socket::Role::Observer && !peer.account.empty() &&
		    lobby_bans::IsBanned(peer.account, _bans))
		{
			observers.push_back(connection);
		}
	}
	for (const auto connection : observers)
	{
		Log(spdlog::level::info, fmt::format("The lobby removed the observer of connection {} from the game", connection));
		Refuse(connection, "kicked");
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
	const auto seat = PlayerSeat(god);
	const auto file = seat.has_value() ? _startCreatures.find(*seat) : _startCreatures.end();
	return file != _startCreatures.end() ? std::span<const uint8_t>(file->second) : std::span<const uint8_t> {};
}

std::optional<float> LockstepHost::StartAlignment(PlayerNames god) const
{
	const auto seat = PlayerSeat(god);
	const auto alignment = seat.has_value() ? _startAlignments.find(*seat) : _startAlignments.end();
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

void LockstepHost::WaitForGodsOfTicket(const join_ticket::Claims& claims)
{
	// A server with no member list of the lobby gets the gods of the lobby's game from the tickets. The room then waits
	// for each of these gods, so that a god alone does not start a game of two.
	if (_settings.lobby.has_value() || claims.lobby.empty() || claims.gods == 0)
	{
		return;
	}
	const auto gods = game_lifetime::GodsToWaitFor(claims.gods, _settings.minGods, false);
	if (gods == _godsToWaitFor)
	{
		return;
	}
	_godsToWaitFor = gods;
	Log(spdlog::level::info,
	    fmt::format("The game of the lobby {} has {} gods: the room starts when each of them is ready", claims.lobby, gods));
}

void LockstepHost::EndJoinWindow()
{
	if (!_settings.lobby.has_value() || _godsToWaitFor == _settings.minGods)
	{
		return;
	}
	_godsToWaitFor = _settings.minGods;
	Log(spdlog::level::info,
	    fmt::format("The time to join ended with {} gods in the room: the room starts with {} gods at least", GodsInRoom(),
	                _settings.minGods));
}

size_t LockstepHost::GodsInRoom() const
{
	return static_cast<size_t>(std::ranges::count(_seats, SeatState::Taken, &Seat::state));
}

bool LockstepHost::GameOver() const
{
	if (AllComputerGods())
	{
		// A room with no human god is over when the observers that joined all left. An observer that lost its socket can
		// come back for a time, so the room stays open until it is back or its session ends.
		return _started && _observerSeen && Observers() == 0 && _watchers.empty();
	}
	// After the turns stop, the defeat of a god that left comes at no turn
	return _started && (_leaving.empty() || _stoppedAfter.has_value()) && std::ranges::none_of(_seats, [](const Seat& seat) {
		       return seat.state == SeatState::Taken || seat.state == SeatState::Away;
	       });
}

bool LockstepHost::HasTurn(uint32_t turn) const
{
	// A restored game first turns the turns that the server sent before its restart
	if (_replayed.contains(turn))
	{
		return true;
	}
	// The defeat of a god that left still comes at a turn, so a world with a god leaving turns once more. No turn comes
	// after the turns of the game stop.
	if (AllComputerGods())
	{
		return _started && _observersJoined && !_stoppedAfter.has_value();
	}
	// The turns of a lobby game wait for the loads of the gods' players. A god who leaves in this time leaves in the
	// first turn after the wait.
	if (_loadWait)
	{
		return false;
	}
	return _started && !_stoppedAfter.has_value() && (AnyGodConnected() || !_leaving.empty());
}

std::vector<player_commands::Command> LockstepHost::TakeTurn(uint32_t turn)
{
	if (_replayed.contains(turn))
	{
		return TakeReplayedTurn(turn);
	}
	turn_codec::TurnBundle bundle {.turn = turn, .blocks = {}};
	for (const auto seat : _joining)
	{
		bundle.blocks.push_back(turn_codec::StampedBlock {
		    .sender = SeatPlayer(seat), .sequence = 0, .actions = {player_commands::PlayerJoined {}}});
	}
	_joining.clear();
	for (const auto seat : _leaving)
	{
		bundle.blocks.push_back(
		    turn_codec::StampedBlock {.sender = SeatPlayer(seat), .sequence = 0, .actions = {player_commands::PlayerLeft {}}});
	}
	_leaving.clear();
	// The god of the observers comes into the game with its first observer, and goes when its last observer left. Each
	// observer that came or left goes in the turn between them.
	AddObserverBlocks(bundle, turn);
	// The joins and the leaves of the gods and of the observers always go in the turn
	const auto fixedBlocks = bundle.blocks.size();
	for (auto& pending : _pending)
	{
		// The turn holds the block even when the turn is too large and drops its commands. The client never sends it again.
		if (pending.observer != 0)
		{
			if (const auto blocks = _observerBlocks.find(pending.observer); blocks != _observerBlocks.end())
			{
				blocks->second.lastBlockInTurn = pending.block.sequence;
			}
		}
		else if (auto& seat = _seats[pending.seat]; pending.session == seat.sessionNumber)
		{
			seat.lastBlockInTurn = pending.block.sequence;
		}
		if (!pending.block.actions.empty())
		{
			bundle.blocks.push_back(std::move(pending.block));
		}
	}
	_pending.clear();
	_pendingBytes.clear();
	_pendingObserverBytes.clear();
	auto bytes = turn_codec::Encode(bundle);
	// A turn larger than a frame drops the commands of the god with the most bytes in the turn, until the turn fits. The
	// commands of the other gods stay in the turn. The turn itself always goes.
	while (bytes.empty())
	{
		const auto heaviest = HeaviestSender(std::span(bundle.blocks).subspan(fixedBlocks));
		if (!heaviest.has_value())
		{
			Log(spdlog::level::err, fmt::format("The commands of turn {} do not fit in a frame", turn));
			bundle.blocks.resize(fixedBlocks);
			bytes = turn_codec::Encode(bundle);
			break;
		}
		Log(spdlog::level::err, fmt::format("The commands of turn {} do not fit in a frame: the server drops the commands "
		                                    "of slot {}",
		                                    turn, PlayerSeat(*heaviest).value_or(0)));
		const auto from = bundle.blocks.begin() + static_cast<std::ptrdiff_t>(fixedBlocks);
		bundle.blocks.erase(
		    std::remove_if(from, bundle.blocks.end(),
		                   [&heaviest](const turn_codec::StampedBlock& block) { return block.sender == *heaviest; }),
		    bundle.blocks.end());
		bytes = turn_codec::Encode(bundle);
	}
	// The journal keeps the turn before any client gets it, so that a restart of the server can send it again
	if (_turnJournal)
	{
		_turnJournal(turn, bytes, BlocksInTurns());
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
			commands.push_back(
			    player_commands::Command {.player = block.sender, .action = std::move(action), .observer = block.observer});
		}
	}
	return commands;
}

std::vector<player_commands::Command> LockstepHost::TakeReplayedTurn(uint32_t turn)
{
	auto bytes = std::move(_replayed.at(turn));
	_replayed.erase(turn);
	std::vector<player_commands::Command> commands;
	auto decoded = turn_codec::Decode(bytes);
	auto* bundle = decoded.frame.has_value() ? std::get_if<turn_codec::TurnBundle>(&*decoded.frame) : nullptr;
	if (bundle == nullptr || bundle->turn != turn)
	{
		Log(spdlog::level::err, fmt::format("The replayed turn {} does not read", turn));
	}
	else
	{
		for (auto& block : bundle->blocks)
		{
			// The replayed turn already has the joins and the leaves that waited in the room of the snapshot
			const auto seat = PlayerSeat(block.sender);
			for (auto& action : block.actions)
			{
				if (seat.has_value() && std::holds_alternative<player_commands::PlayerLeft>(action))
				{
					std::erase(_leaving, *seat);
				}
				if (seat.has_value() && std::holds_alternative<player_commands::PlayerJoined>(action))
				{
					std::erase(_joining, *seat);
				}
				if (std::holds_alternative<player_commands::ObserversJoined>(action))
				{
					_observersIn = true;
				}
				if (std::holds_alternative<player_commands::ObserversLeft>(action))
				{
					_observersIn = false;
				}
				// The replayed turn already told of the observers that came and left in the room of the snapshot
				if (std::holds_alternative<player_commands::ObserverIn>(action))
				{
					std::erase_if(_observerArrivals, [&block](const auto& arrival) { return arrival.first == block.observer; });
				}
				if (std::holds_alternative<player_commands::ObserverOut>(action))
				{
					std::erase(_observerDepartures, block.observer);
					_observerBlocks.erase(block.observer);
				}
				commands.push_back(
				    player_commands::Command {.player = block.sender, .action = std::move(action), .observer = block.observer});
			}
		}
	}
	_history[turn] = std::move(bytes);
	if (turn >= _settings.keptTurns)
	{
		_history.erase(_history.begin(), _history.lower_bound(turn - _settings.keptTurns));
	}
	_turnsDone = turn + 1;
	if (_replayed.empty())
	{
		Log(spdlog::level::info,
		    fmt::format("The server did again the turns that it sent before its restart, to turn {}", _turnsDone));
	}
	return commands;
}

snapshots::Room LockstepHost::RoomRecord() const
{
	snapshots::Room room {.start = game_socket::ToJson(_settings.start),
	                      .seats = {},
	                      .watchers = {},
	                      .stoppedAfter = _stoppedAfter,
	                      .leaving = _leaving,
	                      .joining = _joining,
	                      .watcherNames = {},
	                      .observersIn = _observersIn};
	for (const auto& seat : _seats)
	{
		room.seats.push_back({.state = static_cast<uint8_t>(seat.state),
		                      .session = seat.sessionHash,
		                      .account = seat.account,
		                      .name = seat.name});
	}
	for (const auto& watcher : _watchers)
	{
		if (watcher.sessionHash.has_value())
		{
			room.watchers.push_back(*watcher.sessionHash);
			room.watcherNames.push_back(watcher.name);
			room.watcherNumbers.push_back(watcher.number);
		}
	}
	for (const auto& [number, name] : _observerArrivals)
	{
		room.arrivingNumbers.push_back(number);
		room.arrivingNames.push_back(name);
	}
	room.departingNumbers = _observerDepartures;
	return room;
}

snapshots::BlockSequences LockstepHost::BlocksInTurns() const
{
	snapshots::BlockSequences blocks;
	for (size_t i = 0; i < _seats.size(); ++i)
	{
		if (_seats[i].lastBlockInTurn.has_value())
		{
			blocks.emplace(static_cast<uint8_t>(i), *_seats[i].lastBlockInTurn);
		}
	}
	for (const auto& [number, observer] : _observerBlocks)
	{
		if (observer.lastBlockInTurn.has_value())
		{
			blocks.emplace(static_cast<uint8_t>(snapshots::k_ObserverBlocksKey + number), *observer.lastBlockInTurn);
		}
	}
	return blocks;
}

void LockstepHost::Restore(const snapshots::Room& room, uint32_t turnsDone, std::map<uint32_t, std::vector<uint8_t>> replayed,
                           const snapshots::BlockSequences& blocks)
{
	const auto now = _clock();
	_started = true;
	_restored = true;
	_startStep = StartStep::None;
	_awaited.clear();
	_turnsDone = turnsDone;
	_stoppedAfter = room.stoppedAfter;
	_leaving = room.leaving;
	_joining = room.joining;
	_observersIn = room.observersIn;
	// No player is connected yet: each god with a player waits for the player to come back
	for (size_t i = 0; i < _seats.size() && i < room.seats.size(); ++i)
	{
		const auto& saved = room.seats[i];
		auto state = static_cast<SeatState>(saved.state);
		if (state == SeatState::Taken)
		{
			state = SeatState::Away;
		}
		_seats[i] = Seat {.state = state,
		                  .connection = std::nullopt,
		                  .session = {},
		                  .sessionHash = saved.session,
		                  .account = saved.account,
		                  .name = saved.name,
		                  .ready = true,
		                  .awaySince = now,
		                  .compares = {},
		                  .resumes = 0,
		                  .commands = {},
		                  .sessionNumber = ++_sessions,
		                  .lastBlock = std::nullopt,
		                  .lastBlockInTurn = std::nullopt,
		                  .blocksAgain = 0,
		                  .refusedBlocks = 0,
		                  .staff = false};
		// The server takes only the blocks after the last block that the sent turns hold
		if (const auto last = blocks.find(static_cast<uint8_t>(i)); last != blocks.end())
		{
			_seats[i].lastBlock = last->second;
			_seats[i].lastBlockInTurn = last->second;
		}
	}
	_watchers.clear();
	_observerBlocks.clear();
	_observerArrivals.clear();
	_observerDepartures = room.departingNumbers;
	for (const auto number : room.departingNumbers)
	{
		_observerBlocks.emplace(number, ObserverBlocks {});
	}
	for (size_t i = 0; i < room.arrivingNumbers.size(); ++i)
	{
		_observerBlocks.emplace(room.arrivingNumbers[i], ObserverBlocks {});
		_observerArrivals.emplace_back(room.arrivingNumbers[i],
		                               i < room.arrivingNames.size() ? room.arrivingNames[i] : std::string());
	}
	// The players and the observers come back with the secrets of their sessions, which the server checks against the
	// hashes
	for (size_t i = 0; i < room.watchers.size(); ++i)
	{
		auto name = i < room.watcherNames.size() ? room.watcherNames[i] : std::string();
		const auto saved = i < room.watcherNumbers.size() ? room.watcherNumbers[i] : uint8_t {0};
		uint8_t number = 0;
		if (saved != 0 && saved <= turn_codec::k_MostObserverNumber && !_observerBlocks.contains(saved))
		{
			number = saved;
			_observerBlocks.emplace(number, ObserverBlocks {});
		}
		else
		{
			// An observer of a file of format 4 or older has no number: it gets one, and the next turn tells the worlds
			number = ObserverCame(name);
		}
		_watchers.push_back(Watcher {.session = {},
		                             .sessionHash = room.watchers[i],
		                             .connection = std::nullopt,
		                             .awaySince = now,
		                             .resumes = 0,
		                             .name = std::move(name),
		                             .number = number,
		                             .staff = false});
	}
	// The server takes only the blocks of each observer after the last block that the sent turns hold
	for (const auto& [key, sequence] : blocks)
	{
		if (key <= snapshots::k_ObserverBlocksKey)
		{
			continue;
		}
		if (const auto found = _observerBlocks.find(static_cast<uint8_t>(key - snapshots::k_ObserverBlocksKey));
		    found != _observerBlocks.end())
		{
			found->second.lastBlock = sequence;
			found->second.lastBlockInTurn = sequence;
		}
	}
	// A room with no human god turned before the restart, so it turns again when an observer comes back
	_observersJoined = true;
	_observerSeen = true;
	std::erase_if(replayed, [turnsDone](const auto& entry) { return entry.first < turnsDone; });
	_replayed = std::move(replayed);
	Log(spdlog::level::info, fmt::format("The game continues from the snapshot of turn {}, with {} sent turns to do again",
	                                     turnsDone, _replayed.size()));
}

void LockstepHost::TurnChecksum(uint32_t turn, uint64_t checksum)
{
	_checksums[turn] = checksum;
	if (_capture)
	{
		_capture->TurnDone(turn);
	}
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
	if (differs && !compares.differs)
	{
		if (_partListener)
		{
			_partListener(seat, turn);
		}
		if (_desyncListener)
		{
			_desyncListener(seat, turn, own->second, checksum);
		}
	}
	compares.differs = differs;
	if (!differs)
	{
		++compares.same;
	}
	else if (!compares.firstDifferent.has_value())
	{
		compares.firstDifferent = turn;
		Log(spdlog::level::warn,
		    fmt::format("The world of slot {} ({}) parts from the server's world at turn {} (client {:016x}, server {:016x})",
		                seat, _seats[seat].name, turn, checksum, own->second));
	}
	if (compares.compared % k_CompareLogEvery == 0)
	{
		Log(spdlog::level::info,
		    fmt::format("Slot {}: {} checksums compared, {} the same", seat, compares.compared, compares.same));
	}
}

PlayerNames LockstepHost::ObserversGod() const
{
	return player_slots::ObserversGodOf(_seats.size());
}

size_t LockstepHost::ObserversInGame() const
{
	if (!_started)
	{
		return 0;
	}
	const auto away = std::ranges::count_if(_watchers, [](const Watcher& watcher) { return !watcher.connection.has_value(); });
	return Observers() + static_cast<size_t>(away);
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
