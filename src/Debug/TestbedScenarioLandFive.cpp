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

#include <array>

#include <glm/vec2.hpp>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using Argument = ScriptArgument;

// Places of the fifth land that the quests' scripts use, from the middle of the map
/// The crusaders' camp by the cave's mouth, and their pyre
constexpr glm::vec2 k_Camp {-663.55f, -213.43f};
constexpr glm::vec2 k_Pyre {-676.70f, -210.00f};
/// The five blowholes along the hill over the cave, from the mouth's end, and the cave's exit
constexpr std::array<glm::vec2, 5> k_Blowholes {{
    {-733.87f, -254.20f},
    {-815.06f, -334.73f},
    {-896.42f, -393.63f},
    {-978.99f, -437.69f},
    {-1093.93f, -476.15f},
}};
constexpr glm::vec2 k_CaveExit {-1158.51f, -496.69f};
/// The farmer's house at the edge of the Japanese village, and the first pile of dung west of it
constexpr glm::vec2 k_FarmersHouse {-200.68f, 830.02f};
constexpr glm::vec2 k_FirstDung {-305.09f, 826.62f};
/// The four Wonders: in Nemesis's Aztec, Greek and Tibetan villages, and in his own village
constexpr std::array<glm::vec2, 4> k_Wonders {{
    {103.77f, 1111.00f},
    {-228.08f, 21.00f},
    {-801.56f, -222.95f},
    {590.26f, 49.82f},
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
	                   "scroll. 60 seconds after the start the player heals the camp, and 150 seconds after the start "
	                   "throws a fireball at the pyre. Then the camera goes to each blowhole in turn, 25 seconds apart, and "
	                   "last to the cave's "
	                   "exit. Click the texts to go on.",
	    .expected = "When all five are healed, the leader asks for the pyre to be lit. The fireball sets the pyre alight: "
	                "smoke pours from it and then from each blowhole in turn, the pyre goes, the crusaders charge into "
	                "the cave and fade at its mouth. At each blowhole the crusaders' voices and the dragon's roar are "
	                "heard in order; at the fourth the camera shakes. At the fifth, three crusaders run out of the exit "
	                "in flames, a flying flock dispenser appears there with an influence ring, the leader gives it to the "
	                "player and the dispenser's own reward scene plays.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Camp.x + 10.0f, 25.0f, k_Camp.y - 40.0f},
	                .look = {k_Camp.x, 2.0f, k_Camp.y}},
	    .miracles = {{.type = MagicType::Heal, .point = k_Camp, .handOffset = k_Camp, .delaySeconds = 60.0f},
	                 {.type = MagicType::Fireball, .point = k_Pyre, .handOffset = k_Pyre, .delaySeconds = 150.0f}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "Crusaders"},
	                 {.kind = Kind::ClickScroll, .delaySeconds = 2.0f, .point = k_Camp},
	                 {.kind = Kind::CameraLooksAt, .delaySeconds = 257.5f, .point = k_Blowholes[0]},
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
	    .description = "The fifth land loads with its own scripts. The brown bear quest starts as when the Japanese "
	                   "village becomes the player's, and two seconds later the player clicks its scroll over the "
	                   "farmer's house. Pick up each pile of dung with the hand to go on.",
	    .expected = "A pile of dung with flies over it lies about 100 metres west of the house. A Japanese farmer comes "
	                "out, retches and asks the player to check the stench from the forest. Each pile that the hand picks "
	                "up goes at once, its flies stop, and the next pile appears with its own flies. After the eighth, a "
	                "brown bear appears with a sparkle, a heal reward chest falls where it appeared, and the bear then "
	                "offers itself for a creature swap.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_FirstDung, k_FarmersHouse}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SwapToBrownBear"},
	                 {.kind = Kind::ClickScroll, .delaySeconds = 2.0f, .point = k_FarmersHouse}},
	});
	all.push_back({
	    .id = "story.l5_curse_strikes",
	    .name = "Story: Nemesis's curse strikes the creature at night on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts at 23:00, with the player's tiger near the Greek Wonder. "
	                   "The curse starts with the land's four Wonders, as the land's script starts it.",
	    .expected = "At once (it is night) a dust cloud covers the tiger for 8 seconds. Its strength falls by 0.16, its "
	                "height by 1 and its alignment moves 0.4 away from the player's, each with a short wobble. The tiger "
	                "is frightened, both advisors cling to the edges of the screen and one of them asks what is wrong "
	                "with the creature. Then the good advisor says the creature seems to be all right. The curse does "
	                "not strike again until a day has passed.",
	    .environment = {.land = Land::Story, .storyLand = "Land5.txt", .hour = 23.0f, .dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {PlayersTiger(k_Wonders[1] + glm::vec2(30.0f, -30.0f))},
	    .commands = {FindBuilding(k_Wonders[0], 0.5f),
	                 FindBuilding(k_Wonders[1], 0.0f),
	                 FindBuilding(k_Wonders[2], 0.0f),
	                 FindBuilding(k_Wonders[3], 0.0f),
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "CreatureCurse",
	                  .scriptArguments = {{.kind = Argument::Kind::Made, .index = 0},
	                                      {.kind = Argument::Kind::Made, .index = 1},
	                                      {.kind = Argument::Kind::Made, .index = 2},
	                                      {.kind = Argument::Kind::Made, .index = 3}},
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}
