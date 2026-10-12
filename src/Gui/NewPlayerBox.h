/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/PlayerProfiles.h"
#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::gui
{

class BigButton;
class EditBox;
class GameFont;
class StaticText;
class SymbolPicture;
class TextDatabase;

/// The box a new player types their name into, which makes their profile. The game shows it before the first land when
/// no player has a profile yet.
///
/// A 600 by 400 box in the middle of the dialog space: "Please type your name in the box below:" across its top, the
/// name's box under it, "Player" and "Symbol" either side of the player's symbol to pick, an arrow button labelled OK
/// at its bottom left and one labelled Cancel at its bottom right, hidden while no player has a profile. The name's box
/// has the focus as the box comes up, empty; it takes 29 characters. OK, or Return, makes the profile; a name already
/// taken or no name at all brings up a message in front, dimming the box, until its own OK, Return or Escape takes it
/// away (the name's box having lost the focus). Escape is Cancel when Cancel shows. The box fades in over half a second
/// and out over a fifth.
class NewPlayerBox
{
public:
	/// The name and symbol of the profile the player made
	struct Answer
	{
		std::u16string name;
		int symbol {0};
	};
	using CheckName = std::function<player_profiles::NameCheck(std::u16string_view name)>;

	static constexpr DialogRect k_Box {.min = {100, 100}, .max = {700, 500}};
	static constexpr DialogRect k_Question {.min = {100, 150}, .max = {700, 200}};
	static constexpr DialogRect k_Name {.min = {200, 230}, .max = {600, 280}};
	static constexpr DialogRect k_PlayerLabel {.min = {100, 300}, .max = {350, 364}};
	static constexpr DialogRect k_SymbolLabel {.min = {450, 300}, .max = {700, 364}};
	static constexpr DialogRect k_Symbol {.min = {368, 300}, .max = {432, 364}};
	static constexpr glm::ivec2 k_OkPosition {150, 400};
	static constexpr glm::ivec2 k_CancelPosition {610, 400};
	static constexpr int k_ButtonSize = 40;
	/// The message in front of the box: an opaque white box, the message and an OK button
	static constexpr DialogRect k_MessageBox {.min = {150, 200}, .max = {650, 400}};
	static constexpr DialogRect k_Message {.min = {200, 220}, .max = {600, 350}};
	static constexpr DialogRect k_MessageOk {.min = {325, 338}, .max = {475, 370}};

	NewPlayerBox(const TextDatabase& texts, const GameFont& font);
	NewPlayerBox(const NewPlayerBox&) = delete;
	NewPlayerBox& operator=(const NewPlayerBox&) = delete;
	~NewPlayerBox();

	/// Brings the box up, its name's box empty and holding the focus, the first symbol picked; Cancel shows only when a
	/// player has a profile to go back to
	void Show(bool canCancel);
	/// Up and taking input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	/// Up, or fading out
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }
	/// A message is up in front of the box
	[[nodiscard]] bool IsShowingMessage() const noexcept { return _message.has_value(); }

	/// Each of these is true when a control was let go over, or a key acted: the game clicks
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	bool MouseUp(glm::ivec2 point);
	void TextInput(std::u16string_view text);
	void KeyDown(int key);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// The name and symbol the player made their profile with, once
	[[nodiscard]] std::optional<Answer> TakeAnswer() { return std::exchange(_answer, std::nullopt); }
	/// Whether the player cancelled, once
	[[nodiscard]] bool TakeCancelled() { return std::exchange(_cancelled, false); }

	/// Asked whether a name may be taken as OK is pressed
	CheckName checkName;

	/// The controls that act, for tools pressing them by name: the message's OK while it is up, else the box's
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;
	[[nodiscard]] std::u16string_view GetName() const;

private:
	void Submit();
	void Cancel();
	void Hide();
	void ShowMessage(std::u16string text);
	void CloseMessage();

	const TextDatabase& _texts;
	const GameFont& _font;
	Dialog _dialog;
	EditBox* _name {nullptr};
	SymbolPicture* _symbol {nullptr};
	BigButton* _cancel {nullptr};
	/// The message in front, while it is up
	std::optional<Dialog> _message;
	StaticText* _messageText {nullptr};
	std::optional<Answer> _answer;
	bool _cancelled {false};
	bool _active {false};
	Zoomer _fade;
	Zoomer _messageFade;
};

} // namespace openblack::gui
