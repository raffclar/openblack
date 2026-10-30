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

#include "Gui/MenuSettings.h"

namespace openblack::simulation
{

/// The settings that the game's menu starts with, and the name that it greets the player with
struct ProfileMenu
{
	std::u16string playerName;
	gui::MenuSettings settings;
};

/// The settings of the current profile, or of the game when there is no profile
[[nodiscard]] ProfileMenu ProfileMenuSettings();

} // namespace openblack::simulation
