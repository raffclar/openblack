/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a football match of the Soccer Add-On on a town's pitch

#include <array>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddFootballScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	constexpr glm::vec2 k_Pitch {0.0f, 45.0f};
	// The referee, five players for each team and three villagers of the crowd come in this order
	constexpr size_t k_Villagers = 14;
	constexpr std::array<VillagerInfo, 2> k_Kinds {VillagerInfo::CelticFarmerMale, VillagerInfo::CelticForesterMale};
	std::vector<ObjectSetup> objects {{.type = AbodeInfo::CelticFootballPitch, .offset = k_Pitch}};
	std::vector<Command> commands;
	for (size_t i = 0; i < k_Villagers; ++i)
	{
		const auto column = static_cast<float>(i % 7);
		const auto row = static_cast<float>(i / 7);
		objects.push_back({.type = k_Kinds.at(i % k_Kinds.size()),
		                   .offset = glm::vec2 {-18.0f + column * 6.0f, 10.0f + row * 5.0f},
		                   .joinTown = true});
		commands.push_back({.kind = Kind::JoinFootball, .delaySeconds = i == 0 ? 2.0f : 0.3f, .object = i + 1});
	}
	all.push_back({
	    .id = "villagers.football_match",
	    .name = "A football match of the Soccer Add-On",
	    .facet = Facet::Idle,
	    .description = "The switch of the Soccer Add-On is on. A Celtic football pitch of the player's town, with fourteen "
	                   "grown villagers of the town in front of it. One after the other, they come to the pitch as a "
	                   "villager that chooses football to relax.",
	    .expected = "The first villager is the referee and does not walk to the pitch. The next five are the home team and "
	                "the five after them the away team: each walks to a place near the centre spot and waits there. The "
	                "last three stand at the side of the pitch and watch it, and one of them waves its arms when the wave "
	                "comes round to it. Thirty seconds after the first player came, the ball goes on the centre spot, and "
	                "the match starts once most players stand still. The players take the roles of attacker, defender and "
	                "goalkeeper, run after the ball and kick it, pass it and shoot at the goals. When the ball goes over an "
	                "end of the pitch, the players stop and look at it, and one player fetches it and kicks it back to the "
	                "centre spot. After a goal, the team that scored celebrates and the other team mourns for seven "
	                "seconds.",
	    .environment = {.dispenserGrid = false, .football = true},
	    .framing = {.shot = Shot::Overview, .include = {k_Pitch, glm::vec2 {0.0f, 10.0f}}, .distance = 0.5f},
	    .objects = std::move(objects),
	    .commands = std::move(commands),
	});
}
