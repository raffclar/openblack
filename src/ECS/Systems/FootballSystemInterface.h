/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/fwd.hpp>

#include "ECS/FootballRules.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

namespace openblack::ecs::systems
{

/// The football of the free Soccer Add-On: the town's pitch (components::FootballPitch), its ball and its match, and
/// the villagers that play or watch (components::Footballer). The add-on switch turns the matches on. A pitch that a
/// script makes still makes its ball and takes players when the switch is off, but its match never starts.
class FootballSystemInterface
{
public:
	virtual ~FootballSystemInterface() = default;

	/// The game reads the add-on switch when a game starts. The switch has no effect in a multiplayer game.
	virtual void StartGame(bool addOnSwitch, bool multiplayer) = 0;
	/// Whether the game plays football
	[[nodiscard]] virtual bool IsOn() const = 0;

	/// Each turn of a town: a pitch that now works becomes the town's pitch. While the game plays football, the town's
	/// pitch also looks after its ball, its match and its crowd.
	virtual void ProcessTown(entt::entity town) = 0;

	/// How much a villager of the town wants to play on the town's pitch, none when the town has no pitch
	[[nodiscard]] virtual std::optional<float> RelaxationDesire(entt::entity town, entt::entity villager) const = 0;
	/// A villager comes to the town's pitch: it becomes the referee, a player or one of the crowd. Whether it came.
	virtual bool Join(entt::entity town, entt::entity villager) = 0;

	/// A villager leaves its team. The fetcher of a dead ball stops its fetch. One of the crowd keeps its place in the
	/// wave.
	virtual void Leave(entt::entity villager) = 0;

	/// The pitch of a villager that plays or watches, none for another villager
	[[nodiscard]] virtual entt::entity PitchOf(entt::entity villager) const = 0;
	/// The ball of a pitch. The pitch makes a ball on its centre spot when it has none.
	virtual entt::entity BallOf(entt::entity pitch) = 0;
	/// What a player does each turn in its role: after its count, it does its task, and looks for a new task when the
	/// task stops
	virtual uint32_t PlayRole(components::LivingAction& action) = 0;
	/// A player that fetched a dead ball kicks it to the centre spot, and play starts again
	virtual void RestartDeadBall(entt::entity villager) = 0;
	/// What happens on a pitch, none for a thing that is not a pitch
	[[nodiscard]] virtual std::optional<football_rules::PlayState> PlayStateOf(entt::entity pitch) const = 0;
};

} // namespace openblack::ecs::systems
