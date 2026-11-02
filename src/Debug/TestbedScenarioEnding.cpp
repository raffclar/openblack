/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios of the end of the story on the fifth land: the fight with the mirror creature, the walk into
// the volcano, the fall of the temple of Nemesis and the credits

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
constexpr std::string_view k_FifthLand = "Land5.txt";

/// The player's tiger, fully grown and well
const CreatureSetup k_PlayersTiger {
    .label = "player's tiger",
    .species = CreatureType::Tiger,
    .phase = 3,
    .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
};

const Framing k_LandOverview {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}};

/// The land's set-up script runs first, so that Nemesis has his creature, then the script of one part of the end
std::vector<Command> SetUpThenRun(std::string_view script)
{
	using Kind = Command::Kind;
	return {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SetupLand5"},
	        {.kind = Kind::RunScript, .delaySeconds = 2.0f, .script = script}};
}
} // namespace

void testbed_scenarios::AddEndingScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.l5_mirror_creature",
	    .name = "Story: Nemesis makes the mirror creature and the last fight starts",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky, and the player's Tiger stands on it. The set-up "
	                   "script of the land runs, so that Nemesis has his creature. Then the script of the last fight "
	                   "runs.",
	    .expected = "A cut scene with the music of Nemesis. The advisors cheer and go home. Nemesis speaks to the Tiger. "
	                "Glittering smoke turns round the creature of Nemesis, which goes invisible. A new Tiger of Nemesis, "
	                "named Mirror, fades in over 6 seconds with the opposite alignment, and the old creature fades away. "
	                "The log shows the gold scroll. When the cut scene ends, the two Tigers fight.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = k_LandOverview,
	    .creatures = {k_PlayersTiger},
	    .commands = SetUpThenRun("TheBigFightMain"),
	});
	all.push_back({
	    .id = "story.l5_into_the_volcano",
	    .name = "Story: the creature walks into the volcano of the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky, and the player's Tiger stands on it. The set-up "
	                   "script of the land runs, then the script of the walk into the volcano.",
	    .expected = "A cut scene with the music of Nemesis. The Tiger walks along the causeway; the screen shakes and it "
	                "looks up. It walks to the top, the creeds fuse and the game goes to half speed. At 80% through the "
	                "animation the moan and the end music start. At 90% the film of the fall plays, if there is one, then "
	                "the speed and the sound come back and the Tiger is put far across the land.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = k_LandOverview,
	    .creatures = {k_PlayersTiger},
	    .commands = SetUpThenRun("EndOfGameCreatureSequenceGoToVolcano"),
	});
	all.push_back({
	    .id = "story.l5_temple_of_nemesis_falls",
	    .name = "Story: the temple of Nemesis falls and the creature comes back",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky, and the player's Tiger stands on it. The set-up "
	                   "script of the land runs, then the script of the fall of the temple of Nemesis.",
	    .expected = "The hand of Nemesis flies to his temple. Rings of beams turn round it, and the temple explodes twice, "
	                "20 seconds apart. The advisors speak. The Tiger lies burnt near the Greek town in four clouds of "
	                "smoke, at a tenth of its health. When it is healed above 80%, the creed glows go from its hands, and "
	                "a last cut scene shows it praying while the screen fades to black.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = k_LandOverview,
	    .creatures = {k_PlayersTiger},
	    .commands = SetUpThenRun("NemesisCitadelDestroyed"),
	});
	all.push_back({
	    .id = "story.credits",
	    .name = "Story: the closing credits on the fifth land",
	    .facet = Facet::Story,
	    .description = "The fifth land loads with its own scripts and sky. The set-up script of the land runs, then the "
	                   "script of the closing credits.",
	    .expected = "With the outro music and no sound effects, 17 windows open in turn on the left and the right of a "
	                "black screen. Each shows a small scene of the people of the story, with the job titles in grey and "
	                "the names in white. Then the crowd of the story's people claps, fireworks go off and the camera moves "
	                "through the crowd. A small boy runs to his mother. The scene holds until the space bar is pressed.",
	    .environment = {.land = Land::Story, .storyLand = k_FifthLand, .dispenserGrid = false},
	    .framing = k_LandOverview,
	    .creatures = {k_PlayersTiger},
	    .commands = SetUpThenRun("VillageWavingSequence"),
	});
}
