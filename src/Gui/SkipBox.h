/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Draws through raffclar's DialogPainter (adopted, Begin fixed to the original's dialog-space adjustment) with our
// graphics::GameFont.

#include <cstdint>

#include <array>
#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"

namespace openblack::gui
{
class DialogPainter;

/// The tutorial requester: a 400 x 290 box made by the front end, shown when the game asks about skipping the
/// tutorial (OPENBLACK_TEST_SKIP_ANSWER answers it in test runs).
class SkipBox
{
public:
	/// The answer, turned into three game flags (bits 23 / 24 / 25): 0 none, 1 bit 23, 2 bits 23 and 24, 3 all three
	using Answer = int32_t;

	/// The texts are read now (patch 4..8 and the main text "OK"), the selection is 0 and the fade is started
	explicit SkipBox(float textSize);

	/// Advances the box's two Zoomers (fade to 1 and scale to 0, in half a second)
	void Update(float seconds);
	/// The pointer over the box (dialog space), for the hover of the check boxes and of the button
	void MouseMove(glm::ivec2 point);
	/// A click on the control under `point`: a check box 60..63 selects id % 10; the button 11 returns the answer and
	/// hides the box (the caller unpauses the game)
	std::optional<Answer> Click(glm::ivec2 point);
	[[nodiscard]] bool IsVisible() const noexcept { return _visible; }
	[[nodiscard]] Answer GetSelection() const noexcept { return _selection; }

	void Draw(const DialogPainter& painter) const;

	/// The box's layout (dialog space, 800 x 600)
	struct Rect
	{
		glm::ivec2 min;
		glm::ivec2 size;
		[[nodiscard]] bool Contains(glm::ivec2 p) const noexcept
		{
			return p.x >= min.x && p.y >= min.y && p.x < min.x + size.x && p.y < min.y + size.y;
		}
	};
	/// Centred on (400, 300) with the size (400, 290): (inferred: to confirm in the box's draw) the box spans
	/// (200, 155)..(600, 445)
	static constexpr Rect k_Box {{200, 155}, {400, 290}};
	/// The question text, centred
	static constexpr Rect k_Question {{150, 190}, {500, 70}};
	/// The OK button, 32 x 32, label on the right
	static constexpr glm::ivec2 k_ButtonAt {220, 400};
	static constexpr int k_ButtonId = 11;
	/// The check boxes 60 + i at (250, 225 + 44 i), 24 x 24
	static constexpr int k_CheckX = 250;
	static constexpr int k_CheckY = 225;
	static constexpr int k_CheckStep = 44;
	static constexpr int k_CheckSize = 24;
	/// The check boxes' labels (patch[5 + i]) at (290, 225 + 44 i), 300 x 40, left-justified
	static constexpr int k_LabelX = 290;
	static constexpr glm::ivec2 k_LabelSize {300, 40};

private:
	std::u16string _question;              ///< patch[4] "Please choose from one of the following options:"
	std::u16string _ok;                    ///< HELP_TEXT_REQUESTER_BOXES_04
	std::array<std::u16string, 4> _labels; ///< patch[5..8]
	float _textSize;                       ///< The mid text size (22, one more when bigger text is needed)
	Answer _selection {0};
	std::optional<int> _hover; ///< (pending) the original's hover and focus rules
	bool _visible {true};
	Zoomer _fade;  ///< (inferred) the alpha
	Zoomer _scale; ///< To 0: the closing fade (alpha - 0.75 x it)
};

} // namespace openblack::gui
