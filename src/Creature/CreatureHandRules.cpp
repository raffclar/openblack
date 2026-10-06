/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandRules.h"

using namespace openblack;

bool creature_hand::MayTouch(PlayerNames player, PlayerNames owner, bool guidesCreature)
{
	if (player == PlayerNames::NEUTRAL || owner == PlayerNames::NEUTRAL)
	{
		return false;
	}
	return owner == player || guidesCreature;
}

bool creature_hand::IsClick(float heldMs, bool strokedOrSlapped)
{
	return heldMs < k_ClickMaxMs && !strokedOrSlapped;
}
