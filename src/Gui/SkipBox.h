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
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/DeveloperTools.h"
#include "Common/Zoomer.h"
#include "Dialog.h"
#include "Story/NewGameChoice.h"

namespace openblack::gui
{

class CheckBox;
class GameFont;
class StaticText;
class TextDatabase;

namespace skip_box
{

/// The distance from one radio button to the next
inline constexpr int k_RadioStep = 44;

/// The places of the parts of the box
struct Layout
{
	/// The background of the box
	DialogRect box;
	DialogRect question;
	glm::ivec2 buttonPosition;
	/// The first radio button. Each radio button after it is k_RadioStep lower.
	glm::ivec2 firstRadio;
	/// The text of the first answer, at the right of its radio button
	glm::ivec2 firstAnswer;
};

/// The layout of the game for its four answers. A fifth answer makes the box one radio step taller. The box then moves
/// up half a step, so that it stays in the middle of the dialog space.
[[nodiscard]] constexpr Layout LayoutFor(bool fifthAnswer) noexcept
{
	const int grow = fifthAnswer ? k_RadioStep : 0;
	const int up = grow / 2;
	return {.box = {.min = {200, 155 - up}, .max = {600, 445 + up}},
	        .question = {.min = {150, 190 - up}, .max = {650, 260 - up}},
	        .buttonPosition = {220, 400 + up},
	        .firstRadio = {250, 225 - up},
	        .firstAnswer = {290, 225 - up}};
}

} // namespace skip_box

/// The question a player is asked as a new game starts: start the game normally, skip straight to the creature select,
/// skip all of the creature tutorial or keep the old creature. A fifth answer, not in the original game, is a debug
/// quickstart that skips the creature choice and starts with the Cow as the player's creature. Keeping the old creature is left
/// out, its radio button and its answer both hidden in their place, when the current profile has no creature.
///
/// A 400 by 290 box in the middle of the dialog space: the question across its top, four radio buttons each with its
/// answer beside it, and an arrow button labelled OK that answers. With the developer tools, the box is one radio step
/// taller for the fifth answer, and it stays in the middle. The box fades in over half a second and, once answered, out
/// over a fifth, taking no input as it goes. It can't be escaped out of and no key answers it. The answer picked stays
/// picked the next time the box is shown.
class SkipBox
{
public:
	struct MouseUpResult
	{
		/// A control was let go over: it clicks
		bool clicked {false};
		std::optional<new_game_choice::Choice> answer;
	};

	static constexpr int k_ButtonSize = 32;
	static constexpr int k_RadioSize = 24;
	static constexpr int k_RadioStep = skip_box::k_RadioStep;
	static constexpr glm::ivec2 k_AnswerSize {300, 40};

	/// Only a build with the developer tools has the fifth answer, the debug start with the Cow
	static constexpr skip_box::Layout k_Layout = skip_box::LayoutFor(developer_tools::k_Built);
	static constexpr DialogRect k_Box = k_Layout.box;
	static constexpr DialogRect k_Question = k_Layout.question;
	static constexpr glm::ivec2 k_ButtonPosition = k_Layout.buttonPosition;
	static constexpr glm::ivec2 k_FirstRadio = k_Layout.firstRadio;
	static constexpr glm::ivec2 k_FirstAnswer = k_Layout.firstAnswer;

	/// textSize is the dialogs' mid text size
	SkipBox(const TextDatabase& texts, const GameFont& font, int textSize);
	SkipBox(const SkipBox&) = delete;
	SkipBox& operator=(const SkipBox&) = delete;

	/// Brings the box up. A mouse button already held down doesn't count as a click. Keeping the old creature is offered
	/// only when the current profile has one; a selection it can't take moves to the one it stands for.
	void Show(bool currentProfileHasCreature);
	/// Closes the box with no answer, and the box fades out. Returns true when the box was open.
	bool Withdraw();
	/// Up and taking input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	/// Up, or fading out
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }
	[[nodiscard]] new_game_choice::Choice GetSelection() const noexcept { return _selection; }
	/// Whether a choice is shown and can be picked
	[[nodiscard]] bool IsOffered(new_game_choice::Choice choice) const noexcept;

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	MouseUpResult MouseUp(glm::ivec2 point);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	[[nodiscard]] static DialogRect GetRadioRect(size_t index);

	/// The controls that act, for tools pressing them by name: the OK button by its label, then each radio button by
	/// the answer beside it (kind "radio"), in the order of the answers
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	/// Ticks the radio button of the selected answer and no other
	void ShowSelection();

	Dialog _dialog;
	std::array<CheckBox*, new_game_choice::k_ChoiceCount> _radios {};
	/// The answers' texts, which name their radio buttons for tools
	std::array<std::u16string, new_game_choice::k_ChoiceCount> _answers;
	std::array<StaticText*, new_game_choice::k_ChoiceCount> _answerTexts {};
	new_game_choice::Choice _selection {new_game_choice::Choice::StartNormally};
	std::optional<new_game_choice::Choice> _answer;
	bool _active {false};
	Zoomer _fade;
};

} // namespace openblack::gui
