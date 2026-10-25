/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReportedCamera.h"

#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs
{

namespace
{
bool IsNetworkGame()
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}
} // namespace

std::optional<systems::HandSystemInterface::ReportedCamera> ReportedPlayerCamera(PlayerNames player)
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetReportedCamera(player) : std::nullopt;
}

std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCreatureCamera(entt::entity creature)
{
	if (!IsNetworkGame())
	{
		return ReportedPlayerCamera(PlayerNames::PLAYER_ONE);
	}
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const components::Creature>(creature) : nullptr;
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL)
	{
		return std::nullopt;
	}
	return ReportedPlayerCamera(body->owner);
}

} // namespace openblack::ecs
