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
/// The place by the middle of the village where the reward falls after a good verdict
constexpr glm::vec2 k_FireReward = glm::vec2(2282.773f, 2601.362f) - k_FifthLandMiddle;
/// The reward of the challenge: the Itchy creature miracle seed, as the reward scripts number it
constexpr float k_ItchySeedReward = 46.0f;

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
	all.push_back({
	    .id = "story.l5_heavenly_fire_reward",
	    .name = "Story: the reward of the Heavenly Fire falls from the sky by the Greek village",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The shared script of a reward from the sky runs "
	                   "as the challenge runs it after a good verdict on the buildings: the Itchy creature miracle seed, "
	                   "at the place by the middle of the village, with no film.",
	    .expected = "A reward chest falls from the sky by the middle of the village. The camera does not follow it. When "
	                "you open the chest, a seed of the Itchy creature miracle comes out.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FireReward.x + 30.0f, 40.0f, k_FireReward.y + 30.0f},
	                .look = {k_FireReward.x, 0.0f, k_FireReward.y}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "rewardfromsky",
	                  .scriptArguments = {{.number = k_ItchySeedReward},
	                                      {.kind = Argument::Kind::Marker, .point = k_FireReward},
	                                      {.number = 0.0f},
	                                      {.number = 0.0f}}}},
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

/// The place where the woman comes back after a death, from the middle of the fourth land
constexpr glm::vec2 k_WomansSoapBox = glm::vec2(3716.7786f, 3116.0618f) - k_FourthLandMiddle;
/// The Aztec village of the brother, which the set-up script of the land names, from the middle of the fourth land
constexpr glm::vec2 k_BrothersVillage = glm::vec2(3385.63f, 3100.74f) - k_FourthLandMiddle;
/// The number of player slots that have a belief in a town
constexpr int k_PlayerSlots = 9;

/// The commands that give the Aztec village to the player and then start the script of the scroll with the count of
/// deaths and the state of the journey given. A journey state of 1 is an arrival and 2 is a death. With a state given,
/// the script goes to its ending as soon as the introduction after the scroll click ends.
std::vector<Command> TreacherousPathEnding(float deaths, float journey)
{
	std::vector<Command> commands {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	                               // The village is the first thing that the scenario's natives give back
	                               {.kind = Kind::CallNative,
	                                .delaySeconds = 0.5f,
	                                .script = "CALL_NEAR",
	                                .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 9.0f},
	                                                    {.kind = Argument::Kind::Integer, .number = 5000.0f},
	                                                    {.kind = Argument::Kind::Position, .point = k_BrothersVillage},
	                                                    {.number = 50.0f},
	                                                    {.kind = Argument::Kind::Integer, .number = 0.0f}}}};
	for (int player = 0; player < k_PlayerSlots; ++player)
	{
		commands.push_back({.kind = Kind::CallNative,
		                    .script = "SET_PLAYER_BELIEF",
		                    .scriptArguments = {{.kind = Argument::Kind::Made, .index = 0},
		                                        {.number = static_cast<float>(player)},
		                                        {.number = player == 1 ? 10.0f : 0.0f}}});
	}
	commands.push_back(
	    {.kind = Kind::RunScript,
	     .delaySeconds = 0.5f,
	     .script = "BlindWomanMain",
	     .scriptGlobals = {{"TimesDied", {.number = deaths}}, {"BlindWomanStillGoingStrong", {.number = journey}}}});
	return commands;
}

/// The command that runs the scene of a death of the woman, after the deaths before it
std::vector<Command> TreacherousPathDeath(float deathsBefore)
{
	return {{.kind = Kind::RunScript,
	         .delaySeconds = 0.5f,
	         .script = "BlindWomanKilled",
	         .scriptGlobals = {{"TimesDied", {.number = deathsBefore}},
	                           {"TotalLives", {.number = 3.0f}},
	                           {"BlindWomanIntroSoapBox", {.kind = Argument::Kind::Marker, .point = k_WomansSoapBox}}}}};
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
	const Framing soapBox {.shot = Shot::Placed,
	                       .eye = {k_WomansSoapBox.x - 30.0f, 25.0f, k_WomansSoapBox.y - 30.0f},
	                       .look = {k_WomansSoapBox.x, 0.0f, k_WomansSoapBox.y}};
	const Framing hut {.shot = Shot::Placed,
	                   .eye = {k_WomansHut.x - 40.0f, 40.0f, k_WomansHut.y - 40.0f},
	                   .look = {k_WomansHut.x, 0.0f, k_WomansHut.y}};
	const Environment fourthLand {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false};
	all.push_back({
	    .id = "story.l4_treacherous_path_first_death",
	    .name = "Story: the woman of the Treacherous Path uses her first potion",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The scene of a death of the woman runs, with no "
	                   "death before it.",
	    .expected = "After two seconds the screen goes black. The woman comes again by her hut and the camera looks at "
	                "her. She says that she used one of her potions, that she has two left and needs one for her brother, "
	                "and asks you to watch over her. Then the screen goes black again and comes back where the camera "
	                "was.",
	    .environment = fourthLand,
	    .framing = soapBox,
	    .commands = TreacherousPathDeath(0.0f),
	});
	all.push_back({
	    .id = "story.l4_treacherous_path_second_death",
	    .name = "Story: the woman of the Treacherous Path uses her second potion",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The scene of a death of the woman runs, with one "
	                   "death before it.",
	    .expected = "As the first death, but she says that she used another potion, that her brother needs the last one, "
	                "and asks you to keep her from harm.",
	    .environment = fourthLand,
	    .framing = soapBox,
	    .commands = TreacherousPathDeath(1.0f),
	});
	all.push_back({
	    .id = "story.l4_treacherous_path_third_death",
	    .name = "Story: the woman of the Treacherous Path has no potions left",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The scene of a death of the woman runs, with two "
	                   "deaths before it.",
	    .expected = "As the first death, but she says that she has no potions left and goes to see her brother on his "
	                "death bed, if she gets there at all.",
	    .environment = fourthLand,
	    .framing = soapBox,
	    .commands = TreacherousPathDeath(2.0f),
	});
	all.push_back({
	    .id = "story.l4_treacherous_path_brother_healed",
	    .name = "Story: the woman of the Treacherous Path comes to her brother with a potion",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The player gets the Aztec village, so the "
	                   "silver scroll comes over the hut of the woman. The script starts as if she came to the end of "
	                   "her path with no death. Click the Action button on the scroll.",
	    .expected = "After the introduction the screen goes black and comes back at the house of the brother. He crawls "
	                "out, she thanks you, points at him and heals him. They hug, and a wolf comes. The advisors speak "
	                "about the wolf, and its offer of a creature swap starts. Then the two go into the house.",
	    .environment = fourthLand,
	    .framing = hut,
	    .creatures = {PlayersTiger(k_WomansHut + glm::vec2(-40.0f, -30.0f))},
	    .commands = TreacherousPathEnding(0.0f, 1.0f),
	});
	all.push_back({
	    .id = "story.l4_treacherous_path_brother_dies",
	    .name = "Story: the woman of the Treacherous Path comes to her brother with no potion",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The player gets the Aztec village, so the "
	                   "silver scroll comes over the hut of the woman. The script starts as if she came to the end of "
	                   "her path after three deaths. Click the Action button on the scroll.",
	    .expected = "After the introduction the screen goes black and comes back at the house of the brother. He crawls "
	                "out and dies. She mourns and says that she has no potions left. The evil advisor points at him and "
	                "the good advisor speaks against him. She goes into the house in despair. No wolf comes.",
	    .environment = fourthLand,
	    .framing = hut,
	    .creatures = {PlayersTiger(k_WomansHut + glm::vec2(-40.0f, -30.0f))},
	    .commands = TreacherousPathEnding(3.0f, 1.0f),
	});
	all.push_back({
	    .id = "story.l4_treacherous_path_last_life",
	    .name = "Story: the woman of the Treacherous Path walks with no potions left",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The player gets the Aztec village, so the "
	                   "silver scroll comes over the hut of the woman. The script starts as if she died three times. "
	                   "Click the Action button on the scroll. When she walks her path, kill her.",
	    .expected = "She does not come back. The camera goes to her body and the evil advisor points at it and laughs at "
	                "her and her brother. The screen goes black and comes back at the house of the brother: he crawls out "
	                "and dies there. The advisors speak, and his body goes when it is off the screen.",
	    .environment = fourthLand,
	    .framing = hut,
	    .creatures = {PlayersTiger(k_WomansHut + glm::vec2(-40.0f, -30.0f))},
	    .commands = TreacherousPathEnding(3.0f, 0.0f),
	});
}
} // namespace

void testbed_scenarios::AddLateSilverScrollScenarios(std::vector<Scenario>& all)
{
	AddHeavenlyFire(all);
	AddStanleyTheWolf(all);
	AddTreacherousPath(all);
}
