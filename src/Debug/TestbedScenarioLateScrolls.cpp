/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of three silver scrolls of the late lands: the Heavenly Fire and Stanley the Wolf on the fifth
// land, and the Treacherous Path on the fourth land

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Argument = ScriptArgument;
using Kind = Command::Kind;

/// The middle of the blocks of the fifth land and of the fourth land. The testbed measures the points of a scenario
/// from it.
constexpr glm::vec2 k_FifthLandMiddle {2720.0f, 3040.0f};
constexpr glm::vec2 k_FourthLandMiddle {2720.0f, 2800.0f};

/// The middle of the Greek village that the meteors fall on, from the middle of the fifth land
constexpr glm::vec2 k_FireTownCentre = glm::vec2(2307.239f, 2589.891f) - k_FifthLandMiddle;
/// The tower in the middle of the village, where the last meteor of the first volley falls
constexpr glm::vec2 k_FireTower = glm::vec2(2318.51f, 2585.088f) - k_FifthLandMiddle;

/// The tilt puzzle of the wolf and the man who sits beside it, from the middle of the fifth land
constexpr glm::vec2 k_TiltPuzzle = glm::vec2(1620.0453f, 2969.1855f) - k_FifthLandMiddle;

/// The hut of the woman with the potions and the ridge of the ogre Morg, from the middle of the fourth land
constexpr glm::vec2 k_WomansHut = glm::vec2(3703.723f, 3105.303f) - k_FourthLandMiddle;
constexpr glm::vec2 k_OgreRidge = glm::vec2(3209.364f, 3277.383f) - k_FourthLandMiddle;
/// The player's creature, fully grown, at the place given from the middle of the land
CreatureSetup PlayersTiger(glm::vec2 offset)
{
	return {.label = "player's tiger", .species = CreatureType::Tiger, .offset = offset};
}

void AddHeavenlyFire(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l5_heavenly_fire",
	    .name = "Story: the Heavenly Fire falls on the Greek village of the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The script of the challenge starts from its own "
	                   "start. The camera looks at the middle of the Greek village from the north, with the hand empty.",
	    .expected = "Five minutes pass. Then a meteor on fire comes on the ground north of the village. A film starts: the "
	                "camera shakes, the evil advisor points at the meteor and the camera goes near it. The advisors talk, "
	                "and a warning meteor flies from high in the sky to the edge of the village. Then five volleys of "
	                "five meteors fall on the village, each with a long trail of fire. Each meteor bursts with a small "
	                "fireball after it lands. At the end the advisors judge the deaths and the damage.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FireTownCentre.x, 70.0f, k_FireTownCentre.y + 90.0f},
	                .look = {k_FireTownCentre.x, 0.0f, k_FireTownCentre.y}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "FireOnHighMain"}},
	});
	all.push_back({
	    .id = "story.l5_heavenly_fire_meteor",
	    .name = "Story: one meteor of the Heavenly Fire falls on the tower of the Greek village",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The script of one meteor of the volleys starts, "
	                   "with the tower in the middle of the village as its target.",
	    .expected = "A meteor at half size, on fire, comes 250 metres high to the east of the village. It flies to the "
	                "tower and lands there in 4 seconds, with a long trail of fire and smoke. A sound of a lava bomb "
	                "plays at the tower. At least 4 seconds after the meteor stops, a small fireball bursts by it with "
	                "the sound of an explosion, and the meteor breaks into pieces.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FireTower.x - 40.0f, 60.0f, k_FireTower.y + 80.0f},
	                .look = {k_FireTower.x, 0.0f, k_FireTower.y}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "FireOnHighMeteor",
	                  .scriptArguments = {{.kind = Argument::Kind::Marker, .point = k_FireTower, .height = 0.0f}}}},
	});
}

void AddStanleyTheWolf(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l5_stanley_the_wolf",
	    .name = "Story: Stanley the Wolf, the silver scroll of the tilt puzzle on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The script of the silver scroll starts from its "
	                   "own start. The camera looks at the puzzle. Click the Action button on the scroll to start the "
	                   "introduction.",
	    .expected = "The tilt puzzle comes with a silver scroll over it. A man sits beside a bonfire near the puzzle. When "
	                "you click the scroll, a film starts: the man stands up and says three lines, a sign of advice comes "
	                "at the corner of the fence, and the camera goes high over the grid. Then the evil advisor says "
	                "\"Start ringing those bells, Boss.\" The man sits again.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_TiltPuzzle.x + 30.0f, 45.0f, k_TiltPuzzle.y - 40.0f},
	                .look = {k_TiltPuzzle.x, 0.0f, k_TiltPuzzle.y}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "LionPuzzle"}},
	});
}

/// The commands that run the set-up script of the land, which names the Aztec village, then start the script of the
/// scroll
std::vector<Command> StartTreacherousPath()
{
	return {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	        {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "BlindWomanMain"}};
}

void AddTreacherousPath(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_treacherous_path",
	    .name = "Story: the Treacherous Path, the woman with the potions on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The set-up script of the land runs, which names "
	                   "the Aztec village. Then the script of the silver scroll starts from its own start. The "
	                   "player's Tiger stands near the hut of the woman. Click the Action button on the scroll when it "
	                   "comes.",
	    .expected = "The ogre Morg comes on his ridge, half the size of the Tiger, and throws small boulders at the sheep "
	                "every 10 seconds. When the player owns the Aztec village, a silver scroll comes over the hut of the "
	                "woman. Click it: the woman comes, a film shows her, and the camera flies along her path. Then she "
	                "walks the path to the village of her brother. The advisors speak when she comes near the wolves, the "
	                "fire in the wood and Morg.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_WomansHut.x - 40.0f, 40.0f, k_WomansHut.y - 40.0f},
	                .look = {k_WomansHut.x, 0.0f, k_WomansHut.y}},
	    .creatures = {PlayersTiger(k_WomansHut + glm::vec2(-40.0f, -30.0f))},
	    .commands = StartTreacherousPath(),
	});
	all.push_back({
	    .id = "story.l4_treacherous_path_morg",
	    .name = "Story: Morg the ogre of the Treacherous Path meets the creature on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The script of the silver scroll starts from its "
	                   "own start, which makes Morg on his ridge. The set-up script of the land runs first. The "
	                   "player's Tiger stands 40 metres from the ridge.",
	    .expected = "Morg comes on his ridge at half size. The Tiger is within 50 metres, so Morg grows to the size of the "
	                "Tiger in steps. The two creatures look at each other and Morg starts a fight. If Morg loses, five "
	                "sparkles burst and a man takes his place, who dies at once. Else Morg walks back to the ridge.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OgreRidge.x - 50.0f, 60.0f, k_OgreRidge.y - 50.0f},
	                .look = {k_OgreRidge.x, 0.0f, k_OgreRidge.y}},
	    .creatures = {PlayersTiger(k_OgreRidge + glm::vec2(-40.0f, 0.0f))},
	    .commands = StartTreacherousPath(),
	});
}
} // namespace

void testbed_scenarios::AddLateSilverScrollScenarios(std::vector<Scenario>& all)
{
	AddHeavenlyFire(all);
	AddStanleyTheWolf(all);
	AddTreacherousPath(all);
}
