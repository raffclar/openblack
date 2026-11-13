/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Lobbies.h>
#include <LobbyApi.h>

/// The model of the lobby list and of the lobby room of the player. The model reads the snapshots and the push events of
/// the lobby service and keeps the state that the screens show. It has no global state and makes no calls, so that the
/// tests can give it events.
namespace openblack::online::lobby
{

/// The seconds of the countdown before the start
constexpr int32_t k_CountdownSeconds = 5;
/// The fewest gods in god slots for a start
constexpr int32_t k_LeastGods = 1;
/// The most lines that the room keeps. The oldest line goes first.
constexpr size_t k_MostRoomLines = 500;

/// The first box of the multiplayer front end on the internet
enum class EntryBox : uint8_t
{
	/// The player has a session, or the site can give one: the lobby list opens at once
	LobbyList,
	/// The player must log in: the box with the choice of the internet or the local network opens
	FindGames,
};

/// A player with a session, or a browser player that is signed in on the site, goes to the lobby list at once
[[nodiscard]] constexpr EntryBox FirstBox(bool hasSession, bool siteCanSignIn) noexcept
{
	return hasSession || siteCanSignIn ? EntryBox::LobbyList : EntryBox::FindGames;
}

/// The phase of the room for the player
enum class RoomPhase : uint8_t
{
	Open,
	Countdown,
	/// The lobby waits in the line for a free game server
	Queued,
	/// The lobby has a game server. The player gets a ticket and joins it.
	Assigned,
	InGame,
	/// The player is not in the lobby now. The screens go back to the lobby list.
	Ended,
};

/// Why the player is not in the lobby now
enum class EndReason : uint8_t
{
	/// The player left
	Left,
	/// The player closed the lobby as its host
	OwnClose,
	/// The host closed the lobby
	HostClosed,
	/// The host left the lobby before the start, so that the lobby ended
	HostLeft,
	Kicked,
	Banned,
	/// The game of the lobby ended
	GameEnded,
	/// The game server of the lobby stopped
	GameFailed,
	/// A new version of openblack is live
	NewVersion,
	/// The service closed the lobby for a different reason
	Closed,
};

struct RoomEnd
{
	EndReason reason {EndReason::Left};
	/// The host that kicked or banned the player
	std::string by;
};

/// The message box that the player sees on the lobby list after the end. None when the player sees no box.
[[nodiscard]] std::optional<std::string> EndMessage(const RoomEnd& end, lobby_api::ClientKind client);

/// A line that the room writes in the chat from an event of the lobby
enum class SystemLineKind : uint8_t
{
	Joined,
	Left,
	Kicked,
	Banned,
	NewHost,
	MapChanged,
	SettingsChanged,
	ReadyCleared,
	/// One second of the countdown
	Countdown,
	/// The start stopped because a player is not ready
	NotReady,
	/// The start stopped for a different reason
	StartCancelled,
	/// The game server did not start
	CouldNotStart,
	/// Too few gods joined the game server in the time to join
	NotEnoughJoined,
	/// The lobby waits in the line for a free game server
	Queued,
};

/// The number of the kinds of the system lines
constexpr size_t k_SystemLineKinds = static_cast<size_t>(SystemLineKind::Queued) + 1;

struct SystemLine
{
	SystemLineKind kind {SystemLineKind::Joined};
	/// The player or the map that the line is about
	std::string subject;
	/// The host that did the action
	std::string actor;
	/// The seconds of the countdown, or the place in the line
	int32_t number {0};
};

[[nodiscard]] std::string SystemLineText(const SystemLine& line);

enum class RoomLineKind : uint8_t
{
	/// A chat line of the player
	Own,
	/// A chat line of a different player
	Other,
	/// A line that the room writes from an event
	System,
};

struct RoomLine
{
	RoomLineKind kind {RoomLineKind::System};
	/// The name of the player that wrote a chat line
	std::string from;
	/// The text of a chat line
	std::string text;
	/// The line of a System line
	SystemLine system;
};

/// The text of a line as the chat box shows it: "name> text" for the player, "name: text" for others
[[nodiscard]] std::string RoomLineText(const RoomLine& line);

/// What a god slot holds
enum class SlotHolder : uint8_t
{
	Player,
	Open,
	Computer,
	Closed,
};

struct SlotView
{
	int32_t slot {0};
	SlotHolder holder {SlotHolder::Open};
	/// The player of the slot, when the holder is Player
	lobby_api::AccountRef account;
	bool ready {false};
	bool host {false};
	/// The slot of the player of this computer
	bool own {false};
	bool away {false};
	/// The player of the slot has the map of the lobby. False while the player downloads it.
	bool haveMap {true};
};

/// The lobby room of the player
class LobbyRoom
{
public:
	/// A room from the snapshot of the lobby. The player is the account of the session.
	LobbyRoom(lobby_api::Lobby lobby, lobby_api::AccountRef self);

	[[nodiscard]] const lobby_api::Lobby& Data() const noexcept { return _lobby; }
	[[nodiscard]] const lobby_api::AccountRef& Self() const noexcept { return _self; }
	/// The push channel of the lobby: "lobby-<id>"
	[[nodiscard]] std::string Channel() const;
	[[nodiscard]] RoomPhase Phase() const noexcept { return _phase; }
	/// Why the player left the room, in the phase Ended
	[[nodiscard]] const std::optional<RoomEnd>& End() const noexcept { return _end; }
	[[nodiscard]] bool IsHost() const noexcept { return _lobby.host.id == _self.id; }
	[[nodiscard]] const lobby_api::LobbyMember* FindMember(int64_t account) const;
	[[nodiscard]] const lobby_api::LobbyMember* OwnMember() const { return FindMember(_self.id); }
	/// One view for each god slot of the lobby
	[[nodiscard]] std::vector<SlotView> Slots() const;
	[[nodiscard]] std::vector<lobby_api::AccountRef> Observers() const;
	[[nodiscard]] std::span<const RoomLine> Lines() const noexcept { return _lines; }
	[[nodiscard]] std::optional<int32_t> QueuePosition() const noexcept { return _lobby.queuePosition; }
	[[nodiscard]] std::optional<int64_t> CountdownAt() const noexcept { return _lobby.countdownAt; }
	[[nodiscard]] const std::string& GameId() const noexcept { return _lobby.gameId; }
	/// The address of the game socket of "game.assigned", empty before it
	[[nodiscard]] const std::string& GameUrl() const noexcept { return _gameUrl; }
	/// The number of the members in god slots
	[[nodiscard]] int32_t GodsInSlots() const;
	/// The host can start when each other player in a god slot is ready, and the host has the map. The ready of the host
	/// comes with the start.
	[[nodiscard]] bool CanStart() const;
	[[nodiscard]] bool CanReady() const;
	/// The player can move to an open slot that nobody has, before the player is ready
	[[nodiscard]] bool CanPickSlot(int32_t slot) const;
	/// The number of the changes. The screens compare it to see a change.
	[[nodiscard]] uint32_t Revision() const noexcept { return _revision; }
	/// The number of the times that the host set the map, also when the host set the same map again. A failed download
	/// of the map starts again at the next time, as the download of the original game starts again at each map that the
	/// host sets.
	[[nodiscard]] uint32_t MapSets() const noexcept { return _mapSets; }
	/// The number of the system lines of this kind that the room wrote. The count does not go down when the room
	/// removes its oldest lines.
	[[nodiscard]] uint32_t SystemLineCount(SystemLineKind kind) const
	{
		return _systemLineCounts.at(static_cast<size_t>(kind));
	}

	/// Uses an event of the push channel. The room ignores an event of a different lobby.
	void Apply(const lobby_api::PushEvent& event);
	/// A change of the lobby came, and the room needs the lobby again for the new values. The flag clears.
	[[nodiscard]] bool TakeReloadWanted() noexcept { return std::exchange(_reloadWanted, false); }
	/// Writes the line of each second of the countdown. The time is milliseconds since 1970 (UTC).
	void Tick(int64_t nowMs);
	/// Uses a new snapshot of the lobby, after the push channel lost events. The lines stay.
	void Reload(lobby_api::Lobby lobby);
	/// Uses the answer of a change of the member of the player
	void ApplyOwnMember(lobby_api::LobbyMember member);
	/// Uses the answer of the start of the host
	void ApplyStart(const lobby_api::StartReply& reply);
	/// The player is not in the lobby now
	void Finish(EndReason reason, std::string by = {});

private:
	lobby_api::LobbyMember* MutableMember(int64_t account);
	[[nodiscard]] lobby_api::SlotKind KindOf(int32_t slot) const;
	[[nodiscard]] bool IsBeforeStart() const noexcept;
	void AddLine(RoomLine line);
	void AddSystemLine(SystemLine line);
	void ApplyMemberEvent(std::string_view type, const lobby_api::MemberEvent& event);
	void ApplyChange(const lobby_api::LobbyChange& change);
	void ApplyStateEvent(std::string_view type, const lobby_api::LobbyStateEvent& event);
	/// Writes the lines of the change of the lobby that the last reload shows
	void WriteChangeLines(const lobby_api::LobbyMap& oldMap);
	void StartCountdown(std::optional<int64_t> at);
	void EnterQueue(std::optional<int32_t> position);
	void Changed() noexcept { ++_revision; }

	lobby_api::Lobby _lobby;
	lobby_api::AccountRef _self;
	RoomPhase _phase {RoomPhase::Open};
	std::optional<RoomEnd> _end;
	std::vector<RoomLine> _lines;
	std::string _gameUrl;
	/// The last second of the countdown that has a line
	int32_t _announced {k_CountdownSeconds + 1};
	uint32_t _revision {0};
	uint32_t _mapSets {0};
	std::array<uint32_t, k_SystemLineKinds> _systemLineCounts {};
	/// A change of the lobby came: the room needs the lobby again
	bool _reloadWanted {false};
	/// The fields of the changes that have no line yet, until the next reload
	std::vector<std::string> _changedFields;
	/// A change cleared the ready of each member, and the line comes after the next reload
	bool _readyClearedLine {false};
};

/// The choices of the lobby list. The screens keep them in the profile.
struct ListFilter
{
	/// A part of the name of the lobby, of the host or of the map. The case of the letters does not matter.
	std::string search;
	/// The file of the map, or empty for all maps
	std::string mapFile;
	bool showFull {true};
	bool showLocked {true};
	bool showInGame {false};
	bool showOtherVersions {false};
};

/// The lobby needs a different build of openblack ("(Wrong Version)")
[[nodiscard]] bool IsWrongVersion(const lobby_api::LobbySummary& lobby, std::string_view ownBuild);
[[nodiscard]] bool IsFull(const lobby_api::LobbySummary& lobby);
/// The game of the lobby started
[[nodiscard]] bool IsInGame(const lobby_api::LobbySummary& lobby);
/// The player can ask to join the lobby as a god
[[nodiscard]] bool CanJoin(const lobby_api::LobbySummary& lobby, std::string_view ownBuild);
/// The order of the list with no sort column: the open lobbies first, then the most gods, then the newest
[[nodiscard]] bool DefaultOrderBefore(const lobby_api::LobbySummary& left, const lobby_api::LobbySummary& right);
/// The lobbies that the filter shows, in the default order
[[nodiscard]] std::vector<lobby_api::LobbySummary> VisibleLobbies(std::span<const lobby_api::LobbySummary> lobbies,
                                                                  const ListFilter& filter, std::string_view ownBuild);

/// The lobby list with the changes of the push channel
class LobbyListModel
{
public:
	void Replace(lobby_api::LobbyList list);
	/// Uses an event of the channel "lobbies". Gives true when the list changed.
	bool Apply(const lobby_api::PushEvent& event);
	void Clear();

	[[nodiscard]] std::span<const lobby_api::LobbySummary> Entries() const noexcept { return _lobbies; }
	[[nodiscard]] const lobby_api::LobbySummary* Find(std::string_view id) const;
	[[nodiscard]] std::optional<lobby_api::PoolState> Pool() const noexcept { return _pool; }
	[[nodiscard]] uint32_t Revision() const noexcept { return _revision; }

private:
	std::vector<lobby_api::LobbySummary> _lobbies;
	std::optional<lobby_api::PoolState> _pool;
	uint32_t _revision {0};
};

} // namespace openblack::online::lobby
