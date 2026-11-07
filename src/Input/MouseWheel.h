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

/// What the computer's system tells of the mouse
namespace openblack::platform_mouse
{

/// The kind of computer system that the game runs on, as far as the mouse wheel answer goes
enum class System
{
	Windows,
	Other, // Linux, macOS and the web
};

/// The number of mouse buttons that systems with no query give
constexpr uint32_t k_OtherSystemMouseButtons = 3;

/// What the system part of the wheel answer is. Windows gives what its own query says. Every other system says that the
/// mouse has a wheel, because every mouse has one (user, 2026-11-01).
[[nodiscard]] constexpr bool SystemSaysWheelFor(System system, bool windowsQuerySaysWheel) noexcept
{
	return system == System::Windows ? windowsQuerySaysWheel : true;
}

/// Whether the system part of the wheel answer is yes, on the system that the game runs on.
[[nodiscard]] bool SystemSaysWheel();

/// How many buttons the mouse has, as the system says. Windows gives what its own query says, 0 when it finds no mouse.
/// The other systems have no such query and give 3 (user, 2026-11-04).
[[nodiscard]] constexpr uint32_t SystemMouseButtonsFor(System system, int32_t windowsQueryButtons) noexcept
{
	if (system != System::Windows)
	{
		return k_OtherSystemMouseButtons;
	}
	return windowsQueryButtons > 0 ? static_cast<uint32_t>(windowsQueryButtons) : 0;
}

/// How many buttons the mouse has, on the system that the game runs on
[[nodiscard]] uint32_t SystemMouseButtons();

/// The rows of the mice picture, data/textures/mousehelp.raw: a mouse with two buttons, a mouse with more buttons, and a
/// mouse with a wheel
constexpr int k_TwoButtonMouseRow = 0;
constexpr int k_ThreeButtonMouseRow = 1;
constexpr int k_WheelMouseRow = 2;

/// The row of the mice picture for the player's mouse: the wheel mouse's row when the mouse has a wheel, else the row
/// with more buttons when it has more than two, else the two-button row
[[nodiscard]] constexpr int MousePictureRow(bool hasWheel, uint32_t buttons) noexcept
{
	if (hasWheel)
	{
		return k_WheelMouseRow;
	}
	return buttons > 2 ? k_ThreeButtonMouseRow : k_TwoButtonMouseRow;
}

/// The row of the mice picture for the mouse of the computer. The mouse has a wheel when the system says so, or when
/// the wheel turned since the game started.
[[nodiscard]] int PlayerMousePictureRow(bool wheelTurned);

} // namespace openblack::platform_mouse
