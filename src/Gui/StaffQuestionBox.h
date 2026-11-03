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
#include <utility>
#include <vector>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "ECS/GatheringBoxRules.h"

namespace openblack::gui
{

class Button;
class EditBox;
class GameFont;
class StaticText;
class TextDatabase;

/// The box that asks a question of the staff of the game (see gathering_box::question). It is a framed box of 700 by
/// 250 in the middle of the dialog space:
/// - The name of the sender and the question across its top.
/// - Under them, the line of the answer, only for a question that asks for a line.
/// - At the bottom, the first button at the right ("Yes." or "OK") and the No button at the left, only for a question
///   with a Yes and a No.
/// The line of the answer has the focus when the box opens, also when it does not show, and keeps its text from the
/// last question. A button takes the box away at once and gives the answer. The box fades in over half a second.
class StaffQuestionBox
{
public:
	static constexpr DialogRect k_Box {.min = {50, 175}, .max = {750, 425}};
	static constexpr DialogRect k_Question {.min = {50, 190}, .max = {700, 290}};
	static constexpr DialogRect k_Line {.min = {100, 285}, .max = {680, 335}};
	static constexpr DialogRect k_First {.min = {580, 350}, .max = {680, 400}};
	static constexpr DialogRect k_No {.min = {120, 350}, .max = {220, 400}};
	static constexpr int k_QuestionSize = 28;

	StaffQuestionBox(const TextDatabase& texts, const GameFont& font);
	StaffQuestionBox(const StaffQuestionBox&) = delete;
	StaffQuestionBox& operator=(const StaffQuestionBox&) = delete;
	~StaffQuestionBox();

	void Ask(gathering_box::question::Question question);
	[[nodiscard]] bool IsActive() const noexcept { return _question.has_value(); }
	[[nodiscard]] const std::optional<gathering_box::question::Question>& GetQuestion() const noexcept { return _question; }

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control was let go over: the game clicks
	bool MouseUp(glm::ivec2 point);
	void TextInput(std::u16string_view text);
	/// A key with no modifier key
	void KeyDown(int key, bool modifier);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// The question and its answer, once
	[[nodiscard]] std::optional<std::pair<gathering_box::question::Question, std::u16string>> TakeAnswer()
	{
		return std::exchange(_answer, std::nullopt);
	}

	/// The controls that show, for tools that press them by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	void Pushed(gathering_box::question::Push push);

	const TextDatabase& _texts;
	Dialog _dialog;
	StaticText* _text {nullptr};
	EditBox* _line {nullptr};
	Button* _first {nullptr};
	Button* _no {nullptr};
	Zoomer _fade;
	std::optional<gathering_box::question::Question> _question;
	std::optional<std::pair<gathering_box::question::Question, std::u16string>> _answer;
};

} // namespace openblack::gui
