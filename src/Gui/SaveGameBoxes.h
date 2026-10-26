/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::gui
{

class BigButton;
class EditBox;
class GameFont;
class StaticText;
class TextDatabase;

/// The boxes of the temple's save game room, over the room: a question with a Yes and a No, and the box that asks for
/// the name of a save.
///
/// The question stands in a white opaque box of 500 by 200 in the middle of the dialog space. Behind it is the framed
/// box of 400 by 300 that holds it, dimmed to a quarter. Both fade in over half a second. The Y key answers Yes and
/// the N key answers No. An answer takes the question away at once.
///
/// A message stands in the same white box in front of the same dimmed box, with one OK button under it. OK, Return or
/// Escape takes it away at once.
///
/// The name box is a framed box of 500 by 250: "Save Game Message:" across its top, the name's box under it with the
/// focus, then OK and Cancel. The name's box takes 32 characters. Return in the name's box gives the focus to OK, and
/// Return on OK saves. Escape is Cancel. The box fades in over half a second and out over a fifth.
class SaveGameBoxes
{
public:
	enum class Answer : uint8_t
	{
		Yes,
		No,
		/// The player gave a name and pressed OK
		Named,
		Cancelled,
		/// The player took a message away
		Ok,
	};

	static constexpr DialogRect k_HoldingBox {.min = {200, 150}, .max = {600, 450}};
	static constexpr DialogRect k_QuestionBox {.min = {150, 200}, .max = {650, 400}};
	static constexpr DialogRect k_Question {.min = {200, 220}, .max = {600, 350}};
	static constexpr glm::ivec2 k_YesPosition {200, 338};
	static constexpr glm::ivec2 k_NoPosition {568, 338};
	static constexpr int k_ArrowSize = 32;
	/// A message's OK button
	static constexpr DialogRect k_MessageOk {.min = {325, 338}, .max = {475, 370}};
	static constexpr DialogRect k_NameBox {.min = {150, 175}, .max = {650, 425}};
	static constexpr DialogRect k_NamePrompt {.min = {175, 200}, .max = {625, 250}};
	static constexpr DialogRect k_Name {.min = {175, 275}, .max = {625, 325}};
	static constexpr DialogRect k_Ok {.min = {250, 350}, .max = {350, 400}};
	static constexpr DialogRect k_Cancel {.min = {450, 350}, .max = {550, 400}};

	SaveGameBoxes(const TextDatabase& texts, const GameFont& font);
	SaveGameBoxes(const SaveGameBoxes&) = delete;
	SaveGameBoxes& operator=(const SaveGameBoxes&) = delete;
	~SaveGameBoxes();

	/// Asks a question with a Yes and a No
	void Ask(std::u16string question);
	/// Shows a message with an OK
	void Tell(std::u16string message);
	/// Asks for a name, the name's box holding the text given
	void AskName(std::u16string_view name);
	/// Takes a message with an OK away at once, with no answer, as when another box opens over the game
	void WithdrawMessage();
	/// A box is up and takes the input
	[[nodiscard]] bool IsActive() const noexcept { return _question.has_value() || _nameActive; }
	/// The question being asked or the message shown, empty while there is none
	[[nodiscard]] std::u16string_view GetQuestion() const noexcept;
	[[nodiscard]] bool IsAskingName() const noexcept { return _nameActive; }

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control was let go over: the game clicks
	bool MouseUp(glm::ivec2 point);
	void TextInput(std::u16string_view text);
	void KeyDown(int key);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// The player's answer, once
	[[nodiscard]] std::optional<Answer> TakeAnswer() { return std::exchange(_answer, std::nullopt); }
	/// The name in the name's box
	[[nodiscard]] std::u16string_view GetName() const;

	/// The controls that act, for tools pressing them by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	struct Question
	{
		std::u16string text;
		Dialog answers;
		StaticText* textControl {nullptr};
		/// A message with an OK, not a question
		bool message {false};
	};
	/// Puts up the question's or the message's box with its text, fading in
	Question& Show(std::u16string text);
	void Answered(Answer answer);
	void SubmitName();
	void CancelName();

	const TextDatabase& _texts;
	const GameFont& _font;
	std::optional<Question> _question;
	/// The box behind the question, and the question's own fade
	Zoomer _holdingFade;
	Zoomer _questionFade;
	Dialog _nameDialog;
	EditBox* _name {nullptr};
	Control* _ok {nullptr};
	bool _nameActive {false};
	Zoomer _nameFade;
	std::optional<Answer> _answer;
};

} // namespace openblack::gui
