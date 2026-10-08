/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <glm/vec2.hpp>

#include "Canvas.h"
#include "DialogPainter.h"
#include "GameFont.h"
#include "GameMenu.h"
#include "SkipBox.h"
#include "TextDatabase.h"
#include "ToolTips.h"

union SDL_Event;

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

/// The game's own interface, drawn over the scene: for now the menu that Escape brings up.
///
/// It loads what Black & White's dialogs are made of: the texts of the info scripts, the font j0 and the front end
/// atlas, and takes the mouse and keyboard from the game while a dialog is open.
class GameInterface
{
public:
	/// Null when the game's files for it are missing
	static std::unique_ptr<GameInterface> Create(std::u16string_view playerName, MenuSettings settings);
	~GameInterface();

	/// Takes Escape, and every key press and mouse event while the menu is open. True when the event was taken.
	bool ProcessEvent(const SDL_Event& event, glm::u16vec2 resolution);
	/// What the player last chose in the menu, once
	GameMenu::Action TakeAction();
	/// Whether the menu's settings changed since last asked
	bool TakeSettingsChanged() { return _menu->TakeSettingsChanged(); }

	void Update(float deltaSeconds);
	/// The open dialogs and, over them, the pointer at the mouse
	void Draw(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds);

	[[nodiscard]] GameMenu& GetMenu() noexcept { return *_menu; }
	/// The skip tutorial box at the start of a game: shown until answered
	void ShowSkipBox();
	/// (test hook) the SkipBox answered as its OK would
	void AnswerSkipBox(int32_t answer)
	{
		if (IsSkipBoxShown())
		{
			_skipBox.reset();
			_skipAnswer = answer;
		}
	}
	[[nodiscard]] bool IsSkipBoxShown() const noexcept { return _skipBox.has_value() && _skipBox->IsVisible(); }
	/// The SkipBox's answer once given (its OK), then nothing
	std::optional<int32_t> TakeSkipAnswer() { return std::exchange(_skipAnswer, std::nullopt); }
	[[nodiscard]] const TextDatabase& GetTexts() const noexcept { return _texts; }
	/// The dialogs' font, which the temple's scrolls are written in too
	[[nodiscard]] const GameFont& GetFont() const noexcept { return _font; }
	/// The font's glyphs, their coverage in the texture's one channel
	[[nodiscard]] const graphics::Texture2D& GetFontTexture() const { return _font.GetTexture(); }

	/// Words shown over the screen, wrapped across the dialogs' 800 by 600 and 60 high from its top, as the temple's
	/// future room shows them
	struct Message
	{
		std::u16string text;
		float alpha;
	};
	void SetMessage(std::optional<Message> message) { _message = std::move(message); }

	/// What the hand shows for what it is over
	[[nodiscard]] ToolTips& GetToolTips() noexcept { return _toolTips; }
	/// Where on the screen the hand is, in pixels, which the temple's tooltip is shown by, or nowhere to show none
	void SetHandOnScreen(std::optional<glm::vec2> position) { _handOnScreen = position; }
	[[nodiscard]] const std::optional<glm::vec2>& GetHandOnScreen() const noexcept { return _handOnScreen; }

private:
	GameInterface(TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
	              std::unique_ptr<graphics::Texture2D> fontTexture, std::shared_ptr<graphics::Texture2D> symbols,
	              std::shared_ptr<graphics::Texture2D> mice, std::u16string_view playerName, MenuSettings settings);

	TextDatabase _texts;
	GameFont _font;
	std::shared_ptr<graphics::Texture2D> _atlas; ///< from the texture cache, as _symbols and _mice
	std::unique_ptr<graphics::Texture2D> _fontTexture;
	std::shared_ptr<graphics::Texture2D> _symbols;
	std::shared_ptr<graphics::Texture2D> _mice;
	Canvas _canvas;
	DialogPainter _painter;
	std::unique_ptr<GameMenu> _menu;
	GameMenu::Action _action {GameMenu::Action::None};
	std::optional<SkipBox> _skipBox;
	std::optional<int32_t> _skipAnswer;
	std::optional<Message> _message;
	ToolTips _toolTips;
	std::optional<glm::vec2> _handOnScreen;
};

} // namespace openblack::gui
