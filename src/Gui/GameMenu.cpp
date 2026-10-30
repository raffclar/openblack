/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameMenu.h"

#include <algorithm>
#include <functional>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

#include <SDL_keycode.h>

#include "ConditionsBox.h"
#include "GameFont.h"
#include "Input/KeyRebinding.h"
#include "StatisticsBox.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
using Look = BigButton::Look;
using LabelSide = BigButton::LabelSide;
using Layout = StaticText::Layout;
constexpr auto k_Big = DialogPainter::k_BigTextSize;
/// The middle text size for the language of the install
[[nodiscard]] int Mid()
{
	return DialogPainter::MidTextSize();
}

// Dialogs and message boxes: fading in when shown and out when hidden
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;

// The main menu's buttons
constexpr int k_ButtonLeft = 180;
constexpr int k_ButtonWidth = 440;
constexpr int k_ButtonHeight = 70;
constexpr int k_FirstButtonTop = 145;
constexpr int k_ButtonSpacing = 80;
// In a skirmish or a network game the main menu's buttons are bigger and further apart
constexpr int k_FewerButtonsHeight = 80;
constexpr int k_FewerButtonsTop = 150;
constexpr int k_FewerButtonsSpacing = 100;

// The options' pages: Back at the bottom left, 40 pixel arrows
constexpr glm::ivec2 k_BackPosition {30, 530};
constexpr int k_BackSize = 40;
// The selectors: a 200 by 30 button between two 32 pixel arrows
constexpr int k_SelectorLeft = 300;
constexpr int k_SelectorWidth = 200;
constexpr int k_SelectorHeight = 30;
constexpr int k_SelectorLeftArrow = 260;
constexpr int k_SelectorRightArrow = 508;
constexpr int k_ArrowSize = 32;
constexpr int k_CheckBoxTop = 450;

constexpr std::array k_DetailTexts = {"HELP_TEXT_DIALOG_DETAIL_00", "HELP_TEXT_DIALOG_DETAIL_01", "HELP_TEXT_DIALOG_DETAIL_02",
                                      "HELP_TEXT_DIALOG_DETAIL_03", "HELP_TEXT_DIALOG_DETAIL_04", "HELP_TEXT_DIALOG_DETAIL_05",
                                      "HELP_TEXT_DIALOG_DETAIL_06"};
constexpr std::array k_HelpTexts = {"HELP_TEXT_DIALOG_NOHELP", "HELP_TEXT_DIALOG_LOWHELP", "HELP_TEXT_DIALOG_MIDDLEHELP",
                                    "HELP_TEXT_DIALOG_HIGHHELP", "HELP_TEXT_DIALOG_ALLHELP"};
constexpr std::array k_StoryTexts = {"HELP_TEXT_DIALOG_NOTEXT", "HELP_TEXT_DIALOG_STORYTEXT", "HELP_TEXT_DIALOG_ALLTEXT"};
constexpr std::array k_ToolTipTexts = {"HELP_TEXT_DIALOG_NOTOOLTIPS", "HELP_TEXT_DIALOG_MINIMUMTOOLTIPS",
                                       "HELP_TEXT_DIALOG_INTELLIGENTNOTOOLTIPS", "HELP_TEXT_DIALOG_ALLTOOLTIPS"};

/// The pictures of the mouse that the Controls page draws beside an action
enum class Mouse
{
	None,
	Left,
	Middle,
	WheelUp,
	WheelDown,
	Right,
	/// Both buttons, for the zoom actions that have no mouse input of their own
	Both,
};

/// The names of the actions, in the order that the Controls page lists them
constexpr std::array<std::string_view, openblack::input::k_KeyBindingCount> k_ActionTexts = {
    "HELP_TEXT_ACTIONS_HELP",
    "HELP_TEXT_ACTIONS_SELECT",
    "HELP_TEXT_ACTIONS_APPLY",
    "HELP_TEXT_ACTIONS_ZOOMOUT",
    "HELP_TEXT_ACTIONS_ZOOMIN",
    "HELP_TEXT_ACTIONS_TALK",
    "HELP_TEXT_ACTIONS_ZOOMON",
    "HELP_TEXT_ACTIONS_MOVELEFT",
    "HELP_TEXT_ACTIONS_MOVERIGHT",
    "HELP_TEXT_ACTIONS_MOVEFORWARD",
    "HELP_TEXT_ACTIONS_MOVEBACKWARD",
    "HELP_TEXT_ACTIONS_TILTUP",
    "HELP_TEXT_ACTIONS_TILTDOWN",
    "HELP_TEXT_ACTIONS_ROTATELEFT",
    "HELP_TEXT_ACTIONS_ROTATERIGHT",
    "HELP_TEXT_ACTIONS_TILTROTATEON",
    "HELP_TEXT_ACTIONS_TILTROTATEAROUNDMOUSE",
    "HELP_TEXT_ACTIONS_ZOOMTO_CITADEL",
    "HELP_TEXT_ACTIONS_ZOOMTO_CREATURE",
    "HELP_TEXT_ACTIONS_ZOOMTO_REALM",
    "HELP_TEXT_ACTIONS_ZOOMTO_INSIDECITADEL",
    "HELP_TEXT_ACTIONS_ZOOMTO_CREATUREROOM",
    "HELP_TEXT_ACTIONS_ZOOMTO_CHALLENGEROOM",
    "HELP_TEXT_ACTIONS_ZOOMTO_SAVEGAMEROOM",
    "HELP_TEXT_ACTIONS_ZOOMTO_OPTIONSROOM",
    "HELP_TEXT_ACTIONS_ZOOMTO_MULTIPLAYERROOM",
    "HELP_TEXT_ACTIONS_LEASH",
    "HELP_TEXT_ACTIONS_SHOWVILLAGERNAME",
    "HELP_TEXT_ACTIONS_SHOWVILLAGERDETAILS",
    "HELP_TEXT_TOOLTIP_155",
    "HELP_TEXT_TOOLTIP_154",
    "HELP_TEXT_ACTIONS_PREVIOUS_LEASH",
    "HELP_TEXT_ACTIONS_NEXT_LEASH",
};

/// The mouse that the Controls page draws beside a row: the row's own mouse input. A zoom action with no mouse input
/// shows both buttons while the move and the action are on the left and the right buttons, either way round.
Mouse ShownMouse(std::span<const openblack::input::KeyBinding> bindings, size_t row)
{
	using openblack::input::BindableActionMap;
	using openblack::input::MouseInput;
	const auto& binding = bindings[row];
	if (binding.mouse != MouseInput::None)
	{
		return static_cast<Mouse>(binding.mouse);
	}
	const bool zoom = binding.action == BindableActionMap::ZOOM_OUT || binding.action == BindableActionMap::ZOOM_IN ||
	                  binding.action == BindableActionMap::ZOOM_ON;
	if (!zoom)
	{
		return Mouse::None;
	}
	const auto mouseOf = [bindings](BindableActionMap action) {
		const auto index = openblack::input::IndexOf(bindings, action);
		return index.has_value() ? bindings[*index].mouse : MouseInput::None;
	};
	const auto move = mouseOf(BindableActionMap::MOVE);
	const auto act = mouseOf(BindableActionMap::ACTION);
	const bool buttons = (act == MouseInput::LeftButton && move == MouseInput::RightButton) ||
	                     (act == MouseInput::RightButton && move == MouseInput::LeftButton);
	return buttons ? Mouse::Both : Mouse::None;
}

/// The gap and the brackets that the Controls page writes a mouse input in: the gap leaves space for the picture
std::u16string MouseWords(std::u16string_view name)
{
	return u"     (" + std::u16string(name) + u")";
}

/// The Controls page's mice for a wheel mouse: the third row of data/textures/mousehelp.raw, the left button the
/// mirrored right one. The wheel turns over the fourth row.
constexpr int k_MouseRow = 2;
std::optional<std::pair<int, bool>> MouseCell(Mouse mouse)
{
	switch (mouse)
	{
	case Mouse::Left:
		return std::pair((k_MouseRow * 4) + 1, true);
	case Mouse::Right:
		return std::pair((k_MouseRow * 4) + 1, false);
	case Mouse::Both:
		return std::pair((k_MouseRow * 4) + 2, false);
	case Mouse::Middle:
		return std::pair((k_MouseRow * 4) + 3, false);
	case Mouse::WheelUp:
	case Mouse::WheelDown:
		return std::pair(11, false);
	default:
		return std::nullopt;
	}
}
} // namespace

GameMenu::GameMenu(const TextDatabase& texts, const GameFont& font, std::u16string_view playerName, MenuSettings settings)
    : _texts(texts)
    , _font(font)
    , _playerName(playerName)
    , _yes(texts.Get("HELP_TEXT_REQUESTER_BOXES_01"))
    , _no(texts.Get("HELP_TEXT_REQUESTER_BOXES_02"))
    , _yesKey(texts.Get("HELP_TEXT_PAUSE_STATS_145"))
    , _noKey(texts.Get("HELP_TEXT_PAUSE_STATS_144"))
    , _ok(texts.Get("HELP_TEXT_REQUESTER_BOXES_04"))
    , _back(texts.Get("HELP_TEXT_DIALOG_BACK"))
    , _restart(texts.Get("HELP_TEXT_FRONT_END_12"))
    , _restartQuestion(texts.Get("HELP_TEXT_DIALOG_RESTART_WARNING"))
    , _restartNotice(texts.Get("HELP_TEXT_DIALOG_RESTART"))
    , _deleteQuestion(texts.Get("HELP_TEXT_DIALOG_AERYOUSUREDELETE"))
    , _newGameQuestion(texts.Get("HELP_TEXT_DIALOG_ADDITION_20"))
    , _quitQuestion(texts.Get("HELP_TEXT_DIALOG_QUIT_QUESTION"))
    , _quitGameQuestion(texts.Get("HELP_TEXT_DIALOG_AREYOUSUREQUIT"))
    , _leaveSkirmishQuestion(texts.Get("HELP_TEXT_DIALOG_ADDITION_167"))
    , _videoChange(texts.Get("HELP_TEXT_DIALOG_VIDEOCHANGE"))
    , _settings(std::move(settings))
    , _exitOnlineLabel(texts.Get("HELP_TEXT_FRONT_END_03"))
    , _leaveSkirmishLabel(texts.Get("HELP_TEXT_DIALOG_ADDITION_170"))
    , _multiplayerTab(texts.Get("HELP_TEXT_MULTIPLAYER_MAIN_TAB"))
    , _mainMenuTab(texts.Get("HELP_TEXT_DIALOG_ADDITION_88"))
    , _worldRoomTab(texts.Get("HELP_TEXT_ROOM_WORLD_TITLE"))
{
	const auto read = [&texts](auto& labels, const auto& names) {
		for (size_t i = 0; i < labels.size(); ++i)
		{
			labels.at(i) = texts.Get(names.at(i));
		}
	};
	read(_detailLabels, k_DetailTexts);
	read(_helpLabels, k_HelpTexts);
	read(_storyLabels, k_StoryTexts);
	read(_toolTipLabels, k_ToolTipTexts);
	_settings.detailLevels = std::clamp(_settings.detailLevels, 1, static_cast<int>(_detailLabels.size()));
	_settings.detail = std::clamp(_settings.detail, 0, _settings.detailLevels - 1);
	_detailTold = _settings.detail;
	_settings.helpLevel = std::clamp(_settings.helpLevel, 0, static_cast<int>(_helpLabels.size()) - 1);
	_settings.storyText = std::clamp(_settings.storyText, 0, static_cast<int>(_storyLabels.size()) - 1);
	_settings.toolTips = std::clamp(_settings.toolTips, 0, static_cast<int>(_toolTipLabels.size()) - 1);
	if (_settings.creatureName.empty())
	{
		_settings.creatureName = _playerName;
	}

	BuildMain(texts);
	BuildOptions(texts);
	BuildPlayers(texts);
	BuildAdvanced(texts);
	BuildControls(texts);
	// The Game Statistics box's first tab and its Back arrow return to the main menu, as do those of the box of the
	// winning conditions
	_statistics = std::make_unique<StatisticsBox>(texts, font, [this] { ShowPage(Page::Main); });
	_conditions = std::make_unique<ConditionsBox>(texts, font, [this] { ShowPage(Page::Main); });
}

GameMenu::~GameMenu() = default;

DialogRect GameMenu::GetButtonRect(size_t index)
{
	const auto top = k_FirstButtonTop + (k_ButtonSpacing * static_cast<int>(index));
	return {.min = {k_ButtonLeft, top}, .max = {k_ButtonLeft + k_ButtonWidth, top + k_ButtonHeight}};
}

std::vector<Dialog::Tab> GameMenu::OptionsTabs(const TextDatabase& texts, Page selected)
{
	// The options' tabs: Main Menu, Options, Players, Advanced and Controls. Inside the temple the first is the World
	// Room, which goes back as Back does.
	(void)selected;
	return {
	    {.label = _insideTemple ? _worldRoomTab : _mainMenuTab, .onSelect = [this] { PickTab(Page::Main); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_OPTIONS")), .onSelect = [this] { PickTab(Page::Options); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_PLAYERLIST")), .onSelect = [this] { PickTab(Page::Players); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_FRONT_END_06")), .onSelect = [this] { PickTab(Page::Advanced); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_CONTROLOPTIONS")),
	     .onSelect = [this] { PickTab(Page::Controls); }},
	};
}

std::vector<Dialog::NamedControl> GameMenu::GetNamedControls() const
{
	return _question.has_value() ? _question->answers->GetNamedControls() : CurrentDialog().GetNamedControls();
}

void GameMenu::BuildMain(const TextDatabase& texts)
{
	// The main menu's tabs: the third, Multiplayer, only has a label in a network game (SetNetworkGame). A tab with no
	// label takes no click.
	std::vector<Dialog::Tab> tabs {
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_88")), .onSelect = {}},
	    {.label = std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_151")), .onSelect = [this] { OpenStatistics(); }},
	    {.label = {}, .onSelect = [this] { OpenConditions(); }},
	};
	auto& page = *(_pages[static_cast<size_t>(Page::Main)] = std::make_unique<Dialog>(std::move(tabs), 0));

	_welcome = texts.Get("HELP_TEXT_DIALOG_WELCOME");
	if (const auto found = _welcome.find(u"%s"); found != std::u16string::npos)
	{
		_welcome.replace(found, 2, _playerName);
	}
	page.Add<StaticText>(DialogRect {.min = {50, 65}, .max = {750, 155}}, _welcome, Layout::Wrapped);

	// "Leave Skirmish Game" (HELP_TEXT_DIALOG_ADDITION_170) during a skirmish. "Exit Online Game."
	// (HELP_TEXT_FRONT_END_03) during a network game (SetNetworkGame).
	const std::array<std::pair<const char*, std::function<void()>>, k_ButtonCount> buttons = {{
	    {"HELP_TEXT_DIALOG_CONTINUEGAME", [this] { Settle(Action::Continue); }},
	    {"HELP_TEXT_DIALOG_ADDITION_169",
	     [this] {
		     // In a skirmish the menu asks first. Out of a skirmish the game ends at once.
		     if (_skirmish)
		     {
			     Ask(_leaveSkirmishQuestion, Action::LeaveSkirmish);
			     return;
		     }
		     Settle(Action::StartSkirmish);
	     }},
	    {"HELP_TEXT_DIALOG_JOINGONLINE", [this] { Settle(_networkGame ? Action::ExitOnlineGame : Action::JoinOnline); }},
	    {"HELP_TEXT_DIALOG_OPTIONS", [this] { ShowPage(Page::Options); }},
	    {"HELP_TEXT_DIALOG_QUIT", [this] { Ask(_quitQuestion, Action::Quit); }},
	}};
	for (size_t i = 0; i < k_ButtonCount; ++i)
	{
		_buttonLabels.at(i) = texts.Get(buttons.at(i).first);
		_mainButtons.at(i) = &page.Add<Button>(GetButtonRect(i), _buttonLabels.at(i));
		_mainButtons.at(i)->onClick = buttons.at(i).second;
	}
}

void GameMenu::BuildOptions(const TextDatabase& texts)
{
	// The Options page
	auto& page = *(_pages[static_cast<size_t>(Page::Options)] =
	                   std::make_unique<Dialog>(OptionsTabs(texts, Page::Options), static_cast<size_t>(Page::Options)));
	page.Add<StaticText>(DialogRect {.min = {50, 60}, .max = {750, 100}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_SOUNDANDVIDEOCONTROL")), Layout::Wrapped);

	auto& sfx = page.Add<Slider>(DialogRect {.min = {250, 140}, .max = {550, 170}},
	                             std::u16string(texts.Get("HELP_TEXT_DIALOG_MASTERVOLUME")), _settings.sfxVolume);
	sfx.onChange = [this](float value) {
		_settings.sfxVolume = value;
		SettingsChanged();
	};
	auto& music = page.Add<Slider>(DialogRect {.min = {250, 200}, .max = {550, 230}},
	                               std::u16string(texts.Get("HELP_TEXT_DIALOG_MUSICVOLUME")), _settings.musicVolume);
	music.onChange = [this](float value) {
		_settings.musicVolume = value;
		SettingsChanged();
	};

	page.Add<StaticText>(DialogRect {.min = {50, 280}, .max = {750, 320}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_VIDEOCONTROL")), Layout::Centre);
	// The detail: the button and the right arrow step it on, the left arrow back
	// The game reads the detail once, when it starts: a change here takes effect the next time it starts. The player is
	// told so (HELP_TEXT_DIALOG_VIDEOCHANGE) as they leave the page by Back, a tab or Escape. The settings of this
	// computer keep the choice. The selector steps through as many levels as those settings say.
	auto& detail = page.Add<Button>(
	    DialogRect {.min = {k_SelectorLeft, 350}, .max = {k_SelectorLeft + k_SelectorWidth, 350 + k_SelectorHeight}},
	    _detailLabels.at(static_cast<size_t>(_settings.detail)), Mid());
	const auto stepDetail = [this, &detail](int step) {
		const auto count = _settings.detailLevels;
		_settings.detail = (_settings.detail + step + count) % count;
		detail.label = _detailLabels.at(static_cast<size_t>(_settings.detail));
		SettingsChanged();
	};
	detail.onClick = [stepDetail] { stepDetail(1); };
	page.Add<BigButton>(_font, glm::ivec2(k_SelectorLeftArrow, 349), k_ArrowSize, u"", LabelSide::Right, Look::LeftArrow)
	    .onClick = [stepDetail] { stepDetail(-1); };
	page.Add<BigButton>(_font, glm::ivec2(k_SelectorRightArrow, 349), k_ArrowSize, u"", LabelSide::Right, Look::RightArrow)
	    .onClick = [stepDetail] { stepDetail(1); };

	auto& autoSave = page.Add<CheckBox>(_font, glm::ivec2(285, k_CheckBoxTop),
	                                    std::u16string(texts.Get("HELP_TEXT_DIALOG_AUTOSAVE")), _settings.autoSave);
	autoSave.onChange = [this](bool checked) {
		_settings.autoSave = checked;
		SettingsChanged();
	};
	auto& pushScrolling =
	    page.Add<CheckBox>(_font, glm::ivec2(485, k_CheckBoxTop), std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_116")),
	                       _settings.pushScrolling);
	pushScrolling.onChange = [this](bool checked) {
		_settings.pushScrolling = checked;
		SettingsChanged();
	};

	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] {
		if (!TellDetailChange([this] { Back(); }))
		{
			Back();
		}
	};
	// Inside the temple there is no Quit arrow: the player leaves the options by Back or the World Room tab
	_optionsQuit = &page.Add<BigButton>(_font, glm::ivec2(730, 530), k_BackSize,
	                                    std::u16string(texts.Get("HELP_TEXT_DIALOG_QUIT")), LabelSide::Left, Look::RightArrow);
	// In a network game the arrow asks to leave the game, with the end box's question
	_optionsQuit->onClick = [this] {
		if (_networkGame)
		{
			Settle(Action::AskToLeave);
			return;
		}
		Ask(_quitGameQuestion, Action::Quit);
	};
}

void GameMenu::BuildPlayers(const TextDatabase& texts)
{
	// The Players page, which edits the player's profile
	auto& page = *(_pages[static_cast<size_t>(Page::Players)] =
	                   std::make_unique<Dialog>(OptionsTabs(texts, Page::Players), static_cast<size_t>(Page::Players)));
	page.Add<StaticText>(DialogRect {.min = {100, 60}, .max = {700, 120}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_SELECT_A_PLAYER")), Layout::Wrapped);

	// The profiles, the one played picked; picking another plays as it from now on
	auto& players = page.Add<List>(_font, DialogRect {.min = {200, 120}, .max = {600, 280}}, Mid(), true, false, true);
	players.SetItems({{.text = _playerName, .mouseCell = std::nullopt, .mouseMirrored = false}});
	players.SetSelection(0);
	players.onClick = [this, &players] {
		if (const auto picked = players.GetSelection(); picked.has_value())
		{
			PickPlayer(*picked);
		}
	};
	_players = &players;
	_playerNames = {_playerName};
	_currentPlayer = 0;
	_originalPlayer = 0;
	_originalName = _playerName;

	page.Add<StaticText>(DialogRect {.min = {170, 295}, .max = {360, 359}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_PLAYER")), Layout::Right);
	page.Add<StaticText>(DialogRect {.min = {440, 295}, .max = {630, 359}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_SYMBOL")), Layout::Left);
	auto& symbol = page.Add<SymbolPicture>(DialogRect {.min = {368, 295}, .max = {432, 359}}, _settings.symbol);
	_symbolPicture = &symbol;
	symbol.onChange = [this](int value) {
		_settings.symbol = value;
		SettingsChanged();
	};

	page.Add<StaticText>(DialogRect {.min = {0, 370}, .max = {190, 410}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_CREATURENAME")), Layout::Right, Mid());
	// The name box takes 29 characters
	auto& name = page.Add<EditBox>(_font, DialogRect {.min = {200, 370}, .max = {600, 410}}, _settings.creatureName, 29, Mid());
	_creatureNameBox = &name;
	name.onChange = [this](const std::u16string& text) {
		_settings.creatureName = text;
		SettingsChanged();
	};

	// The square buttons; UpdatePlayersPage spreads them out
	const std::array<std::pair<const char*, std::function<void()>>, 4> buttons = {{
	    {"HELP_TEXT_DIALOG_CREATENEWPLAYER",
	     [this] {
		     KeepPlayerEdits();
		     Settle(Action::CreatePlayer);
	     }},
	    {"HELP_TEXT_DIALOG_DELETEPLAYER",
	     [this] {
		     // Asks about the profile picked, by its name
		     if (!_currentPlayer.has_value())
		     {
			     return;
		     }
		     auto question = _deleteQuestion;
		     if (const auto found = question.find(u"%s"); found != std::u16string::npos)
		     {
			     question.replace(found, 2, _playerNames.at(*_currentPlayer));
		     }
		     Ask(std::move(question), Action::DeletePlayer);
	     }},
	    {"HELP_TEXT_DIALOG_EDITTATOO",
	     [this] {
		     KeepPlayerEdits();
		     Settle(Action::EditTattoo);
	     }},
	    {"HELP_TEXT_DIALOG_STARTGAME", [this] { Ask(_newGameQuestion, Action::StartNewGame); }},
	}};
	for (size_t i = 0; i < buttons.size(); ++i)
	{
		auto& button = page.Add<BigButton>(_font, glm::ivec2(0, k_CheckBoxTop), 25,
		                                   std::u16string(texts.Get(buttons.at(i).first)), LabelSide::Below, Look::Square);
		button.onClick = buttons.at(i).second;
		_playerButtons.at(i) = &button;
	}

	_playersBack = &page.Add<BigButton>(_font, k_BackPosition, k_BackSize, _back, LabelSide::Right, Look::LeftArrow);
	_playersBack->onClick = [this] { PlayersBack(); };
	UpdatePlayersPage();
}

void GameMenu::UpdatePlayersPage()
{
	// Back becomes Restart once leaving the page starts the game again, and Edit Tattoo hides then too
	const bool restart = IsRestartPending();
	_playersBack->label = restart ? _restart : _back;
	const bool tattoo = _tattooAvailable && !restart;
	_playerButtons.at(2)->visible = tattoo;
	// The square buttons spread out over the page, three or four of them
	constexpr std::array k_ThreeLefts = {188, 388, 0, 588};
	constexpr std::array k_FourLefts = {138, 305, 471, 638};
	for (size_t i = 0; i < _playerButtons.size(); ++i)
	{
		const auto left = tattoo ? k_FourLefts.at(i) : k_ThreeLefts.at(i);
		auto& rect = _playerButtons.at(i)->rect;
		rect = {.min = {left, rect.min.y}, .max = {left + rect.Width(), rect.max.y}};
	}
}

void GameMenu::SetPlayers(std::vector<std::u16string> names, std::optional<size_t> current)
{
	std::vector<List::Item> items;
	items.reserve(names.size());
	for (const auto& name : names)
	{
		items.push_back({.text = name, .mouseCell = std::nullopt, .mouseMirrored = false});
	}
	_players->SetItems(std::move(items));
	_playerNames = std::move(names);
	_currentPlayer = current.has_value() && *current < _playerNames.size() ? current : std::nullopt;
	_players->SetSelection(_currentPlayer);
	// The profile the game was started as may have gone: leaving the page then starts the game again
	const auto original = std::ranges::find(_playerNames, _originalName);
	_originalPlayer = original != _playerNames.end() && !_originalName.empty()
	                      ? std::optional(static_cast<size_t>(original - _playerNames.begin()))
	                      : std::nullopt;
	if (_page != Page::Players)
	{
		// Shown afresh, the page starts from the profile played
		_originalPlayer = _currentPlayer;
		_originalName = _currentPlayer.has_value() ? _playerNames.at(*_currentPlayer) : std::u16string {};
		_mustRestart = false;
	}
	UpdatePlayersPage();
}

void GameMenu::ShowPlayer(std::u16string_view creatureName, int symbol, const player_profiles::Settings& settings)
{
	_settings.creatureName = creatureName;
	_settings.symbol = symbol;
	_creatureNameBox->SetText(creatureName);
	_symbolPicture->SetSymbol(symbol);
	TakeProfileSettings(_settings, settings);
	// The other pages show the profile's options
	const auto page = _page;
	BuildOptions(_texts);
	BuildAdvanced(_texts);
	_optionsQuit->visible = !_insideTemple;
	_page = page;
}

void GameMenu::SetSoundInstalled(bool installed)
{
	_soundInstalled = installed;
	_readSpeed->visible = !installed;
}

void GameMenu::SetCovered(bool covered)
{
	if (_covered == covered)
	{
		return;
	}
	_covered = covered;
	CurrentDialog().Reset();
	if (_open)
	{
		_fade.SetDestination(covered ? 0.0f : 1.0f, covered ? k_FadeOutSeconds : k_FadeInSeconds);
	}
}

void GameMenu::SetTattooAvailable(bool available)
{
	_tattooAvailable = available;
	UpdatePlayersPage();
}

void GameMenu::PickPlayer(size_t index)
{
	if (index >= _playerNames.size())
	{
		return;
	}
	// The edits are kept with the profile played before another is
	KeepPlayerEdits();
	const bool changed = _currentPlayer != index;
	_currentPlayer = index;
	UpdatePlayersPage();
	if (changed)
	{
		_pickedPlayer = index;
		Settle(Action::PickPlayer);
	}
}

void GameMenu::PrepareDeletion()
{
	if (!_currentPlayer.has_value())
	{
		return;
	}
	_playerToDelete = _playerNames.at(*_currentPlayer);
	auto remaining = _playerNames;
	remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(*_currentPlayer));
	// Deleting the profile the game was started as forgets it: the first left is played, and leaving the page then
	// starts the game again. With none left the new player's box asks for one.
	if (_playerToDelete == _originalName)
	{
		_originalName.clear();
		_mustRestart = !remaining.empty();
	}
	const auto original = std::ranges::find(remaining, _originalName);
	if (original != remaining.end() && !_originalName.empty())
	{
		_pickedPlayer = static_cast<size_t>(original - remaining.begin());
	}
	else
	{
		_pickedPlayer = remaining.empty() ? std::nullopt : std::optional<size_t>(0);
	}
}

void GameMenu::KeepPlayerEdits()
{
	_playerEdits = PlayerEdits {.creatureName = _settings.creatureName, .symbol = _settings.symbol};
}

void GameMenu::PlayersBack()
{
	KeepPlayerEdits();
	if (_mustRestart)
	{
		Tell(_restartNotice, Action::Restart);
	}
	else if (IsRestartPending())
	{
		Ask(_restartQuestion, Action::Restart);
	}
	else
	{
		Back();
	}
}

void GameMenu::PickTab(Page page)
{
	if (_page == Page::Players && page != Page::Players && IsRestartPending())
	{
		KeepPlayerEdits();
		Ask(_restartQuestion, Action::Restart);
		return;
	}
	// Away from the Options page after a change of the detail, the player is told first; OK then picks the tab
	if (_page == Page::Options && page != Page::Options && TellDetailChange([this, page] { PickTab(page); }))
	{
		return;
	}
	if (page == Page::Main)
	{
		Back();
	}
	else
	{
		ShowPage(page);
	}
}

bool GameMenu::TellDetailChange(std::function<void()> then)
{
	if (_page != Page::Options || _settings.detail == _detailTold)
	{
		return false;
	}
	// The player is told once for each change
	_detailTold = _settings.detail;
	TellThen(_videoChange, std::move(then));
	return true;
}

const std::u16string& GameMenu::GetQuestionText() const
{
	static const std::u16string k_None;
	return _question.has_value() ? _question->text : k_None;
}

void GameMenu::BuildAdvanced(const TextDatabase& texts)
{
	// The Advanced page
	auto& page = *(_pages[static_cast<size_t>(Page::Advanced)] =
	                   std::make_unique<Dialog>(OptionsTabs(texts, Page::Advanced), static_cast<size_t>(Page::Advanced)));
	page.Add<StaticText>(DialogRect {.min = {50, 60}, .max = {750, 100}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_HELPCONTROLS")), Layout::Wrapped);

	// Three selectors, the button and the right arrow stepping them on and the left arrow back
	// TODO(raffclar): help, story text and tooltips don't change anything yet
	const auto addSelector = [this, &page](int top, int& value, auto& labels) {
		auto& button = page.Add<Button>(
		    DialogRect {.min = {k_SelectorLeft, top}, .max = {k_SelectorLeft + k_SelectorWidth, top + k_SelectorHeight}},
		    labels.at(static_cast<size_t>(value)), Mid());
		const auto step = [this, &button, &value, &labels](int by) {
			const auto count = static_cast<int>(labels.size());
			value = (value + by + count) % count;
			button.label = labels.at(static_cast<size_t>(value));
			SettingsChanged();
		};
		button.onClick = [step] { step(1); };
		page.Add<BigButton>(_font, glm::ivec2(k_SelectorLeftArrow, top - 1), k_ArrowSize, u"", LabelSide::Right,
		                    Look::LeftArrow)
		    .onClick = [step] { step(-1); };
		page.Add<BigButton>(_font, glm::ivec2(k_SelectorRightArrow, top - 1), k_ArrowSize, u"", LabelSide::Right,
		                    Look::RightArrow)
		    .onClick = [step] { step(1); };
	};
	addSelector(150, _settings.helpLevel, _helpLabels);
	addSelector(215, _settings.storyText, _storyLabels);
	addSelector(280, _settings.toolTips, _toolTipLabels);

	// The speed of the help's text, shown only when the game has no sound
	auto& readSpeed = page.Add<Slider>(DialogRect {.min = {250, 365}, .max = {550, 395}},
	                                   std::u16string(texts.Get("HELP_TEXT_DIALOG_HELPTEXTSPEED")), _settings.readSpeed);
	readSpeed.onChange = [this](float value) {
		_settings.readSpeed = value;
		SettingsChanged();
	};
	readSpeed.visible = !_soundInstalled;
	_readSpeed = &readSpeed;

	const std::array<std::tuple<int, const char*, bool*>, 3> boxes = {{
	    {385, "HELP_TEXT_DIALOG_CREATUREHELP", &_settings.creatureHelp},
	    {185, "HELP_TEXT_DIALOG_LEFTHANDED", &_settings.leftHandedHand},
	    {585, "HELP_TEXT_DIALOG_ADDITION_126", &_settings.textFromBottom},
	}};
	for (const auto& [x, label, setting] : boxes)
	{
		auto& box = page.Add<CheckBox>(_font, glm::ivec2(x, k_CheckBoxTop), std::u16string(texts.Get(label)), *setting);
		box.onChange = [this, setting](bool checked) {
			*setting = checked;
			SettingsChanged();
		};
	}

	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] { Back(); };
}

void GameMenu::BuildControls(const TextDatabase& texts)
{
	// The Controls page
	auto& page = *(_pages[static_cast<size_t>(Page::Controls)] =
	                   std::make_unique<Dialog>(OptionsTabs(texts, Page::Controls), static_cast<size_t>(Page::Controls)));
	page.Add<StaticText>(DialogRect {.min = {100, 60}, .max = {700, 160}},
	                     std::u16string(texts.Get("HELP_TEXT_DIALOG_CONTROLTITLE")), Layout::Wrapped);
	for (size_t i = 0; i < k_ActionTexts.size(); ++i)
	{
		_actionNames.at(i) = texts.Get(k_ActionTexts.at(i));
	}
	_keyReminder = texts.Get("HELP_TEXT_DIALOG_KEYREMINDER");
	_loadDefaults = texts.Get("HELP_TEXT_DIALOG_LOADDEFAULTS");
	_cancel = texts.Get("HELP_TEXT_REQUESTER_BOXES_03");

	// Halfway between the big and the mid text size
	_controls =
	    &page.Add<List>(_font, DialogRect {.min = {100, 120}, .max = {700, 440}}, (k_Big + Mid()) / 2, true, true, false);
	RefreshControls();

	// Load Defaults asks first, then puts every binding back
	page.Add<Button>(DialogRect {.min = {260, 530}, .max = {540, 570}}, _loadDefaults, Mid()).onClick = [this] {
		Confirm(_loadDefaults, [this] {
			_bindings = input::DefaultKeyBindings();
			RefreshControls();
			_controlsChanged = true;
		});
	};

	// Back keeps the bindings with the player's profile
	page.Add<BigButton>(_font, k_BackPosition, k_BackSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_BACK")), LabelSide::Right,
	                    Look::LeftArrow)
	    .onClick = [this] {
		_keepControls = true;
		Back();
	};
}

void GameMenu::RefreshControls()
{
	// Each binding reads "<action> : <key> <mouse>", the mouse's name after a gap of five spaces for its picture
	std::vector<List::Item> items;
	_wheelMice.clear();
	for (size_t i = 0; i < _bindings.size(); ++i)
	{
		const auto& binding = _bindings.at(i);
		auto text = _actionNames.at(i) + u" : " + input::rebinding::ChordName(binding.key) + u" ";
		if (const auto mouseName = input::rebinding::MouseTextName(binding.mouse); !mouseName.empty())
		{
			text += MouseWords(_texts.Get(mouseName));
		}
		const auto shown = ShownMouse(_bindings, i);
		const auto cell = MouseCell(shown);
		items.push_back({.text = std::move(text),
		                 .mouseCell = cell ? std::optional(cell->first) : std::nullopt,
		                 .mouseMirrored = cell && cell->second});
		if (shown == Mouse::WheelUp || shown == Mouse::WheelDown)
		{
			_wheelMice.emplace_back(i, shown == Mouse::WheelUp);
		}
	}
	// The list keeps the row picked and where it is scrolled to
	_controls->SetItems(std::move(items));
	// A mouse input taken for an earlier binding is forgotten
	_mouseChosen = false;
}

void GameMenu::SetControls(const input::KeyBindingTable& bindings)
{
	_bindings = bindings;
	RefreshControls();
}

void GameMenu::DoubleClick(glm::ivec2 point)
{
	// A double click on the row picked asks for its new key or mouse input
	if (!_open || _question || _page != Page::Controls || !_controls->rect.Contains(point))
	{
		return;
	}
	const auto row = _controls->ItemAt(point.y);
	if (!row.has_value() || row != _controls->GetSelection() || *row >= _bindings.size())
	{
		return;
	}
	_capture = Capture {.row = *row, .key = std::nullopt};
	Confirm(CaptureText(std::nullopt), [this] { KeepCapture(); });
}

std::u16string GameMenu::CaptureText(const std::optional<std::u16string>& input) const
{
	auto text = _keyReminder + u" " + (_capture ? _actionNames.at(_capture->row) : std::u16string {}) + u":";
	if (input.has_value())
	{
		text += u"\n" + *input;
	}
	return text;
}

void GameMenu::ShowCapture(const std::u16string& input)
{
	if (_question && _question->words != nullptr)
	{
		_question->words->text = CaptureText(input);
		_question->text = _question->words->text;
	}
}

bool GameMenu::CaptureKey(SDL_Scancode key, input::rebinding::HeldModifiers held)
{
	if (!_capture || !_question)
	{
		return false;
	}
	if (!input::rebinding::IsBindable(key))
	{
		return true;
	}
	_capture->key = input::rebinding::CapturedKey(key, held);
	_mouseChosen = false;
	ShowCapture(input::rebinding::ChordName(_capture->key));
	return true;
}

void GameMenu::PollCapture(bool left, bool right, bool middle, glm::ivec2 point)
{
	if (!_capture || !_question)
	{
		return;
	}
	// The buttons of the box take their own clicks
	if (std::ranges::any_of(_question->buttons,
	                        [point](const Control* button) { return button != nullptr && button->HitTest(point); }))
	{
		return;
	}
	const auto mouse = input::rebinding::CapturedMouse(left, right, middle, _captureWheel);
	if (!mouse.has_value() && !_mouseChosen)
	{
		return;
	}
	if (mouse.has_value())
	{
		// The wheel's steps are used only when no button gives the input
		if (!left && !right && !middle)
		{
			_captureWheel = 0;
		}
		_pendingMouse = *mouse;
		_mouseChosen = true;
	}
	ShowCapture(MouseWords(_texts.Get(input::rebinding::MouseTextName(_pendingMouse))));
}

void GameMenu::KeepCapture()
{
	if (!_capture)
	{
		return;
	}
	if (_mouseChosen)
	{
		input::rebinding::AssignMouse(_bindings, _capture->row, _pendingMouse);
	}
	else
	{
		// With no key pressed the action loses its key
		input::rebinding::AssignKey(_bindings, _capture->row, _capture->key);
	}
	_capture.reset();
	RefreshControls();
	_controlsChanged = true;
}

void GameMenu::Ask(std::u16string text, Action yes)
{
	// A question with a Yes and a No: the question in the middle of a 500 by 200 box
	auto answers = std::make_unique<Dialog>();
	auto& words = answers->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, text, Layout::Wrapped);
	auto& yesButton =
	    answers->Add<BigButton>(_font, glm::ivec2(200, 338), k_ArrowSize, _yes, LabelSide::Right, Look::LeftArrow);
	yesButton.onClick = [this, yes] { Settle(yes); };
	auto& noButton = answers->Add<BigButton>(_font, glm::ivec2(568, 338), k_ArrowSize, _no, LabelSide::Left, Look::RightArrow);
	noButton.onClick = [this] { Settle(Action::None); };
	_question = Question {.text = std::move(text),
	                      .answers = std::move(answers),
	                      .fade = Zoomer(),
	                      .words = &words,
	                      .buttons = {&yesButton, &noButton},
	                      .kind = dialog_keys::Buttons::YesNo};
	_question->fade.SetDestination(1.0f, k_FadeInSeconds);
	CurrentDialog().Reset();
}

void GameMenu::Tell(std::u16string text, Action ok)
{
	// A message with an OK button 150 by 32 under it, in the same box as a question
	auto answers = std::make_unique<Dialog>();
	auto& words = answers->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, text, Layout::Wrapped);
	auto& okButton = answers->Add<Button>(DialogRect {.min = {325, 338}, .max = {475, 370}}, _ok, Mid());
	okButton.onClick = [this, ok] { Settle(ok); };
	_question = Question {.text = std::move(text),
	                      .answers = std::move(answers),
	                      .fade = Zoomer(),
	                      .words = &words,
	                      .buttons = {&okButton, nullptr},
	                      .kind = dialog_keys::Buttons::Ok};
	_question->fade.SetDestination(1.0f, k_FadeInSeconds);
	CurrentDialog().Reset();
}

void GameMenu::TellThen(std::u16string text, std::function<void()> then)
{
	// A message with an OK button, as Tell shows it, which does what it is given
	auto answers = std::make_unique<Dialog>();
	auto& words = answers->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, text, Layout::Wrapped);
	auto& okButton = answers->Add<Button>(DialogRect {.min = {325, 338}, .max = {475, 370}}, _ok, Mid());
	okButton.onClick = [this, then = std::move(then)] {
		then();
		Settle(Action::None);
	};
	_question = Question {.text = std::move(text),
	                      .answers = std::move(answers),
	                      .fade = Zoomer(),
	                      .words = &words,
	                      .buttons = {&okButton, nullptr},
	                      .kind = dialog_keys::Buttons::Ok};
	_question->fade.SetDestination(1.0f, k_FadeInSeconds);
	CurrentDialog().Reset();
}

void GameMenu::Confirm(std::u16string text, std::function<void()> ok)
{
	// A question with a Cancel at the left and an OK at the right, in the same box as a question with a Yes and a No
	auto answers = std::make_unique<Dialog>();
	auto& words = answers->Add<StaticText>(DialogRect {.min = {200, 220}, .max = {600, 350}}, text, Layout::Wrapped);
	auto& cancelButton =
	    answers->Add<BigButton>(_font, glm::ivec2(200, 338), k_ArrowSize, _cancel, LabelSide::Right, Look::LeftArrow);
	cancelButton.onClick = [this] { Settle(Action::None); };
	auto& okButton = answers->Add<BigButton>(_font, glm::ivec2(568, 338), k_ArrowSize, _ok, LabelSide::Left, Look::RightArrow);
	okButton.onClick = [this, ok = std::move(ok)] {
		ok();
		Settle(Action::None);
	};
	_question = Question {.text = std::move(text),
	                      .answers = std::move(answers),
	                      .fade = Zoomer(),
	                      .words = &words,
	                      .buttons = {&cancelButton, &okButton},
	                      .kind = dialog_keys::Buttons::CancelOk};
	_question->fade.SetDestination(1.0f, k_FadeInSeconds);
	CurrentDialog().Reset();
}

void GameMenu::Settle(Action action)
{
	switch (action)
	{
	case Action::DeletePlayer:
		PrepareDeletion();
		[[fallthrough]];
	case Action::StartNewGame:
	case Action::Restart:
		KeepPlayerEdits();
		break;
	default:
		break;
	}
	_action = action;
	// The question goes once the click on its answer is over
	_answered = _question.has_value();
}

Dialog& GameMenu::CurrentDialog() const
{
	if (_page == Page::Statistics)
	{
		return _statistics->GetDialog();
	}
	return _page == Page::Conditions ? _conditions->GetDialog() : *_pages.at(static_cast<size_t>(_page));
}

void GameMenu::OpenConditions()
{
	if (!_networkGame)
	{
		return;
	}
	_conditions->Open(_conditionsSource ? _conditionsSource() : conditions_box::Figures {}, false);
	ShowPage(Page::Conditions);
}

void GameMenu::SetNetworkGame(bool networkGame)
{
	_networkGame = networkGame;
	auto& page = *_pages.at(static_cast<size_t>(Page::Main));
	page.SetTabLabel(2, networkGame ? _multiplayerTab : std::u16string());
	_mainButtons.at(2)->label = networkGame ? _exitOnlineLabel : _buttonLabels.at(2);
	// Start Skirmish Game does not show in a network game
	_mainButtons.at(1)->visible = !networkGame;
	LayOutMainButtons();
}

void GameMenu::SetSkirmish(bool skirmish)
{
	_skirmish = skirmish;
	_mainButtons.at(1)->label = skirmish ? _leaveSkirmishLabel : _buttonLabels.at(1);
	// Join Online Game does not show in a skirmish
	_mainButtons.at(2)->visible = !skirmish;
	LayOutMainButtons();
}

void GameMenu::LayOutMainButtons()
{
	const bool fewer = _networkGame || _skirmish;
	const auto height = fewer ? k_FewerButtonsHeight : k_ButtonHeight;
	const auto spacing = fewer ? k_FewerButtonsSpacing : k_ButtonSpacing;
	auto top = fewer ? k_FewerButtonsTop : k_FirstButtonTop;
	for (auto* button : _mainButtons)
	{
		button->rect = {.min = {k_ButtonLeft, top}, .max = {k_ButtonLeft + k_ButtonWidth, top + height}};
		if (button->visible)
		{
			top += spacing;
		}
	}
}

void GameMenu::OpenStatistics()
{
	auto [figures, local] =
	    _statisticsSource ? _statisticsSource() : std::pair<statistics::BoxFigures, PlayerNames> {{}, PlayerNames::PLAYER_ONE};
	_statistics->Open(std::move(figures), local, false);
	ShowPage(Page::Statistics);
}

void GameMenu::ShowPage(Page page)
{
	CurrentDialog().Reset();
	if (_page == Page::Players && page != Page::Players)
	{
		KeepPlayerEdits();
	}
	if (page == Page::Players && _page != Page::Players)
	{
		// Shown afresh, the page starts from the profile played
		_originalPlayer = _currentPlayer;
		_originalName = _currentPlayer.has_value() ? _playerNames.at(*_currentPlayer) : std::u16string {};
		_mustRestart = false;
		_previousProfile = _originalName;
		UpdatePlayersPage();
	}
	if (page == Page::Options && _page != Page::Options)
	{
		// Shown afresh, the page tells of a change of the detail made from now on
		_detailTold = _settings.detail;
	}
	if (page == Page::Controls && _page != Page::Controls)
	{
		// Shown afresh, the page picks its first row and shows the bindings as they are now
		_controls->SetSelection(0);
		RefreshControls();
	}
	_page = page;
	CurrentDialog().Reset();
}

void GameMenu::Back()
{
	// The options' pages are shown over the main menu, and going back leaves them for it. Inside the temple the options
	// room shows them on their own, so going back closes them and the room goes back to the temple's main hall.
	if (_insideTemple)
	{
		Close();
	}
	else
	{
		ShowPage(Page::Main);
	}
}

void GameMenu::SetInsideTemple(bool inside)
{
	if (_insideTemple == inside)
	{
		return;
	}
	_insideTemple = inside;
	_optionsQuit->visible = !inside;
	for (const auto page : {Page::Options, Page::Players, Page::Advanced, Page::Controls})
	{
		_pages.at(static_cast<size_t>(page))->SetTabLabel(0, inside ? _worldRoomTab : _mainMenuTab);
	}
}

void GameMenu::Open()
{
	if (_open)
	{
		return;
	}
	_open = true;
	_question.reset();
	_page = Page::Main;
	CurrentDialog().Reset();
	_fade.SetDestination(1.0f, k_FadeInSeconds);
}

void GameMenu::Close()
{
	if (_page == Page::Players)
	{
		KeepPlayerEdits();
	}
	_open = false;
	_covered = false;
	_question.reset();
	CurrentDialog().Reset();
	_fade.SetDestination(0.0f, k_FadeOutSeconds);
}

void GameMenu::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	_statistics->Update(deltaSeconds);
	// The box that asks for a new input has gone, by its Cancel, by Escape or with the menu
	if (!_question)
	{
		_capture.reset();
		_captureWheel = 0;
	}
	if (_question)
	{
		_question->fade.Update(deltaSeconds);
	}
	if (_open && !_covered)
	{
		CurrentDialog().Update(_question ? 0.0f : deltaSeconds);
		if (_question)
		{
			_question->answers->Update(deltaSeconds);
		}
	}

	// The wheel turns over a picture every tenth of a second, resting one in five
	_wheelTime += deltaSeconds;
	const auto frame = static_cast<int>(_wheelTime * 10.0f) % 5;
	for (const auto& [index, up] : _wheelMice)
	{
		// The wheel turns the four frames one way scrolling up and the other scrolling down, then rests
		auto cell = 11;
		if (frame < 4)
		{
			cell = up ? 12 + frame : 15 - frame;
		}
		_controls->SetMouseCell(index, cell);
	}
}

void GameMenu::MouseMove(glm::ivec2 point)
{
	if (_open)
	{
		(_question ? *_question->answers : CurrentDialog()).MouseMove(point);
	}
}

void GameMenu::MouseDown(glm::ivec2 point)
{
	if (_open)
	{
		(_question ? *_question->answers : CurrentDialog()).MouseDown(point);
	}
}

GameMenu::Action GameMenu::MouseUp(glm::ivec2 point)
{
	if (!_open)
	{
		return Action::None;
	}
	if ((_question ? *_question->answers : CurrentDialog()).MouseUp(point))
	{
		_clicked = true;
	}
	if (std::exchange(_answered, false))
	{
		_question.reset();
	}
	return std::exchange(_action, Action::None);
}

void GameMenu::Wheel(glm::ivec2 point, int steps)
{
	if (IsCapturing())
	{
		_captureWheel += steps;
		return;
	}
	if (_open && !_question)
	{
		CurrentDialog().Wheel(point, steps);
	}
}

GameMenu::Action GameMenu::TextInput(std::u16string_view text)
{
	if (!_open)
	{
		return Action::None;
	}
	if (_question)
	{
		if (text.empty() || _yesKey.empty() || _noKey.empty())
		{
			return Action::None;
		}
		return AnswerQuestion(dialog_keys::AnswerTyped(_question->kind, text.front(), _yesKey.front(), _noKey.front()));
	}
	CurrentDialog().TextInput(text);
	return Action::None;
}

GameMenu::Action GameMenu::KeyDown(int key, bool shift)
{
	if (!_open)
	{
		return Action::None;
	}
	const bool isReturn = key == SDLK_RETURN || key == SDLK_KP_ENTER;
	if (_question)
	{
		if (isReturn)
		{
			return AnswerQuestion(dialog_keys::AnswerKey(_question->kind, dialog_keys::Key::Return));
		}
		// Tab moves the focus over the question's buttons, but a button with the focus does not take Return
		_question->answers->KeyDown(key, shift);
		return Action::None;
	}
	if (_page == Page::Players && isReturn)
	{
		KeepPlayerEdits();
	}
	CurrentDialog().KeyDown(key, shift);
	return Action::None;
}

GameMenu::Action GameMenu::AnswerQuestion(dialog_keys::Answer answer)
{
	if (!_question)
	{
		return Action::None;
	}
	// The first button is Yes, OK alone or Cancel; the second is No or the OK beside a Cancel
	Control* button = nullptr;
	switch (answer)
	{
	case dialog_keys::Answer::Yes:
	case dialog_keys::Answer::Cancel:
		button = _question->buttons[0];
		break;
	case dialog_keys::Answer::No:
		button = _question->buttons[1];
		break;
	case dialog_keys::Answer::Ok:
		button = _question->kind == dialog_keys::Buttons::CancelOk ? _question->buttons[1] : _question->buttons[0];
		break;
	case dialog_keys::Answer::None:
		break;
	}
	if (button == nullptr)
	{
		return Action::None;
	}
	button->Activate(glm::ivec2(-1));
	if (std::exchange(_answered, false))
	{
		_question.reset();
	}
	return std::exchange(_action, Action::None);
}

GameMenu::Action GameMenu::Escape()
{
	if (!_open)
	{
		return Action::None;
	}
	if (_question)
	{
		return AnswerQuestion(dialog_keys::AnswerKey(_question->kind, dialog_keys::Key::Escape));
	}
	if (_page == Page::Players)
	{
		KeepPlayerEdits();
		// Leaving the page would start the game again: Escape does nothing
		if (IsRestartPending())
		{
			return Action::None;
		}
	}
	// Escape away from the Options page after a change of the detail tells the player first; OK then goes back
	if (TellDetailChange([this] { Back(); }))
	{
		return Action::None;
	}
	if (_page != Page::Main && !_insideTemple)
	{
		ShowPage(Page::Main);
		return Action::None;
	}
	// The first page closes on Escape only while none of its controls has the focus. The game gives Escape to the
	// control with the focus first, and that control does nothing with it.
	if (_page == Page::Main && !_insideTemple && CurrentDialog().GetFocus() != nullptr)
	{
		return Action::None;
	}
	// The first page, or inside the temple any page: there is nothing under the menu to go back to
	return Action::Continue;
}

void GameMenu::Draw(const DialogPainter& painter) const
{
	const auto fade = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	if (fade <= 0.0f)
	{
		return;
	}
	painter.SetAlpha(fade);
	CurrentDialog().Draw(painter, _open && !_question && !_covered);
	if (_question)
	{
		// The question: an opaque box over the menu, fading in
		painter.SetAlpha(fade * std::clamp(_question->fade.GetValue(), 0.0f, 1.0f));
		painter.DrawBackground(GetQuestionRect(), glm::vec3(1.0f), true, DialogPainter::All);
		_question->answers->Draw(painter, _open);
	}
	painter.SetAlpha(1.0f);
}
