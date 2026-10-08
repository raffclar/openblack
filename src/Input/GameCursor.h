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

#include <glm/vec2.hpp>

// The game's cursor in window pixels: the real mouse, or the hand demo's or a test's fixed position. The Game writes it
// every frame; the hand, the gestures, the help spirits and the input map read it. Kept in Locator::inputState.
namespace openblack::input
{

/// The cursor; (0, 0) when there is no input state (no game, unit tests)
[[nodiscard]] glm::ivec2 GameCursor();
/// The Game's cursor to write; the input state must exist
[[nodiscard]] glm::ivec2& GameCursorRef();

/// The buttons the hand reads this frame: the mouse's, or a hand demo's
struct HandButtons
{
	bool gripping {false};
	bool action {false};
};
/// The Game's hand buttons to write and read; the input state must exist
[[nodiscard]] HandButtons& HandButtonsRef();

/// OPENBLACK_MOUSE_AT, the test runs' fixed cursor as a fraction of the window ("0.5,0.6"): the environment's value,
/// read on every call, unless a test hook replaced it (OverrideMouseAt). Null when unset
[[nodiscard]] const char* MouseAt();
/// A test hook's new OPENBLACK_MOUSE_AT for the rest of the run; an empty value unsets it. The input state must exist
void OverrideMouseAt(std::string value);
/// MouseAt's rule: the override when there is one (empty: null), else the environment's value
[[nodiscard]] const char* MouseAtFrom(const std::optional<std::string>& override, const char* environment);

} // namespace openblack::input
