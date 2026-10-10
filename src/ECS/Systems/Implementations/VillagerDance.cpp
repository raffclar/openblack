/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerDance.h"

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_dance = openblack::ecs::villager_dance;

namespace
{
/// The state the interface puts a villager in
constexpr auto k_InterfaceState = static_cast<VillagerStates>(24);

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}
} // namespace

uint32_t villager_dance::DanceSex(entt::entity villager)
{
	const auto* person = Locator::entitiesRegistry::value().TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value())
	{
		return ecs::dance_rules::k_AnySex;
	}
	const auto& info =
	    Locator::infoConstants::value().villager.at(static_cast<size_t>(GVillagerInfo::Find(person->tribe, person->number)));
	return info.sex == SexType::Male ? ecs::dance_rules::k_Men : ecs::dance_rules::k_Women;
}

uint32_t villager_dance::InDance(LivingAction& /*action*/)
{
	// TODO(opening): a dancer in a group walks to its place in the group's shape and dances the group's moves there,
	// from the dance's file; until those are ported it stands where it is
	return 1;
}

bool villager_dance::ExitInDance(LivingAction& action, VillagerStates next)
{
	// Into another of the scripts' states it stays in the dance
	if (StateInfo(next).isScriptState != 0)
	{
		return true;
	}
	// TODO(opening): it remembers it was dancing, to go back to the script's state after reacting
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	ecs::dances::RemoveDancer(registry, villager);
	// TODO(opening): its walk round things starts afresh
	// It holds on for the states that come and go without changing what it was doing
	return StateInfo(next).isScriptInterruptableState != 0 || next == k_InterfaceState || next == VillagerStates::InDance;
}
