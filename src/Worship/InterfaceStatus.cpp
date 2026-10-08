/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InterfaceStatus.h"

#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/SpellSeed.h"

using namespace openblack;

bool worship::interface::HasInterface(PlayerNames player)
{
	return Locator::handSystem::has_value() && magic::players::IsHuman(player);
}

bool worship::interface::IsHandReadyForObject(PlayerNames player)
{
	return HasInterface(player) && !Locator::handSystem::value().GetHeldObject().has_value();
}

entt::entity worship::interface::HeldSpellSeed(PlayerNames player)
{
	if (!HasInterface(player))
	{
		return entt::null;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!held.has_value() || !registry.Valid(*held) || !registry.AllOf<ecs::components::SpellSeed>(*held))
	{
		return entt::null;
	}
	return *held;
}

int worship::interface::PlaceSeedInMagicHand(PlayerNames player, entt::entity seed)
{
	if (!IsHandReadyForObject(player))
	{
		return 0;
	}
	// InterfaceSetInMagicHand first; the hand takes it when that returns 1
	const int result = magic::seed::InterfaceSetInMagicHand(seed);
	if (result == 1)
	{
		Locator::handSystem::value().PlaceObjectInMagicHand(seed);
	}
	return result;
}
