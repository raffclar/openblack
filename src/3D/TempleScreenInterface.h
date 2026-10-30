/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

namespace openblack
{
namespace graphics
{
class Texture2D;
}

namespace gui
{
class GameFont;
class ScreenFade;
class TextDatabase;
class ToolTips;
} // namespace gui

namespace help
{
enum class StoryTurn;
}

/// The parts of the game's screen that the temple uses: the texts and the font of its scrolls and signs, the fade, the
/// tooltips, the words on the screen, the game's menu, the boxes of the save game room and the buttons of the story
/// history. The game's interface is this screen. The dedicated server has no screen.
class TempleScreenInterface
{
public:
	/// Words at the middle of the screen, as the temple's future room shows them
	struct Message
	{
		std::u16string text;
		float alpha;
	};

	/// Lines of words centred at the top of the screen, in black and again in yellow just right of and below, as the
	/// temple's challenge room titles the picture it looks at closely
	struct Caption
	{
		std::vector<std::u16string> lines;
		/// From 0 to 255
		uint8_t alpha;
		/// Words that start a line's height below the top line, broken into lines across the middle half of the screen.
		/// The temple's save game room writes the name of a save here.
		std::u16string details;
	};

	/// The answer of the player to a box of the save game room
	enum class SaveAnswer : uint8_t
	{
		Yes,
		No,
		/// The player gave a name and pressed OK
		Named,
		Cancelled,
		/// The player took a message away
		Ok,
	};

	TempleScreenInterface() = default;
	TempleScreenInterface(const TempleScreenInterface&) = delete;
	TempleScreenInterface& operator=(const TempleScreenInterface&) = delete;
	TempleScreenInterface(TempleScreenInterface&&) = delete;
	TempleScreenInterface& operator=(TempleScreenInterface&&) = delete;
	virtual ~TempleScreenInterface() = default;

	[[nodiscard]] virtual const gui::TextDatabase& GetTexts() const noexcept = 0;
	[[nodiscard]] virtual const gui::GameFont& GetFont() const noexcept = 0;
	[[nodiscard]] virtual const graphics::Texture2D& GetFontTexture() const noexcept = 0;
	/// Whether the tattoo editor holds the left button of the mouse, or any button of the mouse
	[[nodiscard]] virtual bool IsTattooEditorLeftHeld() const noexcept = 0;
	[[nodiscard]] virtual bool IsTattooEditorButtonHeld() const noexcept = 0;
	/// The colour over the whole screen, which the temple fades in and out by
	[[nodiscard]] virtual gui::ScreenFade& GetScreenFade() noexcept = 0;
	/// What the hand shows for what it is over
	[[nodiscard]] virtual gui::ToolTips& GetToolTips() noexcept = 0;
	virtual void SetMessage(std::optional<Message> message) = 0;
	virtual void SetCaption(std::optional<Caption> caption) = 0;
	/// The name of a saved game that the tooltip of the save game room shows
	virtual void SetToolTipName(std::u16string name) = 0;
	/// Where the hand is on the screen, which the tooltip is put by; none when the hand is not on the screen
	virtual void SetHandOnScreen(std::optional<glm::vec2> position) = 0;

	[[nodiscard]] virtual bool IsMenuOpen() const noexcept = 0;
	/// Opens the game's menu at its options page
	virtual void OpenOptions() = 0;
	virtual void CloseMenu() = 0;
	/// Tells the menu whether the player is in the temple, which changes its first page
	virtual void SetMenuInsideTemple(bool inside) = 0;

	/// True while a box of the save game room is up
	[[nodiscard]] virtual bool IsSaveBoxActive() const noexcept = 0;
	/// Asks the player a question with the answers yes and no
	virtual void AskSaveQuestion(std::u16string question) = 0;
	/// Asks the player for the name of a save, which starts as the name given
	virtual void AskSaveName(std::u16string_view name) = 0;
	/// The last answer of the player, once
	[[nodiscard]] virtual std::optional<SaveAnswer> TakeSaveAnswer() = 0;
	/// The name that the player wrote in the box
	[[nodiscard]] virtual std::u16string_view GetSaveName() const = 0;

	/// Shows or hides the buttons of the story history
	virtual void SetStoryHistoryShown(bool shown) = 0;
	/// The last press on a button of the story history, once
	[[nodiscard]] virtual std::optional<help::StoryTurn> TakeStoryHistoryTurn() = 0;
};

} // namespace openblack
