/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

namespace openblack::online
{

/// Opens a web page in the system's browser. In the browser build the page opens in a new tab.
void OpenPage(std::string_view url);

} // namespace openblack::online
