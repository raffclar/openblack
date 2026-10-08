/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandPackets.h"

#include <utility>

#include "Creature/LeashKeys.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

void creature_hand_packets::ApplyFeedback(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet)
{
	if (minds == nullptr || packet.object == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!ecs::IsAvailable(packet.object) || !registry.AllOf<components::Creature>(packet.object))
	{
		return;
	}
	minds->ReceiveFeedback(packet.object, packet.data[0]);
}

void creature_hand_packets::ApplyLeashClick(systems::LeashSystemInterface* leash, PlayerNames player)
{
	// (not ported) the interface flag the packet clears first
	if (leash != nullptr)
	{
		(void)leash->PressKey(player, creature_leash::LeashKey::Leash);
	}
}
