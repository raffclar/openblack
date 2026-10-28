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
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/GameEndRules.h"

namespace openblack::ecs::systems
{

/// The end of a skirmish or a network game (see game_end::ProcessTurn): each turn after the players' turns, the gods
/// with no temple are out, and the game is decided when one god is left
class GameEndSystemInterface
{
public:
	virtual ~GameEndSystemInterface() = default;

	/// The end of the game for one turn
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// What happened in the last turn that the players see
	[[nodiscard]] virtual const std::vector<game_end::Event>& LastEvents() const = 0;
	/// The game is decided
	[[nodiscard]] virtual bool IsDecided() const = 0;
	/// What happened in the turns since the last call, which the view shows. A frame can do more than one turn.
	[[nodiscard]] virtual std::vector<game_end::Event> TakeEventsToShow() = 0;
	/// The one god among the first four players that is still in, when every other god is out
	[[nodiscard]] virtual std::optional<PlayerNames> LastGodIn() const = 0;
	/// Where each god stands, for the gods that a human or a computer mind plays, in the order of the players. A god with
	/// no standing yet has an empty one.
	struct GodStanding
	{
		PlayerNames god;
		ecs::components::GameStanding standing;
	};
	[[nodiscard]] virtual std::vector<GodStanding> Standings() const = 0;

	/// The time limit of a network game in minutes. The game server sets it at the start, and every computer decides
	/// the game at the same turn. With no time limit set, the limit is the default limit of the game.
	virtual void SetTimeLimit(uint32_t minutes) = 0;
	[[nodiscard]] virtual uint32_t GetTimeLimit() const = 0;
	/// A winning condition of a player goes up by an amount, in a network game and when the player has the condition.
	/// No player counts nothing.
	virtual void AddToCondition(std::optional<PlayerNames> player, game_end::Condition condition, int32_t amount) = 0;
	/// Before the turn of a player, in a network game: the conditions of the player that the world measures take their
	/// values. These are the food and the wood in the stores, the prayer power and the belief.
	virtual void MeasureConditions(PlayerNames player) = 0;
	/// A building of a town starts to work: it is built, repaired or made built with the land. When it completes a new
	/// town, the town remembers it, and in a network game the player counts a new town.
	virtual void BuildingWorks(entt::entity building) = 0;
};

} // namespace openblack::ecs::systems
