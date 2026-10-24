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
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
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

/// The camera of a computer god whose mind plays, none for another player
std::optional<glm::vec3> ComputerCamera(const Registry& registry, PlayerNames name)
{
	std::optional<glm::vec3> camera;
	registry.Each<const components::Player, const components::ComputerPlayer>(
	    [&](const components::Player& player, const components::ComputerPlayer& computer) {
		    if (player.name == name && computer.switchedOn && !camera.has_value())
		    {
			    camera = computer.hand.camera;
		    }
	    });
	return camera;
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
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		if (player == hands.GetOwnPlayer())
		{
			continue;
		}
		// A computer god shows a sparkle at its camera while its mind plays. Of the other players, only those with
		// hands of their own play, and a player shows no camera before its first report.
		auto origin = ComputerCamera(registry, player);
		if (!origin.has_value())
		{
			const auto camera = hands.GetReportedCamera(player);
			if (!hands.GetHandsOf(player).has_value() || !camera.has_value() || Left(registry, player))
			{
				continue;
			}
			origin = camera->origin;
		}
		const auto tint = random.LocalRand(player_sparkles::k_Tints);
		const auto colour = player_sparkles::Colour(player_colour::OnLand(player, land), tint);
		const auto kind = player_sparkles::KindOf(random.LocalRand(2));
		liquids.AddForPlayer(player, *origin, glm::vec3(0.0f), player_sparkles::k_Size, colour, kind);
		liquids.SetPlayerPoint(player, *origin);
	}
}

} // namespace openblack::ecs::other_player_sparkles
