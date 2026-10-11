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
#include <optional>
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
	/// "debug" for the debug windows, "game" for the game's own: its menu, the start-of-game question, the tattoo
	/// editor and the Creature Cave
	std::string kind;
	bool open {false};
	/// For the game's menu, the page it shows; for the game's dialogs, the controls that can be pressed by name
	std::string page;
	std::vector<std::string> buttons;
	/// For the start-of-game question, the answer picked now
	std::string selected;
};

/// The labels over the villagers' heads that the Show Villager Names (N) and Show Villager Details (S) keys toggle
struct OverlayInfo
{
	/// The named villagers' names are up, and every villager's details
	bool names {false};
	bool details {false};
	/// The names display's size, growing towards 1 while the names are up and shrinking to 0 once put down
	float nameFade {0.0f};
	/// The villagers whose details are shown on their own, toggled with S over them
	size_t detailsForOne {0};
	/// The labels given to the interface for the frame drawn last
	size_t labels {0};
	/// Whether the keys act now: not while a script has the cinema bars in and holds the interface, nor in the temple
	bool keysAct {false};
	/// What keeps every label off the screen now: "cinema bars", "temple"
	std::vector<std::string> hiddenBy;
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
	[[nodiscard]] virtual OverlayInfo Overlays() const = 0;
	/// Puts the villagers' names and every villager's details up or down, whatever the keys would do now; empty when
	/// done, else why not
	virtual std::string SetOverlays(std::optional<bool> names, std::optional<bool> details) = 0;
};

///   gui.windows                       the windows: the debug windows and the game's own, open or not
///   gui.open  {window}                opens one by its name
///   gui.close {window}                closes one
///   gui.press {window, button, path?} presses a button by its label, as a click on it would
///   gui.overlays                      the villagers' labels (N names, S details): up or not, and what hides them
///   gui.set_overlays {names?, details?} puts the names or the details up or down directly
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeGuiProvider(GuiTargetInterface& gui);

} // namespace openblack::inspector
