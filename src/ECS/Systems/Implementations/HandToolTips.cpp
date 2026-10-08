/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's tooltips: the interface's hand state and the tooltip function of each state, which submit to
// help::tooltips once per turn. Wiki: docs/bw1-notes/hand-and-interface.md, "Tooltips".

#define LOCATOR_IMPLEMENTATIONS

#include "Creature/CreatureHandRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/FishShoals.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownBelief.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
// BINDABLE_ACTION 1 (LoadDefaults: the left button) and 2 (the right button, the action button)
constexpr int32_t k_ActionLeft = 1;
constexpr int32_t k_ActionRight = 2;
// the align bits of the icons' arrows: 0x300 up and down, 0xF00 all four
constexpr uint32_t k_ArrowsUpDown = 0x300;
constexpr uint32_t k_ArrowsAll = 0xF00;
} // namespace

int32_t HandSystem::InterfaceHandState() const noexcept
{
	// an interface that is not active: 25; inside the citadel: 30; else the row of the action state openblack's hand
	// is in
	if (!interface_active::IsActive())
	{
		return 25;
	}
	if (game_clock::IsInsideCitadel())
	{
		return 30;
	}
	if (_screenGrip)
	{
		return 0x1D; // action state 17 BUBBLE GRIP: row 17's hand state, Drag Bubble
	}
	if (_gripPoint)
	{
		// action state 2 LANDSCAPE LOCK, whatever the hand holds: 20 Grip Landscape. (not ported) 22 Zoom Landscape with
		// both buttons
		return 20;
	}
	if (IsHoldingSeed())
	{
		return 5; // something in the hand, a spell seed: Has Magic
	}
	if (_releaseArmed)
	{
		return 23; // action state 12 IN THROW: row 23, In Throw
	}
	if (_creatureLock)
	{
		// the locked select on a creature: 19 Creature Interaction while held, 18 while it starts and ends. (pending) a
		// field of the creature that gives 14 instead
		return _creatureLockState.phase == hand_creature::LockPhase::Held ? 0x13 : 18;
	}
	if (_pickSource && _held)
	{
		return 14; // action state 4 IN LOCKED SELECT with an object (no creature): Select Lock
	}
	if (_held)
	{
		return 24; // something in the hand: Object In Hand
	}
	// a tug is action state 13 WAIT FOR PLUCK with nothing placed in the hand yet: the object branch. (inferred) the
	// tree stays the object under the hand during it. Action state 2 LANDSCAPE LOCK: (not ported) its 20 / 22 need the
	// grab and action bits; the fallback is the object branch, which with nothing under the hand is 3.
	// The object under the hand:
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> object = _tug ? _tug : _hovered;
	if (!object && _cursorObject && ecs::IsAvailable(*_cursorObject) &&
	    (hand_tap::Find(*_cursorObject) != nullptr || registry.AllOf<Creature>(*_cursorObject)))
	{
		object = _cursorObject; // a tap-only object (an abode) or a creature is the interface's object too
	}
	if (!object || !ecs::IsAvailable(*object))
	{
		// nothing under the hand but a screen object: 28 Over Bubble. (not ported) its extra help call
		return _screenObject ? 0x1C : 3;
	}
	if (registry.AllOf<Creature>(*object))
	{
		// a creature is neither a pile nor a field and never goes into the hand: 18, in or out of the influence
		return 18;
	}
	// (not ported) the leash 32 and the bubble 28; not interactable 3: an abode's interactable test (Abode, Field,
	// StoragePit) needs its percent built, not read. Out of the influence when interaction needs it: 18
	if (!InInfluence())
	{
		return 18;
	}
	// valid for a locked select: a pile that is not the hand's own type, a field with growth and food: 13.
	// (approximate) 3 on the frame the grab-land button is pressed: the tooltips run once a turn here
	const auto source = PotInfoOf(*object);
	const bool pile = source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood;
	const auto* field = registry.TryGet<const Field>(*object);
	if (pile || (field != nullptr && field->growth > 0.0f && field->food > 1.0f))
	{
		return 13;
	}
	// valid to place in the hand and not cannot-be-picked-up: 9
	if (ValidForPlaceInHand(*object) && !ecs::object_flags::IsCannotBePickedUp(*object))
	{
		return 9;
	}
	return 18;
}

void HandSystem::SubmitLandToolTips() const noexcept
{
	// (not ported) the leash first; the camera's tricons 0xE76 "Tilt" and 0xE77 "Rotate", set from the mouse near the
	// screen's centre: openblack has no tricons.
	// over the water in the influence, a shown fish within 2 units: 0xE73 "Pick Up"
	if (_interactionPoint && !IsLand(*_interactionPoint) && InInfluence() && ecs::FindFishFarmAt(*_interactionPoint))
	{
		help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		return;
	}
	// (not ported) 0xE8B "Zoom Out" with the screen's centre on the land, the heading distance < 15 and the pitch > 0.55
	help::tooltips::Submit(0xE7E, k_ActionLeft, k_ArrowsAll, false); // "Move"
}

void HandSystem::SubmitToolTips() noexcept
{
	// the row of the hand state in the tooltip table, as InterfaceHandState left it this turn
	const auto& registry = Locator::entitiesRegistry::value();
	const int32_t state = _interfaceHandState;
	const auto object = [&]() -> entt::entity {
		if (_hovered && ecs::IsAvailable(*_hovered))
		{
			return *_hovered;
		}
		return _cursorObject && ecs::IsAvailable(*_cursorObject) ? *_cursorObject : entt::null;
	}();
	switch (state)
	{
	case 3:  // Normal
	case 20: // Grip Landscape (its row's align 0xF00 is overwritten by the land tooltips)
		SubmitLandToolTips();
		break;
	case 5: // Has Magic. (pending) the seed's apply-to-object / to-map tests (0xE81 "Cast", 0xEF1)
		if (!InInfluence())
		{
			SubmitLandToolTips();
		}
		break;
	case 9: // Can Pick up
	{
		// (not ported) an interface flag (the land tooltips instead)
		if (physics::PhysicsObjects::IsFlying(object))
		{
			// a flying object (in physics, PhysicsObjects::IsFlying, not a resting proxy; and altitude >= 0): 0xE80
			// "Catch"
			help::tooltips::Submit(0xE80, k_ActionRight, 0, false);
			break;
		}
		// cannot be picked up: nothing. (not ported) the leash
		if (ecs::object_flags::IsCannotBePickedUp(object))
		{
			break;
		}
		// a one-shot orb: only its own pick-up tooltip, forced, and nothing else. (pending) its magic effect info's text
		if (registry.AllOf<OneOffSpellSeed>(object))
		{
			break;
		}
		// the other ported classes do not override the pick-up tooltip: 0xE73 "Pick Up"
		help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		// a second submit when it can also be tapped: its own tap tooltip or 0xE7A "Tap"; a rock gives 0xEF7 "Tap to
		// Break"
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (hand_tap::ValidToTap(object, is))
		{
			help::tooltips::Submit(Rocks::IsRock(object) ? 0xEF7 : 0xE7A, k_ActionRight, 0, false);
		}
		break;
	}
	case 13: // Can Select Lock
	{
		// (not ported) the leash first; a town centre or a totem statue: 0xECC / 0xEFC (the believers, the population)
		// locked by someone: the land tooltips; openblack locks only the hand's own source
		if (_pickSource && *_pickSource == object)
		{
			SubmitLandToolTips();
			break;
		}
		// not locked: its own interact tooltip, else 0xE85 "Interact"
		if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
		{
			// a pot: a food pile ? 0xEFD "Food Amount" : 0xEFE "Wood Amount", with the pile's resource
			const bool food =
			    Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType == ResourceType::Food;
			help::tooltips::Submit(food ? 0xEFD : 0xEFE, k_ActionRight, 0, false, static_cast<float>(pot->amount));
		}
		else if (registry.AllOf<Field>(object))
		{
			help::tooltips::Submit(0xE73, k_ActionRight, 0, false);
		}
		else
		{
			help::tooltips::Submit(0xE85, k_ActionRight, 0, false);
		}
		break;
	}
	case 14: // Select Lock: the row's 2 / 0xE85 / 0x300
		help::tooltips::Submit(0xE85, k_ActionRight, k_ArrowsUpDown, false);
		break;
	case 0x13: // Creature Interaction: the row's 2 / 0xE85 / 0xF00
		help::tooltips::Submit(0xE85, k_ActionRight, k_ArrowsAll, false);
		break;
	case 18: // Over Object, the can-select-lock check
	{
		// (not ported) the leash first; a site to build (0xEFB, 0xEF4), the needs visuals (0xEFB / 0xECE / 0xEFF), the
		// citadel heart 0xEE8. A town centre ((pending) a totem statue): 0xECC then 0xEFC, before the influence test
		if (const auto* abode = registry.TryGet<const Abode>(object);
		    abode != nullptr && abode->type == AbodeNumber::TownCentre)
		{
			if (const auto town = ecs::abode_villagers::TownOf(object); town != entt::null && registry.AllOf<Town>(town))
			{
				const auto& data = registry.Get<const Town>(town);
				// the belief in me minus the most belief in another player, x 1000; below 0: x -1 and the text 0xEE0
				// "%3.0f Believers Needed"
				const float lead = (ecs::town_belief::GetBeliefInPlayer(data.belief, PlayerNames::PLAYER_ONE) -
				                    ecs::town_belief::GetMaxBeliefMeNotIncluded(data.belief, PlayerNames::PLAYER_ONE)) *
				                   1000.0f;
				help::tooltips::Numbers believers;
				believers.values[0] = static_cast<double>(lead < 0.0f ? lead * -1.0f : lead);
				believers.count = 1;
				if (lead < 0.0f)
				{
					believers.text = 0xEE0;
				}
				help::tooltips::Submit(0xECC, k_ActionRight, 0, false, believers);
				// "Population: %3.0f" of the adults + children, then "%s/%d" with the adult and child places
				help::tooltips::Numbers population;
				population.values[0] = static_cast<double>(data.stats.adults + data.stats.children);
				population.count = 1;
				population.outOf = static_cast<int32_t>(data.stats.adultPlaces + data.stats.childPlaces);
				help::tooltips::Submit(0xEFC, k_ActionRight, 0, false, population);
			}
			break;
		}
		// a creature: the interact text over the player's own creature, or over another player's in the influence; a
		// creature of no player gets the land tooltips. (pending) a flag of the creature that skips it
		if (const auto* creature = registry.TryGet<const Creature>(object); creature != nullptr)
		{
			if (creature_hand::ShowsInteractTip(creature->owner == PlayerNames::PLAYER_ONE,
			                                    creature->owner != PlayerNames::NEUTRAL, InInfluence()))
			{
				help::tooltips::Submit(0xE85, k_ActionRight, 0, false);
			}
			else
			{
				SubmitLandToolTips();
			}
			break;
		}
		// out of the influence: the land tooltips; (pending) a spell icon's own tap tooltip forced, a built workshop
		// 0xEF3
		if (!InInfluence())
		{
			SubmitLandToolTips();
			break;
		}
		// a temple's entrance whose heart is built: mine gives 0xECB "Enter Temple" on the action button, another
		// player's gives no tooltip at all; one whose heart is not built goes on as any other object
		if (const auto entrance = worship::citadel::EntranceToolTipFor(object, PlayerNames::PLAYER_ONE);
		    entrance != worship::citadel::EntranceToolTip::NotEntrance)
		{
			if (entrance == worship::citadel::EntranceToolTip::Enter)
			{
				help::tooltips::Submit(0xECB, k_ActionRight, 0, false);
			}
			break;
		}
		// a storage pit: action -1 and 0xEF9 "Food Stored: %3.0f Wood: %3.0f", its food then its wood (through a float)
		if (registry.AllOf<StoragePit>(object))
		{
			help::tooltips::Numbers stored;
			stored.values[0] =
			    static_cast<double>(static_cast<float>(ecs::StoragePitStore::GetResource(object, ResourceType::Food)));
			stored.values[1] =
			    static_cast<double>(static_cast<float>(ecs::StoragePitStore::GetResource(object, ResourceType::Wood)));
			stored.count = 2;
			help::tooltips::Submit(0xEF9, -1, 0, false, stored);
			break;
		}
		// (not ported) a sacrifice altar 0xEE9, a town desire flag 0xE88, a script highlight 0xEA0 / 0xE83.
		// tappable and not cannot-be-picked-up: its own tap tooltip or 0xE7A "Tap"
		const pot_resource::Dropper is {true, PlayerNames::PLAYER_ONE, true};
		if (InInfluence() && hand_tap::ValidToTap(object, is) && !ecs::object_flags::IsCannotBePickedUp(object))
		{
			// a rock: 0xEF7; a spell icon ((pending) its info's text) and the abodes keep 0xE7A
			help::tooltips::Submit(Rocks::IsRock(object) ? 0xEF7 : 0xE7A, k_ActionRight, 0, false);
			break;
		}
		SubmitLandToolTips();
		break;
	}
	case 24: // Object In Hand
	{
		// (not ported) a query icon 0xE7C; out of the influence: the land tooltips
		if (!InInfluence())
		{
			SubmitLandToolTips();
			break;
		}
		const auto held = _held ? *_held : entt::null;
		// the target under the hand: (not ported) the scaffolds 0xEA5, a creature to give to 0xE87; valid to apply the
		// held object to it: 0xE8E (0xE8D on a sacrifice altar, not ported)
		if (held != entt::null && _cursorObject && ecs::IsAvailable(*_cursorObject) && HeldValidToApplyTo(*_cursorObject))
		{
			help::tooltips::Submit(0xE8E, k_ActionRight, 0, false);
			break;
		}
		// its own drop tooltip: a tree 0xEEF "Plant", else 0xEEE "Drop"; then 0xE74 "Throw"
		const bool tree = held != entt::null && registry.AllOf<Tree>(held);
		help::tooltips::Submit(tree ? 0xEEF : 0xEEE, k_ActionRight, 0, false);
		help::tooltips::Submit(0xE74, k_ActionRight, k_ArrowsAll, false);
		break;
	}
	default: // 23 In Throw and the rows with no text
		break;
	}
}
