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

bool creature_hand::TakesHold(const Holdable& creature)
{
	// The game also keeps the hand off a creature part way into one particular action of its own; that action isn't
	// known here
	return creature.owner != PlayerNames::NEUTRAL && creature.species != CreatureType::Ogre && !creature.asleep &&
	       !creature.frozen && !creature.scriptControlled;
}

bool creature_hand::MayHold(const Holdable& creature, PlayerNames handPlayer)
{
	// An allied god's creature would be held too; allies aren't modelled
	return TakesHold(creature) && creature.owner == handPlayer;
}

bool creature_hand::ShowsInteractTip(PlayerNames player, const Holdable& creature, bool held, bool handInInfluence)
{
	if (player == PlayerNames::NEUTRAL || creature.owner == PlayerNames::NEUTRAL || creature.scriptControlled)
	{
		return false;
	}
	// Within the influence a creature that would take hold offers it unless a hand already holds it
	if (handInInfluence && TakesHold(creature))
	{
		return !held;
	}
	return creature.owner == player || handInInfluence;
}

creature_hand::InteractTip creature_hand::InteractTipOf(PlayerNames player, const Holdable& creature, bool held,
                                                        bool handInInfluence)
{
	if (!ShowsInteractTip(player, creature, held, handInInfluence))
	{
		return InteractTip::None;
	}
	return handInInfluence && TakesHold(creature) ? InteractTip::WithAction : InteractTip::WordsOnly;
}

bool creature_hand::IsClick(float heldMs, bool strokedOrSlapped)
{
	return heldMs < k_ClickMaxMs && !strokedOrSlapped;
}

creature_hand::CreaturePress creature_hand::PressOn(bool playersOwnCreature, bool leashOn)
{
	return !playersOwnCreature && leashOn ? CreaturePress::Leash : CreaturePress::Hold;
}
