/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The boxes of the lobby sessions: the lobby list, the box that makes a session and the room of a session. They use
// the controls, the frame and the fonts of the vanilla boxes of the multiplayer front end.

#include <algorithm>
#include <array>
#include <string>

#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <fmt/format.h>

#include "Controls.h"
#include "MultiplayerBoxes.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using namespace openblack::gui::multiplayer;
namespace screens = openblack::gui::lobby_screens;
using openblack::online::lobby::RoomPhase;
using openblack::online::lobby::SlotHolder;

namespace
{

using Look = BigButton::Look;
using LabelSide = BigButton::LabelSide;
using Layout = StaticText::Layout;
using Justify = DialogPainter::Justify;
constexpr auto k_Big = DialogPainter::k_BigTextSize;

[[nodiscard]] int Mid()
{
	return DialogPainter::MidTextSize();
}

[[nodiscard]] int Small()
{
	return DialogPainter::SmallTextSize();
}

constexpr glm::ivec2 k_BackPosition {30, 530};
constexpr glm::ivec2 k_NextPosition {730, 530};
constexpr int k_ArrowSize = 40;
constexpr int k_TickSize = 20;
constexpr DialogRect k_Title {.min = {40, 62}, .max = {760, 100}};

// The vanilla texts that the boxes use again
constexpr std::string_view k_BackText = "HELP_TEXT_DIALOG_BACK";
constexpr std::string_view k_RoomBackText = "HELP_TEXT_DIALOG_TATOOBACK";
constexpr std::string_view k_InternetListTitle = "HELP_TEXT_DIALOG_ADDITION_57";
constexpr std::string_view k_JoinGameText = "HELP_TEXT_DIALOG_ADDITION_76";
constexpr std::string_view k_RefreshText = "HELP_TEXT_PATCH_09";
constexpr std::string_view k_WrongVersionText = "HELP_TEXT_MPFE_WRONGVER";
constexpr std::string_view k_GameNameText = "HELP_TEXT_MPFE_CS_GAME_LABEL";
constexpr std::string_view k_GamePasswordText = "HELP_TEXT_MPFE_CS_PASSWORD_LABEL";
constexpr std::string_view k_FilterText = "HELP_TEXT_PATCH_10";
constexpr std::string_view k_IncomingText = "HELP_TEXT_DIALOG_ADDITION_72";
constexpr std::string_view k_TypeChatText = "HELP_TEXT_DIALOG_ADDITION_140";
constexpr std::string_view k_HostText = "HELP_TEXT_DIALOG_ADDITION_03";
constexpr std::string_view k_CurrentText = "HELP_TEXT_DIALOG_ADDITION_02";
constexpr std::string_view k_GodText = "HELP_TEXT_DIALOG_ADDITION_63";
constexpr std::string_view k_TimeLimitText = "HELP_TEXT_MULTIPLAYER_TIMELIMIT";
constexpr std::string_view k_ReadyText = "HELP_TEXT_MULTIPLAYER_READY";
constexpr std::string_view k_YesText = "HELP_TEXT_REQUESTER_BOXES_01";
constexpr std::string_view k_NoText = "HELP_TEXT_REQUESTER_BOXES_02";
constexpr std::string_view k_OkText = "HELP_TEXT_REQUESTER_BOXES_04";

// The message box, as the vanilla boxes have it
constexpr DialogRect k_MessageText {.min = {200, 220}, .max = {600, 350}};
constexpr DialogRect k_PromptText {.min = {200, 215}, .max = {600, 290}};
constexpr DialogRect k_PromptEdit {.min = {220, 295}, .max = {580, 327}};
constexpr DialogRect k_MessageOk {.min = {325, 338}, .max = {475, 370}};
constexpr glm::ivec2 k_YesPosition {200, 338};
constexpr glm::ivec2 k_NoPosition {568, 338};
constexpr int k_AnswerArrowSize = 32;

// The lobby list: the columns of the list, from their left
constexpr DialogRect k_ListRect {.min = {40, 150}, .max = {760, 405}};
constexpr std::array<int, screens::k_ColumnCount + 1> k_ColumnLeft {44, 300, 420, 560, 625, 736};
constexpr std::array<std::u16string_view, screens::k_ColumnCount> k_Headings {u"Name", u"Host", u"Map", u"Gods", u"Status"};

// The room of a session
constexpr DialogRect k_SlotsRect {.min = {40, 118}, .max = {470, 345}};
constexpr DialogRect k_MapsRect {.min = {490, 172}, .max = {760, 300}};
/// The width of the warning symbol and of the tick of each slot
constexpr int k_MarkSize = 18;
/// The colour of the warning symbol: the colour of the text under the pointer
constexpr glm::vec4 k_WarningColour = DialogPainter::k_HoverColour;
constexpr DialogRect k_TipRect {.min = {0, 0}, .max = {360, 70}};

[[nodiscard]] bool IsReturn(int key)
{
	return key == SDLK_RETURN || key == SDLK_KP_ENTER;
}

[[nodiscard]] bool ControlDown()
{
	return (SDL_GetModState() & KMOD_CTRL) != 0;
}

/// The list's items change only when the lines changed, which keeps the scroll and the selection
void SetItemsIfChanged(List& list, std::vector<List::Item> items)
{
	const auto& old = list.GetItems();
	const bool same = std::ranges::equal(old, items, [](const List::Item& a, const List::Item& b) {
		return a.text == b.text && a.colour == b.colour && a.bar == b.bar;
	});
	if (!same)
	{
		const auto selection = list.GetSelection();
		list.SetItems(std::move(items));
		list.SetSelection(selection.has_value() && *selection < list.GetItems().size() ? selection : std::nullopt);
	}
}

[[nodiscard]] std::u16string MapLine(const MultiplayerFrontEnd::SessionMap& map)
{
	auto line = map.name + ToUtf16(fmt::format(" ({})", map.places));
	if (!map.map.sha256.empty())
	{
		line += u" - download";
	}
	return line;
}

} // namespace

bool MultiplayerBoxes::IsLobbyBox() const
{
	const auto box = _frontEnd->GetBox();
	return box == Box::LobbyList || box == Box::CreateLobby || box == Box::LobbyRoom;
}

void MultiplayerBoxes::BuildLobbyList(Dialog& dialog)
{
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_BackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	_next = &dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, Text(k_JoinGameText), LabelSide::Left, Look::RightArrow);
	_next->onClick = [this] { _frontEnd->JoinSelectedLobby(); };
	dialog.Add<StaticText>(k_Title, Text(k_InternetListTitle), Layout::Centre, k_Big);

	// The search box finds a part of the name of the game, of the host or of the map
	dialog.Add<StaticText>(DialogRect {.min = {40, 104}, .max = {230, 132}}, Text(k_FilterText), Layout::Right, Mid());
	_search = &dialog.Add<EditBox>(_font, DialogRect {.min = {240, 104}, .max = {520, 132}},
	                               ToUtf16(_frontEnd->GetListFilter().search), lobby_api::k_MostLobbyNameCharacters, Mid());
	_search->onChange = [this](const std::u16string& text) {
		auto filter = _frontEnd->GetListFilter();
		filter.search = ToUtf8(text);
		_frontEnd->SetListFilter(std::move(filter));
	};
	auto& refresh = dialog.Add<Button>(DialogRect {.min = {560, 102}, .max = {760, 134}}, Text(k_RefreshText), Mid());
	refresh.onClick = [this] { _frontEnd->RefreshLobbies(); };

	// A click on a heading sorts the list by its column
	for (size_t i = 0; i < screens::k_ColumnCount; ++i)
	{
		auto& heading =
		    dialog.Add<Button>(DialogRect {.min = {k_ColumnLeft.at(i) - 4, 136}, .max = {k_ColumnLeft.at(i + 1) - 4, 150}},
		                       std::u16string(), Small());
		heading.onClick = [this, i] { _frontEnd->ClickListHeading(static_cast<screens::Column>(i)); };
		_headings.at(i) = &heading;
	}

	_list = &dialog.Add<List>(_font, k_ListRect, Mid(), false, true, true);
	_list->ShowScrollBarWhenNeeded();
	_list->onClick = [this] {
		const auto line = _list->GetSelection();
		_frontEnd->SelectLobby(line.has_value() && *line < _shownLobbies.size() ? _shownLobbies[*line].id : std::string());
	};
	// The columns of each line
	_list->drawOver = [this](const DialogPainter& painter, const List::ItemPlace& place) {
		if (place.index >= _shownLobbies.size() || place.shownTop != place.top || place.shownBottom != place.bottom)
		{
			return;
		}
		const auto& lobby = _shownLobbies[place.index];
		const bool picked = lobby.id == _frontEnd->GetSelectedLobby();
		const auto colour = picked ? DialogPainter::k_FocusColour : DialogPainter::k_TextColour;
		const auto status = screens::StatusOf(lobby, _frontEnd->GetLobbyBuild());
		const std::array<std::u16string, screens::k_ColumnCount> cells {
		    screens::NameText(lobby),
		    ToUtf16(lobby.host.name),
		    ToUtf16(lobby.map.name.empty() ? lobby.map.file : lobby.map.name),
		    screens::GodsText(lobby),
		    screens::StatusText(lobby, status, _frontEnd->NowMs(), Text(k_WrongVersionText)),
		};
		const auto size = Mid();
		for (size_t i = 0; i < cells.size(); ++i)
		{
			const auto width = static_cast<float>(std::min(k_ColumnLeft.at(i + 1), place.right) - k_ColumnLeft.at(i) - 6);
			const auto text = screens::FitText(
			    cells.at(i), width, [&painter, size](std::u16string_view t) { return painter.GetTextWidth(t, size); });
			painter.DrawText({k_ColumnLeft.at(i), place.top + 2}, static_cast<int>(width), Justify::Left, text, size, colour);
		}
	};

	// The filters
	const auto& filter = _frontEnd->GetListFilter();
	const auto addFilter = [this, &dialog](int x, std::u16string label, bool checked, bool online::lobby::ListFilter::* field) {
		auto& box =
		    dialog.Add<CheckBox>(_font, glm::ivec2 {x, 412}, k_TickSize, std::move(label), LabelSide::Right, checked, false);
		box.onChange = [this, field](bool on) {
			auto changed = _frontEnd->GetListFilter();
			changed.*field = on;
			_frontEnd->SetListFilter(std::move(changed));
		};
		return &box;
	};
	_showFull = addFilter(44, u"Full", filter.showFull, &online::lobby::ListFilter::showFull);
	_showLocked = addFilter(170, u"Password", filter.showLocked, &online::lobby::ListFilter::showLocked);
	_showInGame = addFilter(330, u"In game", filter.showInGame, &online::lobby::ListFilter::showInGame);
	_showOtherVersions =
	    addFilter(480, u"Other versions", filter.showOtherVersions, &online::lobby::ListFilter::showOtherVersions);

	_detail =
	    &dialog.Add<StaticText>(DialogRect {.min = {40, 440}, .max = {760, 464}}, std::u16string(), Layout::Left, Small());
	_servers =
	    &dialog.Add<StaticText>(DialogRect {.min = {40, 464}, .max = {760, 488}}, std::u16string(), Layout::Left, Small());

	// The buttons of the list. "Create session" is the user's name for the button (2026-11-12).
	auto& create = dialog.Add<Button>(DialogRect {.min = {110, 492}, .max = {290, 524}}, u"Create session", Mid());
	create.onClick = [this] { _frontEnd->OpenCreateLobby(); };
	auto& quick = dialog.Add<Button>(DialogRect {.min = {300, 492}, .max = {460, 524}}, u"Quick join", Mid());
	quick.onClick = [this] { _frontEnd->QuickJoinLobby(); };
	auto& code = dialog.Add<Button>(DialogRect {.min = {470, 492}, .max = {650, 524}}, u"Join by code", Mid());
	code.onClick = [this] { _frontEnd->AskLobbyCode(); };
}

void MultiplayerBoxes::SyncLobbyList()
{
	_shownLobbies = _frontEnd->GetShownLobbies();
	std::vector<List::Item> items;
	std::optional<size_t> selection;
	for (size_t i = 0; i < _shownLobbies.size(); ++i)
	{
		// The columns draw over each line. The text of the line is the name, for the tools that read the list.
		items.push_back({.text = u" ", .mouseCell = std::nullopt, .mouseMirrored = false, .colour = glm::vec4(0.0f)});
		if (_shownLobbies[i].id == _frontEnd->GetSelectedLobby())
		{
			selection = i;
		}
	}
	SetItemsIfChanged(*_list, std::move(items));
	if (!_dialog->IsHolding())
	{
		_list->SetSelection(selection);
	}
	const auto sort = _frontEnd->GetListSort();
	for (size_t i = 0; i < screens::k_ColumnCount; ++i)
	{
		_headings.at(i)->label = screens::HeadingText(k_Headings.at(i), static_cast<screens::Column>(i), sort);
	}
	const auto* picked = _frontEnd->GetSelectedSummary();
	_next->visible = picked != nullptr;
	_detail->text = picked != nullptr
	                    ? screens::DetailText(*picked, ToUtf16(picked->map.name.empty() ? picked->map.file : picked->map.name))
	                    : (_shownLobbies.empty() ? std::u16string(u"No open game. Click Create session.")
	                                             : std::u16string(u"Click a game, then Join Game."));
	_servers->text = screens::ServersText(_frontEnd->GetServerPool());
}

void MultiplayerBoxes::BuildCreateLobby(Dialog& dialog)
{
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_BackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	_next = &dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, u"Create session", LabelSide::Left, Look::RightArrow);
	_next->onClick = [this] { _frontEnd->SubmitCreateLobby(); };
	dialog.Add<StaticText>(k_Title, u"Create session", Layout::Centre, k_Big);

	const auto& form = _frontEnd->GetCreateForm();
	dialog.Add<StaticText>(DialogRect {.min = {40, 112}, .max = {290, 142}}, Text(k_GameNameText), Layout::Right, Mid());
	_sessionName = &dialog.Add<EditBox>(_font, DialogRect {.min = {300, 112}, .max = {640, 142}}, form.name,
	                                    lobby_api::k_MostLobbyNameCharacters, Mid());
	_sessionName->onChange = [this](const std::u16string& text) {
		auto changed = _frontEnd->GetCreateForm();
		changed.name = text;
		_frontEnd->SetCreateForm(std::move(changed));
	};

	dialog.Add<StaticText>(DialogRect {.min = {40, 152}, .max = {290, 180}}, u"Map:", Layout::Right, Mid());
	_mapList = &dialog.Add<List>(_font, DialogRect {.min = {300, 152}, .max = {640, 300}}, Mid(), false, true, true);
	_mapList->ShowScrollBarWhenNeeded();
	_mapList->onClick = [this] {
		const auto line = _mapList->GetSelection();
		if (!line.has_value())
		{
			return;
		}
		const auto maps = _frontEnd->GetSessionMaps();
		auto changed = _frontEnd->GetCreateForm();
		changed.map = line;
		// A new map gives a slot to each of its places
		changed.godSlots = screens::MostGodSlots(maps.at(*line).places);
		_frontEnd->SetCreateForm(std::move(changed));
	};

	// The gods and the time limit change by steps with the arrows
	const auto addStepper = [this, &dialog](int y, std::u16string label, StaticText*& value, int step, bool gods) {
		dialog.Add<StaticText>(DialogRect {.min = {40, y}, .max = {290, y + 30}}, std::move(label), Layout::Right, Mid());
		auto& less = dialog.Add<Button>(DialogRect {.min = {300, y}, .max = {340, y + 30}}, u"<", Mid());
		value = &dialog.Add<StaticText>(DialogRect {.min = {340, y}, .max = {440, y + 30}}, std::u16string(), Layout::Centre,
		                                Mid());
		auto& more = dialog.Add<Button>(DialogRect {.min = {440, y}, .max = {480, y + 30}}, u">", Mid());
		const auto change = [this, gods](int by) {
			auto changed = _frontEnd->GetCreateForm();
			const auto maps = _frontEnd->GetSessionMaps();
			const auto places =
			    changed.map.has_value() && *changed.map < maps.size() ? maps[*changed.map].places : lobby_api::k_MostGodSlots;
			if (gods)
			{
				changed.godSlots = screens::ClampGodSlots(changed.godSlots + by, places);
			}
			else
			{
				changed.timeLimitMinutes = screens::ClampTimeLimit(changed.timeLimitMinutes + by);
			}
			_frontEnd->SetCreateForm(std::move(changed));
		};
		less.onClick = [change, step] { change(-step); };
		more.onClick = [change, step] { change(step); };
	};
	addStepper(310, u"Gods:", _countValue, 1, true);
	addStepper(348, Text(k_TimeLimitText), _timeValue, screens::k_TimeLimitStep, false);

	_privateBox = &dialog.Add<CheckBox>(_font, glm::ivec2 {300, 390}, k_TickSize, u"Private: join only by its code",
	                                    LabelSide::Right, form.isPrivate, false);
	_privateBox->onChange = [this](bool on) {
		auto changed = _frontEnd->GetCreateForm();
		changed.isPrivate = on;
		_frontEnd->SetCreateForm(std::move(changed));
	};

	dialog.Add<StaticText>(DialogRect {.min = {40, 425}, .max = {290, 455}}, Text(k_GamePasswordText), Layout::Right, Mid());
	_password = &dialog.Add<EditBox>(_font, DialogRect {.min = {300, 425}, .max = {640, 455}}, form.password,
	                                 lobby_api::k_MostLobbyPasswordCharacters, Mid());
	_password->masked = true;
	_password->onChange = [this](const std::u16string& text) {
		auto changed = _frontEnd->GetCreateForm();
		changed.password = text;
		_frontEnd->SetCreateForm(std::move(changed));
	};
	_help =
	    &dialog.Add<StaticText>(DialogRect {.min = {40, 462}, .max = {760, 488}}, std::u16string(), Layout::Centre, Small());
	dialog.SetFocus(_sessionName);
}

void MultiplayerBoxes::SyncCreateLobby()
{
	const auto& form = _frontEnd->GetCreateForm();
	const auto maps = _frontEnd->GetSessionMaps();
	std::vector<List::Item> items;
	for (const auto& map : maps)
	{
		items.push_back({.text = MapLine(map)});
	}
	SetItemsIfChanged(*_mapList, std::move(items));
	if (!_dialog->IsHolding())
	{
		_mapList->SetSelection(form.map);
	}
	_countValue->text = ToUtf16(fmt::format("{}", form.godSlots));
	_timeValue->text = TimeLimitText(static_cast<uint32_t>(form.timeLimitMinutes));
	_privateBox->SetChecked(form.isPrivate);
	const auto problem = screens::CreateProblem(form);
	_help->text = problem.has_value() ? ToUtf16(*problem) : std::u16string();
}

void MultiplayerBoxes::BuildLobbyRoom(Dialog& dialog)
{
	_caption = &dialog.Add<StaticText>(DialogRect {.min = {40, 62}, .max = {760, 90}}, std::u16string(), Layout::Centre, Mid());
	_status =
	    &dialog.Add<StaticText>(DialogRect {.min = {40, 90}, .max = {760, 114}}, std::u16string(), Layout::Centre, Small());

	// The slots: a click picks a slot. The host gets the actions of the slot, a player can take an open slot.
	_slots = &dialog.Add<List>(_font, k_SlotsRect, Mid(), false, true, true);
	_slots->onClick = [this] { _shownRevision.reset(); };
	_slots->drawOver = [this](const DialogPainter& painter, const List::ItemPlace& place) {
		const auto* room = _frontEnd->GetLobbyRoom();
		if (room == nullptr || place.shownTop != place.top || place.shownBottom != place.bottom)
		{
			return;
		}
		const auto slots = room->Slots();
		if (place.index >= slots.size())
		{
			return;
		}
		const auto& slot = slots[place.index];
		auto right = place.right - 6;
		const auto middle = (place.top + place.bottom) / 2;
		if (slot.holder == SlotHolder::Player)
		{
			// The ready tick of each player, which all the players see (user, 2026-11-12)
			painter.DrawSquare({right - k_MarkSize, middle - (k_MarkSize / 2)}, k_MarkSize, slot.ready, false, false);
			right -= k_MarkSize + 6;
		}
		if (screens::ShowsMapWarning(*room, slot))
		{
			// The warning symbol of a player without the custom map. The pointer on it shows why (user, 2026-11-12).
			const DialogRect mark {.min = {right - k_MarkSize, middle - (k_MarkSize / 2)},
			                       .max = {right, middle + (k_MarkSize / 2)}};
			painter.DrawBevelBox(mark, 1, DialogPainter::All, k_WarningColour);
			painter.DrawText({mark.min.x, mark.min.y - 2}, k_MarkSize, Justify::Centre, u"!", Mid(), k_WarningColour);
			_warningRects.push_back(mark);
			right -= k_MarkSize + 6;
		}
		const auto note = screens::SlotNote(slot);
		if (!note.empty())
		{
			const auto width = static_cast<int>(painter.GetTextWidth(note, Small()));
			painter.DrawText({right - width, middle - (Small() / 2)}, width, Justify::Left, note, Small(),
			                 DialogPainter::k_HoverColour);
		}
	};

	// The map, the time limit and the controls of the host
	_mapLine =
	    &dialog.Add<StaticText>(DialogRect {.min = {490, 118}, .max = {760, 144}}, std::u16string(), Layout::Left, Mid());
	_timeLimit =
	    &dialog.Add<StaticText>(DialogRect {.min = {490, 144}, .max = {760, 168}}, std::u16string(), Layout::Left, Small());
	_mapList = &dialog.Add<List>(_font, k_MapsRect, Small(), false, true, true);
	_mapList->ShowScrollBarWhenNeeded();
	_mapList->onClick = [this] {
		if (const auto line = _mapList->GetSelection(); line.has_value())
		{
			_frontEnd->ChangeLobbyMap(*line);
		}
	};
	_passwordButton = &dialog.Add<Button>(DialogRect {.min = {490, 306}, .max = {620, 336}}, u"Password", Mid());
	_passwordButton->onClick = [this] { _frontEnd->AskLobbyPassword(); };
	_closeButton = &dialog.Add<Button>(DialogRect {.min = {630, 306}, .max = {760, 336}}, u"Close", Mid());
	_closeButton->onClick = [this] { _frontEnd->CloseLobby(); };

	// The actions of the picked slot
	for (int i = 0; i < 3; ++i)
	{
		auto& button = dialog.Add<Button>(DialogRect {.min = {40 + (i * 145), 350}, .max = {175 + (i * 145), 378}},
		                                  std::u16string(), Small());
		button.onClick = [this, i] {
			const auto* room = _frontEnd->GetLobbyRoom();
			const auto line = _slots->GetSelection();
			if (room == nullptr || !line.has_value())
			{
				return;
			}
			const auto actions = screens::HostActionsOf(*room, static_cast<int32_t>(*line));
			if (static_cast<size_t>(i) < actions.size())
			{
				_frontEnd->DoHostAction(static_cast<int32_t>(*line), actions[static_cast<size_t>(i)]);
			}
		};
		_hostButtons.push_back(&button);
	}
	_takeSlot = &dialog.Add<Button>(DialogRect {.min = {40, 350}, .max = {200, 378}}, u"Take this slot", Small());
	_takeSlot->onClick = [this] {
		if (const auto line = _slots->GetSelection(); line.has_value())
		{
			_frontEnd->PickLobbySlot(static_cast<int32_t>(*line));
		}
	};

	// The chat of the session, with the lines of the system
	dialog.Add<StaticText>(DialogRect {.min = {40, 380}, .max = {760, 402}}, Text(k_IncomingText), Layout::Centre, Small());
	_chat = &dialog.Add<List>(_font, DialogRect {.min = {40, 404}, .max = {760, 482}}, Small(), false, true, false);
	_chatEdit = &dialog.Add<EditBox>(_font, DialogRect {.min = {40, 486}, .max = {760, 514}}, Text(k_TypeChatText),
	                                 k_MostChatCharacters, Mid());
	_chatEdit->onClick = [this] { SendChat(); };

	// Leave at the left, Ready in the middle, the Start of the host at the right
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_RoomBackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	_ready =
	    &dialog.Add<CheckBox>(_font, glm::ivec2 {360, 532}, k_TickSize + 6, Text(k_ReadyText), LabelSide::Right, false, false);
	_ready->onChange = [this](bool) { _frontEnd->ToggleLobbyReady(); };
	_next = &dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, u"Start", LabelSide::Left, Look::RightArrow);
	_next->onClick = [this] { _frontEnd->StartOrCancelLobby(); };
	dialog.SetFocus(_chatEdit);
}

void MultiplayerBoxes::SyncLobbyRoom()
{
	const auto* room = _frontEnd->GetLobbyRoom();
	if (room == nullptr)
	{
		return;
	}
	const auto& lobby = room->Data();
	_caption->text = ToUtf16(lobby.name + " - " + lobby.host.name);
	auto status = screens::RoomStatusText(*room);
	if (const auto seconds = _frontEnd->GetCountdownSeconds(); seconds.has_value())
	{
		status += ToUtf16(fmt::format("   Start in {}", *seconds));
	}
	_status->text = std::move(status);

	const screens::SlotTexts texts {.god = Text(k_GodText), .host = Text(k_HostText), .current = Text(k_CurrentText)};
	const auto slots = room->Slots();
	std::vector<List::Item> items;
	for (const auto& slot : slots)
	{
		std::optional<glm::vec4> colour;
		if (slot.holder != SlotHolder::Player)
		{
			colour = DialogPainter::k_TextColour * glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
		}
		items.push_back(
		    {.text = screens::SlotLine(slot, texts), .mouseCell = std::nullopt, .mouseMirrored = false, .colour = colour});
	}
	SetItemsIfChanged(*_slots, std::move(items));

	// The map of the room and, for the host before the start, the maps to choose
	_mapLine->text = ToUtf16("Map: " + (lobby.map.name.empty() ? lobby.map.file : lobby.map.name));
	_timeLimit->text = Text(k_TimeLimitText) + u" " + TimeLimitText(static_cast<uint32_t>(lobby.settings.timeLimitMinutes));
	const bool hostBeforeStart = room->IsHost() && room->Phase() == RoomPhase::Open;
	_mapList->visible = hostBeforeStart;
	if (hostBeforeStart)
	{
		const auto maps = _frontEnd->GetSessionMaps();
		const auto players = screens::PlayersOfRoom(*room);
		std::vector<List::Item> mapItems;
		std::optional<size_t> current;
		for (size_t i = 0; i < maps.size(); ++i)
		{
			// A map with too few places for the players is grey (user, 2026-11-12)
			const bool fits = screens::MapFitsRoom(maps[i].places, players);
			mapItems.push_back(
			    {.text = MapLine(maps[i]),
			     .mouseCell = std::nullopt,
			     .mouseMirrored = false,
			     .colour =
			         fits ? std::nullopt : std::optional(DialogPainter::k_TextColour * glm::vec4(0.5f, 0.5f, 0.5f, 1.0f))});
			if (maps[i].map.file == lobby.map.file)
			{
				current = i;
			}
		}
		SetItemsIfChanged(*_mapList, std::move(mapItems));
		if (!_dialog->IsHolding())
		{
			_mapList->SetSelection(current);
		}
	}
	_passwordButton->visible = room->IsHost() && room->Phase() != RoomPhase::Ended;
	// No Close after the start (user, 2026-11-12)
	_closeButton->visible = hostBeforeStart;

	// The actions of the picked slot
	const auto picked = _slots->GetSelection();
	const auto actions =
	    picked.has_value() ? screens::HostActionsOf(*room, static_cast<int32_t>(*picked)) : std::vector<screens::HostAction> {};
	for (size_t i = 0; i < _hostButtons.size(); ++i)
	{
		_hostButtons[i]->visible = i < actions.size();
		_hostButtons[i]->label = i < actions.size() ? std::u16string(screens::HostActionText(actions[i])) : std::u16string();
	}
	_takeSlot->visible = actions.empty() && picked.has_value() && room->CanPickSlot(static_cast<int32_t>(*picked));

	// The chat lines and the lines of the system
	if (room->Lines().size() != _chatLines)
	{
		std::vector<List::Item> lines;
		for (const auto& line : room->Lines())
		{
			lines.push_back({.text = ToUtf16(online::lobby::RoomLineText(line))});
		}
		_chatLines = lines.size();
		_chat->SetItems(std::move(lines));
		_chat->SetSelection(std::nullopt);
		_chat->ScrollToSelection();
	}

	const auto* own = room->OwnMember();
	_ready->SetChecked(own != nullptr && own->ready);
	_ready->visible = own != nullptr && own->role == lobby_api::MemberRole::God && room->Phase() == RoomPhase::Open;
	switch (screens::StartButtonOf(*room))
	{
	case screens::StartButton::Start:
		_next->visible = true;
		// The Start of the host has the warning symbol while a member does not have the custom map
		_next->label = screens::StartShowsMapWarning(*room) ? std::u16string(u"! Start") : std::u16string(u"Start");
		break;
	case screens::StartButton::Cancel:
		_next->visible = true;
		_next->label = u"Cancel";
		break;
	case screens::StartButton::None:
		_next->visible = false;
		break;
	}
}

void MultiplayerBoxes::DrawWarningTip(const DialogPainter& painter) const
{
	if (_frontEnd->GetBox() != Box::LobbyRoom)
	{
		return;
	}
	const auto* room = _frontEnd->GetLobbyRoom();
	// The Start of the host has the same warning and the same text
	const bool onStart = room != nullptr && _next != nullptr && _next->visible &&
	                     screens::StartButtonOf(*room) == screens::StartButton::Start && screens::StartShowsMapWarning(*room) &&
	                     _next->HitTest(_pointer);
	const auto under = std::ranges::find_if(_warningRects, [this](const DialogRect& rect) { return rect.Contains(_pointer); });
	if (under == _warningRects.end() && !onStart)
	{
		return;
	}
	// The text box stays inside the dialog space, above the pointer
	const auto width = k_TipRect.max.x - k_TipRect.min.x;
	const auto height = k_TipRect.max.y - k_TipRect.min.y;
	const auto left = std::clamp(_pointer.x - (width / 2), 10, 790 - width);
	const auto top = std::clamp(_pointer.y - height - 12, 10, 590 - height);
	const DialogRect tip {.min = {left, top}, .max = {left + width, top + height}};
	painter.SetAlpha(1.0f);
	painter.DrawBackground(tip, glm::vec3(1.0f), true, DialogPainter::All);
	painter.SetAlpha(1.0f);
	painter.DrawTextWrapped({.min = {tip.min.x + 10, tip.min.y + 6}, .max = {tip.max.x - 10, tip.max.y - 6}}, true,
	                        screens::k_MapWarningText, Small(), DialogPainter::k_ShadowColour);
}

void MultiplayerBoxes::BuildLobbyMessage(MessageShown& shown)
{
	const auto message = shown.message;
	shown.text = _frontEnd->GetLobbyMessageText();
	const bool asksText =
	    message == Message::LobbyPassword || message == Message::LobbyCode || message == Message::LobbyNewPassword;
	shown.textControl = &shown.answers.Add<StaticText>(asksText ? k_PromptText : k_MessageText, shown.text, Layout::Wrapped);
	shown.textControl->takesClicks = true;
	if (asksText)
	{
		shown.edit = &shown.answers.Add<EditBox>(_font, k_PromptEdit, std::u16string(),
		                                         message == Message::LobbyCode ? lobby_api::k_MostLobbyNameCharacters
		                                                                       : lobby_api::k_MostLobbyPasswordCharacters,
		                                         Mid());
		shown.edit->masked = message != Message::LobbyCode;
		shown.answers.SetFocus(shown.edit);
		shown.answers.Add<Button>(k_MessageOk, Text(k_OkText), Mid()).onClick = [this] {
			if (_message.has_value() && _message->edit != nullptr)
			{
				_frontEnd->AnswerLobbyText(_message->edit->GetText());
			}
		};
		return;
	}
	if (message == Message::LobbyQuestion)
	{
		shown.answers
		    .Add<BigButton>(_font, k_YesPosition, k_AnswerArrowSize, Text(k_YesText), LabelSide::Right, Look::LeftArrow)
		    .onClick = [this] { _frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Yes); };
		shown.answers.Add<BigButton>(_font, k_NoPosition, k_AnswerArrowSize, Text(k_NoText), LabelSide::Left, Look::RightArrow)
		    .onClick = [this] { _frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::No); };
		return;
	}
	shown.answers.Add<Button>(k_MessageOk, Text(k_OkText), Mid()).onClick = [this] {
		_frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Ok);
	};
}

bool MultiplayerBoxes::LobbyKeyDown(int key)
{
	const auto box = _frontEnd->GetBox();
	const auto screen = box == Box::LobbyList     ? screens::KeyScreen::List
	                    : box == Box::CreateLobby ? screens::KeyScreen::Create
	                                              : screens::KeyScreen::Room;
	const auto* focus = _dialog->GetFocus();
	const bool editing =
	    focus != nullptr && (focus == _search || focus == _chatEdit || focus == _sessionName || focus == _password);
	switch (screens::KeyActionOf(screen, {.key = key, .control = ControlDown(), .editing = editing}))
	{
	case screens::KeyAction::Refresh:
		_frontEnd->RefreshLobbies();
		return true;
	case screens::KeyAction::Search:
		_dialog->SetFocus(_search);
		return true;
	case screens::KeyAction::JoinByCode:
		_frontEnd->AskLobbyCode();
		return true;
	case screens::KeyAction::ToggleReady:
		_frontEnd->ToggleLobbyReady();
		return true;
	case screens::KeyAction::Start:
		_frontEnd->StartOrCancelLobby();
		return true;
	case screens::KeyAction::Back:
		_frontEnd->Back();
		return true;
	case screens::KeyAction::Join:
		_frontEnd->JoinSelectedLobby();
		return true;
	case screens::KeyAction::None:
		break;
	}
	// Return in the box that makes a session makes it, unless an edit box gives the focus to the next control
	if (box == Box::CreateLobby && IsReturn(key) && !editing)
	{
		_frontEnd->SubmitCreateLobby();
		return true;
	}
	return false;
}
