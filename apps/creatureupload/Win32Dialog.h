/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if defined(_WIN32)

#include <string>
#include <string_view>

#include <windows.h>

#include "CreatureUploadApp.h"

/// The parts of the dialog that the dialog and the look pictures share
namespace openblack::creature_upload::app::win32
{

[[nodiscard]] std::wstring Wide(std::string_view utf8);

/// Shows a change of the flow that is not a message box
void Apply(HWND dialog, const dialog::Effect& effect);

/// Sets the icon, the profiles and the texts, as the game's tool does when its dialog opens
void Prepare(HWND dialog, const Setup& setup, const dialog::Texts& texts, HICON icon);

/// The icon of the game's tool, from the file of the tool in the folder of the game. The module must stay loaded while
/// the icon shows.
[[nodiscard]] HICON GameToolIcon(const Setup& setup, HMODULE& module);

} // namespace openblack::creature_upload::app::win32

#endif
