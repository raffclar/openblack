/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The worship icons as the gesture selection sees them (Magic/Gestures/PowerUpSystem.h IconProvider): the local
// player's (the interface's) icons through Worship/PlayerSpellIcons.h.

namespace openblack::worship::gesture_icons
{
/// Registers the provider with the gesture system (gestures::SetIconProvider); once per land
void Register();
} // namespace openblack::worship::gesture_icons
