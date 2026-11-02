/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of three quests of the fifth land: the crusaders and the dragon in the cave, the brown bear's
// trail of dung, and Nemesis's curse on the creature. Each loads the fifth land and starts the quest's own script.

#include <cstddef>

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using Argument = ScriptArgument;

// Places of the fifth land that the quests' scripts use, from the middle of the land (2720, 3040)
/// The crusaders' camp by the cave's mouth, and their pyre
constexpr glm::vec2 k_Camp {-823.55f, -693.43f};
constexpr glm::vec2 k_Pyre {-836.70f, -690.00f};
/// The five blowholes along the hill over the cave, from the mouth's end, and the cave's exit
constexpr std::array<glm::vec2, 5> k_Blowholes {{
    {-893.86f, -734.20f},
    {-975.06f, -814.73f},
    {-1056.42f, -873.63f},
    {-1138.99f, -917.69f},
    {-1253.93f, -956.15f},
}};
constexpr glm::vec2 k_CaveExit {-1299.04f, -971.28f};
/// The farmer's house at the edge of the Japanese village, and the first pile of dung west of it
constexpr glm::vec2 k_FarmersHouse {-360.68f, 350.02f};
constexpr glm::vec2 k_FirstDung {-465.09f, 346.62f};
/// The eight piles of dung of the brown bear's trail, in the order that the script makes them
constexpr std::array<glm::vec2, 8> k_DungPiles {{
    {-465.09f, 346.62f},
    {-493.62f, 364.89f},
    {-479.39f, 394.66f},
    {-447.47f, 423.43f},
    {-476.78f, 414.88f},
    {-506.38f, 449.39f},
    {-521.07f, 478.46f},
    {-502.60f, 493.42f},
}};
/// Where the brown bear appears past the last pile, where its reward chest falls, and where the bear walks to
constexpr glm::vec2 k_BearAppears {-480.56f, 510.29f};
constexpr glm::vec2 k_BearStands {-480.63f, 534.82f};
/// The Japanese village's number in the land's script
constexpr size_t k_JapaneseVillage = 7;
/// The four Wonders: in Nemesis's Aztec, Greek and Tibetan villages, and in his own village
constexpr std::array<glm::vec2, 4> k_Wonders {{
    {-56.23f, 631.00f},
    {-388.08f, -459.00f},
    {-961.56f, -702.95f},
    {430.26f, -430.18f},
}};

/// The player's creature, at the home town of the fifth land
CreatureSetup PlayersTiger(glm::vec2 offset)
{
	return {.label = "player's tiger",
	        .species = CreatureType::Tiger,
	        .offset = offset,
	        .phase = 3,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

/// A script's call for the building nearest a place, as the land's script finds each Wonder
Command FindBuilding(glm::vec2 at, float delaySeconds)
{
	return {.kind = Kind::CallNative,
	        .delaySeconds = delaySeconds,
	        .script = "CALL",
	        .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 2.0f},
	                            {.kind = Argument::Kind::Integer, .number = 5000.0f},
	                            {.kind = Argument::Kind::Position, .point = at},
	                            {.kind = Argument::Kind::Number, .number = 0.0f}}};
}

/// A script's order to the town at a place to build its Wonder there, with the desire that the land's script gives
Command BuildWonder(glm::vec2 at, float delaySeconds)
{
	return {
	    .kind = Kind::CallNative,
	    .delaySeconds = delaySeconds,
	    .script = "BUILD_BUILDING",
	    .scriptArguments = {{.kind = Argument::Kind::Position, .point = at}, {.kind = Argument::Kind::Number, .number = 0.6f}}};
}
/// The commands that start the brown bear's quest as the land's script starts it, click its scroll, and then pick up
/// the eight piles one after another with the hand, the first some time after the click
std::vector<Command> BrownBearTrail(float firstPileSeconds)
{
	std::vector<Command> commands {
	    {.kind = Kind::GiveLandTown, .delaySeconds = 0.5f, .value = k_JapaneseVillage, .amount = 5.0f},
	    {.kind = Kind::RunScript, .delaySeconds = 10.0f, .script = "SwapToBrownBear"},
	    {.kind = Kind::ClickScroll, .delaySeconds = 2.0f, .point = k_FarmersHouse},
	};
	for (size_t pile = 0; pile < k_DungPiles.size(); ++pile)
	{
		// The evil advisor speaks when the seventh pile is picked up, and the script makes the last pile after the line
		const float delay = pile == 0 ? firstPileSeconds : (pile == k_DungPiles.size() - 1 ? 15.0f : 4.0f);
		commands.push_back({.kind = Kind::HandTakeNear, .delaySeconds = delay, .point = k_DungPiles.at(pile)});
	}
	return commands;
}

/// The commands that make it night and find the land's four Wonders, as the land's script finds them
std::vector<Command> CurseAtNight()
{
	return {{.kind = Kind::SetHour, .delaySeconds = 0.2f, .hour = 23.0f},
	        BuildWonder(k_Wonders[0], 0.3f),
	        BuildWonder(k_Wonders[1], 0.0f),
	        BuildWonder(k_Wonders[2], 0.0f),
	        FindBuilding(k_Wonders[0], 0.5f),
	        FindBuilding(k_Wonders[1], 0.0f),
	        FindBuilding(k_Wonders[2], 0.0f),
	        FindBuilding(k_Wonders[3], 0.0f)};
}

/// The land's script starting the curse with the four Wonders that the scenario found
Command StartCurse()
{
	return {.kind = Kind::RunScript,
	        .delaySeconds = 0.5f,
	        .script = "CreatureCurse",
	        .scriptArguments = {{.kind = Argument::Kind::Made, .index = 0},
	                            {.kind = Argument::Kind::Made, .index = 1},
	                            {.kind = Argument::Kind::Made, .index = 2},
	                            {.kind = Argument::Kind::Made, .index = 3}}};
}

/// A script deletes a Wonder that the scenario found (index), as a blast that removes it does
Command DeleteWonder(size_t index)
{
	return {
	    .kind = Kind::CallNative,
	    .delaySeconds = 0.0f,
	    .script = "OBJECT_DELETE",
	    .scriptArguments = {{.kind = Argument::Kind::Made, .index = index}, {.kind = Argument::Kind::Integer, .number = 0.0f}}};
}
} // namespace

void testbed_scenarios::AddLandFiveQuestScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l5_crusaders_camp",
	    .name = "Story: the crusaders' camp by the dragon's cave on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts. The crusaders' quest starts, and two seconds later the "
	                   "player clicks its scroll over the camp.",
	    .expected = "A small cauldron (a fifth of its size) stands in the camp. Five crusaders sit round the camp, facing "
	                "it. A pile of wood (the pyre) is just outside the cave's mouth. A silver scroll hangs over the camp. "
	                "The click starts the first scene: the leader stands up, says that they will fight the dragon once they "
	                "are healed, a dragon roars in the cave, and the leader asks the player to heal them.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Camp.x + 10.0f, 25.0f, k_Camp.y - 40.0f},
	                .look = {k_Camp.x, 2.0f, k_Camp.y}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "Crusaders"},
	                 {.kind = Kind::ClickScroll, .delaySeconds = 2.0f, .point = k_Camp}},
	});
	all.push_back({
	    .id = "story.l5_crusaders_dragon",
	    .name = "Story: the crusaders smoke out the dragon on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts. The crusaders' quest starts and the player clicks its "
	                   "scroll, and clicks it again 50 seconds later. 60 seconds after the start the player heals the camp, "
	                   "and 150 seconds after the start "
	                   "throws a fireball at the pyre. Then the camera goes to each blowhole in turn, 25 seconds apart, and "
	                   "last to the cave's "
	                   "exit. Click the texts to go on.",
	    .expected = "When all five are healed, the leader asks for the pyre to be lit. The fireball sets the pyre alight: "
	                "smoke pours from it and then from each blowhole in turn, the pyre goes, the crusaders charge into "
	                "the cave and fade at its mouth. At each blowhole the crusaders' voices and the dragon's roar are "
	                "heard in order; at the fourth the camera shakes. At the fifth, three crusaders run out of the exit "
	                "in flames, a flying flock dispenser appears there with an influence ring, the leader gives it to the "
	                "player and the dispenser's own reward scene plays. The second click on the scroll makes the good "
	                "advisor step out and say \"These brave followers are in need of healing, Leader.\"",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Camp.x + 10.0f, 25.0f, k_Camp.y - 40.0f},
	                .look = {k_Camp.x, 2.0f, k_Camp.y}},
	    .miracles = {{.type = MagicType::Heal, .point = k_Camp, .handOffset = k_Camp, .delaySeconds = 60.0f},
	                 {.type = MagicType::Fireball, .point = k_Pyre, .handOffset = k_Pyre, .delaySeconds = 150.0f}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "Crusaders"},
	                 {.kind = Kind::ClickScroll, .delaySeconds = 2.0f, .point = k_Camp},
	                 {.kind = Kind::ClickScroll, .delaySeconds = 50.0f, .point = k_Camp},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 207.5f, .point = k_Blowholes[0]},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 25.0f, .point = k_Blowholes[1]},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 25.0f, .point = k_Blowholes[2]},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 25.0f, .point = k_Blowholes[3]},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 25.0f, .point = k_Blowholes[4]},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 40.0f, .point = k_CaveExit}},
	});
	all.push_back({
	    .id = "story.l5_brown_bear_trail",
	    .name = "Story: the brown bear's trail of dung on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts. The Japanese village becomes the player's, the brown "
	                   "bear quest starts as the land's script starts it then, and two seconds later the player clicks its "
	                   "scroll over the "
	                   "farmer's house. Pick up each pile of dung with the hand to go on.",
	    .expected = "A pile of dung with flies over it lies about 100 metres west of the house. A Japanese farmer comes "
	                "out, retches and asks the player to check the stench from the forest. Each pile that the hand picks "
	                "up goes at once, its flies stop, and the next pile appears with its own flies. After the eighth, a "
	                "brown bear appears with a sparkle, a heal reward chest falls where it appeared, and the bear then "
	                "offers itself for a creature swap.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_FirstDung, k_FarmersHouse}},
	    .commands = {{.kind = Kind::GiveLandTown, .delaySeconds = 0.5f, .value = k_JapaneseVillage, .amount = 5.0f},
	                 {.kind = Kind::RunScript, .delaySeconds = 10.0f, .script = "SwapToBrownBear"},
	                 {.kind = Kind::ClickScroll, .delaySeconds = 2.0f, .point = k_FarmersHouse}},
	});
	all.push_back({
	    .id = "story.l5_curse_strikes",
	    .name = "Story: Nemesis's curse strikes the creature at night on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts, with the player's tiger near the Greek Wonder. The hour "
	                   "jumps to 23:00. The towns are told to build their Wonders as the land's script tells them, and the "
	                   "curse starts with the land's four Wonders, as the land's script starts it.",
	    .expected = "At once (it is night) a dust cloud covers the tiger for 8 seconds. Its strength falls by 0.16, its "
	                "height by 1 and its alignment moves 0.4 away from the player's, each with a short wobble. The tiger "
	                "is frightened, both advisors cling to the edges of the screen and one of them asks what is wrong "
	                "with the creature. When the fright ends the tiger looks at the hand. Then the good advisor says the "
	                "creature seems to be all right. The curse does not strike again until a day has passed.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {PlayersTiger(k_Wonders[1] + glm::vec2(30.0f, -30.0f))},
	    .commands =
	        [] {
		        auto commands = CurseAtNight();
		        commands.push_back(StartCurse());
		        return commands;
	        }(),
	});
	all.push_back({
	    .id = "story.l5_curse_pull_leashed",
	    .name = "Story: the curse pulls a leashed creature towards Nemesis's village on the fifth land",
	    .facet = Facet::Story,
	    .description = "As the curse scenario, but the player's tiger is on a rope leash when the curse strikes at 23:00.",
	    .expected = "The curse strikes as in the curse scenario. When the tiger has looked at the hand, it stares towards "
	                "Nemesis's village in the east, points at it, and is frightened again. It does not walk there. Then "
	                "the good advisor says the creature seems to be all right.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {PlayersTiger(k_Wonders[1] + glm::vec2(30.0f, -30.0f))},
	    .commands =
	        [] {
		        std::vector<Command> commands {
		            {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 0.1f, .leash = LeashType::Rope}};
		        std::ranges::copy(CurseAtNight(), std::back_inserter(commands));
		        commands.push_back(StartCurse());
		        return commands;
	        }(),
	});
	all.push_back({
	    .id = "story.l5_curse_only_pull",
	    .name = "Story: the curse with only Nemesis's own Wonder left on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts. The Wonders of the Aztec, Greek and Tibetan villages "
	                   "are found and deleted before the curse starts at 23:00. After 90 seconds the hour jumps to 12:00, "
	                   "and 20 seconds later to 23:00 again.",
	    .expected = "On the first night the tiger is frightened and its alignment moves 0.4 towards good, but its "
	                "strength and height stay. It walks off east towards Nemesis's village until the good advisor's line "
	                "is read. On the second night there is no fright and no line: the tiger only walks off towards the "
	                "village again.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {PlayersTiger(k_Wonders[1] + glm::vec2(30.0f, -30.0f))},
	    .commands =
	        [] {
		        auto commands = CurseAtNight();
		        commands.push_back(DeleteWonder(0));
		        commands.push_back(DeleteWonder(1));
		        commands.push_back(DeleteWonder(2));
		        commands.push_back(StartCurse());
		        commands.push_back({.kind = Kind::SetHour, .delaySeconds = 90.0f, .hour = 12.0f});
		        commands.push_back({.kind = Kind::SetHour, .delaySeconds = 20.0f, .hour = 23.0f});
		        return commands;
	        }(),
	});
	all.push_back({
	    .id = "story.l5_brown_bear_found",
	    .name = "Story: the brown bear is found and offered on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts. The brown bear quest starts and its scroll is clicked. "
	                   "50 seconds later the hand picks up the eight piles of dung one after another. 45 seconds after the "
	                   "last pile the player opens the reward chest, and 15 seconds later clicks the bear's scroll while the "
	                   "tiger is far away. The tiger then walks to "
	                   "the bear, the player clicks the bear's scroll again, and then clicks the bear to say yes. Click "
	                   "the texts to go on.",
	    .expected = "The trail plays as in the trail scenario. After the last pile the bear appears with a sparkle and "
	                "the short epic music. The bear walks north, looks back at where it appeared, and then turns to the "
	                "camera while a heal reward chest falls there. The challenge closes as a success. The first click on "
	                "the bear's scroll gives \"You'll need to bring our Creature, Boss.\" The second makes the tiger walk "
	                "to the bear, both look at each other and the good advisor asks if the player is sure. The click on "
	                "the bear moves the tiger's mind into the bear.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_FirstDung, k_BearStands}},
	    .creatures = {PlayersTiger(k_FarmersHouse + glm::vec2(60.0f, 0.0f))},
	    .commands =
	        [] {
		        auto commands = BrownBearTrail(50.0f);
		        commands.push_back({.kind = Kind::TapRewardNear, .delaySeconds = 45.0f, .point = k_BearAppears});
		        commands.push_back({.kind = Kind::ClickScroll, .delaySeconds = 15.0f, .point = k_BearStands});
		        commands.push_back({.kind = Kind::WalkTo,
		                            .creature = 0,
		                            .delaySeconds = 10.0f,
		                            .point = k_BearStands + glm::vec2(8.0f, -12.0f)});
		        // The second click comes as soon as the tiger stands near the bear, before it walks off on its own
		        commands.push_back({.kind = Kind::ClickScroll,
		                            .creature = 0,
		                            .delaySeconds = 0.5f,
		                            .waitUntilFree = true,
		                            .point = k_BearStands});
		        commands.push_back({.kind = Kind::ClickCreatureNear, .delaySeconds = 30.0f, .point = k_BearStands});
		        return commands;
	        }(),
	});
	all.push_back({
	    .id = "story.l5_brown_bear_house_hurt",
	    .name = "Story: the farmer's house is hurt during the brown bear's trail on the fifth land",
	    .facet = Facet::Story,
	    .description =
	        "The fifth land loads with its own scripts. The brown bear quest starts and its scroll is clicked. "
	        "70 seconds after the start, when the farmer's scene is over, the player throws a fireball at the "
	        "farmer's house. 80 seconds after the first click the player clicks the scroll again. Click the texts to go on.",
	    .expected =
	        "The fireball hurts the house. The good advisor steps out, points at the house and says \"The owner of "
	        "that house will be a little distraught.\" only once. The second click on the scroll makes the good advisor "
	        "step out and say \"The people here are complaining about the stench.\"",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_FirstDung, k_FarmersHouse}},
	    .miracles =
	        {{.type = MagicType::Fireball, .point = k_FarmersHouse, .handOffset = k_FarmersHouse, .delaySeconds = 70.0f}},
	    .commands =
	        [] {
		        auto commands = BrownBearTrail(600.0f);
		        // The player clicks the scroll again after the fireball, which plays the quest's reminder
		        commands.insert(commands.begin() + 3,
		                        {.kind = Kind::ClickScroll, .delaySeconds = 80.0f, .point = k_FarmersHouse});
		        return commands;
	        }(),
	});
	all.push_back({
	    .id = "story.l5_scrolls_wait",
	    .name = "Story: the scrolls of the crusaders and the brown bear wait for a click on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts. The crusaders' quest starts, and 90 seconds later the "
	                   "camera goes to the farmer's house and the brown bear quest starts. Nobody clicks a scroll.",
	    .expected = "While the camera is near the camp, the evil advisor steps out, points at the camp's scroll and says "
	                "\"Hey. There's a job for you. Wanna do it?\", at most once every 30 seconds. Near the farmer's house, "
	                "the good advisor steps out, points at its scroll and says \"We've got something to do. Let's see "
	                "what it is.\", at most once every 30 seconds.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Camp.x + 10.0f, 25.0f, k_Camp.y - 40.0f},
	                .look = {k_Camp.x, 2.0f, k_Camp.y}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "Crusaders"},
	                 {.kind = Kind::GiveLandTown, .delaySeconds = 80.0f, .value = k_JapaneseVillage, .amount = 5.0f},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 10.0f, .point = k_FarmersHouse},
	                 {.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SwapToBrownBear"}},
	});
}
