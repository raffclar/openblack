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
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureStatusPanel.h"
#include "DialogPainter.h"
#include "GameFont.h"
#include "GameMenu.h"
#include "ScreenFade.h"
#include "SkipBox.h"
#include "TattooEditorDialog.h"
#include "TextDatabase.h"
#include "ToolTips.h"

union SDL_Event;

namespace openblack::graphics
{
class Texture2D;
class VideoOverlay;
} // namespace openblack::graphics

namespace openblack::help
{
enum class TextFont : uint8_t;
struct TextRegion;
namespace tip_bubble
{
struct Line;
}
struct TextRun;
} // namespace openblack::help

namespace openblack::gui
{

/// The game's own interface, drawn over the scene: the menu that Escape brings up and the start-of-game question.
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
	/// The open dialogs and, over them, the pointer at the mouse. The pointer also shows over the debug windows, which
	/// hide the hand, and is drawn over them.
	void Draw(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds, bool overDebugWindow);

	[[nodiscard]] GameMenu& GetMenu() noexcept { return *_menu; }

	/// Asks the returning player how to start the new game. The box takes the mouse and every key until answered.
	void ShowSkipBox();
	/// The answer to the start-of-game question, once
	std::optional<new_game_choice::Choice> TakeSkipBoxAnswer() { return std::exchange(_skipBoxAnswer, std::nullopt); }
	/// Whether a dialog is up taking the mouse: the menu, or the start-of-game question
	[[nodiscard]] bool IsDialogOpen() const noexcept { return _menu->IsOpen() || _skipBox->IsActive(); }

	/// The tattoo editor's dialog, shown while the tattoo editor is open
	[[nodiscard]] TattooEditorDialog& GetTattooEditor() noexcept { return *_tattooEditor; }
	/// Where a point of the menu's dialog space is on the screen, as last drawn
	[[nodiscard]] glm::ivec2 DialogToScreen(glm::ivec2 point) const { return _painter.ToScreenPoint(point); }
	[[nodiscard]] const TextDatabase& GetTexts() const noexcept { return _texts; }
	/// The dialogs' font, which the temple's scrolls are written in too
	[[nodiscard]] const GameFont& GetFont() const noexcept { return _font; }
	/// The font's glyphs, white with their coverage in alpha
	[[nodiscard]] const graphics::Texture2D& GetFontTexture() const noexcept { return *_fontTexture; }

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
	/// The colour over the whole screen, over the interface too, which the temple fades in and out by
	[[nodiscard]] ScreenFade& GetScreenFade() noexcept { return _screenFade; }
	/// Where on the screen the hand is, in pixels, which the tooltip is drawn by, or nowhere to show none
	void SetHandOnScreen(std::optional<glm::vec2> position) { _handOnScreen = position; }
	/// The creature's status panel shown this frame, or none. It shows its reward row when the values have a reward,
	/// and near the top of the screen without one, as while the camera follows a creature.
	void SetCreaturePanel(std::optional<creature_panel::Values> values) { _creaturePanel = values; }
	/// The fight's panel shown this frame, or none: each fighter's name over its health and stamina
	void SetFightPanel(std::optional<creature_fight_hud::Values> values) { _fightPanel = std::move(values); }

private:
	/// A font with its glyphs, white with their coverage in alpha
	struct FontFace
	{
		GameFont font;
		std::unique_ptr<graphics::Texture2D> texture;
	};

	GameInterface(TextDatabase texts, GameFont font, std::unique_ptr<graphics::Texture2D> atlas,
	              std::unique_ptr<graphics::Texture2D> fontTexture, std::unique_ptr<graphics::Texture2D> symbols,
	              std::unique_ptr<graphics::Texture2D> mice, std::unique_ptr<graphics::Texture2D> atmos,
	              std::u16string_view playerName, MenuSettings settings);

	/// Every mouse event and key press goes to the start-of-game question while it is up
	bool ProcessSkipBoxEvent(const SDL_Event& event);
	/// While the tattoo editor is open its dialog takes the mouse and keyboard
	bool ProcessTattooEditorEvent(const SDL_Event& event);
	/// Opens the tattoo editor's dialog as the editor opens, and moves it on
	void UpdateTattooEditor(float deltaSeconds);
	/// The tooltip by the hand, its words and then its mouse
	void DrawToolTip(glm::u16vec2 resolution);
	/// The creature's status panel, at the left of the screen
	void DrawCreaturePanel(glm::u16vec2 resolution);
	/// The fight's panel, at the top left of the screen
	void DrawFightPanel(glm::u16vec2 resolution);
	/// The full-screen video playing, between the interface and the scripts' fade
	void DrawVideo(glm::u16vec2 resolution);
	/// The tooltip's glow: a soft box of atmos.raw added round a rectangle
	void DrawGlow(glm::vec2 min, glm::vec2 max, glm::vec4 colour);
	/// The scripts' dialogue: its see-through box above the bottom cinema bar, then its words
	void DrawDialogue(glm::u16vec2 resolution, int barPixels);
	/// One word of the dialogue, cut to the box's top and bottom
	void DrawDialogueRun(const help::TextRun& run);
	/// The tip bubble over the "did you know" sign tapped last, with the pointer over it keeping it up
	void DrawTipBubble(glm::u16vec2 resolution, glm::ivec2 mouse);
	/// A run of the bubble's words' width as the bubble measures it
	[[nodiscard]] float BubbleTextWidth(std::u16string_view text, float size) const;
	/// A line of the bubble's words in a colour, fading from its top to its bottom
	void DrawBubbleLine(const help::tip_bubble::Line& line, glm::vec3 colour);
	/// The "Continue" cue at the right of the dialogue box, so far through its fade
	void DrawClickCue(glm::u16vec2 resolution, const help::TextRegion& box, float share);
	/// The font of the dialogue's words: the advisors each have their own, and j0 stands in for one that is missing
	[[nodiscard]] std::pair<const GameFont*, const graphics::Texture2D*> DialogueFont(help::TextFont font) const;

	TextDatabase _texts;
	GameFont _font;
	std::unique_ptr<graphics::Texture2D> _atlas;
	std::unique_ptr<graphics::Texture2D> _fontTexture;
	std::unique_ptr<graphics::Texture2D> _symbols;
	std::unique_ptr<graphics::Texture2D> _mice;
	/// The atmosphere texture of glows and arrows
	std::unique_ptr<graphics::Texture2D> _atmos;
	/// The tip bubble's texture of box corners and tails
	std::unique_ptr<graphics::Texture2D> _gatheringText;
	/// The good advisor's font f1 and the evil one's f3, which their words in the dialogue are in
	std::optional<FontFace> _goodAdvisorFont;
	std::optional<FontFace> _evilAdvisorFont;
	Canvas _canvas;
	Canvas _pointerCanvas {graphics::RenderPass::Cursor};
	DialogPainter _painter;
	std::unique_ptr<GameMenu> _menu;
	/// Made once and kept, so that the answer picked stays picked
	std::unique_ptr<SkipBox> _skipBox;
	std::optional<new_game_choice::Choice> _skipBoxAnswer;
	std::unique_ptr<TattooEditorDialog> _tattooEditor;
	/// Whether the left mouse button is down, as the tattoo editor's dialog has seen it
	bool _leftButtonDown {false};
	std::optional<Message> _message;
	ToolTips _toolTips;
	ScreenFade _screenFade;
	std::unique_ptr<graphics::VideoOverlay> _video;
	std::optional<glm::vec2> _handOnScreen;
	std::optional<creature_panel::Values> _creaturePanel;
	std::optional<creature_fight_hud::Values> _fightPanel;
	/// Whether the tooltip is left of the hand, which it moves to in the right third of the screen and from in the left
	bool _toolTipOnLeft {false};
	GameMenu::Action _action {GameMenu::Action::None};
};

} // namespace openblack::gui
