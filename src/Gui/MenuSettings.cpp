/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MenuSettings.h"

openblack::player_profiles::Settings openblack::gui::ToProfileSettings(const MenuSettings& settings)
{
	return {.toolTips = settings.toolTips,
	        .storyText = settings.storyText,
	        .helpLevel = settings.helpLevel,
	        .creatureHelp = settings.creatureHelp,
	        .readSpeed = settings.readSpeed,
	        .textFromBottom = settings.textFromBottom,
	        .leftHanded = settings.leftHandedHand,
	        .pushScrolling = settings.pushScrolling,
	        .autoSave = settings.autoSave};
}

void openblack::gui::TakeProfileSettings(MenuSettings& settings, const openblack::player_profiles::Settings& kept)
{
	settings.toolTips = kept.toolTips;
	settings.storyText = kept.storyText;
	settings.helpLevel = kept.helpLevel;
	settings.creatureHelp = kept.creatureHelp;
	settings.readSpeed = kept.readSpeed;
	settings.textFromBottom = kept.textFromBottom;
	settings.leftHandedHand = kept.leftHanded;
	settings.pushScrolling = kept.pushScrolling;
	settings.autoSave = kept.autoSave;
}
