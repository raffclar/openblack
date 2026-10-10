/******************************************************************************
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
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <InspectorProvider.h>

namespace openblack::inspector
{

/// A window the inspector can open, close and press the buttons of
struct WindowInfo
{
	std::string name;
	/// "debug" for the debug windows, "game" for the game's own: its menu and the Creature Cave
	std::string kind;
	bool open {false};
	/// For the game's menu, the page it shows, and the buttons that can be pressed by name
	std::string page;
	std::vector<std::string> buttons;
};

/// A step of the way to a button inside a debug window: a label, or a number its window pushed (a row of a table)
using ButtonPathStep = std::variant<std::string, int32_t>;

/// What the inspector opens, closes and presses
class GuiTargetInterface
{
public:
	virtual ~GuiTargetInterface() = default;
	[[nodiscard]] virtual std::vector<WindowInfo> Windows() const = 0;
	/// Empty when it was done, else why not
	virtual std::string Open(std::string_view window) = 0;
	virtual std::string Close(std::string_view window) = 0;
	/// Presses a button of a window by its label, the steps before it leading into the scopes it sits in
	virtual std::string Press(std::string_view window, const std::vector<ButtonPathStep>& path) = 0;
};

///   gui.windows                       the windows: the debug windows and the game's own, open or not
///   gui.open  {window}                opens one by its name
///   gui.close {window}                closes one
///   gui.press {window, button, path?} presses a button by its label, as a click on it would
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeGuiProvider(GuiTargetInterface& gui);

} // namespace openblack::inspector
