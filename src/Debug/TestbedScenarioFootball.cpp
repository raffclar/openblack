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
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
constexpr glm::vec2 k_Pitch {0.0f, 45.0f};
/// The referee, five players for each team and three villagers of the crowd come in this order
constexpr size_t k_Villagers = 14;
/// The first player of the home team, by its place in the scenario's objects
constexpr size_t k_FirstHomePlayer = 2;
/// The time from the last villager to the creature's action: the match plays by then
constexpr float k_MatchPlaysSeconds = 60.0f;

/// The pitch and fourteen villagers of the town in front of it, and the commands that bring them to the pitch one after
/// the other
std::pair<std::vector<ObjectSetup>, std::vector<Command>> MatchScene()
{
	using Kind = Command::Kind;
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
	return {std::move(objects), std::move(commands)};
}

/// The match scene with the player's tiger by the pitch. When the match plays, the tiger is told to do a football action
/// on the first player of the home team.
Scenario CreatureAction(std::string_view id, std::string_view name, std::string_view action, std::string_view description,
                        std::string_view expected)
{
	auto [objects, commands] = MatchScene();
	commands.push_back(
	    {.kind = Command::Kind::DoAction, .delaySeconds = k_MatchPlaysSeconds, .object = k_FirstHomePlayer, .action = action});
	return {
	    .id = id,
	    .name = name,
	    .facet = Facet::Objects,
	    .description = description,
	    .expected = expected,
	    .environment = {.dispenserGrid = false, .football = true},
	    .framing = {.shot = Shot::Overview, .include = {k_Pitch, glm::vec2 {40.0f, 45.0f}}, .distance = 0.6f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 45.0f}, .facingDegrees = 270.0f}},
	    .objects = std::move(objects),
	    .commands = std::move(commands),
	};
}
} // namespace

void testbed_scenarios::AddFootballScenarios(std::vector<Scenario>& all)
{
	auto [objects, commands] = MatchScene();
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
	std::vector<ObjectSetup> site {{.type = AbodeInfo::CelticFootballPitch, .offset = k_Pitch, .built = 0.3f},
	                               {.type = AbodeInfo::CelticTownCentre, .offset = {-30.0f, -15.0f}},
	                               {.type = AbodeInfo::CelticStoragePit, .offset = {30.0f, -15.0f}, .storedWood = 5000}};
	for (size_t i = 0; i < 6; ++i)
	{
		site.push_back({.type = VillagerInfo::CelticFarmerMale,
		                .offset = glm::vec2 {-15.0f + static_cast<float>(i) * 6.0f, 5.0f},
		                .joinTown = true});
	}
	all.push_back({
	    .id = "villagers.football_pitch_site",
	    .name = "Villagers that build a football pitch",
	    .facet = Facet::Idle,
	    .description = "The switch of the Soccer Add-On is on. A Celtic football pitch of the player's town stands only "
	                   "three tenths built, as a building site. The town has a village centre and a storage pit with 5000 "
	                   "wood. Six grown men of the town stand in a row in front of the pitch.",
	    .expected = "The men come to the site as builders of any building. They bring wood to the pile of the site, stand "
	                "at places on a circle round the pitch, and build it up. A creature or a footpath does not go round "
	                "the pitch. When the pitch is built, the villagers of the town do not run to look at it as they do at "
	                "other new civic buildings.",
	    .environment = {.dispenserGrid = false, .football = true},
	    .framing = {.shot = Shot::Overview, .include = {k_Pitch, glm::vec2 {0.0f, -15.0f}}, .distance = 0.5f},
	    .objects = std::move(site),
	});
	all.push_back(CreatureAction(
	    "creatures.football_throw", "A creature that throws the football at a goal", "AttackerThrowBallAtGoal",
	    "The switch of the Soccer Add-On is on. A Celtic football pitch of the player's town, with fourteen grown villagers of "
	    "the town that come to it one after the other, as in the football match scenario. The player's tiger stands to the "
	    "east of the pitch. One minute after the last villager came, the tiger is told to throw the ball at the goal, for the "
	    "first player of "
	    "the home team.",
	    "The tiger plays for the home team, because its player has an even number. Two times in three it first shows that it "
	    "feels playful. It walks to the ball and picks it up, and its head looks at the ball as it goes. Then it throws the "
	    "ball at the goal that the home team attacks. The players run after the ball when it comes down."));
	all.push_back(CreatureAction(
	    "creatures.football_foul", "A creature that stomps on a footballer", "GoalieFoul",
	    "The switch of the Soccer Add-On is on. A Celtic football pitch of the player's town, with fourteen grown villagers of "
	    "the town that come to it one after the other, as in the football match scenario. The player's tiger stands to the "
	    "east of the pitch. One minute after the last villager came, the tiger is told to do the goalkeeper's foul, for the "
	    "first "
	    "player of the home team.",
	    "The tiger walks to the first player of the away team, with an angry face, and stomps on "
	    "him. The player is hurt or dies as from any stomp, and leaves his team."));
	all.push_back(CreatureAction(
	    "creatures.football_stomp_ball", "A creature that stomps on the football", "DefenderStompOnBall",
	    "The switch of the Soccer Add-On is on. A Celtic football pitch of the player's town, with fourteen grown villagers of "
	    "the town that come to it one after the other, as in the football match scenario. The player's tiger stands to the "
	    "east of the pitch. One minute after the last villager came, the tiger is told to stomp on the ball, for the first "
	    "player of "
	    "the home team.",
	    "The tiger walks to the ball with an angry face, its head on the ball, and stomps on it."));
}
