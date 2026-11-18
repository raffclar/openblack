/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios of the last stand of Nemesis on the fifth land: the spiritual shield over the Tibetan village
// and the defence of his last town

#include <array>
#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

constexpr std::string_view k_FifthLand = "Land5.txt";
/// The middle of the blocks of the fifth land. The testbed measures the points of a scenario from it.
constexpr glm::vec2 k_LandMiddle {2720.0f, 3040.0f};
/// The middle of the spiritual shield, from the middle of the land
constexpr glm::vec2 k_ShieldCentre = glm::vec2(1655.983f, 2331.104f) - k_LandMiddle;
/// The three outer singing stones of the spiritual shield, at their spots, from the middle of the land
constexpr std::array<glm::vec2, 3> k_ShieldStones {glm::vec2(2092.572f, 2070.963f) - k_LandMiddle,
                                                   glm::vec2(1571.99f, 2105.337f) - k_LandMiddle,
                                                   glm::vec2(1396.386f, 3000.66f) - k_LandMiddle};
/// The point near the last town of Nemesis that sets off its defence, from the middle of the land
constexpr glm::vec2 k_DefencePoint = glm::vec2(3088.801f, 2654.14f) - k_LandMiddle;
/// The middle of the last town of Nemesis, from the middle of the land
constexpr glm::vec2 k_LastTown = glm::vec2(3123.780f, 2625.750f) - k_LandMiddle;

/// The player's tiger, fully grown and well, at a point
CreatureSetup PlayersTiger(glm::vec2 offset, float facingDegrees)
{
	return {
	    .label = "player's tiger",
	    .species = CreatureType::Tiger,
	    .offset = offset,
	    .facingDegrees = facingDegrees,
	    .phase = 3,
	    .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	};
}
} // namespace

void testbed_scenarios::AddLastStandScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l5_spiritual_shield",
	    .name = "Story: the spiritual shield of Nemesis over the Tibetan village",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The player's Tiger stands 194 east of the middle "
	                   "of the shield, nearer than 300. The script of the shield starts as the land's script starts it.",
	    .expected = "The three outer singing stones become indestructible. A cut scene: the evil advisor points at the "
	                "village. Three worshippers come by the three stones. The camera flies past each stone while its "
	                "worshipper speaks. A vertical beam comes down to the village, and the shield goes up with a radius "
	                "of 105. Each worshipper dances by his stone and chants, with a beam from his stone to the top of the "
	                "shield. When a worshipper or his stone moves away, his beam stops and the shield gets 35 smaller.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_ShieldCentre.x + 60.0f, 290.0f, k_ShieldCentre.y - 300.0f},
	                .look = {k_ShieldCentre.x, 75.0f, k_ShieldCentre.y + 45.0f}},
	    .creatures = {PlayersTiger(k_ShieldCentre + glm::vec2(194.0f, 0.0f), 270.0f)},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "ThrowThroughShield"}},
	});
	// The player's hand takes each outer stone off its spot and throws it away once the shield is up. The stones are
	// outside the player's influence, so the editor's cheat of infinite influence lets the hand reach them.
	std::vector<Command> fallCommands {{.kind = Kind::InfiniteInfluenceCheat, .delaySeconds = 0.1f, .value = 1},
	                                   {.kind = Kind::RunScript, .delaySeconds = 0.4f, .script = "ThrowThroughShield"}};
	for (size_t stone = 0; stone < k_ShieldStones.size(); ++stone)
	{
		fallCommands.push_back(
		    {.kind = Kind::HandTakeNear, .delaySeconds = stone == 0 ? 90.0f : 1.0f, .point = k_ShieldStones.at(stone)});
		fallCommands.push_back({.kind = Kind::HandRelease, .delaySeconds = 1.0f, .velocity = {20.0f, 10.0f, 0.0f}});
	}
	all.push_back({
	    .id = "story.l5_spiritual_shield_falls",
	    .name = "Story: the spiritual shield of Nemesis falls when its three stones are off their spots",
	    .facet = Facet::Story,
	    .description = "As the scenario of the spiritual shield, with the editor's cheat of infinite influence. When the "
	                   "shield is up, the player's hand takes each of the three outer singing stones off its spot and "
	                   "throws it away.",
	    .expected = "The shield goes up with a radius of 105. Each stone that leaves its spot stops its beam, and the "
	                "shield gets 35 smaller. When the third stone is off its spot, the shield and the vertical beam go "
	                "at once, and nothing of the shield stays on the screen. The good advisor says that the shield is "
	                "down and no one died.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_ShieldCentre.x + 60.0f, 290.0f, k_ShieldCentre.y - 300.0f},
	                .look = {k_ShieldCentre.x, 75.0f, k_ShieldCentre.y + 45.0f}},
	    .creatures = {PlayersTiger(k_ShieldCentre + glm::vec2(194.0f, 0.0f), 270.0f)},
	    .commands = fallCommands,
	    .checks = {{.label = "the shield sphere is up over the village before the hand takes the stones",
	                .quantity = checks::Quantity::ShieldSphereAt,
	                .point = k_ShieldCentre,
	                .afterTurns = 100,
	                .byTurn = 900,
	                .atLeast = 1.0},
	               {.label = "no shield sphere stays over the village after the three stones are off their spots",
	                .quantity = checks::Quantity::ShieldSphereAt,
	                .point = k_ShieldCentre,
	                .afterTurns = 970,
	                .byTurn = 1600,
	                .atMost = 0.0}},
	});
	all.push_back({
	    .id = "story.l5_last_town_defence",
	    .name = "Story: the last town of Nemesis defends itself",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The player's Tiger stands near the last town "
	                   "of Nemesis, nearer than 200 to the point that sets off its defence. The script of the defence "
	                   "starts as the land's script starts it.",
	    .expected = "After 10 seconds, and then each 6 seconds, a burning rock of half size flies from high on the "
	                "mountain behind the town, with a trail of smoke. It comes down on the slope in front of the town "
	                "and rolls. After 6 seconds it bursts with a fireball. A physical shield of radius 75 covers the town "
	                "for 30 seconds in each 30 seconds.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_LastTown.x - 230.0f, 90.0f, k_LastTown.y + 120.0f},
	                .look = {k_LastTown.x, 10.0f, k_LastTown.y}},
	    .creatures = {PlayersTiger(k_DefencePoint + glm::vec2(-100.0f, 45.0f), 120.0f)},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "NemesisDefendHisLastTown"}},
	});
}
