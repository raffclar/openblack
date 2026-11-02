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
	return creature.owner != PlayerNames::NEUTRAL && creature.species != CreatureType::Ogre && !creature.frozen &&
	       !creature.scriptControlled && !creature.fainting;
}

bool creature_hand::MayHold(const Holdable& creature, PlayerNames handPlayer, bool allied)
{
	// The creature of an allied god takes hold as the player's own creature does
	return TakesHold(creature) && !creature.cannotBePickedUp && (creature.owner == handPlayer || allied);
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

bool creature_hand::LetsGoOfCreature(bool rightLetGo, bool rightButtonUp)
{
	return rightLetGo || rightButtonUp;
}

creature_hand::CreaturePress creature_hand::PressOn(bool playersOwnCreature, bool leashOn)
{
	return !playersOwnCreature && leashOn ? CreaturePress::Leash : CreaturePress::Hold;
}

creature_hand::PressStart creature_hand::PressStartOn(bool mayHold, bool leashHeldLoose)
{
	if (mayHold)
	{
		return leashHeldLoose ? PressStart::WaitForTap : PressStart::Hold;
	}
	return leashHeldLoose ? PressStart::TapAtOnce : PressStart::Nothing;
}

bool creature_hand::HoldsAfterWait(uint32_t elapsedMs)
{
	return elapsedMs > k_TapWaitMs;
}

bool creature_hand::TurnTakesHold(bool inWorld, bool heldByAHand)
{
	return inWorld && !heldByAHand;
}
