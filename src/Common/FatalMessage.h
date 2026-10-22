/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

namespace openblack
{

/// Tell the player about an error that stops the game. The desktop build shows a message box of the system. The web
/// build writes the message to the browser console and shows it on the page, because the game thread of the web build
/// cannot open a message box.
void ShowFatalMessage(const std::string& title, const std::string& text);

} // namespace openblack
