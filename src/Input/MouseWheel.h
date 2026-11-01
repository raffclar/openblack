/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// What the computer's system tells of the mouse
namespace openblack::platform_mouse
{

/// The kind of computer system that the game runs on, as far as the mouse wheel answer goes
enum class System
{
	Windows,
	Other, // Linux, macOS and the web
};

/// What the system part of the wheel answer is. Windows gives what its own query says. Every other system says that the
/// mouse has a wheel, because every mouse has one (user, 2026-11-01).
[[nodiscard]] constexpr bool SystemSaysWheelFor(System system, bool windowsQuerySaysWheel) noexcept
{
	return system == System::Windows ? windowsQuerySaysWheel : true;
}

/// Whether the system part of the wheel answer is yes, on the system that the game runs on.
[[nodiscard]] bool SystemSaysWheel();

} // namespace openblack::platform_mouse
