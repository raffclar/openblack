/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyScreenRules.h"

#include <algorithm>
#include <array>

#include <LobbiesJson.h>
#include <SDL_keycode.h>
#include <fmt/format.h>

#include "MultiplayerFrontEndRules.h"
#include "TextDatabase.h"

namespace openblack::gui::lobby_screens
{
namespace
{

namespace problems = lobby_api::lobby_problems;
using lobby_api::LobbyState;
using online::lobby::SlotHolder;

constexpr std::u16string_view k_Ellipsis = u"...";
/// The texts of the session list that vanilla does not have
constexpr std::string_view k_DownloadText = "This game uses another version of openblack.";

std::u16string Utf16(std::string_view text)
{
	return ToUtf16(text);
}

char16_t Lower(char16_t character)
{
	return character >= u'A' && character <= u'Z' ? static_cast<char16_t>(character - u'A' + u'a') : character;
}

/// Compares two texts with no difference between capital and small letters
int CompareFolded(std::string_view left, std::string_view right)
{
	const auto a = Utf16(left);
	const auto b = Utf16(right);
	const auto size = std::min(a.size(), b.size());
	for (size_t i = 0; i < size; ++i)
	{
		const auto x = Lower(a[i]);
		const auto y = Lower(b[i]);
		if (x != y)
		{
			return x < y ? -1 : 1;
		}
	}
	return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

/// The order of the statuses in the status column
int StatusRank(Status status)
{
	return static_cast<int>(status);
}

std::string MegabytesText(int64_t bytes)
{
	constexpr double k_BytesPerMegabyte = 1024.0 * 1024.0;
	return fmt::format("{:.1f} MB", static_cast<double>(bytes) / k_BytesPerMegabyte);
}

} // namespace

Sort ClickHeading(Sort sort, Column column) noexcept
{
	if (sort.column != column)
	{
		return {.column = column, .descending = false};
	}
	if (!sort.descending)
	{
		return {.column = column, .descending = true};
	}
	return {};
}

std::u16string HeadingText(std::u16string_view name, Column column, Sort sort)
{
	std::u16string text(name);
	if (sort.column == column)
	{
		text += sort.descending ? u" v" : u" ^";
	}
	return text;
}

Status StatusOf(const lobby_api::LobbySummary& lobby, std::string_view ownBuild)
{
	if (online::lobby::IsWrongVersion(lobby, ownBuild))
	{
		return Status::WrongVersion;
	}
	switch (lobby.state)
	{
	case LobbyState::Countdown:
		return Status::Starting;
	case LobbyState::Queued:
		return Status::Queued;
	case LobbyState::Assigned:
	case LobbyState::InGame:
	case LobbyState::Ended:
	case LobbyState::Closed:
		return Status::InGame;
	case LobbyState::Open:
		break;
	}
	return online::lobby::IsFull(lobby) ? Status::Full : Status::Open;
}

std::u16string StatusText(const lobby_api::LobbySummary& lobby, Status status, int64_t nowMs,
                          std::u16string_view wrongVersionText)
{
	switch (status)
	{
	case Status::Open:
		return u"Open";
	case Status::Full:
		return u"Full";
	case Status::Starting:
		return u"Starting";
	case Status::Queued:
		return lobby.queuePosition.has_value() ? Utf16(fmt::format("Waiting for a server ({})", *lobby.queuePosition))
		                                       : std::u16string(u"Waiting for a server");
	case Status::InGame:
	{
		if (!lobby.startedAt.has_value() || nowMs < *lobby.startedAt)
		{
			return u"In game";
		}
		constexpr int64_t k_MsPerMinute = 60'000;
		const auto minutes = (nowMs - *lobby.startedAt) / k_MsPerMinute;
		return Utf16(fmt::format("In game {}:{:02}", minutes / 60, minutes % 60));
	}
	case Status::WrongVersion:
		return std::u16string(wrongVersionText);
	}
	return {};
}

std::u16string GodsText(const lobby_api::LobbySummary& lobby)
{
	return Utf16(fmt::format("{}/{}", lobby.gods, lobby.openSlots.value_or(lobby.godSlots)));
}

std::u16string NameText(const lobby_api::LobbySummary& lobby)
{
	// The game's font has no lock: the list of vanilla marks a game with a password by a text, and this column by "#"
	return (lobby.locked ? std::u16string(u"# ") : std::u16string()) + Utf16(lobby.name);
}

std::vector<lobby_api::LobbySummary> ShownLobbies(std::span<const lobby_api::LobbySummary> lobbies,
                                                  const online::lobby::ListFilter& filter, std::string_view ownBuild, Sort sort)
{
	auto shown = online::lobby::VisibleLobbies(lobbies, filter, ownBuild);
	if (!sort.column.has_value())
	{
		return shown;
	}
	const auto column = *sort.column;
	const auto build = std::string(ownBuild);
	const auto compare = [column, &build](const lobby_api::LobbySummary& left, const lobby_api::LobbySummary& right) {
		switch (column)
		{
		case Column::Name:
			return CompareFolded(left.name, right.name);
		case Column::Host:
			return CompareFolded(left.host.name, right.host.name);
		case Column::Map:
			return CompareFolded(left.map.name.empty() ? left.map.file : left.map.name,
			                     right.map.name.empty() ? right.map.file : right.map.name);
		case Column::Gods:
			return left.gods == right.gods ? 0 : (left.gods < right.gods ? -1 : 1);
		case Column::Status:
		{
			const auto a = StatusRank(StatusOf(left, build));
			const auto b = StatusRank(StatusOf(right, build));
			return a == b ? 0 : (a < b ? -1 : 1);
		}
		}
		return 0;
	};
	std::ranges::stable_sort(shown, [&compare, &sort](const auto& left, const auto& right) {
		const auto order = compare(left, right);
		return sort.descending ? order > 0 : order < 0;
	});
	return shown;
}

std::u16string FitText(std::u16string_view text, float width, const MeasureText& measure)
{
	if (!measure || measure(text) <= width)
	{
		return std::u16string(text);
	}
	auto shorter = std::u16string(text);
	while (!shorter.empty())
	{
		shorter.pop_back();
		auto candidate = shorter + std::u16string(k_Ellipsis);
		if (measure(candidate) <= width)
		{
			return candidate;
		}
	}
	return {};
}

std::u16string ServersText(const std::optional<lobby_api::PoolState>& pool)
{
	if (!pool.has_value() || pool->total <= 0)
	{
		return {};
	}
	if (pool->free <= 0)
	{
		return u"All servers are in use: your game will wait for a free one.";
	}
	return Utf16(fmt::format("Servers: {} of {} free", pool->free, pool->total));
}

std::u16string DetailText(const lobby_api::LobbySummary& lobby, std::u16string_view mapName)
{
	if (online::lobby::IsInGame(lobby))
	{
		auto text = std::u16string(mapName) + Utf16(fmt::format(" - Host: {} - Players: ", lobby.host.name));
		const auto shown = std::min(lobby.players.size(), k_MostShownPlayers);
		for (size_t i = 0; i < shown; ++i)
		{
			text += (i == 0 ? std::u16string() : std::u16string(u", ")) + Utf16(lobby.players[i].name);
		}
		if (shown == 0)
		{
			text += Utf16(fmt::format("{}", lobby.gods));
		}
		else if (lobby.players.size() > shown)
		{
			text += Utf16(fmt::format(" and {} more", lobby.players.size() - shown));
		}
		return text;
	}
	auto text = std::u16string(mapName) +
	            Utf16(fmt::format(" - Host: {} - Password: {}", lobby.host.name, lobby.locked ? "yes" : "no"));
	if (!lobby.map.sha256.empty() && lobby.map.size > 0)
	{
		text += Utf16(fmt::format(" - Map download: {}", MegabytesText(lobby.map.size)));
	}
	return text;
}

bool ShowsJoinButton(const lobby_api::LobbySummary* picked)
{
	return picked != nullptr && !online::lobby::IsInGame(*picked);
}

std::string_view RefusalOf(const lobby_api::LobbySummary& lobby, std::string_view ownBuild)
{
	switch (StatusOf(lobby, ownBuild))
	{
	case Status::WrongVersion:
		return problems::k_WrongVersion;
	case Status::Full:
		return problems::k_Full;
	case Status::Starting:
	case Status::Queued:
	case Status::InGame:
		// The session takes no more players after the host launches it (user, 2026-11-12)
		return problems::k_Started;
	case Status::Open:
		break;
	}
	return {};
}

JoinStep JoinStepOf(const lobby_api::LobbySummary& lobby, std::string_view ownBuild)
{
	if (!RefusalOf(lobby, ownBuild).empty())
	{
		return JoinStep::Refuse;
	}
	return lobby.locked ? JoinStep::AskPassword : JoinStep::Join;
}

Problem ProblemOf(const ecs::systems::LobbyError& error, lobby_api::ClientKind client)
{
	const std::string_view code = error.code;
	const auto notice = [](std::string text) { return Problem {.box = ProblemBox::Notice, .text = std::move(text)}; };
	if (code == problems::k_PasswordNeeded)
	{
		return {.box = ProblemBox::AskPassword, .text = "This game needs a password."};
	}
	if (code == problems::k_WrongPassword)
	{
		return {.box = ProblemBox::IncorrectPassword, .text = {}};
	}
	if (code == problems::k_TooManyTries)
	{
		return notice("Too many tries. Wait a few minutes.");
	}
	if (code == problems::k_Banned)
	{
		return notice("You cannot join this game.");
	}
	if (code == problems::k_WrongVersion)
	{
		return notice(GameRefusalText("version", client));
	}
	if (code == problems::k_Full)
	{
		return notice("This game is full.");
	}
	if (code == problems::k_Started || code == problems::k_NotOpen)
	{
		return notice("The game has started.");
	}
	if (code == problems::k_Closed || code == problems::k_NoSuchLobby)
	{
		return notice("The host closed the game.");
	}
	if (code == problems::k_AlreadyHosting)
	{
		return notice("You already host a game.");
	}
	if (code == problems::k_InGame)
	{
		return notice("You are in a game now.");
	}
	if (code == problems::k_NameRefused)
	{
		return notice("That name is not allowed.");
	}
	if (code == problems::k_Throttled || code == problems::k_TooManyLobbies)
	{
		return notice("Too many games were made. Try again in a few minutes.");
	}
	if (code == problems::k_MapMissing)
	{
		return notice("You do not have the map of this game yet.");
	}
	if (code == problems::k_NotReady)
	{
		return notice("Each player must be ready.");
	}
	if (code == problems::k_TooFewGods)
	{
		return notice("There are not enough players.");
	}
	if (code == problems::k_NoGameServers)
	{
		return notice("No game server is free. Try again later.");
	}
	if (code == problems::k_SlotTaken)
	{
		return notice("Another player has that slot.");
	}
	if (code == problems::k_TooManyPlayers)
	{
		return notice(TooManyPlayersText(error.slots));
	}
	if (code == problems::k_NotHost)
	{
		return notice("Only the host can do that.");
	}
	if (code == problems::k_NoLobby)
	{
		return {.box = ProblemBox::AskCreate, .text = "No open game. Create one?"};
	}
	if (code == "bad_code")
	{
		return notice("That code is not correct.");
	}
	if (error.status == 404)
	{
		return notice("There is no such game.");
	}
	return {.box = ProblemBox::ConnectionFailed, .text = {}};
}

std::string GameRefusalText(std::string_view refusal, lobby_api::ClientKind client)
{
	if (refusal == "version")
	{
		return client == lobby_api::ClientKind::Browser
		           ? std::string(k_DownloadText) + " Reload the page to get the new version."
		           : std::string(k_DownloadText) + " Get the new version at bw.zanidrak.com";
	}
	if (refusal == "map")
	{
		return "Your map of this game is not the map of the game server.";
	}
	if (refusal == "ticket" || refusal == "banned")
	{
		return "You cannot join this game.";
	}
	if (refusal == "full")
	{
		return "This game is full.";
	}
	return "The game server refused the join.";
}

std::u16string DefaultSessionName(std::u16string_view profileName)
{
	auto name = std::u16string(profileName) + u"'s game";
	if (name.size() > lobby_api::k_MostLobbyNameCharacters)
	{
		name.resize(lobby_api::k_MostLobbyNameCharacters);
	}
	return name;
}

int32_t MostGodSlots(int32_t mapPlayers) noexcept
{
	return std::clamp(mapPlayers, lobby_api::k_LeastGodSlots, lobby_api::k_MostGodSlots);
}

int32_t ClampGodSlots(int32_t slots, int32_t mapPlayers) noexcept
{
	return std::clamp(slots, lobby_api::k_LeastGodSlots, MostGodSlots(mapPlayers));
}

int32_t ClampTimeLimit(int32_t minutes) noexcept
{
	return std::clamp(minutes, lobby_api::k_LeastTimeLimitMinutes, lobby_api::k_MostTimeLimitMinutes);
}

std::optional<std::string> CreateProblem(const CreateForm& form)
{
	const auto trimmed = std::ranges::find_if(form.name, [](char16_t character) { return character != u' '; });
	if (trimmed == form.name.end())
	{
		return "Type a name for the game.";
	}
	if (form.name.size() > lobby_api::k_MostLobbyNameCharacters)
	{
		return "The name of the game is too long.";
	}
	if (!form.map.has_value())
	{
		return "Pick a map for the game.";
	}
	if (form.password.size() > lobby_api::k_MostLobbyPasswordCharacters)
	{
		return "The password is too long.";
	}
	return std::nullopt;
}

lobby_api::LobbyCreateRequest CreateRequestOf(const CreateForm& form, const lobby_api::LobbyMap& map)
{
	return {
	    .name = ToUtf8(form.name),
	    .map = map,
	    .godSlots = form.godSlots,
	    .slotKinds =
	        std::vector<lobby_api::SlotKind>(static_cast<size_t>(std::max(form.godSlots, 0)), lobby_api::SlotKind::Open),
	    .visibility = form.isPrivate ? lobby_api::LobbyVisibility::Private : lobby_api::LobbyVisibility::Public,
	    .password = form.password.empty() ? std::nullopt : std::optional(ToUtf8(form.password)),
	    .settings = {.timeLimitMinutes = ClampTimeLimit(form.timeLimitMinutes), .conditions = {}, .observersAllowed = true},
	    .buildId = {},
	    .buildHash = {},
	};
}

std::u16string SlotLine(const online::lobby::SlotView& slot, const SlotTexts& texts)
{
	const std::array<multiplayer::TextArgument, 1> god = {int64_t {slot.slot + 1}};
	auto line = multiplayer::FormatText(texts.god, god) + u": ";
	switch (slot.holder)
	{
	case SlotHolder::Player:
		line += Utf16(slot.account.name);
		if (slot.host)
		{
			line += texts.host;
		}
		if (slot.own)
		{
			line += texts.current;
		}
		break;
	case SlotHolder::Open:
		line += u"- Open -";
		break;
	case SlotHolder::Computer:
		line += u"- Computer -";
		break;
	case SlotHolder::Closed:
		line += u"- Closed -";
		break;
	}
	return line;
}

std::u16string SlotNote(const online::lobby::SlotView& slot)
{
	if (slot.holder != SlotHolder::Player)
	{
		return {};
	}
	if (!slot.haveMap)
	{
		return u"downloading map";
	}
	return slot.away ? std::u16string(u"away") : std::u16string();
}

std::vector<HostAction> HostActionsOf(const online::lobby::LobbyRoom& room, int32_t slot)
{
	if (!room.IsHost() || room.Phase() == online::lobby::RoomPhase::Ended)
	{
		return {};
	}
	const auto views = room.Slots();
	if (slot < 0 || static_cast<size_t>(slot) >= views.size())
	{
		return {};
	}
	const auto& view = views[static_cast<size_t>(slot)];
	const bool beforeStart = room.Phase() == online::lobby::RoomPhase::Open;
	if (view.holder == SlotHolder::Player)
	{
		if (view.own)
		{
			return {};
		}
		// The host can kick or ban also after the start (user, 2026-11-12), and pass the host by choice
		return {HostAction::Kick, HostAction::Ban, HostAction::MakeHost};
	}
	if (!beforeStart)
	{
		return {};
	}
	std::vector<HostAction> actions;
	if (view.holder != SlotHolder::Open)
	{
		actions.push_back(HostAction::Open);
	}
	if (view.holder != SlotHolder::Computer)
	{
		actions.push_back(HostAction::Computer);
	}
	if (view.holder != SlotHolder::Closed)
	{
		actions.push_back(HostAction::Closed);
	}
	return actions;
}

std::u16string_view HostActionText(HostAction action) noexcept
{
	switch (action)
	{
	case HostAction::Kick:
		return u"Kick";
	case HostAction::Ban:
		return u"Ban";
	case HostAction::MakeHost:
		return u"Make host";
	case HostAction::Open:
		return u"Open";
	case HostAction::Computer:
		return u"Computer";
	case HostAction::Closed:
		return u"Closed";
	}
	return {};
}

int32_t PlayersOfRoom(const online::lobby::LobbyRoom& room)
{
	const auto& members = room.Data().members;
	return static_cast<int32_t>(std::ranges::count_if(
	    members, [](const lobby_api::LobbyMember& member) { return member.role == lobby_api::MemberRole::God; }));
}

CreateForm FormOfLobby(const lobby_api::Lobby& lobby, std::optional<size_t> map)
{
	return {
	    .name = Utf16(lobby.name),
	    .map = map,
	    .godSlots = lobby.godSlots,
	    .isPrivate = lobby.visibility == lobby_api::LobbyVisibility::Private,
	    .password = {},
	    .timeLimitMinutes = lobby.settings.timeLimitMinutes,
	};
}

std::optional<std::string> ChangeProblem(const CreateForm& form, int32_t mapPlaces, int32_t players)
{
	if (auto problem = CreateProblem(form); problem.has_value())
	{
		return problem;
	}
	if (!MapFitsRoom(mapPlaces, players) || !MapFitsRoom(form.godSlots, players))
	{
		return TooManyPlayersText(MapFitsRoom(mapPlaces, players) ? form.godSlots : mapPlaces);
	}
	return std::nullopt;
}

lobby_api::LobbyChangeRequest ChangeRequestOf(const lobby_api::Lobby& lobby, const CreateForm& form,
                                              const lobby_api::LobbyMap& map)
{
	lobby_api::LobbyChangeRequest change;
	if (const auto name = ToUtf8(form.name); name != lobby.name)
	{
		change.name = name;
	}
	if (map.file != lobby.map.file || map.sha256 != lobby.map.sha256)
	{
		change.map = map;
	}
	if (form.godSlots != lobby.godSlots)
	{
		const auto slots =
		    static_cast<size_t>(std::clamp(form.godSlots, lobby_api::k_LeastGodSlots, lobby_api::k_MostGodSlots));
		auto kinds = lobby.slotKinds;
		kinds.resize(slots, lobby_api::SlotKind::Open);
		change.slotKinds = std::move(kinds);
	}
	const auto visibility = form.isPrivate ? lobby_api::LobbyVisibility::Private : lobby_api::LobbyVisibility::Public;
	if (visibility != lobby.visibility)
	{
		change.visibility = visibility;
	}
	if (!form.password.empty())
	{
		change.password = ToUtf8(form.password);
	}
	if (const auto minutes = ClampTimeLimit(form.timeLimitMinutes); minutes != lobby.settings.timeLimitMinutes)
	{
		auto settings = lobby.settings;
		settings.timeLimitMinutes = minutes;
		change.settings = settings;
	}
	return change;
}

bool IsEmptyChange(const lobby_api::LobbyChangeRequest& change) noexcept
{
	return !change.name.has_value() && !change.map.has_value() && !change.slotKinds.has_value() &&
	       !change.visibility.has_value() && !change.password.has_value() && !change.settings.has_value();
}

bool SettingsOpen(const online::lobby::LobbyRoom& room)
{
	return room.IsHost() && room.Phase() == online::lobby::RoomPhase::Open;
}

bool MapFitsRoom(int32_t mapPlaces, int32_t players) noexcept
{
	return mapPlaces >= players;
}

std::string TooManyPlayersText(int32_t mapPlaces)
{
	if (mapPlaces <= 0)
	{
		return "Remove players first: this land has too few places.";
	}
	return fmt::format("Remove players first: this land has {} places.", mapPlaces);
}

std::vector<std::string> PlayersWithoutMap(const online::lobby::LobbyRoom& room)
{
	std::vector<std::string> names;
	for (const auto& member : room.Data().members)
	{
		if (!member.haveMap)
		{
			names.push_back(member.account.name);
		}
	}
	return names;
}

bool ShowsMapWarning(const online::lobby::LobbyRoom& room, const online::lobby::SlotView& slot)
{
	return !room.Data().map.sha256.empty() && slot.holder == SlotHolder::Player && !slot.haveMap;
}

bool StartShowsMapWarning(const online::lobby::LobbyRoom& room)
{
	return !room.Data().map.sha256.empty() && !PlayersWithoutMap(room).empty();
}

int64_t LoadPauseSecondsLeft(int64_t deadlineMs, int64_t nowMs) noexcept
{
	constexpr int64_t k_MsPerSecond = 1000;
	if (deadlineMs <= nowMs)
	{
		return 0;
	}
	return (deadlineMs - nowMs + k_MsPerSecond - 1) / k_MsPerSecond;
}

std::vector<std::u16string> LoadPauseLines(std::span<const std::string> loadingNames, int64_t deadlineMs, int64_t nowMs)
{
	return LoadPauseLines(loadingNames, LoadPauseSecondsLeft(deadlineMs, nowMs));
}

std::vector<std::u16string> LoadPauseLines(std::span<const std::string> loadingNames, int64_t secondsLeft)
{
	if (loadingNames.empty())
	{
		return {};
	}
	std::string names;
	for (const auto& name : loadingNames)
	{
		if (!names.empty())
		{
			names += ", ";
		}
		names += name;
	}
	const auto seconds = std::max<int64_t>(secondsLeft, 0);
	return {std::u16string(k_LoadPauseText) + Utf16(fmt::format(" ({}:{:02})", seconds / 60, seconds % 60)), Utf16(names)};
}

std::vector<lobby_api::SlotKind> SlotKindsAfter(const lobby_api::Lobby& lobby, int32_t slot, lobby_api::SlotKind kind)
{
	auto kinds = lobby.slotKinds;
	kinds.resize(static_cast<size_t>(std::max(lobby.godSlots, 0)), lobby_api::SlotKind::Open);
	if (slot >= 0 && static_cast<size_t>(slot) < kinds.size())
	{
		kinds[static_cast<size_t>(slot)] = kind;
	}
	return kinds;
}

std::u16string RoomStatusText(const online::lobby::LobbyRoom& room)
{
	const auto& lobby = room.Data();
	std::string text;
	if (!lobby.code.empty())
	{
		text = fmt::format("Code: {}", lobby_api::LobbyCodeText(lobby.code));
	}
	const auto add = [&text](std::string_view part) {
		if (!text.empty())
		{
			text += "   ";
		}
		text += part;
	};
	add(lobby.visibility == lobby_api::LobbyVisibility::Private ? "Private" : "Public");
	add(lobby.locked ? "Password: yes" : "Password: no");
	switch (room.Phase())
	{
	case online::lobby::RoomPhase::Countdown:
		add("Starting");
		break;
	case online::lobby::RoomPhase::Queued:
		add(room.QueuePosition().has_value() ? fmt::format("Waiting for a server ({})", *room.QueuePosition())
		                                     : std::string("Waiting for a server"));
		break;
	case online::lobby::RoomPhase::Assigned:
		add("Joining the game server");
		break;
	case online::lobby::RoomPhase::InGame:
		add("In game");
		break;
	case online::lobby::RoomPhase::Open:
	case online::lobby::RoomPhase::Ended:
		break;
	}
	return Utf16(text);
}

StartButton StartButtonOf(const online::lobby::LobbyRoom& room) noexcept
{
	if (!room.IsHost())
	{
		return StartButton::None;
	}
	switch (room.Phase())
	{
	case online::lobby::RoomPhase::Open:
		return StartButton::Start;
	case online::lobby::RoomPhase::Countdown:
	case online::lobby::RoomPhase::Queued:
		return StartButton::Cancel;
	case online::lobby::RoomPhase::Assigned:
	case online::lobby::RoomPhase::InGame:
	case online::lobby::RoomPhase::Ended:
		break;
	}
	return StartButton::None;
}

bool LeaveAsks(const online::lobby::LobbyRoom& room) noexcept
{
	const auto* own = room.OwnMember();
	return room.IsHost() || (own != nullptr && own->ready);
}

MapNeed MapNeedOf(const lobby_api::LobbyMap& map, bool hasLandOfGame) noexcept
{
	if (!map.sha256.empty())
	{
		return MapNeed::Download;
	}
	return hasLandOfGame ? MapNeed::Have : MapNeed::Missing;
}

KeyAction KeyActionOf(KeyScreen screen, const KeyPress& press) noexcept
{
	const bool isReturn = press.key == SDLK_RETURN || press.key == SDLK_KP_ENTER;
	if (press.key == SDLK_ESCAPE)
	{
		return KeyAction::Back;
	}
	switch (screen)
	{
	case KeyScreen::List:
		if (press.key == SDLK_F5)
		{
			return KeyAction::Refresh;
		}
		if (press.control && press.key == SDLK_f)
		{
			return KeyAction::Search;
		}
		if (press.control && press.key == SDLK_j)
		{
			return KeyAction::JoinByCode;
		}
		if (isReturn && !press.editing)
		{
			return KeyAction::Join;
		}
		break;
	case KeyScreen::Room:
		if (press.control && isReturn)
		{
			return KeyAction::Start;
		}
		if (!press.editing && !press.control && press.key == SDLK_r)
		{
			return KeyAction::ToggleReady;
		}
		break;
	case KeyScreen::Create:
		break;
	}
	return KeyAction::None;
}

} // namespace openblack::gui::lobby_screens
