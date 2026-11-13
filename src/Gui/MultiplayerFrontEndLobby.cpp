/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lobby sessions of the multiplayer front end: the lobby list, the box that makes a session and the room of a
// session. The lobby service keeps the sessions; the lobby room system gives the list and the room.

#include <algorithm>
#include <chrono>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/LobbyRoomSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Locator.h"
#include "MultiplayerFrontEnd.h"
#include "Online/MapDownload.h"
#include "Online/NetworkSession.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "TextDatabase.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack::gui;
using namespace openblack::gui::multiplayer;
namespace screens = openblack::gui::lobby_screens;
using openblack::ecs::systems::LobbyDone;
using openblack::ecs::systems::LobbyError;
using openblack::ecs::systems::LobbyResult;
using openblack::online::lobby::RoomPhase;

bool MultiplayerFrontEnd::UsesLobbySessions() const noexcept
{
	return _services.lobbyRoom != nullptr && _services.lobby != nullptr && !_noLobbySessions;
}

int64_t MultiplayerFrontEnd::NowMs() const
{
	if (_services.nowMs)
	{
		return _services.nowMs();
	}
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void MultiplayerFrontEnd::ShowLobbyNotice(std::u16string text)
{
	_lobbyMessageText = std::move(text);
	Show(Message::LobbyNotice);
}

void MultiplayerFrontEnd::ShowLobbyProblem(const LobbyError& error)
{
	const auto client = _settings.browser ? lobby_api::ClientKind::Browser : lobby_api::ClientKind::Desktop;
	const auto problem = screens::ProblemOf(error, client);
	switch (problem.box)
	{
	case screens::ProblemBox::Notice:
		ShowLobbyNotice(ToUtf16(problem.text));
		break;
	case screens::ProblemBox::AskPassword:
		_lobbyMessageText = ToUtf16(problem.text);
		Show(Message::LobbyPassword);
		break;
	case screens::ProblemBox::AskCreate:
		_lobbyMessageText = ToUtf16(problem.text);
		_lobbyQuestion = LobbyQuestionKind::Create;
		Show(Message::LobbyQuestion);
		break;
	case screens::ProblemBox::IncorrectPassword:
		Show(Message::IncorrectPassword);
		break;
	case screens::ProblemBox::ConnectionFailed:
		Show(Message::ConnectionFailed);
		break;
	}
}

openblack::ecs::systems::LobbyCallback<LobbyDone> MultiplayerFrontEnd::LobbyDoneCallback()
{
	_busy = true;
	return [this, generation = _generation](LobbyResult<LobbyDone>&& result) {
		if (generation != _generation)
		{
			return;
		}
		_busy = false;
		if (!result.Ok())
		{
			ShowLobbyProblem(result.error);
		}
	};
}

void MultiplayerFrontEnd::EnterLobbyList()
{
	_flow.box = Box::LobbyList;
	_selectedLobby.clear();
	if (_services.lobby != nullptr)
	{
		_services.lobby->WatchServers(false);
	}
	_services.lobbyRoom->WatchList(true);
	RefreshLobbies();
	// The names of the maps of the service, for the box that makes a session
	GetLobbyMaps({});
}

void MultiplayerFrontEnd::RefreshLobbies()
{
	if (!UsesLobbySessions())
	{
		return;
	}
	_busy = true;
	const auto generation = ++_generation;
	_services.lobbyRoom->RefreshList([this, generation](LobbyResult<LobbyDone>&& result) {
		if (generation != _generation)
		{
			return;
		}
		_busy = false;
		if (result.Ok())
		{
			return;
		}
		// A lobby with no sessions gives the game list of its game servers
		if (result.error.status == 404 && result.error.code.empty())
		{
			_noLobbySessions = true;
			_services.lobbyRoom->WatchList(false);
			EnterGameList();
			return;
		}
		ShowLobbyProblem(result.error);
	});
}

std::vector<openblack::lobby_api::LobbySummary> MultiplayerFrontEnd::GetShownLobbies() const
{
	if (_services.lobbyRoom == nullptr)
	{
		return {};
	}
	return screens::ShownLobbies(_services.lobbyRoom->List().Entries(), _listFilter, _services.lobbyRoom->BuildId(), _listSort);
}

std::string MultiplayerFrontEnd::GetLobbyBuild() const
{
	return _services.lobbyRoom != nullptr ? _services.lobbyRoom->BuildId() : std::string();
}

std::optional<openblack::lobby_api::PoolState> MultiplayerFrontEnd::GetServerPool() const
{
	return _services.lobbyRoom != nullptr ? _services.lobbyRoom->List().Pool() : std::nullopt;
}

const openblack::lobby_api::LobbySummary* MultiplayerFrontEnd::GetSelectedSummary() const
{
	if (_services.lobbyRoom == nullptr || _selectedLobby.empty())
	{
		return nullptr;
	}
	return _services.lobbyRoom->List().Find(_selectedLobby);
}

void MultiplayerFrontEnd::JoinLobby(std::string id, std::optional<std::string> password)
{
	++_generation;
	_services.lobbyRoom->Join(std::move(id), std::move(password), lobby_api::MemberRole::God, std::nullopt,
	                          LobbyDoneCallback());
}

void MultiplayerFrontEnd::JoinSelectedLobby()
{
	if (_busy || _message != Message::None)
	{
		return;
	}
	const auto* lobby = GetSelectedSummary();
	if (lobby == nullptr)
	{
		return;
	}
	const auto& build = _services.lobbyRoom->BuildId();
	switch (screens::JoinStepOf(*lobby, build))
	{
	case screens::JoinStep::Join:
		JoinLobby(lobby->id, std::nullopt);
		break;
	case screens::JoinStep::AskPassword:
		_passwordLobby = lobby->id;
		_lobbyMessageText = u"This game needs a password.";
		Show(Message::LobbyPassword);
		break;
	case screens::JoinStep::Refuse:
		ShowLobbyProblem(LobbyError {.status = 409, .code = std::string(screens::RefusalOf(*lobby, build))});
		break;
	}
}

void MultiplayerFrontEnd::QuickJoinLobby()
{
	if (_busy || _message != Message::None || !UsesLobbySessions())
	{
		return;
	}
	++_generation;
	_services.lobbyRoom->QuickJoin(LobbyDoneCallback());
}

void MultiplayerFrontEnd::AskLobbyCode()
{
	if (_busy || _message != Message::None)
	{
		return;
	}
	_lobbyMessageText = u"Type the code of the game:";
	Show(Message::LobbyCode);
}

void MultiplayerFrontEnd::OpenCreateLobby()
{
	if (_busy || _message != Message::None || !UsesLobbySessions())
	{
		return;
	}
	_flow.box = Box::CreateLobby;
	_editsLobby = false;
	_createForm.password.clear();
	const auto maps = GetSessionMaps();
	if (_createForm.name.empty())
	{
		_createForm.name = screens::DefaultSessionName(GetPlayerName());
	}
	if (!_createForm.map.has_value() || *_createForm.map >= maps.size())
	{
		_createForm.map = maps.empty() ? std::nullopt : std::optional<size_t>(0);
	}
	if (_createForm.map.has_value())
	{
		_createForm.godSlots = screens::MostGodSlots(maps[*_createForm.map].places);
	}
}

std::vector<MultiplayerFrontEnd::SessionMap> MultiplayerFrontEnd::GetSessionMaps() const
{
	std::vector<SessionMap> maps;
	// The maps of the service first: the custom maps, which the players download
	for (const auto& map : _lobbyMaps)
	{
		maps.push_back({.name = ToUtf16(map.name),
		                .places = map.players,
		                .map = {.file = map.file, .name = map.name, .sha256 = map.sha256, .size = map.size}});
	}
	for (const auto& land : _maps)
	{
		// A land of the service is in the list one time
		if (std::ranges::any_of(maps, [&land](const SessionMap& map) { return map.map.file == land.file; }))
		{
			continue;
		}
		maps.push_back({.name = land.name,
		                .places = land.players,
		                .map = {.file = land.file, .name = ToUtf8(land.name), .sha256 = {}, .size = 0}});
	}
	return maps;
}

void MultiplayerFrontEnd::SubmitCreateLobby()
{
	if (_busy || _message != Message::None || !UsesLobbySessions())
	{
		return;
	}
	if (const auto problem = screens::CreateProblem(_createForm); problem.has_value())
	{
		ShowLobbyNotice(ToUtf16(*problem));
		return;
	}
	const auto maps = GetSessionMaps();
	if (*_createForm.map >= maps.size())
	{
		return;
	}
	const auto& map = maps[*_createForm.map];
	_createForm.godSlots = screens::ClampGodSlots(_createForm.godSlots, map.places);
	if (_editsLobby)
	{
		SubmitLobbySettings(map);
		return;
	}
	++_generation;
	_services.lobbyRoom->Create(screens::CreateRequestOf(_createForm, map.map), LobbyDoneCallback());
}

void MultiplayerFrontEnd::OpenLobbySettings()
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || !screens::SettingsOpen(*room) || _busy || _message != Message::None)
	{
		return;
	}
	const auto maps = GetSessionMaps();
	const auto& lobby = room->Data();
	const auto current = std::ranges::find_if(maps, [&lobby](const SessionMap& map) { return map.map.file == lobby.map.file; });
	_createForm = screens::FormOfLobby(lobby, current != maps.end() ? std::optional(static_cast<size_t>(current - maps.begin()))
	                                                                : std::nullopt);
	_editsLobby = true;
	_flow.box = Box::CreateLobby;
}

void MultiplayerFrontEnd::SubmitLobbySettings(const SessionMap& map)
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || !screens::SettingsOpen(*room))
	{
		// The settings are locked after the start
		_editsLobby = false;
		_flow.box = Box::LobbyRoom;
		return;
	}
	// The host must kick players before a map or a number of slots with fewer places (user, 2026-11-12)
	if (const auto problem = screens::ChangeProblem(_createForm, map.places, screens::PlayersOfRoom(*room));
	    problem.has_value())
	{
		ShowLobbyNotice(ToUtf16(*problem));
		return;
	}
	auto change = screens::ChangeRequestOf(room->Data(), _createForm, map.map);
	_editsLobby = false;
	_createForm.password.clear();
	_flow.box = Box::LobbyRoom;
	if (screens::IsEmptyChange(change))
	{
		return;
	}
	_services.lobbyRoom->Change(std::move(change), LobbyDoneCallback());
}

const openblack::online::lobby::LobbyRoom* MultiplayerFrontEnd::GetLobbyRoom() const
{
	return _services.lobbyRoom != nullptr ? _services.lobbyRoom->Room() : nullptr;
}

void MultiplayerFrontEnd::ToggleLobbyReady()
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || _busy || _message != Message::None)
	{
		return;
	}
	const auto* own = room->OwnMember();
	if (own == nullptr)
	{
		return;
	}
	// Ready waits for the map (user, 2026-11-12)
	if (!own->ready && !room->CanReady())
	{
		if (!own->haveMap)
		{
			ShowLobbyNotice(u"You do not have the map of this game yet.");
		}
		return;
	}
	_services.lobbyRoom->SetReady(!own->ready, LobbyDoneCallback());
}

void MultiplayerFrontEnd::SendLobbyChat(std::u16string_view text)
{
	const auto line = ClampChat(text);
	if (!HasChatText(line) || GetLobbyRoom() == nullptr)
	{
		return;
	}
	// A chat line does not make the box busy: the player can type the next line at once
	_services.lobbyRoom->SendChat(ToUtf8(line), [this, generation = _generation](LobbyResult<LobbyDone>&& result) {
		if (generation == _generation && !result.Ok())
		{
			ShowLobbyProblem(result.error);
		}
	});
}

void MultiplayerFrontEnd::PickLobbySlot(int32_t slot)
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || _busy || _message != Message::None || !room->CanPickSlot(slot))
	{
		return;
	}
	_services.lobbyRoom->PickSlot(slot, LobbyDoneCallback());
}

void MultiplayerFrontEnd::DoHostAction(int32_t slot, screens::HostAction action)
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || _busy || _message != Message::None)
	{
		return;
	}
	const auto actions = screens::HostActionsOf(*room, slot);
	if (std::ranges::find(actions, action) == actions.end())
	{
		return;
	}
	const auto slots = room->Slots();
	const auto& view = slots[static_cast<size_t>(slot)];
	switch (action)
	{
	case screens::HostAction::Kick:
	case screens::HostAction::Ban:
	{
		// The box asks first: "Kick owl?"
		const bool ban = action == screens::HostAction::Ban;
		_lobbyQuestion = ban ? LobbyQuestionKind::Ban : LobbyQuestionKind::Kick;
		_questionAccount = view.account.id;
		_lobbyMessageText = ToUtf16((ban ? "Ban " : "Kick ") + view.account.name + "?");
		Show(Message::LobbyQuestion);
		break;
	}
	case screens::HostAction::MakeHost:
		_services.lobbyRoom->MakeHost(view.account.id, LobbyDoneCallback());
		break;
	case screens::HostAction::Open:
	case screens::HostAction::Computer:
	case screens::HostAction::Closed:
	{
		const auto kind = action == screens::HostAction::Open       ? lobby_api::SlotKind::Open
		                  : action == screens::HostAction::Computer ? lobby_api::SlotKind::Computer
		                                                            : lobby_api::SlotKind::Closed;
		lobby_api::LobbyChangeRequest change;
		change.slotKinds = screens::SlotKindsAfter(room->Data(), slot, kind);
		_services.lobbyRoom->Change(std::move(change), LobbyDoneCallback());
		break;
	}
	}
}

void MultiplayerFrontEnd::StartOrCancelLobby()
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || _busy || _message != Message::None)
	{
		return;
	}
	switch (screens::StartButtonOf(*room))
	{
	case screens::StartButton::Start:
		_services.lobbyRoom->Start(LobbyDoneCallback());
		break;
	case screens::StartButton::Cancel:
		_services.lobbyRoom->CancelStart(LobbyDoneCallback());
		break;
	case screens::StartButton::None:
		break;
	}
}

void MultiplayerFrontEnd::ChangeLobbyMap(size_t map)
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || !room->IsHost() || room->Phase() != RoomPhase::Open || _busy || _message != Message::None)
	{
		return;
	}
	const auto maps = GetSessionMaps();
	if (map >= maps.size() || maps[map].map.file == room->Data().map.file)
	{
		return;
	}
	// The host must kick players before a map with fewer places (user, 2026-11-12)
	if (!screens::MapFitsRoom(maps[map].places, screens::PlayersOfRoom(*room)))
	{
		ShowLobbyNotice(ToUtf16(screens::TooManyPlayersText(maps[map].places)));
		return;
	}
	lobby_api::LobbyChangeRequest change;
	change.map = maps[map].map;
	_services.lobbyRoom->Change(std::move(change), LobbyDoneCallback());
}

void MultiplayerFrontEnd::AskLobbyPassword()
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || !screens::SettingsOpen(*room) || _busy || _message != Message::None)
	{
		return;
	}
	_lobbyMessageText = u"Type the new password. Leave it empty for no password.";
	Show(Message::LobbyNewPassword);
}

void MultiplayerFrontEnd::CloseLobby()
{
	const auto* room = GetLobbyRoom();
	// No Close after the start (user, 2026-11-12)
	if (room == nullptr || !room->IsHost() || room->Phase() != RoomPhase::Open || _busy || _message != Message::None)
	{
		return;
	}
	_lobbyQuestion = LobbyQuestionKind::Close;
	_lobbyMessageText = u"Close this game? Each player goes back to the list.";
	Show(Message::LobbyQuestion);
}

void MultiplayerFrontEnd::LeaveLobby()
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr)
	{
		EnterLobbyList();
		return;
	}
	if (_busy || _message != Message::None)
	{
		return;
	}
	if (screens::LeaveAsks(*room) && room->Phase() != RoomPhase::Ended)
	{
		_lobbyQuestion = LobbyQuestionKind::Leave;
		// Before the start, the game of a host that leaves ends for all (user, 2026-11-12)
		_lobbyMessageText = room->IsHost() && room->Phase() == RoomPhase::Open
		                        ? u"Leave this game? You are the host, so the game closes for each player."
		                        : u"Leave this game?";
		Show(Message::LobbyQuestion);
		return;
	}
	++_generation;
	_services.lobbyRoom->Leave({});
	ForgetLobbyRoom();
	EnterLobbyList();
}

std::optional<int32_t> MultiplayerFrontEnd::GetCountdownSeconds() const
{
	const auto* room = GetLobbyRoom();
	if (room == nullptr || room->Phase() != RoomPhase::Countdown || !room->CountdownAt().has_value())
	{
		return std::nullopt;
	}
	constexpr int64_t k_MsPerSecond = 1000;
	const auto left = std::max<int64_t>(*room->CountdownAt() - NowMs(), 0);
	return static_cast<int32_t>((left + k_MsPerSecond - 1) / k_MsPerSecond);
}

void MultiplayerFrontEnd::AnswerLobbyText(std::u16string text)
{
	const auto message = std::exchange(_message, Message::None);
	switch (message)
	{
	case Message::LobbyPassword:
		if (!_passwordLobby.empty() && !text.empty())
		{
			JoinLobby(_passwordLobby, ToUtf8(text));
		}
		break;
	case Message::LobbyCode:
	{
		if (text.empty())
		{
			break;
		}
		++_generation;
		_busy = true;
		_services.lobbyRoom->FindByCode(ToUtf8(text), [this, generation = _generation](LobbyResult<std::string>&& found) {
			if (generation != _generation)
			{
				return;
			}
			_busy = false;
			if (!found.Ok())
			{
				ShowLobbyProblem(found.error);
				return;
			}
			_passwordLobby = *found.value;
			JoinLobby(*found.value, std::nullopt);
		});
		break;
	}
	case Message::LobbyNewPassword:
	{
		lobby_api::LobbyChangeRequest change;
		// An empty password removes the password
		change.password = ToUtf8(text);
		_services.lobbyRoom->Change(std::move(change), LobbyDoneCallback());
		break;
	}
	default:
		break;
	}
}

void MultiplayerFrontEnd::AnswerLobbyMessage(Message message, Answer answer)
{
	if (message != Message::LobbyQuestion)
	{
		// OK on a notice, or Escape on a box that asks for a text
		return;
	}
	const auto question = std::exchange(_lobbyQuestion, LobbyQuestionKind::None);
	if (answer != Answer::Yes)
	{
		return;
	}
	switch (question)
	{
	case LobbyQuestionKind::Leave:
		++_generation;
		_services.lobbyRoom->Leave({});
		ForgetLobbyRoom();
		EnterLobbyList();
		break;
	case LobbyQuestionKind::Close:
		_services.lobbyRoom->Close(LobbyDoneCallback());
		break;
	case LobbyQuestionKind::Create:
		OpenCreateLobby();
		break;
	case LobbyQuestionKind::Kick:
	case LobbyQuestionKind::Ban:
		_services.lobbyRoom->Kick(_questionAccount, question == LobbyQuestionKind::Ban, LobbyDoneCallback());
		break;
	case LobbyQuestionKind::None:
		break;
	}
}

void MultiplayerFrontEnd::ForgetLobbyRoom()
{
	if (_lobbyGame)
	{
		_lobbyGame = false;
		LeaveRoom();
	}
	_reportedMap.reset();
	_editsLobby = false;
	_ticketGame.clear();
	_roomMapHash.clear();
	if (_mapDownload != nullptr)
	{
		_mapDownload->Stop();
	}
	if (_services.lobbyRoom != nullptr && _services.lobbyRoom->Room() != nullptr &&
	    _services.lobbyRoom->Room()->Phase() == RoomPhase::Ended)
	{
		_services.lobbyRoom->DismissRoom();
	}
}

void MultiplayerFrontEnd::FollowLobbySession()
{
	if (!UsesLobbySessions() || _flow.kind != LobbyKind::Internet)
	{
		return;
	}
	const bool inLobbyBoxes = _flow.box == Box::LobbyList || _flow.box == Box::CreateLobby || _flow.box == Box::LobbyRoom;
	if (!inLobbyBoxes)
	{
		return;
	}
	const auto* room = _services.lobbyRoom->Room();
	FollowLobbyNotices(room);
	if (room == nullptr)
	{
		// The room went without an end, for example after a leave
		if (_flow.box == Box::LobbyRoom)
		{
			ForgetLobbyRoom();
			EnterLobbyList();
		}
		return;
	}
	if (room->Phase() == RoomPhase::Ended)
	{
		// The box tells the end once: kicked, banned, the host left, the host closed, the game ended
		if (_message != Message::None || _busy)
		{
			return;
		}
		const auto client = _settings.browser ? lobby_api::ClientKind::Browser : lobby_api::ClientKind::Desktop;
		const auto text = room->End().has_value() ? online::lobby::EndMessage(*room->End(), client) : std::nullopt;
		ForgetLobbyRoom();
		EnterLobbyList();
		if (text.has_value())
		{
			ShowLobbyNotice(ToUtf16(*text));
		}
		return;
	}
	// The box of the settings stays open while the host can change the settings. A start locks them.
	if (_editsLobby && _flow.box == Box::CreateLobby)
	{
		if (screens::SettingsOpen(*room))
		{
			FollowLobbyMap(*room);
			return;
		}
		_editsLobby = false;
	}
	// A create, a join or a quick join opened the room
	if (_flow.box != Box::LobbyRoom)
	{
		_flow.box = Box::LobbyRoom;
		_services.lobbyRoom->WatchList(false);
	}
	FollowLobbyMap(*room);
	FollowLobbyGame(*room);
}

void MultiplayerFrontEnd::FollowLobbyNotices(const online::lobby::LobbyRoom* room)
{
	if (room == nullptr)
	{
		_noticeSnapshot.reset();
		return;
	}
	auto snapshot = online::lobby::SnapshotOf(*room);
	if (_noticeSnapshot.has_value() && _services.notice)
	{
		const auto notice = online::lobby::NoticeBetween(*_noticeSnapshot, snapshot);
		if (notice.Any())
		{
			_services.notice(notice);
		}
	}
	_noticeSnapshot = std::move(snapshot);
}

void MultiplayerFrontEnd::FollowLobbyMap(const online::lobby::LobbyRoom& room)
{
	const auto& map = room.Data().map;
	const auto need =
	    screens::MapNeedOf(map, std::ranges::any_of(_maps, [&map](const LocalMap& land) { return land.file == map.file; }));
	std::optional<bool> have;
	switch (need)
	{
	case screens::MapNeed::Have:
		have = true;
		break;
	case screens::MapNeed::Missing:
		have = false;
		break;
	case screens::MapNeed::Download:
	{
		const auto failed =
		    _mapFailed || (_mapDownload != nullptr && _mapDownload->GetState() == online::MapDownload::State::Failed);
		// A failed download starts again when the host sets the map again. A second failure shows the box again.
		if (map.sha256 != _roomMapHash || (failed && room.MapSets() != _roomMapSets))
		{
			_roomMapSets = room.MapSets();
			_reportedMap.reset();
			FetchRoomMap(map.sha256);
		}
		if (_mapDownload != nullptr && _mapDownload->GetState() == online::MapDownload::State::Ready)
		{
			have = true;
		}
		else if (_mapFailed || (_mapDownload != nullptr && _mapDownload->GetState() == online::MapDownload::State::Failed))
		{
			have = false;
		}
		break;
	}
	}
	if (!have.has_value())
	{
		return;
	}
	const auto report = std::make_pair(map.sha256.empty() ? map.file : map.sha256, *have);
	if (_reportedMap == report)
	{
		return;
	}
	_reportedMap = report;
	// The service clears the map of each member after a change of the map, so each map gets its own report
	_services.lobbyRoom->ReportMap({.haveMap = *have, .sha256 = map.sha256}, {});
	if (!*have && need == screens::MapNeed::Download && _message == Message::None)
	{
		Show(Message::CouldNotDownloadMap);
	}
}

void MultiplayerFrontEnd::FollowLobbyGame(const online::lobby::LobbyRoom& room)
{
	const bool hasServer = room.Phase() == RoomPhase::Assigned || room.Phase() == RoomPhase::InGame;
	if (!hasServer && _lobbyGame && !IsStarted())
	{
		// The lobby took the game back before the start, for example when too few players joined in time. The session
		// with the game server stops, and the room of the session stays open with its system line.
		_lobbyGame = false;
		_busy = false;
		_ticketGame.clear();
		LeaveRoom();
		return;
	}
	if (!hasServer || _services.network == nullptr || room.GameId().empty() || _ticketGame == room.GameId())
	{
		return;
	}
	_ticketGame = room.GameId();
	const auto generation = _generation;
	_services.lobbyRoom->RequestTicket([this, generation](LobbyResult<lobby_api::JoinTicket>&& result) {
		if (generation != _generation)
		{
			return;
		}
		const auto* current = GetLobbyRoom();
		if (current == nullptr)
		{
			return;
		}
		if (!result.Ok())
		{
			ShowLobbyProblem(result.error);
			return;
		}
		// The game server of the session is the room of the network game. The session room stays on its box.
		_room = lobby_api::ServerEntry {};
		_room->id = current->GameId();
		_room->hostname = current->Data().name;
		_room->url = result.value->url;
		_room->map = current->Data().map.sha256;
		_lobbyGame = true;
		_busy = true;
		RoomOpened(std::move(result.value->ticket), {});
	});
}

void openblack::gui::GiveLobbyNotice(const online::lobby::LobbyNotice& notice)
{
	if (notice.sound)
	{
		audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_MenuButton), std::nullopt);
	}
	bool flashed = false;
	if (Locator::windowing::has_value())
	{
		auto& window = Locator::windowing::value();
		if (online::lobby::FlashesWindow(notice, window.HasFocus()))
		{
			window.AskForAttention();
			flashed = true;
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The lobby room gives a notice: sound {}, flash {}, window flashed {}",
	                   notice.sound, notice.flash, flashed);
}
