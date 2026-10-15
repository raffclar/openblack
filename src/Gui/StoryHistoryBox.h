/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"
#include "Help/StoryHistory.h"

namespace openblack::gui
{

class GameFont;
class TextDatabase;

/// The three buttons that turn the story history while the camera is close to its scroll in the library. They have
/// no box behind them:
/// - "Previous Line", a left arrow at the bottom left, shows the line before the first line.
/// - "Latest Line", a button at the bottom middle, shows the newest line first again.
/// - "Next Line", a right arrow at the bottom right, shows the line after the first line.
/// The buttons fade in over half a second and out over a fifth. They take the mouse only when it is over one of them.
class StoryHistoryBox
{
public:
	static constexpr glm::ivec2 k_PreviousPosition {60, 500};
	static constexpr glm::ivec2 k_NextPosition {700, 500};
	static constexpr int k_ArrowSize = 40;
	static constexpr DialogRect k_Latest {.min = {320, 500}, .max = {480, 540}};

	StoryHistoryBox(const TextDatabase& texts, const GameFont& font);
	StoryHistoryBox(const StoryHistoryBox&) = delete;
	StoryHistoryBox& operator=(const StoryHistoryBox&) = delete;

	/// Shows the buttons, or hides them. The buttons stay as they are when this does not change them.
	void SetShown(bool shown);
	/// Shown and taking input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	/// Shown, or fading out
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }
	/// Whether the buttons take the mouse: the pointer is over a button, or the mouse button is held on one
	[[nodiscard]] bool WantsMouse() const;

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a button acted, which clicks
	bool MouseUp(glm::ivec2 point);
	/// The turn of the last button that the player pressed, once
	[[nodiscard]] std::optional<help::StoryTurn> TakeTurn() { return std::exchange(_turn, std::nullopt); }
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// The buttons, for tools that press them by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	Dialog _dialog;
	std::optional<help::StoryTurn> _turn;
	bool _active {false};
	Zoomer _fade;
};

} // namespace openblack::gui
