/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include "Common/PlayerProfiles.h"

namespace openblack::gui
{

/// What the player sets in the game's menu
struct MenuSettings
{
	/// 0 to 1
	float sfxVolume {1.0f};
	float musicVolume {1.0f};
	/// HELP_TEXT_DIALOG_DETAIL_00 to _06, Minimum to Super Detail (graphics::detail_level)
	int detail {4};
	/// How many detail levels the selector steps through, 1 to 7, as the settings of this computer say (5 by default)
	int detailLevels {5};
	bool autoSave {true};
	bool pushScrolling {false};
	/// HELP_TEXT_DIALOG_NOHELP to ALLHELP
	int helpLevel {3};
	/// HELP_TEXT_DIALOG_NOTEXT to ALLTEXT
	int storyText {1};
	/// HELP_TEXT_DIALOG_NOTOOLTIPS to ALLTOOLTIPS
	int toolTips {2};
	bool leftHandedHand {false};
	bool creatureHelp {true};
	bool textFromBottom {false};
	/// How fast the player reads the texts, from 0 (slow) to 1 (fast); its slider shows only when the game has no sound
	float readSpeed {0.5f};
	std::u16string creatureName;
	/// One of the 16 player symbols
	int symbol {15};
};

/// The options of the menu as the player's profile keeps them
[[nodiscard]] player_profiles::Settings ToProfileSettings(const MenuSettings& settings);
/// The menu's options from the player's profile; the sound and the detail aren't kept with it
void TakeProfileSettings(MenuSettings& settings, const player_profiles::Settings& kept);

} // namespace openblack::gui
