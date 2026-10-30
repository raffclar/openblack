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

#include <optional>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

namespace openblack::gui
{
class TextDatabase;
}

namespace openblack::testbed_scenarios
{

/// The parts of the game that a scenario stages and that only the game on the player's computer has: the menu and the
/// boxes of the interface, the chat controls, the mouse, the logo effect and the requests to join, to continue and to
/// quit. The dedicated server has no stage. A function that stages a part gives the words of the log.
class GameStageInterface
{
public:
	GameStageInterface() = default;
	GameStageInterface(const GameStageInterface&) = delete;
	GameStageInterface& operator=(const GameStageInterface&) = delete;
	GameStageInterface(GameStageInterface&&) = delete;
	GameStageInterface& operator=(GameStageInterface&&) = delete;
	virtual ~GameStageInterface() = default;

	/// True when the game has its interface
	[[nodiscard]] virtual bool HasInterface() const = 0;
	/// The texts of the interface, or null with no interface
	[[nodiscard]] virtual const gui::TextDatabase* GetTexts() const = 0;

	/// The game's menu opens on its Players page, and the tattoo editor over it
	[[nodiscard]] virtual std::string OpenTattooMenu() = 0;
	/// The tattoo editor works as in a network game, or as in a game alone
	[[nodiscard]] virtual std::string TattooEditorAsNetworkGame(bool asNetworkGame) = 0;
	/// The game's menu opens on its Controls page and double clicks a row, as the player does to bind it again
	[[nodiscard]] virtual std::string OpenControlsBox(size_t row) = 0;
	/// The chat of this computer opens with a line in it
	virtual void OpenChat(std::u16string_view line) = 0;
	/// The multiplayer front end opens over the temple's future room with the demonstration lobby, on its way to a stage
	[[nodiscard]] virtual std::string OpenMultiplayerBox(size_t stage) = 0;
	/// The game's menu opens on a page. With no sound, the menu shows what a game with no sound shows.
	[[nodiscard]] virtual std::string OpenMenuPage(size_t page, bool noSound) = 0;
	/// A key pressed in the game's menu, as the keyboard gives it
	[[nodiscard]] virtual std::string PressMenuKey(size_t key, bool shift) = 0;
	/// The main menu of a skirmish opens. With askLeave, a click on Leave Skirmish Game follows.
	[[nodiscard]] virtual std::string OpenSkirmishMenu(bool askLeave) = 0;
	/// The skirmish box opens over the game. With selectSecond, the box selects its second land.
	[[nodiscard]] virtual std::string OpenSkirmishBox(bool selectSecond) = 0;
	/// The main menu of a network game, the question to leave, or a tab of the end box (see the EndBox command)
	[[nodiscard]] virtual std::string OpenEndBox(size_t value) = 0;
	/// The controls of the custom music: the buttons from 1, the play list from 2
	[[nodiscard]] virtual std::string ShowCustomMusicControls(size_t value) = 0;

	/// Makes the sheet of the players' symbols, with the symbol of the profile when one is given
	virtual void MakePlayersSymbols(std::optional<uint32_t> symbol) = 0;
	/// The round trip that the ping line shows in place of the round trip of the network game, or none to stop
	virtual void SetDemoRoundTrip(std::optional<uint64_t> microseconds) = 0;
	/// Puts the mouse at a pixel of the window
	virtual void SetMousePosition(glm::ivec2 position) = 0;
	/// Asks the game to join the online game. False with no game.
	virtual bool RequestJoinOnline() = 0;
	virtual void PlayLogoEffect() = 0;
	/// Continues from the save that the game made at its exit. False when it starts a new game.
	[[nodiscard]] virtual bool ContinueFromExitSave() = 0;
	virtual void RequestQuit() = 0;
};

} // namespace openblack::testbed_scenarios
