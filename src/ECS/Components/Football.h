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

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/FootballRules.h"

namespace openblack::ecs::components
{

/// A football pitch and its match: the ball, the referee, the two teams, the crowd that does the wave, and the score
struct FootballPitch
{
	/// The pitch was built and works, and its town uses it
	bool functional {false};
	football_rules::Match match;
	football_rules::Wave wave;
	/// The pitch's ball. The pitch makes a new ball when it has none.
	entt::entity ball {entt::null};
	/// Where the ball was when the pitch last looked at it in play
	std::optional<glm::vec3> ballLastPlace;
	entt::entity referee {entt::null};
	/// The teams, the newest player first, as the game keeps them
	std::vector<entt::entity> home;
	std::vector<entt::entity> away;
	/// Each team in order of distance to the goal that it attacks, home first
	std::array<std::vector<entt::entity>, 2> ranks;
	/// The places of the crowd that do the wave, and how many are filled
	std::array<entt::entity, football_rules::k_WavePlaces> crowd = [] {
		std::array<entt::entity, football_rules::k_WavePlaces> places {};
		places.fill(entt::null);
		return places;
	}();
	uint32_t crowdPlaces {0};
	/// The player that fetches a dead ball, none while the ball is in play
	entt::entity deadBallFetcher {entt::null};
	/// For each team, the attacker nearest to the ball, and a defender nearer than that attacker
	std::array<entt::entity, 2> attackerMarks {entt::null, entt::null};
	std::array<entt::entity, 2> defenderMarks {entt::null, entt::null};
	/// The turn at which the last match of the town stopped, 0 before the first match
	uint32_t lastMatchTurn {0};
};

/// A villager that plays football, is the referee, or watches the match on a pitch
struct Footballer
{
	entt::entity pitch {entt::null};
	/// The villager is in the match, as a player, the referee or the crowd. Such a villager does not react to things.
	bool inMatch {true};
	/// The task that the player does, none when it has none
	std::optional<uint8_t> task;
	/// The player looks for a new task
	bool chooseTask {false};
	/// The turns that a goalkeeper looks at the ball
	uint8_t lookTurns {0};
	/// The player's place among the attackers or the defenders of its team
	uint8_t number {0};
};

/// The football pitch that a town uses
struct TownFootballPitch
{
	entt::entity pitch {entt::null};
};

} // namespace openblack::ecs::components
