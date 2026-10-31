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

/// Whether the computer's system says that the mouse has a wheel. Only Windows tells this; on the other systems it is
/// false, and only a turn of the wheel shows that the mouse has one.
[[nodiscard]] bool SystemSaysWheel();

} // namespace openblack::platform_mouse
