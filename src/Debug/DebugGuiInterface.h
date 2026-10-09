/*******************************************************************************
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
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Graphics/RenderPass.h"

struct SDL_Window;
struct SDL_Cursor;
union SDL_Event;

namespace openblack::graphics
{
class Renderer;
}

namespace openblack::debug::gui
{
/// The name of the window of the testbed's scenarios, which opens with the testbed
constexpr std::string_view k_TestbedScenariosWindow = "Testbed Scenarios";

class DebugGuiInterface
{
public:
	static std::unique_ptr<DebugGuiInterface> Create(graphics::RenderPass viewId) noexcept;

	virtual ~DebugGuiInterface() noexcept = default;
	[[nodiscard]] virtual bool StealsFocus() const noexcept = 0;
	/// The mouse is over a debug window, or held on one, as of the last frame of them
	[[nodiscard]] virtual bool IsMouseOverWindow() const noexcept = 0;
	virtual void SetScale(float scale) noexcept = 0;
	/// The main menu bar, which only shows while the game's own menu is open
	virtual void SetMenuBarVisible(bool visible) noexcept = 0;
	virtual bool ProcessEvents(const SDL_Event& event) noexcept = 0;
	/// Opens the debug window of the name, if there is one
	virtual void OpenWindow(std::string_view name) noexcept = 0;
	/// The debug windows by name, and whether each is open
	struct WindowState
	{
		std::string name;
		bool open;
	};
	[[nodiscard]] virtual std::vector<WindowState> ListWindows() const noexcept { return {}; }
	virtual void CloseWindow([[maybe_unused]] std::string_view name) noexcept {}
	/// A step of the way to a button in a window: a label or a number the window pushed, as its scopes are named
	using ButtonPathStep = std::variant<std::string, int32_t>;
	/// Presses a button of an open window by the labels and numbers leading to it, the button's label last, as a click
	/// on it would at the window's next frame. False when the window isn't open.
	virtual bool PressButton([[maybe_unused]] std::string_view window,
	                         [[maybe_unused]] std::span<const ButtonPathStep> path) noexcept
	{
		return false;
	}
	/// The player's input is locked out for an agent driving the game: the debug windows take nothing from the mouse, and
	/// a notice says so. Whether the game's own pointer stands in for the mouse, which keeps the mouse off them anyway
	virtual void SetInputLock([[maybe_unused]] bool locked, [[maybe_unused]] bool pointerScripted) noexcept {}
	virtual bool Loop() noexcept = 0;
	virtual void Draw() noexcept = 0;
};
} // namespace openblack::debug::gui
