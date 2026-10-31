/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReportedCamera.h"

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/HandStart.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Hand/HandStart.h"
#include "Locator.h"

namespace openblack::ecs
{

namespace
{
bool IsNetworkGame()
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

/// The player of a creature. None for a creature that is gone or that belongs to the neutral player.
std::optional<PlayerNames> OwnerOf(entt::entity creature)
{
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
	return body->owner;
}
} // namespace

std::optional<systems::HandSystemInterface::ReportedCamera> ReportedPlayerCamera(PlayerNames player)
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetReportedCamera(player) : std::nullopt;
}

std::optional<systems::HandSystemInterface::ReportedCamera> PlayerCameraOf(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& players = Locator::playerSystem::value();
	// A person reports the camera of the player
	if (players.IsHumanPlayer(player))
	{
		return ReportedPlayerCamera(player);
	}
	// Every other god has a camera too: its citadel and its computer mind give it, in a game alone and in a network game
	const auto& registry = Locator::entitiesRegistry::value();
	const auto entity = players.FindPlayer(player);
	if (!entity.has_value() || !registry.Valid(*entity))
	{
		return std::nullopt;
	}
	const auto* start = registry.TryGet<const components::HandStart>(*entity);
	const auto* mind = registry.TryGet<const components::ComputerPlayer>(*entity);
	const auto camera = hand_start::GodCameraOf(mind != nullptr ? std::optional(mind->hand.camera) : std::nullopt,
	                                            start != nullptr ? std::optional(start->point) : std::nullopt);
	return systems::HandSystemInterface::ReportedCamera {.origin = camera.origin, .focus = camera.focus};
}

std::optional<PlayerNames> ReportedCreaturePlayer(entt::entity creature)
{
	if (!IsNetworkGame())
	{
		return PlayerNames::PLAYER_ONE;
	}
	return OwnerOf(creature);
}

std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCreatureCamera(entt::entity creature)
{
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto player = reported_camera::NearestCameraPlayer(OwnerOf(creature), IsNetworkGame(), local);
	return player.has_value() ? PlayerCameraOf(*player) : std::nullopt;
}

std::optional<systems::HandSystemInterface::ReportedCamera> ReportedCameraOfItsPlayer(entt::entity creature)
{
	const auto owner = OwnerOf(creature);
	return owner.has_value() ? PlayerCameraOf(*owner) : std::nullopt;
}

} // namespace openblack::ecs
