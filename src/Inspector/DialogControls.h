/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>

#include "Gui/Dialog.h"
#include "GuiControl.h"
#include "InputControl.h"

namespace openblack::gui
{
class SkipBox;
}

/// The controls of the game's own dialogs (drawn by its dialog painter, not the debug windows), as gui.press finds and
/// clicks them by name
namespace openblack::inspector
{

/// A control of one of the game's dialogs that can be pressed by name
struct DialogControl
{
	/// Its name on the screen, in plain letters
	std::string name;
	/// "button", "check box", "slider", "radio"...
	std::string kind;
	/// Its middle, in the dialog space
	glm::ivec2 middle {0};
	/// Other names it answers to
	std::vector<std::string> aliases;
};

/// The game's text in plain letters, its runs of spaces and line breaks made single spaces, with none at either end
[[nodiscard]] std::string PlainText(std::u16string_view text);

/// A dialog's named controls, as gui.press knows them; those labelled with spaces only are left out
[[nodiscard]] std::vector<DialogControl> DialogControls(std::span<const gui::Dialog::NamedControl> controls);

/// The start-of-game question's controls: its OK button, then each answer's radio button by its answer, which also
/// answers to the name the developers' start gives that answer ("normal", "creature", "story", "old")
[[nodiscard]] std::vector<DialogControl> SkipBoxControls(const gui::SkipBox& box);

/// The middle of the control a path names: its name or another of its names, in any case, with [n] before it for the
/// n-th of that name from 0. Why not, if no control answers
[[nodiscard]] std::variant<glm::ivec2, std::string> FindDialogControl(std::span<const DialogControl> controls,
                                                                      const std::vector<ButtonPathStep>& path);

/// A click as the player makes it: the pointer to a point of the screen, the left button pressed there and let go
[[nodiscard]] std::array<InputEvent, 3> ClickAt(glm::ivec2 screen);

} // namespace openblack::inspector
