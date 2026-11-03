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
#include "ECS/Components/ObserverInterfaces.h"
#include "ECS/Components/Player.h"
#include "ECS/GamePlayers.h"
#include "ECS/ObserverInterfaceRules.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Online/NetworkSession.h"
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

/// The observers of the god of the observers, none when the world has no player of the god or the god has no observer
const components::ObserverInterfaces* ObserversOf(const Registry& registry, PlayerNames god)
{
	if (!Locator::playerSystem::has_value())
	{
		return nullptr;
	}
	const auto player = Locator::playerSystem::value().FindPlayer(god);
	return player.has_value() ? registry.TryGet<const components::ObserverInterfaces>(*player) : nullptr;
}

/// The number of the observer of this computer, none on the computer of a god and on the game server
std::optional<uint8_t> OwnObserver()
{
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	return session != nullptr ? session->OwnObserver() : std::nullopt;
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
	const auto observersGod =
	    Locator::gameEndSystem::has_value() ? Locator::gameEndSystem::value().ObserversGod() : std::nullopt;
	// One sparkle at the camera of an interface of a player, in the colour that the player shows
	const auto sparkle = [&](liquid_particles::Interface interface, glm::vec3 origin) {
		const auto tint = random.LocalRand(player_sparkles::k_Tints);
		const auto shown = player_colour::ShownAs(interface.player, observersGod);
		const auto colour = player_sparkles::Colour(player_colour::OnLand(shown, land), tint);
		const auto kind = player_sparkles::KindOf(random.LocalRand(2));
		liquids.AddForPlayer(interface, origin, glm::vec3(0.0f), player_sparkles::k_Size, colour, kind);
		liquids.SetPlayerPoint(interface, origin);
	};
	for (const auto player : ecs::game_players::All())
	{
		const auto watched = WatchedOf(registry, hands, player);
		// While humans play the god of the observers, each observer is an interface of its own, with a sparkle at its own
		// camera. The observer of this computer shows none.
		if (player == observersGod && watched.kind == player_kind::Kind::Human)
		{
			if (const auto* observers = ObserversOf(registry, player); observers != nullptr)
			{
				for (const auto& observer : observer_interfaces::ShownObservers(*observers, OwnObserver()))
				{
					sparkle({.player = player, .observer = observer.number}, observer.camera);
				}
			}
			continue;
		}
		if (player == hands.GetOwnPlayer())
		{
			continue;
		}
		if (const auto origin = player_sparkles::PlaceOf(watched); origin.has_value())
		{
			sparkle({.player = player}, *origin);
		}
	}
}

} // namespace openblack::ecs::other_player_sparkles
