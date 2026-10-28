/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MultiplayerBoxes.h"

#include <algorithm>
#include <array>
#include <utility>

#include <SDL_keycode.h>
#include <bgfx/bgfx.h>

#include "Controls.h"
#include "DialogKeyRules.h"
#include "Graphics/Texture2D.h"
#include "PingDisplay.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using namespace openblack::gui::multiplayer;

namespace
{

using Look = BigButton::Look;
using LabelSide = BigButton::LabelSide;
using Layout = StaticText::Layout;
constexpr auto k_Big = DialogPainter::k_BigTextSize;
constexpr auto k_Mid = DialogPainter::k_MidTextSize;

constexpr float k_FadeInSeconds = 0.5f;
/// Behind a message the box dims by three quarters
constexpr float k_HoldDims = 0.75f;
/// A message's text starts at the big size and shrinks while it is 125 or more high, down to the small size
constexpr int k_MessageWidth = 400;
constexpr float k_MessageTallest = 125.0f;

// Back and Next of the boxes: 40 pixel arrows at the bottom corners
constexpr glm::ivec2 k_BackPosition {30, 530};
constexpr glm::ivec2 k_NextPosition {730, 530};
constexpr int k_ArrowSize = 40;
// The game room's Back and Next stand 10 further out
constexpr glm::ivec2 k_RoomBackPosition {20, 530};
constexpr glm::ivec2 k_RoomNextPosition {740, 530};
/// The title along the top of each box
constexpr DialogRect k_Title {.min = {40, 70}, .max = {760, 110}};

// The message boxes' buttons
constexpr glm::ivec2 k_YesPosition {200, 338};
constexpr glm::ivec2 k_NoPosition {568, 338};
constexpr int k_AnswerArrowSize = 32;
constexpr DialogRect k_MessageOk {.min = {325, 338}, .max = {475, 370}};

/// The texts of the boxes
constexpr std::string_view k_BackText = "HELP_TEXT_DIALOG_BACK";
constexpr std::string_view k_NextText = "HELP_TEXT_DIALOG_ADDITION_96";
constexpr std::string_view k_RoomBackText = "HELP_TEXT_DIALOG_TATOOBACK";
constexpr std::string_view k_ChooseTitle = "HELP_TEXT_MPFE_TITLE_CHOOSE_CONNECTION";
constexpr std::string_view k_LanText = "HELP_TEXT_DIALOG_LANSTRING";
constexpr std::string_view k_DefaultText = "HELP_TEXT_DIALOG_DEFAULT";
constexpr std::string_view k_FindTitle = "HELP_TEXT_DIALOG_ADDITION_56";
constexpr std::string_view k_OnInternetText = "HELP_TEXT_DIALOG_ADDITION_104";
constexpr std::string_view k_OnLanText = "HELP_TEXT_DIALOG_ADDITION_105";
constexpr std::string_view k_LoginTitle = "HELP_TEXT_DIALOG_ADDITION_86";
constexpr std::string_view k_UserNameText = "HELP_TEXT_MPFE_USERNAME_LABEL";
constexpr std::string_view k_PasswordText = "HELP_TEXT_MPFE_PASSWORD_LABEL";
constexpr std::string_view k_InternetListTitle = "HELP_TEXT_DIALOG_ADDITION_57";
constexpr std::string_view k_LanListTitle = "HELP_TEXT_DIALOG_ADDITION_55";
/// The help of the game list. The internet's own text also tells the player to click Create Game, which does not show
/// (each game server is a game), so both lists show the text without it.
constexpr std::string_view k_GameListHelp = "HELP_TEXT_DIALOG_ADDITION_12";
constexpr std::string_view k_CurrentGamesText = "HELP_TEXT_DIALOG_ADDITION_47";
constexpr std::string_view k_PlayersInGameText = "HELP_TEXT_DIALOG_ADDITION_112";
constexpr std::string_view k_GameNameText = "HELP_TEXT_MPFE_CS_GAME_LABEL";
constexpr std::string_view k_GamePasswordText = "HELP_TEXT_MPFE_CS_PASSWORD_LABEL";
constexpr std::string_view k_FilterText = "HELP_TEXT_PATCH_10";
constexpr std::string_view k_JoinGameText = "HELP_TEXT_DIALOG_ADDITION_76";
constexpr std::string_view k_RefreshText = "HELP_TEXT_PATCH_09";
constexpr std::string_view k_PlayingText = "HELP_TEXT_MPFE_PLAYING";
constexpr std::string_view k_WrongVersionText = "HELP_TEXT_MPFE_WRONGVER";
constexpr std::string_view k_NeedsPasswordText = "HELP_TEXT_MPFE_NEEDS_PASSWORD";
constexpr std::string_view k_LockedText = "HELP_TEXT_PATCH_01";
constexpr std::array<std::string_view, k_RoomTabCount> k_TabTexts = {
    "HELP_TEXT_PAUSE_STATS_126", "HELP_TEXT_PAUSE_STATS_127", "HELP_TEXT_PAUSE_STATS_128", "HELP_TEXT_DIALOG_ADDITION_110"};
constexpr std::string_view k_CaptionText = "HELP_TEXT_DIALOG_ADDITION_10";
constexpr std::string_view k_NoLandText = "HELP_TEXT_DIALOG_ADDITION_14";
constexpr std::string_view k_IncomingText = "HELP_TEXT_DIALOG_ADDITION_72";
constexpr std::string_view k_TypeChatText = "HELP_TEXT_DIALOG_ADDITION_140";
constexpr std::string_view k_LandscapesText = "HELP_TEXT_PAUSE_STATS_135";
constexpr std::string_view k_ThumbnailText = "HELP_TEXT_PAUSE_STATS_136";
constexpr std::string_view k_LandInfoTitle = "HELP_TEXT_DIALOG_ADDITION_80";
constexpr std::string_view k_HostText = "HELP_TEXT_DIALOG_ADDITION_03";
constexpr std::string_view k_CurrentText = "HELP_TEXT_DIALOG_ADDITION_02";
constexpr std::string_view k_ClansOfText = "HELP_TEXT_DIALOG_ADDITION_07";
constexpr std::string_view k_ClanInfoText = "HELP_TEXT_DIALOG_ADDITION_30";
constexpr std::string_view k_ClanNamedInfoText = "HELP_TEXT_DIALOG_ADDITION_28";
constexpr std::string_view k_ClanDescriptionText = "HELP_TEXT_DIALOG_ADDITION_29";
constexpr std::string_view k_ClanLeaderText = "HELP_TEXT_DIALOG_ADDITION_31";
constexpr std::string_view k_ClanUrlText = "HELP_TEXT_DIALOG_ADDITION_32";
constexpr std::string_view k_CreditsText = "HELP_TEXT_MULTIPLAYER_CREDITS";
constexpr std::string_view k_UseClanText = "HELP_TEXT_DIALOG_ADDITION_142";
constexpr std::string_view k_NoClanText = "HELP_TEXT_DIALOG_ADDITION_49";
constexpr std::string_view k_PlayerInfoText = "HELP_TEXT_DIALOG_ADDITION_109";
constexpr std::string_view k_GodText = "HELP_TEXT_DIALOG_ADDITION_63";
constexpr std::string_view k_ObserversText = "HELP_TEXT_PAUSE_STATS_137";
constexpr std::string_view k_TimeLimitText = "HELP_TEXT_MULTIPLAYER_TIMELIMIT";
constexpr std::string_view k_ViewChatText = "HELP_TEXT_MULTIPLAYER_TOCHAT";
constexpr std::string_view k_ReadyText = "HELP_TEXT_MULTIPLAYER_READY";
constexpr std::string_view k_SelectMapText = "HELP_TEXT_MULTIPLAYER_SELECT_CONDITION";
constexpr std::string_view k_YesText = "HELP_TEXT_REQUESTER_BOXES_01";
constexpr std::string_view k_NoText = "HELP_TEXT_REQUESTER_BOXES_02";
constexpr std::string_view k_OkText = "HELP_TEXT_REQUESTER_BOXES_04";
constexpr std::string_view k_YesKeyText = "HELP_TEXT_PAUSE_STATS_145";
constexpr std::string_view k_NoKeyText = "HELP_TEXT_PAUSE_STATS_144";

/// The texts of the messages
std::string_view MessageTextName(Message message)
{
	switch (message)
	{
	case Message::NoConnections:
		return "HELP_TEXT_DIALOG_NOCONNECTIONSAVAILABLE";
	case Message::IncorrectPassword:
		return "HELP_TEXT_DIALOG_ADDITION_171";
	case Message::NotRegistered:
	case Message::EmailRequired:
		return "HELP_TEXT_DIALOG_YOUARENOTREGISTERED";
	case Message::InvalidDetails:
		return "HELP_TEXT_DIALOG_INVALIDDETAILS";
	case Message::ConnectionFailed:
		return "HELP_TEXT_MPFE_GS_COULD_NOT_CONNECT";
	case Message::CouldNotConnect:
		return "HELP_TEXT_DIALOG_ADDITION_41";
	case Message::HostLeft:
		return "HELP_TEXT_DIALOG_ADDITION_127";
	case Message::AlreadyInRoom:
		return "HELP_TEXT_MPFE_ERROR_ALREADY_IN_ROOM";
	case Message::WaitingForTransfer:
		return "HELP_TEXT_DIALOG_ADDITION_144";
	case Message::SendingStartData:
		return "HELP_TEXT_DIALOG_ADDITION_121";
	case Message::StillDownloading:
		return "HELP_TEXT_DIALOG_ADDITION_132";
	case Message::OthersDownloading:
		return "HELP_TEXT_DIALOG_ADDITION_130";
	case Message::KickedFromGame:
		return "HELP_TEXT_MPFER_KICKEDYOUGAME";
	case Message::None:
		return {};
	}
	return {};
}

bool IsReturn(int key)
{
	return key == SDLK_RETURN || key == SDLK_KP_ENTER;
}

std::vector<List::Item> Items(const std::vector<std::u16string>& lines)
{
	std::vector<List::Item> items;
	items.reserve(lines.size());
	for (const auto& line : lines)
	{
		items.push_back({.text = line});
	}
	return items;
}

void SetVisible(const std::vector<Control*>& controls, bool visible)
{
	for (auto* control : controls)
	{
		control->visible = visible;
	}
}

/// The list's items only change when the lines changed, which keeps the scroll and the selection
void SetItemsIfChanged(List& list, std::vector<List::Item> items)
{
	const auto& old = list.GetItems();
	const bool same = std::ranges::equal(
	    old, items, [](const List::Item& a, const List::Item& b) { return a.text == b.text && a.colour == b.colour; });
	if (!same)
	{
		const auto selection = list.GetSelection();
		list.SetItems(std::move(items));
		list.SetSelection(selection.has_value() && *selection < list.GetItems().size() ? selection : std::nullopt);
	}
}

} // namespace

MultiplayerBoxes::MultiplayerBoxes(const TextDatabase& texts, const GameFont& font)
    : _texts(texts)
    , _font(font)
{
}

MultiplayerBoxes::~MultiplayerBoxes() = default;

std::u16string MultiplayerBoxes::Text(std::string_view name) const
{
	return std::u16string(_texts.Get(name));
}

void MultiplayerBoxes::Open(std::unique_ptr<MultiplayerFrontEnd> frontEnd)
{
	_frontEnd = std::move(frontEnd);
	_built.reset();
	_message.reset();
	_left = false;
	Build();
}

void MultiplayerBoxes::Close()
{
	_message.reset();
	_dialog.reset();
	_built.reset();
	_frontEnd.reset();
}

bool MultiplayerBoxes::TakeLeft()
{
	return std::exchange(_left, false);
}

void MultiplayerBoxes::Build()
{
	_list = nullptr;
	_players = nullptr;
	_lanBox = nullptr;
	_internetBox = nullptr;
	_name = nullptr;
	_password = nullptr;
	_passwordLabel = nullptr;
	_help = nullptr;
	_playersLabel = nullptr;
	_next = nullptr;
	_caption = nullptr;
	_roster = nullptr;
	_chatControls.clear();
	_landControls.clear();
	_clanControls.clear();
	_playerControls.clear();
	_conditionControls.clear();
	_chat = nullptr;
	_chatEdit = nullptr;
	_lands = nullptr;
	_landInfo = nullptr;
	_clans = nullptr;
	_clanInfo = nullptr;
	_useClan = nullptr;
	_noClan = nullptr;
	_playerLabel = nullptr;
	_playerInfo = nullptr;
	_conditions = nullptr;
	_selectMap = nullptr;
	_timeLabel = nullptr;
	_timeLimit = nullptr;
	_viewChat = nullptr;
	_ready = nullptr;
	_chatLines = 0;
	_shownTab.reset();
	_shownPick.reset();
	_thumbnailLand.reset();
	_thumbnail.reset();
	_thumbnailShown = false;
	_logoShown = false;
	if (_frontEnd == nullptr)
	{
		_dialog.reset();
		return;
	}
	const auto box = _frontEnd->GetBox();
	_built = box;
	_builtKind = _frontEnd->GetLobbyKind();
	if (box == Box::Room || box == Box::Conditions)
	{
		BuildRoom();
	}
	else
	{
		_dialog = std::make_unique<Dialog>();
		auto& dialog = *_dialog;
		switch (box)
		{
		case Box::ChooseConnection:
			BuildChooseConnection(dialog);
			break;
		case Box::FindGames:
			BuildFindGames(dialog);
			break;
		case Box::Login:
			BuildLogin(dialog);
			break;
		case Box::GameList:
			BuildGameList(dialog);
			break;
		default:
			break;
		}
	}
	_fade.Reset(0.0f);
	_fade.SetDestination(1.0f, k_FadeInSeconds);
	Sync();
}

void MultiplayerBoxes::BuildChooseConnection(Dialog& dialog)
{
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_BackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, Text(k_NextText), LabelSide::Left, Look::RightArrow).onClick =
	    [this] { _frontEnd->Next(); };
	dialog.Add<StaticText>(k_Title, Text(k_ChooseTitle), Layout::Centre, k_Big);
	auto& list = dialog.Add<List>(_font, DialogRect {.min = {150, 200}, .max = {650, 440}}, k_Big, false, true, true);
	_list = &list;
	const auto internetName = _frontEnd->GetInternetName();
	const auto lines =
	    ConnectionLines(_frontEnd->HasLanConnection(),
	                    _frontEnd->HasInternetConnection() ? std::optional(std::u16string_view(internetName)) : std::nullopt,
	                    Text(k_LanText), Text(k_DefaultText));
	list.SetItems(Items(lines));
	list.SetSelection(_frontEnd->GetConnection());
	list.onClick = [this] { _frontEnd->SelectConnection(_list->GetSelection()); };
}

void MultiplayerBoxes::BuildFindGames(Dialog& dialog)
{
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_BackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, Text(k_NextText), LabelSide::Left, Look::RightArrow).onClick =
	    [this] { _frontEnd->Next(); };
	dialog.Add<StaticText>(k_Title, Text(k_FindTitle), Layout::Centre, k_Big);
	// The local network above the internet; a click on one takes the tick from the other
	_lanBox =
	    &dialog.Add<CheckBox>(_font, glm::ivec2 {260, 250}, k_ArrowSize, Text(k_OnLanText), LabelSide::Below, false, true);
	// The game makes only the upper box a radio button, which a click never clears
	_internetBox = &dialog.Add<CheckBox>(_font, glm::ivec2 {260, 370}, k_ArrowSize, Text(k_OnInternetText), LabelSide::Below,
	                                     false, false);
	_lanBox->onChange = [this](bool) { _frontEnd->SetLobbyKind(LobbyKind::Lan); };
	_internetBox->onChange = [this](bool) { _frontEnd->SetLobbyKind(LobbyKind::Internet); };
}

void MultiplayerBoxes::BuildLogin(Dialog& dialog)
{
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_BackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, Text(k_NextText), LabelSide::Left, Look::RightArrow).onClick =
	    [this] { _frontEnd->Login(_name->GetText(), _password->GetText()); };
	dialog.Add<StaticText>(DialogRect {.min = {110, 250}, .max = {310, 295}}, Text(k_UserNameText), Layout::Right, k_Big);
	dialog.Add<StaticText>(DialogRect {.min = {110, 370}, .max = {310, 415}}, Text(k_PasswordText), Layout::Right, k_Big);
	_name = &dialog.Add<EditBox>(_font, DialogRect {.min = {325, 250}, .max = {555, 290}}, _frontEnd->GetLoginName(),
	                             k_MostLoginCharacters, k_Mid);
	_password = &dialog.Add<EditBox>(_font, DialogRect {.min = {325, 370}, .max = {555, 410}}, std::u16string(),
	                                 k_MostLoginCharacters, k_Mid);
	_password->masked = true;
	dialog.Add<StaticText>(k_Title, Text(k_LoginTitle), Layout::Centre, k_Big);
	dialog.SetFocus(_name);
}

void MultiplayerBoxes::BuildGameList(Dialog& dialog)
{
	const bool lan = _frontEnd->GetLobbyKind() == LobbyKind::Lan;
	dialog.Add<BigButton>(_font, k_BackPosition, k_ArrowSize, Text(k_BackText), LabelSide::Right, Look::LeftArrow).onClick =
	    [this] { _frontEnd->Back(); };
	_next = &dialog.Add<BigButton>(_font, k_NextPosition, k_ArrowSize, Text(k_JoinGameText), LabelSide::Left, Look::RightArrow);
	_next->onClick = [this] { Join(); };
	dialog.Add<StaticText>(k_Title, Text(lan ? k_LanListTitle : k_InternetListTitle), Layout::Centre, k_Big);
	_help = &dialog.Add<StaticText>(DialogRect {.min = {420, 175}, .max = {690, 485}}, Text(k_GameListHelp), Layout::Wrapped,
	                                k_Big);
	dialog.Add<StaticText>(DialogRect {.min = {110, 130}, .max = {380, 170}}, Text(k_CurrentGamesText), Layout::Left, k_Mid);
	_list = &dialog.Add<List>(_font, DialogRect {.min = {110, 175}, .max = {380, 485}}, k_Mid, false, true, true);
	_list->onClick = [this] {
		const auto line = _list->GetSelection();
		_frontEnd->SelectGame(line.has_value() && *line < _shownGames.size() ? std::optional(_shownGames[*line])
		                                                                     : std::nullopt);
		// A click on a game puts its name in the name's box and empties the password's box
		const auto games = _frontEnd->GetGames();
		const auto picked = _frontEnd->GetSelectedGame();
		_name->SetText(picked.has_value() ? ToUtf16(games[*picked].hostname) : std::u16string());
		_password->SetText(std::u16string());
		if (!picked.has_value())
		{
			_frontEnd->SetFilter({});
		}
	};
	_playersLabel = &dialog.Add<StaticText>(DialogRect {.min = {420, 130}, .max = {690, 170}}, Text(k_PlayersInGameText),
	                                        Layout::Right, k_Mid);
	_players = &dialog.Add<List>(_font, DialogRect {.min = {420, 175}, .max = {690, 485}}, k_Mid, false, true, false);
	// The game keeps the name's box and its label hidden. The box only holds the name of the game that the player picks.
	auto& nameLabel =
	    dialog.Add<StaticText>(DialogRect {.min = {130, 480}, .max = {330, 510}}, Text(k_GameNameText), Layout::Right, k_Mid);
	nameLabel.visible = false;
	_name = &dialog.Add<EditBox>(_font, DialogRect {.min = {350, 480}, .max = {550, 510}}, std::u16string(),
	                             lobby_api::k_MostNameCharacters, k_Mid);
	_name->visible = false;
	_passwordLabel = &dialog.Add<StaticText>(DialogRect {.min = {130, 530}, .max = {330, 570}}, Text(k_GamePasswordText),
	                                         Layout::Right, k_Mid);
	_password = &dialog.Add<EditBox>(_font, DialogRect {.min = {350, 530}, .max = {550, 570}}, std::u16string(),
	                                 k_MostLoginCharacters, k_Mid);
	// With no game picked, the password's box is the filter of the game names
	_password->onChange = [this](const std::u16string& text) {
		if (!_frontEnd->GetSelectedGame().has_value())
		{
			_frontEnd->SetFilter(text);
		}
	};
	// Refresh List shows only on the internet
	auto& refresh = dialog.Add<Button>(DialogRect {.min = {110, 490}, .max = {250, 530}}, Text(k_RefreshText), k_Mid);
	refresh.visible = !lan;
	refresh.onClick = [this] { _frontEnd->RefreshGames(); };
}

void MultiplayerBoxes::BuildRoom()
{
	const bool lan = _frontEnd->GetLobbyKind() == LobbyKind::Lan;
	std::vector<Dialog::Tab> tabs;
	for (size_t i = 0; i < k_RoomTabCount; ++i)
	{
		// On the local network the Clans and Player Details tabs have no label and do nothing
		const bool blank =
		    lan && (i == static_cast<size_t>(RoomTab::Clans) || i == static_cast<size_t>(RoomTab::PlayerDetails));
		tabs.push_back({.label = blank ? std::u16string() : Text(k_TabTexts.at(i)),
		                .onSelect = blank ? std::function<void()>()
		                                  : [this, i] {
			                                    _frontEnd->SelectTab(static_cast<RoomTab>(i));
			                                    if (_frontEnd->GetBox() == Box::Conditions)
			                                    {
				                                    _frontEnd->Back();
			                                    }
		                                    },
		                .textSize = k_Mid});
	}
	_dialog = std::make_unique<Dialog>(std::move(tabs), static_cast<size_t>(_frontEnd->GetTab()), k_RoomBox);
	auto& dialog = *_dialog;

	_caption = &dialog.Add<StaticText>(DialogRect {.min = {40, 60}, .max = {760, 88}}, std::u16string(), Layout::Centre, k_Mid);
	// The gods and the observers of the room, one line each in their colour
	_roster = &dialog.Add<List>(_font, DialogRect {.min = {40, 95}, .max = {760, 370}}, k_Mid, false, true, true);
	_roster->onClick = [this] { _frontEnd->ShowSeat(_roster->GetSelection()); };

	// The Chat tab
	_chatControls.push_back(
	    &dialog.Add<StaticText>(DialogRect {.min = {40, 380}, .max = {760, 408}}, Text(k_IncomingText), Layout::Centre, k_Mid));
	_chat = &dialog.Add<List>(_font, DialogRect {.min = {140, 410}, .max = {660, 530}}, k_Mid, false, true, false);
	_chatControls.push_back(_chat);
	_chatEdit = &dialog.Add<EditBox>(_font, DialogRect {.min = {140, 545}, .max = {660, 575}}, Text(k_TypeChatText),
	                                 k_MostChatCharacters, k_Mid);
	_chatEdit->onClick = [this] { SendChat(); };
	_chatControls.push_back(_chatEdit);

	// The Landscape tab: the lands, the server's land marked as the host's choice, and the land's information
	_landControls.push_back(&dialog.Add<StaticText>(DialogRect {.min = {140, 380}, .max = {330, 408}}, Text(k_LandscapesText),
	                                                Layout::Centre, k_Mid));
	_landControls.push_back(
	    &dialog.Add<StaticText>(DialogRect {.min = {0, 380}, .max = {800, 408}}, Text(k_ThumbnailText), Layout::Centre, k_Mid));
	_landControls.push_back(&dialog.Add<StaticText>(DialogRect {.min = {470, 380}, .max = {660, 408}}, Text(k_LandInfoTitle),
	                                                Layout::Centre, k_Mid));
	_lands = &dialog.Add<List>(_font, DialogRect {.min = {140, 410}, .max = {330, 530}}, k_Mid, false, true, true);
	_lands->onClick = [this] { _frontEnd->ShowLand(_lands->GetSelection()); };
	_landControls.push_back(_lands);
	_landInfo = &dialog.Add<List>(_font, DialogRect {.min = {470, 410}, .max = {660, 530}}, k_Mid, false, true, false);
	_landControls.push_back(_landInfo);

	// The Clans tab
	_clansLabel =
	    &dialog.Add<StaticText>(DialogRect {.min = {100, 380}, .max = {400, 408}}, std::u16string(), Layout::Centre, k_Mid);
	_clanInfoLabel =
	    &dialog.Add<StaticText>(DialogRect {.min = {400, 380}, .max = {700, 408}}, std::u16string(), Layout::Centre, k_Mid);
	_clans = &dialog.Add<List>(_font, DialogRect {.min = {140, 410}, .max = {390, 530}}, k_Mid, false, true, true);
	_clans->onClick = [this] { _frontEnd->ShowClan(_clans->GetSelection()); };
	_clanInfo = &dialog.Add<List>(_font, DialogRect {.min = {410, 410}, .max = {660, 530}}, k_Mid, false, true, false);
	_useClan = &dialog.Add<Button>(DialogRect {.min = {250, 540}, .max = {550, 570}}, Text(k_UseClanText), k_Mid);
	_useClan->onClick = [this] { _frontEnd->UseShownClan(); };
	_noClan = &dialog.Add<Button>(DialogRect {.min = {250, 540}, .max = {550, 570}}, Text(k_NoClanText), k_Mid);
	_noClan->onClick = [this] { _frontEnd->UseNoClan(); };
	_clanControls = {_clansLabel, _clanInfoLabel, _clans, _clanInfo};

	// The Player Details tab
	_playerLabel =
	    &dialog.Add<StaticText>(DialogRect {.min = {0, 380}, .max = {800, 408}}, std::u16string(), Layout::Centre, k_Mid);
	_playerInfo = &dialog.Add<List>(_font, DialogRect {.min = {140, 410}, .max = {660, 530}}, k_Mid, false, true, false);
	_playerControls = {_playerLabel, _playerInfo};

	// The conditions page, as the game shows it to a player who is not the host. The game server has the host's part,
	// so the value of a condition and the arrows do not show.
	_conditions = &dialog.Add<List>(_font, DialogRect {.min = {100, 120}, .max = {700, 320}}, k_Mid, false, true, true);
	_selectMap = &dialog.Add<StaticText>(DialogRect {.min = {100, 200}, .max = {700, 260}}, Text(k_SelectMapText),
	                                     Layout::Centre, k_Mid);
	_timeLabel =
	    &dialog.Add<StaticText>(DialogRect {.min = {400, 340}, .max = {560, 370}}, Text(k_TimeLimitText), Layout::Left, k_Mid);
	_timeLimit =
	    &dialog.Add<StaticText>(DialogRect {.min = {600, 320}, .max = {680, 380}}, std::u16string(), Layout::Centre, k_Mid);
	_viewChat = &dialog.Add<Button>(DialogRect {.min = {675, 440}, .max = {775, 472}}, Text(k_ViewChatText), k_Mid);
	_viewChat->onClick = [this] {
		_frontEnd->SelectTab(RoomTab::Chat);
		_frontEnd->Back();
	};
	_ready = &dialog.Add<CheckBox>(_font, glm::ivec2 {65, 460}, k_Mid, Text(k_ReadyText), LabelSide::Below, false, false);
	_ready->onChange = [this](bool checked) { _frontEnd->SetReady(checked); };
	_conditionControls = {_conditions, _selectMap, _timeLabel, _timeLimit, _viewChat, _ready};

	// Back leaves the room; Next goes to the conditions page. The host's Start is the server's own: it does not show.
	dialog.Add<BigButton>(_font, k_RoomBackPosition, k_ArrowSize, Text(k_RoomBackText), LabelSide::Right, Look::LeftArrow)
	    .onClick = [this] { _frontEnd->Back(); };
	_next = &dialog.Add<BigButton>(_font, k_RoomNextPosition, k_ArrowSize, Text(k_NextText), LabelSide::Left, Look::RightArrow);
	_next->onClick = [this] { _frontEnd->Next(); };
}

void MultiplayerBoxes::Sync()
{
	if (_frontEnd == nullptr)
	{
		return;
	}
	switch (_frontEnd->GetBox())
	{
	case Box::FindGames:
		_lanBox->SetChecked(_frontEnd->GetLobbyKind() == LobbyKind::Lan);
		_internetBox->SetChecked(_frontEnd->GetLobbyKind() == LobbyKind::Internet);
		break;
	case Box::GameList:
		SyncGameList();
		break;
	case Box::Room:
	case Box::Conditions:
		SyncRoom();
		break;
	default:
		break;
	}
	if (const auto message = _frontEnd->GetMessage(); message != (_message.has_value() ? _message->message : Message::None))
	{
		ShowMessage(message);
	}
}

void MultiplayerBoxes::SyncGameList()
{
	const bool lan = _frontEnd->GetLobbyKind() == LobbyKind::Lan;
	const auto games = _frontEnd->GetGames();
	const auto picked = _frontEnd->GetSelectedGame();
	const GameLineTexts texts {.playing = Text(k_PlayingText),
	                           .wrongVersion = Text(k_WrongVersionText),
	                           .needsPassword = Text(k_NeedsPasswordText),
	                           .locked = Text(k_LockedText)};
	_shownGames = _frontEnd->GetShownGames();
	std::vector<List::Item> items;
	std::optional<size_t> selection;
	for (size_t i = 0; i < _shownGames.size(); ++i)
	{
		const auto& server = games[_shownGames[i]];
		items.push_back({.text = GameLine(server, texts, _frontEnd->GetGameVersion(), _frontEnd->GetLobbyKind())});
		if (picked == _shownGames[i])
		{
			selection = i;
		}
	}
	SetItemsIfChanged(*_list, std::move(items));
	// While the mouse button is down the list keeps the line under it, which the click then picks
	if (!_dialog->IsHolding())
	{
		_list->SetSelection(selection);
	}

	// With no game picked, the password's box is the filter: "Game Name Filter:", its text shown. The players of the
	// game show in place of the help only while a game is picked.
	const bool none = !picked.has_value();
	// A game picked shows its name in the name's box
	if (picked != _shownPick)
	{
		_shownPick = picked;
		_name->SetText(picked.has_value() ? ToUtf16(games[*picked].hostname) : std::u16string());
	}
	_password->masked = !none;
	_passwordLabel->text = Text(none ? k_FilterText : k_GamePasswordText);
	const bool needsPassword = !none && games[*picked].password;
	_password->visible = !lan && (none || needsPassword);
	_passwordLabel->visible = _password->visible;
	_next->visible = !none;
	_help->visible = none;
	_players->visible = !none;
	_playersLabel->visible = !none;
	if (!none)
	{
		std::vector<std::u16string> names;
		for (const auto& player : games[*picked].players)
		{
			names.push_back(ToUtf16(player.account.name));
		}
		SetItemsIfChanged(*_players, Items(names));
	}
}

void MultiplayerBoxes::SyncRoom()
{
	const auto* server = _frontEnd->GetRoomServer();
	if (server == nullptr)
	{
		return;
	}
	const bool lan = _frontEnd->GetLobbyKind() == LobbyKind::Lan;
	const bool conditionsPage = _frontEnd->GetBox() == Box::Conditions;
	const auto tab = _frontEnd->GetTab();
	if (_shownTab != static_cast<size_t>(tab))
	{
		_dialog->SelectTab(static_cast<size_t>(tab));
		_shownTab = static_cast<size_t>(tab);
	}

	// The caption: "<name>, you are in <game>, landscape: <land> (<n> players)."
	const auto maps = _frontEnd->GetLocalMaps();
	const auto land = _frontEnd->GetServerLand();
	const auto landName = land.has_value() ? maps[*land].name : Text(k_NoLandText);
	const auto playerName = _frontEnd->GetPlayerName();
	const auto gameName = ToUtf16(server->hostname);
	const std::array<TextArgument, 4> captionArguments = {std::u16string_view(playerName), std::u16string_view(gameName),
	                                                      std::u16string_view(landName), int64_t {server->numplayers}};
	_caption->text = FormatText(Text(k_CaptionText), captionArguments);

	// The roster: "God 3: name (CURRENT)" in the god's colour, the observers in grey
	const auto* own = _frontEnd->GetOwnSeat();
	std::vector<List::Item> roster;
	const auto godText = Text(k_GodText);
	const auto observersText = Text(k_ObserversText);
	const auto currentText = Text(k_CurrentText);
	const RosterTexts rosterTexts {.god = godText, .observers = observersText, .current = currentText};
	// The ping display (deviation 143) is off unless the player turns it on
	const auto ping = _frontEnd->ShowsPing() ? ping_display::Text(_frontEnd->GetRoundTrip()) : std::nullopt;
	for (const auto& seat : _frontEnd->GetSeats())
	{
		const bool isOwn = own != nullptr && own->account.id == seat.account.id && own->account.name == seat.account.name;
		roster.push_back(
		    {.text = RosterLine(seat, ToUtf16(seat.account.name), isOwn, rosterTexts, ping), .colour = GodColour(seat.god)});
	}
	SetItemsIfChanged(*_roster, std::move(roster));

	// What shows on each tab and on the conditions page. The roster does not show on the Landscape tab.
	const bool onTab = !conditionsPage;
	_roster->visible = onTab && tab != RoomTab::Landscape;
	SetVisible(_chatControls, onTab && tab == RoomTab::Chat);
	SetVisible(_landControls, onTab && tab == RoomTab::Landscape);
	SetVisible(_clanControls, onTab && tab == RoomTab::Clans && !lan);
	SetVisible(_playerControls, onTab && tab == RoomTab::PlayerDetails && !lan);
	SetVisible(_conditionControls, conditionsPage);
	_next->visible = onTab;

	// The chat scrolls to its newest line
	const auto& chat = _frontEnd->GetChat();
	if (chat.size() != _chatLines)
	{
		std::vector<List::Item> lines;
		lines.reserve(chat.size());
		for (const auto& line : chat)
		{
			lines.push_back({.text = line.text, .colour = line.colour});
		}
		_chat->SetItems(std::move(lines));
		_chat->SetSelection(std::nullopt);
		_chat->ScrollToSelection();
		_chatLines = chat.size();
	}

	// The lands: the server's land is the host's choice
	std::vector<std::u16string> landLines;
	for (size_t i = 0; i < maps.size(); ++i)
	{
		landLines.push_back(maps[i].name + (land == i ? Text(k_HostText) : std::u16string()));
	}
	SetItemsIfChanged(*_lands, Items(landLines));
	const auto shownLand = _frontEnd->GetShownLand();
	if (!_dialog->IsHolding())
	{
		_lands->SetSelection(shownLand);
	}
	// The thumbnail of the land picked
	if (shownLand != _thumbnailLand)
	{
		_thumbnailLand = shownLand;
		_thumbnail = shownLand.has_value() && *shownLand < maps.size() && _thumbnailSource
		                 ? _thumbnailSource(maps[*shownLand].file)
		                 : nullptr;
	}
	// The first three lands have texts of their own; the other lands show the description of their thumbnail
	std::vector<std::u16string> info;
	if (shownLand.has_value())
	{
		if (const auto text = LandInfoText(*shownLand); text.has_value())
		{
			info.push_back(Text(*text));
		}
		else if (_thumbnail != nullptr)
		{
			info.emplace_back(_thumbnail->description.begin(), _thumbnail->description.end());
		}
	}
	SetItemsIfChanged(*_landInfo, Items(info));

	// The clans of the player, and the information of the clan picked
	const std::array<TextArgument, 1> nameArgument = {std::u16string_view(playerName)};
	_clansLabel->text = FormatText(Text(k_ClansOfText), nameArgument);
	std::vector<std::u16string> clanNames;
	for (const auto& clan : _frontEnd->GetMyClans())
	{
		clanNames.push_back(ToUtf16(clan.name));
	}
	SetItemsIfChanged(*_clans, Items(clanNames));
	const auto& shownClan = _frontEnd->GetShownClan();
	if (shownClan.has_value())
	{
		const auto clanName = ToUtf16(shownClan->name);
		const std::array<TextArgument, 1> clanArgument = {std::u16string_view(clanName)};
		_clanInfoLabel->text = FormatText(Text(k_ClanNamedInfoText), clanArgument);
		const ClanInfoTexts clanTexts {.description = Text(k_ClanDescriptionText),
		                               .leader = Text(k_ClanLeaderText),
		                               .url = Text(k_ClanUrlText),
		                               .credits = Text(k_CreditsText)};
		SetItemsIfChanged(*_clanInfo, Items(ClanInfoLines(*shownClan, clanTexts)));
	}
	else
	{
		_clanInfoLabel->text = Text(k_ClanInfoText);
		SetItemsIfChanged(*_clanInfo, {});
	}
	// A god chooses a clan for itself; an observer has no clan
	const bool clansShown = onTab && tab == RoomTab::Clans && !lan && own != nullptr && own->god > 0;
	const auto used = _frontEnd->GetUsedClan();
	const bool showsUsed = shownClan.has_value() && used == shownClan->id;
	_useClan->visible = clansShown && shownClan.has_value() && !showsUsed;
	_noClan->visible = clansShown && used.has_value() && !_useClan->visible;

	// The player picked in the roster, else the player
	const auto seats = _frontEnd->GetSeats();
	const auto shownSeat = _frontEnd->GetShownSeat();
	const auto* seat = shownSeat.has_value() && *shownSeat < seats.size() ? &seats[*shownSeat] : own;
	const auto seatName = seat != nullptr ? ToUtf16(seat->account.name) : playerName;
	const std::array<TextArgument, 1> seatArgument = {std::u16string_view(seatName)};
	_playerLabel->text = FormatText(Text(k_PlayerInfoText), seatArgument);
	// The game shows only the lines of the lobby's player database under the title. The lobby has no such lines yet.
	SetItemsIfChanged(*_playerInfo, Items(PlayerDetailLines({})));

	// The conditions page: the conditions of the game and its time limit, or "Please select a map first." A game of
	// the game server has no conditions, so the list is empty.
	const auto page = ConditionsPageOf(_frontEnd->GetConditions().has_value());
	_selectMap->visible = conditionsPage && page.selectMap;
	_conditions->visible = conditionsPage && page.conditions;
	_timeLabel->visible = conditionsPage && page.timeLimit;
	_timeLimit->visible = conditionsPage && page.timeLimit;
	_viewChat->visible = conditionsPage && page.viewChat;
	SetItemsIfChanged(*_conditions, Items({}));
	_timeLimit->text = TimeLimitText(_frontEnd->GetTimeLimit());
	_ready->SetChecked(_frontEnd->IsReady());

	SyncPictures(onTab && tab == RoomTab::Landscape, onTab && tab == RoomTab::Clans && !lan);
}

void MultiplayerBoxes::SyncPictures(bool landShown, bool clanShown)
{
	_thumbnailShown = landShown && _thumbnail != nullptr && _thumbnail->side > 0;
	if (_frontEnd->TakeLogoChanged())
	{
		const auto& logo = _frontEnd->GetShownLogo();
		_logoSheet = logo.has_value() ? land_thumbnail::LogoSheet(*logo) : land_thumbnail::Texels {};
		_logoChanged = true;
	}
	_logoShown = clanShown && !_logoSheet.empty();
}

std::vector<DialogRect> MultiplayerBoxes::GetPictureRects() const
{
	std::vector<DialogRect> rects;
	if (_thumbnailShown)
	{
		rects.push_back(land_thumbnail::k_Place);
	}
	if (_logoShown && _clanInfo != nullptr)
	{
		rects.push_back(land_thumbnail::LogoPlace(_clanInfo->rect));
	}
	return rects;
}

void MultiplayerBoxes::DrawPictures(const DialogPainter& painter) const
{
	if (_logoChanged)
	{
		_logoChanged = false;
		_logoTexture.reset();
		if (!_logoSheet.empty())
		{
			_logoTexture = std::make_unique<graphics::Texture2D>("Clan logo");
			_logoTexture->Create(land_thumbnail::k_TextureSide, land_thumbnail::k_TextureSide, 1,
			                     graphics::TextureFormat::BGRA4, graphics::Wrapping::Repeat, graphics::Filter::Linear,
			                     bgfx::copy(_logoSheet.data(), static_cast<uint32_t>(_logoSheet.size() * sizeof(uint16_t))));
		}
	}
	// The game draws the pictures after the controls, at the box's opacity
	if (_thumbnailShown && _thumbnail->texture != nullptr)
	{
		painter.DrawPicture(land_thumbnail::k_Place, *_thumbnail->texture, glm::vec2(0.0f),
		                    land_thumbnail::UvMax(_thumbnail->side), glm::vec4(1.0f));
	}
	// The logo is a faint mark behind the lines of the clan's information
	if (_logoShown && _logoTexture != nullptr && _clanInfo != nullptr)
	{
		const auto alpha = painter.GetAlpha();
		painter.SetAlpha(land_thumbnail::LogoAlpha(alpha));
		const auto [uvMin, uvMax] = land_thumbnail::LogoCellUv();
		painter.DrawPicture(land_thumbnail::LogoPlace(_clanInfo->rect), *_logoTexture, uvMin, uvMax, glm::vec4(1.0f));
		painter.SetAlpha(alpha);
	}
}

void MultiplayerBoxes::ShowMessage(Message message)
{
	if (message == Message::None)
	{
		_message.reset();
		return;
	}
	auto& shown = _message.emplace();
	shown.message = message;
	shown.text = Text(MessageTextName(message));
	if (message == Message::KickedFromGame)
	{
		const auto name = _frontEnd->GetMessageName();
		const std::array<TextArgument, 1> nameArgument = {std::u16string_view(name)};
		shown.text = FormatText(shown.text, nameArgument);
	}
	shown.textControl = &shown.answers.Add<StaticText>(k_MessageText, shown.text, Layout::Wrapped);
	shown.textControl->takesClicks = true;
	switch (ButtonsOf(message))
	{
	case MessageButtons::Ok:
		shown.answers.Add<Button>(k_MessageOk, Text(k_OkText), k_Mid).onClick = [this] {
			_frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Ok);
		};
		break;
	case MessageButtons::YesNo:
		shown.answers
		    .Add<BigButton>(_font, k_YesPosition, k_AnswerArrowSize, Text(k_YesText), LabelSide::Right, Look::LeftArrow)
		    .onClick = [this] { _frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Yes); };
		shown.answers.Add<BigButton>(_font, k_NoPosition, k_AnswerArrowSize, Text(k_NoText), LabelSide::Left, Look::RightArrow)
		    .onClick = [this] { _frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::No); };
		break;
	case MessageButtons::None:
		break;
	}
	_messageFade.Reset(0.0f);
	_messageFade.SetDestination(1.0f, k_FadeInSeconds);
}

void MultiplayerBoxes::SendChat()
{
	if (_chatEdit == nullptr)
	{
		return;
	}
	// The box first holds its prompt: a click on it clears the prompt and sends nothing, as the game's box does
	if (_chatEdit->GetText() != Text(k_TypeChatText))
	{
		_frontEnd->SendChat(_chatEdit->GetText());
	}
	_chatEdit->SetText(std::u16string());
	// The chat's box keeps the keyboard for the next line
	_dialog->SetFocus(_chatEdit);
}

void MultiplayerBoxes::Join()
{
	// The name's box holds the name of the game that the player picked, and that name picks the game
	const auto games = _frontEnd->GetGames();
	const auto typed = ToUtf8(_name->GetText());
	if (const auto found = std::ranges::find(games, typed, &lobby_api::ServerEntry::hostname); found != games.end())
	{
		_frontEnd->SelectGame(static_cast<size_t>(found - games.begin()));
	}
	_frontEnd->Join(_frontEnd->GetSelectedGame().has_value() ? _password->GetText() : std::u16string());
}

void MultiplayerBoxes::MouseMove(glm::ivec2 point)
{
	if (_message.has_value())
	{
		_message->answers.MouseMove(point);
	}
	else if (_dialog != nullptr)
	{
		_dialog->MouseMove(point);
	}
}

void MultiplayerBoxes::MouseDown(glm::ivec2 point)
{
	if (_message.has_value())
	{
		_message->answers.MouseDown(point);
	}
	else if (_dialog != nullptr && !_frontEnd->IsBusy())
	{
		_dialog->MouseDown(point);
	}
}

bool MultiplayerBoxes::MouseUp(glm::ivec2 point)
{
	if (_message.has_value())
	{
		return _message->answers.MouseUp(point);
	}
	return _dialog != nullptr && _dialog->MouseUp(point);
}

void MultiplayerBoxes::Wheel(glm::ivec2 point, int steps)
{
	if (!_message.has_value() && _dialog != nullptr)
	{
		_dialog->Wheel(point, steps);
	}
}

void MultiplayerBoxes::TextInput(std::u16string_view text)
{
	if (_message.has_value())
	{
		// The first letters of the words "yes" and "no" answer a question
		const auto yes = _texts.Get(k_YesKeyText);
		const auto no = _texts.Get(k_NoKeyText);
		if (ButtonsOf(_message->message) != MessageButtons::YesNo || text.empty() || yes.empty() || no.empty())
		{
			return;
		}
		switch (dialog_keys::AnswerTyped(dialog_keys::Buttons::YesNo, text.front(), yes.front(), no.front()))
		{
		case dialog_keys::Answer::Yes:
			_frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Yes);
			break;
		case dialog_keys::Answer::No:
			_frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::No);
			break;
		default:
			break;
		}
		return;
	}
	if (_dialog != nullptr)
	{
		_dialog->TextInput(text);
	}
}

void MultiplayerBoxes::KeyDown(int key, bool shift)
{
	if (_message.has_value())
	{
		// A message's OK answers Return and Escape
		if (ButtonsOf(_message->message) == MessageButtons::Ok && (IsReturn(key) || key == SDLK_ESCAPE))
		{
			_frontEnd->AnswerMessage(MultiplayerFrontEnd::Answer::Ok);
		}
		return;
	}
	if (_dialog == nullptr)
	{
		return;
	}
	// Return in the chat's box sends the line
	if (IsReturn(key) && _chatEdit != nullptr && _dialog->GetFocus() == _chatEdit)
	{
		SendChat();
		return;
	}
	_dialog->KeyDown(key, shift);
}

void MultiplayerBoxes::Update(float deltaSeconds)
{
	if (_frontEnd == nullptr)
	{
		return;
	}
	_frontEnd->Update();
	if (_frontEnd->TakeLeft())
	{
		_left = true;
		return;
	}
	if (_built != _frontEnd->GetBox() && !(_built == Box::Room && _frontEnd->GetBox() == Box::Conditions) &&
	    !(_built == Box::Conditions && _frontEnd->GetBox() == Box::Room))
	{
		Build();
	}
	else if (_builtKind != _frontEnd->GetLobbyKind() && _frontEnd->GetBox() == Box::GameList)
	{
		Build();
	}
	else
	{
		Sync();
	}
	_fade.Update(deltaSeconds);
	_messageFade.Update(deltaSeconds);
	if (_dialog != nullptr)
	{
		_dialog->Update(deltaSeconds);
	}
	if (_message.has_value())
	{
		_message->answers.Update(deltaSeconds);
	}
}

void MultiplayerBoxes::Draw(const DialogPainter& painter) const
{
	if (_dialog == nullptr || _frontEnd == nullptr)
	{
		return;
	}
	const auto fade = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	const auto messageAlpha = _message.has_value() ? std::clamp(_messageFade.GetValue(), 0.0f, 1.0f) : 0.0f;
	// Behind a message the box dims by three quarters as the message fades in
	const auto alpha = std::clamp(fade - (messageAlpha * k_HoldDims), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	const auto box = _frontEnd->GetBox();
	if (box != Box::Room && box != Box::Conditions)
	{
		painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::All);
		painter.SetAlpha(alpha);
	}
	_dialog->Draw(painter, !_message.has_value() && !_frontEnd->IsBusy());
	DrawPictures(painter);
	if (_message.has_value())
	{
		// The message's text starts as large as it fits in under 125 high
		auto size = DialogPainter::k_BigTextSize + 1;
		do
		{
			--size;
		} while (painter.GetTextHeight(k_MessageWidth, _message->text, size) >= k_MessageTallest &&
		         size > DialogPainter::k_SmallTextSize);
		_message->textControl->size = size;
		painter.SetAlpha(messageAlpha);
		painter.DrawBackground(k_MessageBox, glm::vec3(1.0f), true, DialogPainter::All);
		painter.SetAlpha(messageAlpha);
		_message->answers.Draw(painter, true);
	}
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> MultiplayerBoxes::GetNamedControls() const
{
	if (_message.has_value())
	{
		return _message->answers.GetNamedControls();
	}
	return _dialog != nullptr ? _dialog->GetNamedControls() : std::vector<Dialog::NamedControl> {};
}

std::vector<DialogRect> MultiplayerBoxes::GetVisibleRects() const
{
	return _dialog != nullptr ? _dialog->GetVisibleRects() : std::vector<DialogRect> {};
}

std::u16string_view MultiplayerBoxes::GetMessageText() const
{
	return _message.has_value() ? std::u16string_view(_message->text) : std::u16string_view();
}
