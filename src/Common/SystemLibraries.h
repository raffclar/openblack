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
#include <string_view>

/// Controls the order in which the operating system closes some of its libraries when the game exits.
namespace openblack::system_libraries
{

/// The system libraries that the game loads before the renderer starts.
/// The system closes libraries in the opposite order to the order that it opened them. A library in this list thus
/// closes after a graphics driver that the renderer loads. Some graphics drivers ask the system for random numbers
/// when the process ends. If the library that makes these numbers closes first, the process crashes at exit.
constexpr std::array<std::wstring_view, 1> k_LoadFirst = {L"bcryptprimitives.dll"};

/// Loads each library in k_LoadFirst from the system folder and keeps it until the process ends.
/// Call it in main before the renderer starts. On systems other than Windows, it does nothing.
void LoadBeforeDrivers();

} // namespace openblack::system_libraries
