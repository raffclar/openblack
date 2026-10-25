/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugGuiInterface.h"

namespace openblack::debug::gui
{

namespace
{

/// The debug GUI of a build without the developer tools. It has no windows and no menu, takes no input and draws
/// nothing.
class NoDebugGui final: public DebugGuiInterface
{
public:
	[[nodiscard]] bool StealsFocus() const noexcept override { return false; }
	[[nodiscard]] bool IsMouseOverWindow() const noexcept override { return false; }
	void SetScale([[maybe_unused]] float scale) noexcept override {}
	void SetMenuBarVisible([[maybe_unused]] bool visible) noexcept override {}
	bool ProcessEvents([[maybe_unused]] const SDL_Event& event) noexcept override { return false; }
	void OpenWindow([[maybe_unused]] std::string_view name) noexcept override {}
	bool Loop() noexcept override { return false; }
	void Draw() noexcept override {}
};

} // namespace

std::unique_ptr<DebugGuiInterface> DebugGuiInterface::Create([[maybe_unused]] graphics::RenderPass viewId) noexcept
{
	return std::make_unique<NoDebugGui>();
}

} // namespace openblack::debug::gui
