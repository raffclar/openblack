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

namespace openblack::gui
{

/// The texts of the game's "Online Maps" folder that the multiplayer front end reads
struct OnlineMapTexts
{
	/// localmaps.txt: the lands that the player has
	std::string localMaps;
	/// ConditionTemplate.txt: the winning conditions of each land
	std::string conditionTemplate;
};

/// Reads the texts from the game's folder. A file that is not there gives an empty text.
[[nodiscard]] OnlineMapTexts ReadOnlineMapTexts();

} // namespace openblack::gui
