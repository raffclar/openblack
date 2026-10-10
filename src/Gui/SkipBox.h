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

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "Story/NewGameChoice.h"

namespace openblack::gui
{

class CheckBox;
class GameFont;
class TextDatabase;

/// The question a returning player is asked as a new game starts: start the game normally, skip straight to the
/// creature select, skip all of the creature tutorial or keep the old creature.
///
/// A 400 by 290 box in the middle of the dialog space: the question across its top, four radio buttons each with its
/// answer beside it, and an arrow button labelled OK that answers. The box fades in over half a second and, once
/// answered, out over a fifth, taking no input as it goes. It can't be escaped out of and no key answers it. The answer
/// picked stays picked the next time the box is shown.
class SkipBox
{
public:
	struct MouseUpResult
	{
		/// A control was let go over: it clicks
		bool clicked {false};
		std::optional<new_game_choice::Choice> answer;
	};

	/// The box's background
	static constexpr DialogRect k_Box {.min = {200, 155}, .max = {600, 445}};
	static constexpr DialogRect k_Question {.min = {150, 190}, .max = {650, 260}};
	static constexpr glm::ivec2 k_ButtonPosition {220, 400};
	static constexpr int k_ButtonSize = 32;
	/// The first radio button; each one after it is 44 lower
	static constexpr glm::ivec2 k_FirstRadio {250, 225};
	static constexpr int k_RadioSize = 24;
	static constexpr int k_RadioStep = 44;
	/// Each answer's text, beside its radio button
	static constexpr glm::ivec2 k_FirstAnswer {290, 225};
	static constexpr glm::ivec2 k_AnswerSize {300, 40};

	/// textSize is the dialogs' mid text size
	SkipBox(const TextDatabase& texts, const GameFont& font, int textSize);
	SkipBox(const SkipBox&) = delete;
	SkipBox& operator=(const SkipBox&) = delete;

	/// Brings the box up. A mouse button already held down doesn't count as a click.
	void Show();
	/// Up and taking input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	/// Up, or fading out
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }
	[[nodiscard]] new_game_choice::Choice GetSelection() const noexcept { return _selection; }

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	MouseUpResult MouseUp(glm::ivec2 point);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	[[nodiscard]] static DialogRect GetRadioRect(size_t index);

private:
	/// Ticks the radio button of the selected answer and no other
	void ShowSelection();

	Dialog _dialog;
	std::array<CheckBox*, new_game_choice::k_ChoiceCount> _radios {};
	new_game_choice::Choice _selection {new_game_choice::Choice::StartNormally};
	std::optional<new_game_choice::Choice> _answer;
	bool _active {false};
	Zoomer _fade;
};

} // namespace openblack::gui
