/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/fwd.hpp>
#include <imgui.h>

#include "DebugGuiInterface.h"
#include "DebugWindowMouse.h"
#include "Graphics/GraphicsHandle.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::debug::gui
{
class Window;

class Gui final: public DebugGuiInterface
{
public:
	Gui(ImGuiContext* imgui, uint16_t viewId, std::vector<std::unique_ptr<Window>>&& debugWindows, bool headless) noexcept;
	~Gui() noexcept override;

	[[nodiscard]] bool StealsFocus() const noexcept override;
	[[nodiscard]] bool IsMouseOverWindow() const noexcept override;
	void SetScale(float scale) noexcept override;
	void SetMenuBarVisible(bool visible) noexcept override { _menuBarVisible = visible; }
	bool ProcessEvents(const SDL_Event& event) noexcept override;
	void OpenWindow(std::string_view name) noexcept override;
	void SetInputLock(bool locked) noexcept override;
	void KeepOffTheMouse(bool off) noexcept override;
	void FollowDrivenPointer(bool follow) noexcept override;
	[[nodiscard]] std::vector<WindowState> ListWindows() const noexcept override;
	void CloseWindow(std::string_view name) noexcept override;
	[[nodiscard]] std::string WindowTakingEvent(const SDL_Event& event) const noexcept override;
	bool PressButton(std::string_view window, std::span<const ButtonPathStep> path) noexcept override;
	bool Loop() noexcept override;
	void Draw() noexcept override;
	void WarmUp() noexcept override;

private:
	/// Whether the debug windows are kept off the mouse, as while a scenario drives the hand
	[[nodiscard]] static bool MouseKeptOff() noexcept;
	/// Applies the mouse events that the debug windows take to Dear ImGui and to its backend
	void ApplyWindowMouse() noexcept;
	bool CreateFontsTextureBgfx() noexcept;
	/// Gives the texels of the font atlas to the graphics device again after a restore
	void GiveFontTexelsAgain() const;
	bool CreateDeviceObjectsBgfx() noexcept;
	void NewFrame() noexcept;
	void RenderDrawDataBgfx(ImDrawData* drawData) noexcept;

	void RenderArrow(const std::string& name, const ImVec2& pos, const ImVec2& size) const noexcept;
	std::optional<glm::uvec4> RenderVillagerName(const std::vector<glm::vec4>& coveredAreas, const std::string& name,
	                                             const std::string& text, const glm::vec4& color, const ImVec2& pos,
	                                             float arrowLength, std::function<void(void)> debugCallback) const noexcept;
	bool ShowMenu() noexcept;
	void ShowVillagerNames() noexcept;
	/// Each miracle dispenser's miracle over it
	void ShowDispenserNames() noexcept;
	void ShowCameraPositionOverlay() noexcept;
	/// While the player's input is locked out for an agent, a notice saying so
	void ShowInputLockNotice() const noexcept;

	ImGuiContext* _imgui;
	const bool _headless;
	ImVec2 _menuBarSize;
	uint64_t _time {0};
	graphics::ProgramHandle _program;
	graphics::ProgramHandle _imageProgram;
	graphics::TextureHandle _texture;
	/// Whether the GUI is one of the owners that give their data to the graphics device again
	bool _givesTexelsAgain {false};
	graphics::UniformHandle _s_tex;
	graphics::UniformHandle _u_imageLodEnabled;
	const uint16_t _viewId;
	std::vector<std::unique_ptr<Window>> _debugWindows;
	std::string _screenshotFilename = "screenshot.png";
	bool _stealsFocus = false;
	bool _menuBarVisible = true;
	/// The input of the player was locked last frame
	bool _inputLocked = false;
	/// A scenario drives the hand: the debug windows take no press of the mouse
	bool _mouseKeptOff = false;
	/// The inspector drives the pointer: the debug windows follow the driven pointer
	bool _followsDrivenPointer = false;
	/// The mouse events that the debug windows take now
	WindowMouse _windowMouse = WindowMouse::Player;
	/// A network game was live last frame: the debug windows and the menu do not show and take no input
	bool _networkDormant = false;
	/// Closes the editor when a network game starts, and tells the log when the debug windows stop or start again
	void FollowNetworkSession() noexcept;
};
} // namespace openblack::debug::gui
