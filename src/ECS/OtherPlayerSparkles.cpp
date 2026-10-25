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

#include "3D/TempleInteriorInterface.h"
#include "Common/GameRandom.h"
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
		// Only the players with hands of their own play. A player shows no camera before its first report.
		const auto camera = hands.GetReportedCamera(player);
		if (player == hands.GetOwnPlayer() || !hands.GetHandsOf(player).has_value() || !camera.has_value() ||
		    Left(registry, player))
		{
			continue;
		}
		const auto tint = random.LocalRand(player_sparkles::k_Tints);
		const auto colour = player_sparkles::Colour(player_colour::OnLand(player, land), tint);
		const auto kind = player_sparkles::KindOf(random.LocalRand(2));
		liquids.AddForPlayer(player, camera->origin, glm::vec3(0.0f), player_sparkles::k_Size, colour, kind);
		liquids.SetPlayerPoint(player, camera->origin);
	}
}

} // namespace openblack::ecs::other_player_sparkles
