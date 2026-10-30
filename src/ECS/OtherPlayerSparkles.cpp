/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OtherPlayerSparkles.h"

#include <cstddef>

#include <optional>

#include <glm/vec3.hpp>

#include "3D/TempleInteriorInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/Player.h"
#include "ECS/GamePlayers.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Particles/PlayerSparkles.h"

namespace openblack::ecs::other_player_sparkles
{

namespace
{
/// Whether the player left the network game
bool Left(const Registry& registry, PlayerNames name)
{
	bool left = false;
	registry.Each<const components::Player>([&](entt::entity entity, const components::Player& player) {
		left = left || (player.name == name && registry.AllOf<components::LeftGame>(entity));
	});
	return left;
}

/// Who plays the player
player_kind::Kind KindOf(PlayerNames name)
{
	if (!Locator::playerSystem::has_value())
	{
		return player_kind::Kind::None;
	}
	const auto& players = Locator::playerSystem::value();
	if (players.IsComputerPlayer(name))
	{
		return player_kind::Kind::Computer;
	}
	return players.IsHumanPlayer(name) ? player_kind::Kind::Human : player_kind::Kind::None;
}

/// What the player shows a sparkle from
player_sparkles::Watched WatchedOf(const Registry& registry, const systems::HandSystemInterface& hands, PlayerNames name)
{
	player_sparkles::Watched watched {.kind = KindOf(name)};
	registry.Each<const components::Player, const components::ComputerPlayer>(
	    [&](const components::Player& player, const components::ComputerPlayer& computer) {
		    if (player.name == name && computer.switchedOn && !watched.mindOn)
		    {
			    watched.mindOn = true;
			    watched.mindCamera = computer.hand.camera;
		    }
	    });
	if (hands.GetHandsOf(name).has_value())
	{
		if (const auto camera = hands.GetReportedCamera(name); camera.has_value())
		{
			watched.reportedCamera = camera->origin;
		}
	}
	watched.left = Left(registry, name);
	return watched;
}
} // namespace

void AddAtCameras()
{
	if (!Locator::handSystem::has_value() || !Locator::liquidParticleSystem::has_value() || !Locator::gameRandom::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	if ((Locator::temple::has_value() && Locator::temple::value().Active()) ||
	    (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().DirectsFight()))
	{
		return;
	}
	const auto& hands = Locator::handSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	auto& random = Locator::gameRandom::value();
	auto& liquids = Locator::liquidParticleSystem::value();
	const auto land = registry.Context().mapScriptGlobals.landNumber;
	for (const auto player : ecs::game_players::All())
	{
		if (player == hands.GetOwnPlayer())
		{
			continue;
		}
		const auto origin = player_sparkles::PlaceOf(WatchedOf(registry, hands, player));
		if (!origin.has_value())
		{
			continue;
		}
		const auto tint = random.LocalRand(player_sparkles::k_Tints);
		const auto colour = player_sparkles::Colour(player_colour::OnLand(player, land), tint);
		const auto kind = player_sparkles::KindOf(random.LocalRand(2));
		liquids.AddForPlayer(player, *origin, glm::vec3(0.0f), player_sparkles::k_Size, colour, kind);
		liquids.SetPlayerPoint(player, *origin);
	}
}

} // namespace openblack::ecs::other_player_sparkles
