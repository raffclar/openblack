/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TakeResource.h"

#include <spdlog/spdlog.h>

#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/Physics/PhysicsObjects.h"
// TODO(Intro HEAD): #include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::ecs;

void take_resource::TriggerSupplyHelpIfThrownByMe(entt::entity object)
{
	// in physics (PhysicsObjects::IsFlying; a resting proxy keeps its list entry but is not in physics), then its
	// physics object (PhysicsObjects::Find)
	if (!physics::PhysicsObjects::IsFlying(object))
	{
		return;
	}
	const auto* po = physics::PhysicsObjects::Find(object);
	// the physics object's interface is the local one
	if (po == nullptr || !po->byPlayer)
	{
		return;
	}
	// the help profile's trigger 6 (SUPPLY)
	// TODO(Intro HEAD): help_profile::Trigger(help_profile::Event::Supply);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "HelpProfile::Trigger(SUPPLY) for {} (pending: help_profile)",
	                    static_cast<uint32_t>(object));
}

bool take_resource::StoragePit(entt::entity store, entt::entity object, const pot_resource::Dropper& is)
{
	// the interface's player, read first. No player is openblack's NEUTRAL (as the other CreateReaction callers)
	const PlayerNames player = is.hasInterface ? is.player : PlayerNames::NEUTRAL;
	TriggerSupplyHelpIfThrownByMe(object);
	// the store takes it (what was taken is not used)
	object_delivery::DoDeleteObjectAndTakeResource(store, object, is);
	// REACTION 22 REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, initiated by the store, stamped with the turn (the last
	// argument); the returned reaction is dropped
	effects::reactions::CreateReaction(store, Reaction::ReactToHandPuttingStuffInStoragePit, player, true);
	return true;
}
