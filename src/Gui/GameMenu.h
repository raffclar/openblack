/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/PlayerProfiles.h"
#include "Common/Zoomer.h"
#include "Dialog.h"
#include "ECS/StatisticsRules.h"
#include "Input/KeyBindings.h"
#include "Input/KeyRebinding.h"

namespace openblack::gui
{

class GameFont;
class StatisticsBox;
class TextDatabase;

/// What the player sets in the game's menu
struct MenuSettings
{
	/// 0 to 1
	float sfxVolume {1.0f};
	float musicVolume {1.0f};
	/// HELP_TEXT_DIALOG_DETAIL_00 to _04, Minimum to Maximum Detail (graphics::detail_level)
	int detail {4};
	bool autoSave {true};
	bool pushScrolling {false};
	/// HELP_TEXT_DIALOG_NOHELP to ALLHELP
	int helpLevel {3};
	/// HELP_TEXT_DIALOG_NOTEXT to ALLTEXT
	int storyText {1};
	/// HELP_TEXT_DIALOG_NOTOOLTIPS to ALLTOOLTIPS
	int toolTips {2};
	bool leftHandedHand {false};
	bool creatureHelp {true};
	bool textFromBottom {false};
	/// How fast the player reads the texts, from 0 (slow) to 1 (fast); its slider shows only when the game has no sound
	float readSpeed {0.5f};
	std::u16string creatureName;
	/// One of the 16 player symbols
	int symbol {15};
};

/// The options of the menu as the player's profile keeps them
[[nodiscard]] player_profiles::Settings ToProfileSettings(const MenuSettings& settings);
/// The menu's options from the player's profile; the sound and the detail aren't kept with it
void TakeProfileSettings(MenuSettings& settings, const player_profiles::Settings& kept);

/// The menu Escape brings up during a game, and the options behind it.
///
/// The first page, the main menu, greets the player above five buttons: Continue Game, Start Skirmish Game, Join
/// Online Game, Options and Quit Black & White, with a Main Menu and a Statistics tab. Options opens the options'
/// pages, each a tab of the same box: Options for sound, detail, autosave and push scrolling, Players for the player's
/// profile, Advanced for help, the hand and text and Controls for the keys and buttons of the game's actions. Their
/// Main Menu tab, or Back, goes back to the first page (inside the temple, see SetInsideTemple).
///
/// The menu fades in over half a second and out over a fifth. Quitting first asks whether the player is sure, in a
/// smaller opaque box over the menu with a Yes and a No arrow.
class GameMenu
{
public:
	enum class Action
	{
		None,
		Continue,
		StartSkirmish,
		JoinOnline,
		/// Create New Player: the new player's box is to be shown
		CreatePlayer,
		/// The player picked another profile from the list (GetPickedPlayer), to be played from now on
		PickPlayer,
		/// The player said yes to deleting the profile picked (GetPlayerToDelete); the one to play next is
		/// GetPickedPlayer, none when no profile is left
		DeletePlayer,
		EditTattoo,
		/// The player said yes to starting a new game
		StartNewGame,
		/// The player said yes to the game starting again, as the profile now picked
		Restart,
		Quit,
	};

	enum class Page
	{
		Main,
		Options,
		Players,
		Advanced,
		Controls,
		/// The Game Statistics box, from the main menu's Statistics tab
		Statistics,
		_Count
	};

	/// The texts are read as the menu is made, the font measures them. playerName fills in the welcome and is the
	/// player's profile.
	GameMenu(const TextDatabase& texts, const GameFont& font, std::u16string_view playerName, MenuSettings settings);
	~GameMenu();

	/// Starts fading in on the first page, if it isn't open already
	void Open();
	/// Starts fading out, closing the question about quitting too
	void Close();
	/// Open and taking the player's input, not fading out
	[[nodiscard]] bool IsOpen() const noexcept { return _open; }
	/// On the screen, fading out included
	[[nodiscard]] bool IsVisible() const noexcept { return _open || _fade.GetValue() > 0.0f; }
	/// A question or a message is up over the menu
	[[nodiscard]] bool IsAskingToQuit() const noexcept { return _question.has_value(); }
	[[nodiscard]] Page GetPage() const noexcept { return _page; }
	/// Inside the temple the options are shown on their own, by the options room: their first tab is the World Room
	/// rather than the Main Menu, Back and that tab close them rather than go back to the main menu, Escape closes them
	/// too, and there is no Quit arrow
	void SetInsideTemple(bool inside);
	[[nodiscard]] bool IsInsideTemple() const noexcept { return _insideTemple; }
	void ShowPage(Page page);
	/// What the Game Statistics box shows as it opens: the figures now and the player at this machine
	using StatisticsSource = std::function<std::pair<statistics::BoxFigures, PlayerNames>()>;
	void SetStatisticsSource(StatisticsSource source) { _statisticsSource = std::move(source); }
	/// Opens the Game Statistics box with the figures as they are now
	void OpenStatistics();
	[[nodiscard]] const StatisticsBox& GetStatistics() const noexcept { return *_statistics; }

	void Update(float deltaSeconds);

	// Input at points of the dialog space
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// What the player chose by letting go of the button, Quit only once they have said yes
	Action MouseUp(glm::ivec2 point);
	void Wheel(glm::ivec2 point, int steps);
	void TextInput(std::u16string_view text);
	void KeyDown(int key);
	/// Escape backs out of the question, then out of the options, then continues the game. Inside the temple it
	/// continues from any page.
	Action Escape();

	/// Whether a control was clicked since last asked, which the game's dialogs answer with the menu button sound
	bool TakeClicked() noexcept { return std::exchange(_clicked, false); }
	/// Whether the settings changed since last asked
	bool TakeSettingsChanged() noexcept { return std::exchange(_settingsChanged, false); }
	[[nodiscard]] const MenuSettings& GetSettings() const noexcept { return _settings; }

	/// What the player gave on the Players page for the profile played: its creature's name and its symbol
	struct PlayerEdits
	{
		std::u16string creatureName;
		int symbol {0};
	};
	/// The Players page's edits, once they are to be kept: as the page is left, before what its buttons do, and on
	/// Return or Escape
	std::optional<PlayerEdits> TakePlayerEdits() noexcept { return std::exchange(_playerEdits, std::nullopt); }
	/// The profiles' names in the list's order and the one played. The first time the Players page is shown after
	/// being left, the one played is the one the game was started as: picking another makes Back the Restart arrow.
	void SetPlayers(std::vector<std::u16string> names, std::optional<size_t> current);
	/// The profile picked shows its creature's name, its symbol and its options
	void ShowPlayer(std::u16string_view creatureName, int symbol, const player_profiles::Settings& settings);
	/// Edit Tattoo shows only while the creature can have its tattoo edited
	void SetTattooAvailable(bool available);
	/// Another dialog over the menu, as the tattoo editor from the Players page: the menu fades out while it stays
	/// open, and fades back in as the dialog goes
	void SetCovered(bool covered);
	[[nodiscard]] bool IsCovered() const noexcept { return _covered; }
	[[nodiscard]] std::optional<size_t> GetPickedPlayer() const noexcept { return _pickedPlayer; }
	[[nodiscard]] const std::u16string& GetPlayerToDelete() const noexcept { return _playerToDelete; }
	/// The player has picked another profile or lost the one played, so leaving the page starts the game again
	[[nodiscard]] bool IsRestartPending() const noexcept { return _mustRestart || _currentPlayer != _originalPlayer; }
	[[nodiscard]] const List& GetPlayers() const noexcept { return *_players; }
	/// The profile played as the Players page last opened in this game. None when the page did not open. The save on
	/// exit goes to this profile when the player picked another one.
	[[nodiscard]] const std::optional<std::u16string>& GetPreviousProfile() const noexcept { return _previousProfile; }
	/// A game ends: the next game has no previous profile until the Players page opens again
	void ForgetPreviousProfile() noexcept { _previousProfile.reset(); }
	/// The Players page's arrow: Back, or Restart once leaving the page starts the game again
	[[nodiscard]] const std::u16string& GetPlayersBackLabel() const noexcept { return _playersBack->label; }
	/// Where the Players page's square buttons are: Create New Player, Delete Player, Edit Tattoo and Start a New Game
	[[nodiscard]] DialogRect GetPlayerButtonRect(size_t index) const { return _playerButtons.at(index)->rect; }
	[[nodiscard]] bool IsPlayerButtonVisible(size_t index) const { return _playerButtons.at(index)->visible; }
	[[nodiscard]] const std::u16string& GetQuestionText() const;

	void Draw(const DialogPainter& painter) const;

	// The layout of the first page, for hit tests
	static constexpr size_t k_ButtonCount = 5;
	[[nodiscard]] static DialogRect GetButtonRect(size_t index);
	[[nodiscard]] static constexpr DialogRect GetQuestionRect() { return {.min = {150, 200}, .max = {650, 400}}; }
	[[nodiscard]] const std::u16string& GetWelcome() const noexcept { return _welcome; }
	[[nodiscard]] const std::u16string& GetButtonLabel(size_t index) const { return _buttonLabels.at(index); }
	/// The lines of the Controls page
	[[nodiscard]] const List& GetControls() const noexcept { return *_controls; }
	/// The bindings that the Controls page shows and changes
	[[nodiscard]] const input::KeyBindingTable& GetControlBindings() const noexcept { return _bindings; }
	/// The Controls page shows these bindings, as the profile or the game has them
	void SetControls(const input::KeyBindingTable& bindings);
	/// Whether the player changed a binding since last asked: the game then uses the page's bindings
	bool TakeControlsChanged() noexcept { return std::exchange(_controlsChanged, false); }
	/// Whether the player left the Controls page by Back since last asked: the profile then keeps the bindings
	bool TakeControlsToKeep() noexcept { return std::exchange(_keepControls, false); }
	/// A double click of the left button. On the row picked of the Controls page, it opens the box that asks for the
	/// row's new key or mouse input.
	void DoubleClick(glm::ivec2 point);
	/// Whether the box that asks for a new key or mouse input is open
	[[nodiscard]] bool IsCapturing() const noexcept { return _capture.has_value() && _question.has_value(); }
	/// A key press while the box asks for a new key: the box shows the key that the press gives, and OK binds it. The
	/// game keeps some keys for itself, and the box ignores them. Whether the box took the press.
	bool CaptureKey(SDL_Scancode key, input::rebinding::HeldModifiers held);
	/// Once a frame while the box asks for a new input: the mouse buttons held, and the pointer. A button held, or the
	/// wheel turned, with the pointer off the box's buttons, gives the mouse input that OK binds.
	void PollCapture(bool left, bool right, bool middle, glm::ivec2 point);
	/// What the player can press by name now: the question's answers while it is asked, else the page's buttons, check
	/// boxes, sliders and tabs
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	struct Question
	{
		std::u16string text;
		std::unique_ptr<Dialog> answers;
		Zoomer fade;
		/// The text of the box, which can change while the box is open
		StaticText* words {nullptr};
		/// The box's buttons, which take their own clicks
		std::array<const Control*, 2> buttons {};
	};

	/// The binding that the box asks a new key or mouse input for: its row, and the key pressed
	struct Capture
	{
		size_t row {0};
		std::optional<input::KeyChord> key;
	};

	void BuildMain(const TextDatabase& texts);
	void BuildOptions(const TextDatabase& texts);
	void BuildPlayers(const TextDatabase& texts);
	void BuildAdvanced(const TextDatabase& texts);
	void BuildControls(const TextDatabase& texts);
	/// The Controls page's rows from the bindings
	void RefreshControls();
	/// The words of the box that asks for a new key or mouse input, with the input given so far
	[[nodiscard]] std::u16string CaptureText(const std::optional<std::u16string>& input) const;
	void ShowCapture(const std::u16string& input);
	/// OK in the box: the row takes the input given
	void KeepCapture();
	/// The tabs of the options' pages, with selected open
	std::vector<Dialog::Tab> OptionsTabs(const TextDatabase& texts, Page selected);
	/// A question with a Yes, which settles on the action, and a No
	void Ask(std::u16string text, Action yes);
	/// A message with an OK, which settles on the action
	void Tell(std::u16string text, Action ok);
	/// A question with a Cancel and an OK, which does what it is given
	void Confirm(std::u16string text, std::function<void()> ok);
	void Settle(Action action);
	/// Back from an options page, or their first tab
	void Back();
	/// A tab of the options was picked: from the Players page, while leaving it starts the game again, the player is
	/// asked first
	void PickTab(Page page);
	/// The Players page's arrow: Back, or the game starting again
	void PlayersBack();
	void PickPlayer(size_t index);
	/// Yes to deleting the profile picked: which is deleted and which is played after it
	void PrepareDeletion();
	/// The Players page's edits are to be kept
	void KeepPlayerEdits();
	/// Back or Restart, and Edit Tattoo shown or not, the square buttons spread out over the space
	void UpdatePlayersPage();
	void SettingsChanged() { _settingsChanged = true; }
	[[nodiscard]] Dialog& CurrentDialog() const;

	const TextDatabase& _texts;
	const GameFont& _font;
	std::u16string _playerName;
	std::u16string _welcome;
	std::array<std::u16string, k_ButtonCount> _buttonLabels;
	std::u16string _yes;
	std::u16string _no;
	std::u16string _ok;
	std::u16string _back;
	std::u16string _restart;
	/// HELP_TEXT_DIALOG_RESTART_WARNING, asked before the game starts again, and HELP_TEXT_DIALOG_RESTART, told when it
	/// must
	std::u16string _restartQuestion;
	std::u16string _restartNotice;
	/// HELP_TEXT_DIALOG_AERYOUSUREDELETE, with the player's name for its %s
	std::u16string _deleteQuestion;
	std::u16string _newGameQuestion;
	/// HELP_TEXT_DIALOG_QUIT_QUESTION, asked by the first page, and HELP_TEXT_DIALOG_AREYOUSUREQUIT by the options
	std::u16string _quitQuestion;
	std::u16string _quitGameQuestion;
	std::array<std::u16string, 5> _detailLabels;
	std::array<std::u16string, 5> _helpLabels;
	std::array<std::u16string, 3> _storyLabels;
	std::array<std::u16string, 4> _toolTipLabels;
	MenuSettings _settings;
	std::array<std::unique_ptr<Dialog>, static_cast<size_t>(Page::_Count)> _pages;
	/// The Game Statistics box, its own page's dialog
	std::unique_ptr<StatisticsBox> _statistics;
	StatisticsSource _statisticsSource;
	/// The options' first tab, outside the temple and inside it
	std::u16string _mainMenuTab;
	std::u16string _worldRoomTab;
	bool _insideTemple {false};
	/// The options page's Quit arrow, hidden inside the temple
	BigButton* _optionsQuit {nullptr};
	List* _controls {nullptr};
	input::KeyBindingTable _bindings {input::k_DefaultKeyBindings};
	std::array<std::u16string, input::k_KeyBindingCount> _actionNames;
	/// HELP_TEXT_DIALOG_KEYREMINDER, HELP_TEXT_DIALOG_LOADDEFAULTS and the Cancel of a box
	std::u16string _keyReminder;
	std::u16string _loadDefaults;
	std::u16string _cancel;
	std::optional<Capture> _capture;
	/// The last mouse input given in the box, and whether it came after the last key. They stay after a Cancel, so the
	/// next box starts with them, until the page shows its rows again.
	input::MouseInput _pendingMouse {input::MouseInput::None};
	bool _mouseChosen {false};
	/// Steps of the wheel turned while the box is open and not used yet
	int _captureWheel {0};
	bool _controlsChanged {false};
	bool _keepControls {false};
	List* _players {nullptr};
	EditBox* _creatureNameBox {nullptr};
	SymbolPicture* _symbolPicture {nullptr};
	BigButton* _playersBack {nullptr};
	std::array<BigButton*, 4> _playerButtons {};
	std::vector<std::u16string> _playerNames;
	std::optional<size_t> _currentPlayer;
	/// The profile the game was started as, by its name, and its place in the list
	std::u16string _originalName;
	std::optional<size_t> _originalPlayer;
	/// The profile the game was started as was deleted: leaving the page starts the game again
	bool _mustRestart {false};
	/// The profile played as the Players page last opened. A deletion does not change it.
	std::optional<std::u16string> _previousProfile;
	bool _tattooAvailable {false};
	bool _covered {false};
	std::optional<size_t> _pickedPlayer;
	std::u16string _playerToDelete;
	std::optional<PlayerEdits> _playerEdits;
	/// The wheel's mice on the Controls page turn over
	std::vector<std::pair<size_t, bool>> _wheelMice;
	float _wheelTime {0.0f};

	Page _page {Page::Main};
	bool _open {false};
	Zoomer _fade;
	std::optional<Question> _question;
	Action _action {Action::None};
	bool _clicked {false};
	bool _answered {false};
	bool _settingsChanged {false};
};

} // namespace openblack::gui
