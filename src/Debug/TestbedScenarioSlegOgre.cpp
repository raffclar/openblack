/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the ogre of the fourth land, his gremlins and the undead village

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Argument = ScriptArgument;
using Kind = Command::Kind;

/// The middle of the blocks of the fourth land. The testbed measures the points of a scenario from it.
constexpr glm::vec2 k_LandMiddle {2720.0f, 2800.0f};
/// The place in front of the lair where the ogre stops to speak, from the middle of the land
constexpr glm::vec2 k_FightIntro = glm::vec2(1597.958f, 2689.638f) - k_LandMiddle;
/// The lair of the ogre, from the middle of the land
constexpr glm::vec2 k_Lair = glm::vec2(1705.717f, 2790.110f) - k_LandMiddle;
/// A point 55 metres from the place of the speech, nearer than the 150 metres that start the fight
constexpr glm::vec2 k_NearTheFight = glm::vec2(1560.0f, 2650.0f) - k_LandMiddle;
/// A point inside the ring that sends the creature back: 200 metres from the place of the speech and 132 metres from the
/// lair, from the middle of the land
constexpr glm::vec2 k_FenceSpot = glm::vec2(1798.0f, 2695.0f) - k_LandMiddle;
/// The spot on the road near the home village where the script sends a creature that went too near the ogre, from the
/// middle of the land
/// The place of the gold scroll over the lair, from the middle of the land, its kind of highlight and the challenge
/// that the Heartbroken Man's ending gives it
constexpr glm::vec2 k_OgreScroll = glm::vec2(1682.351f, 2841.488f) - k_LandMiddle;
constexpr float k_GoldScroll = 3.0f;
constexpr float k_HeartbrokenChallenge = 45.0f;
constexpr glm::vec2 k_RoadRetreat = glm::vec2(2183.6868f, 3095.9983f) - k_LandMiddle;
/// The middle of the undead village and the radius that the land's script uses to find it
constexpr glm::vec2 k_UndeadVillage = glm::vec2(3001.083f, 3457.676f) - k_LandMiddle;
constexpr float k_UndeadVillageSearch = 100.0f;
/// The mountain top where the Creed comes, from the middle of the land
constexpr glm::vec2 k_CreedTop = glm::vec2(2694.475f, 2594.948f) - k_LandMiddle;
/// The kinds of thing and of town that the land's script gives to find a village
constexpr float k_TownThing = 9.0f;
constexpr float k_AnyTown = 5000.0f;

/// The player's creature, fully grown, at the place given from the middle of the land
CreatureSetup PlayersTiger(glm::vec2 offset)
{
	return {.label = "player's tiger", .species = CreatureType::Tiger, .offset = offset};
}

void AddSlegFight(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_sleg_fight",
	    .name = "Story: Sleg the ogre fights the creature at his lair on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The set-up script of the land runs, then the "
	                   "script of the ogre. The player's Tiger stands 55 metres from the place where the ogre speaks.",
	    .expected = "The ogre Sleg comes at his lair, with the Guardian Stone under a bonfire smoke beside him. He sits. "
	                "The Tiger is near, so the scene starts: Sleg walks to his side of the fight and says his four "
	                "lines. Then he starts a fight with the Tiger. When the fight ends with no winner, Sleg laughs, "
	                "walks back and the Tiger goes to the temple. Sleg then sits in his lair again.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FightIntro.x - 30.0f, 35.0f, k_FightIntro.y - 40.0f},
	                .look = {k_FightIntro.x, 5.0f, k_FightIntro.y}},
	    .creatures = {PlayersTiger(k_NearTheFight)},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "Land4Ogre",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}

void AddSlegBeatenScroll(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_sleg_scroll_removed",
	    .name = "Story: the gold scroll over Sleg's lair goes when Sleg is beaten on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The gold scroll over the lair is made as the "
	                   "end of the Heartbroken Man's quest makes it, and the ogre's script keeps it. Then the set-up "
	                   "script of the land runs, then the script of the ogre. The player's Tiger stands 55 metres from "
	                   "the place where the ogre speaks. Win the fight for the Tiger.",
	    .expected = "The gold scroll turns above the lair. Sleg says his lines and fights the Tiger. When the Tiger "
	                "wins, the scroll goes before the scene of the beaten ogre starts.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FightIntro.x - 30.0f, 35.0f, k_FightIntro.y - 40.0f},
	                .look = {k_FightIntro.x, 5.0f, k_FightIntro.y}},
	    .creatures = {PlayersTiger(k_NearTheFight)},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_HIGHLIGHT",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = k_GoldScroll},
	                                      {.kind = Argument::Kind::Position, .point = k_OgreScroll},
	                                      {.kind = Argument::Kind::Integer, .number = k_HeartbrokenChallenge}}},
	                 {.kind = Kind::RunScript, .delaySeconds = 0.0f, .script = "SetupLand4"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "Land4Ogre",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                                    {"OgreHighlight", {.kind = Argument::Kind::LastResult}}}}},
	});
}

void AddGremlins(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_gremlins",
	    .name = "Story: the four gremlins of Sleg the ogre on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The set-up script of the land runs, then the "
	                   "script of the ogre, with the flag of the man's story already raised. The player's Tiger stands "
	                   "far from the lair.",
	    .expected = "Sleg comes at his lair. Four small gremlins come round the lair within 15 metres. They walk about a "
	                "place near the lair. Ten minutes after they come, if the home village has more than 10 people and "
	                "no fireballs fall, they go by two points of the road to the home village. They take people back to "
	                "Sleg. The good advisor warns when a gremlin comes within 75 metres of the village.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lair.x - 60.0f, 60.0f, k_Lair.y - 60.0f},
	                .look = {k_Lair.x, 5.0f, k_Lair.y}},
	    .creatures = {PlayersTiger(glm::vec2(2236.0f, 2652.0f) - k_LandMiddle)},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "Land4Ogre",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                                    {"ManHasExplainedAboutNemesis", {.number = 1.0f}}}}},
	});
}

void AddOgreFence(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_ogre_fence",
	    .name = "Story: the creature of the fourth land is sent back from the way to Sleg",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The set-up script of the land runs, then the "
	                   "script that keeps the creature away from the ogre. The player's Tiger is not on the leash. It "
	                   "stands 200 metres from the place where the ogre speaks and 132 metres from his lair.",
	    .expected = "The Tiger is near the way to the ogre, but not near enough to start the fight, and it is not on the "
	                "leash. So the script sends it back to the home village. The Tiger walks to a place within 50 "
	                "metres of the village, and after 3 seconds the script lets it go. It is sent back again each time it "
	                "comes into that ring.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_FenceSpot.x + 40.0f, 60.0f, k_FenceSpot.y - 60.0f},
	                .look = {k_FenceSpot.x, 20.0f, k_FenceSpot.y}},
	    .creatures = {PlayersTiger(k_FenceSpot)},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "KeepCreatureAwayFromOgre",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}

void AddCreatureSentBack(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_creature_sent_back",
	    .name = "Story: the creature that went too near Sleg is sent back to the road on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The player's Tiger stands 132 metres from "
	                   "the lair of the ogre. The script that stops the player's exploring runs with the flag that the "
	                   "creature went too far already raised. The camera is far from the lair and from the Japanese "
	                   "village.",
	    .expected = "The script lowers its watch at once and sends the Tiger back: the Tiger walks to a place on the "
	                "road within 50 metres of the spot near the home village, and stays there.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_RoadRetreat.x - 60.0f, 120.0f, k_RoadRetreat.y + 60.0f},
	                .look = {k_RoadRetreat.x, 8.0f, k_RoadRetreat.y}},
	    .creatures = {PlayersTiger(k_FenceSpot)},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "StopPlayerExploring",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                                    {"CreatureVenturedTooFar", {.number = 1.0f}}}}},
	});
}

void AddSlegEatsVillagers(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_sleg_eats_villagers",
	    .name = "Story: Sleg the ogre eats the villagers near his lair on the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The set-up script of the land runs, then the "
	                   "script of the ogre. The player's Tiger stands far from the lair. Two villagers stand 4 and 8 "
	                   "metres from the place where Sleg comes. 5 seconds after the ogre's script starts, an empty "
	                   "script raises the flag of the man's story. Thus the ogre's first sit starts before the family "
	                   "script starts, as in the game.",
	    .expected = "Sleg comes at his lair with his four gremlins. The family script chooses the nearer villager, and "
	                "Sleg walks to him, picks him up and eats him. Then the script chooses the other villager if he is "
	                "within 50 metres, and orders the meal 20 seconds after the first meal. It does not look again "
	                "where he is. If he walked away to work in that time, Sleg follows him for a short time and stops, "
	                "and the script waits for ever, as in the game.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Lair.x - 30.0f, 30.0f, k_Lair.y - 30.0f},
	                .look = {k_Lair.x, 5.0f, k_Lair.y}},
	    .creatures = {PlayersTiger(glm::vec2(2236.0f, 2652.0f) - k_LandMiddle)},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = k_Lair + glm::vec2(4.0f, 0.0f)},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Lair + glm::vec2(0.0f, 8.0f)}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand4"},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "Land4Ogre",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 5.0f,
	                  .script = "CreatureNullReminder",
	                  .scriptGlobals = {{"ManHasExplainedAboutNemesis", {.number = 1.0f}}}}},
	});
}

void AddUndeadVillageCreed(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l4_undead_village_creed",
	    .name = "Story: the undead village of the fourth land is freed and the Creed comes",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky. The scenario finds the undead village as the "
	                   "land's script finds it. Then the watch of the two totems runs with no totems, so it counts the "
	                   "curse as broken at once.",
	    .expected = "A skeleton villager walks to the middle of the village and goes into the ground in a large puff of "
	                "dust. The village and the man stop being skeletons, and the man speaks three lines. Then the Creed "
	                "comes on the mountain top, with a ring of 30 metres round it that the hand can not go into. The "
	                "camera flies along the path of the land that shows where the Creed is.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_CreedTop.x + 40.0f, 35.0f, k_CreedTop.y + 40.0f},
	                .look = {k_CreedTop.x, 2.0f, k_CreedTop.y}},
	    .creatures = {PlayersTiger(k_CreedTop + glm::vec2(60.0f, 0.0f))},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CALL_NEAR",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = k_TownThing},
	                                      {.kind = Argument::Kind::Integer, .number = k_AnyTown},
	                                      {.kind = Argument::Kind::Position, .point = k_UndeadVillage},
	                                      {.number = k_UndeadVillageSearch},
	                                      {.kind = Argument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.0f,
	                  .script = "LookAfterTotems",
	                  .scriptGlobals = {{"UndeadTown", {.kind = Argument::Kind::LastResult}},
	                                    {"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}
} // namespace

void testbed_scenarios::AddLand4SlegScenarios(std::vector<Scenario>& all)
{
	AddSlegFight(all);
	AddSlegBeatenScroll(all);
	AddGremlins(all);
	AddOgreFence(all);
	AddCreatureSentBack(all);
	AddSlegEatsVillagers(all);
	AddUndeadVillageCreed(all);
}
