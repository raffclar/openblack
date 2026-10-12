/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the tutorial's lessons: the help texts of a lesson stacking up in the dialogue box, and a
// hand demonstration's click landing on the thing of its kind nearest where it was recorded

#include <vector>

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The drag lesson's four help texts, by the scripts' numbers, in the order its script shows them
constexpr uint32_t k_DragLessonFirstText = 5128;
constexpr uint32_t k_DragLessonTexts = 4;

/// Where the wood lesson's recording takes wood from the storage pit's wood pile, in metres on the map, and the kind of
/// pile it was: the scripts' third wood pile
constexpr glm::vec2 k_RecordedWoodPile {1830.12f, 2642.61f};
/// The pile is put a little way off the recorded place, within the reach a replayed click looks over
constexpr glm::vec2 k_WoodPileShift {2.5f, 0.0f};

Command ShowText(uint32_t text, float delay)
{
	return {.kind = Kind::RunText, .delaySeconds = delay, .value = text};
}
} // namespace

void testbed_scenarios::AddTutorialScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> texts;
	for (uint32_t i = 0; i < k_DragLessonTexts; ++i)
	{
		texts.push_back(ShowText(k_DragLessonFirstText + i, i == 0 ? 1.0f : 4.0f));
	}
	all.push_back({
	    .id = "tutorial.help_text_stack",
	    .name = "A lesson's help texts stacking up",
	    .facet = Facet::Tutorial,
	    .description = "The drag lesson's four help texts are shown one after another, four seconds apart, as its script "
	                   "shows them.",
	    .expected = "Each new text slides in at the top of the dark box above the bottom of the screen, full size, and the "
	                "ones before it move down, each smaller and fainter than the one above it. The older texts are cut off "
	                "at the bottom edge of the box; nothing is drawn above the box's top edge.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = texts,
	});

	const glm::vec2 pile = k_RecordedWoodPile + k_WoodPileShift - flat_land::k_MapMiddle;
	all.push_back({
	    .id = "tutorial.demo_click_retarget",
	    .name = "A hand demonstration's click finding its wood pile",
	    .facet = Facet::Tutorial,
	    .description = "A wood pile is put two and a half metres east of where the wood lesson's recording takes wood from, "
	                   "and the wood lesson's hand demonstration plays. The camera follows the recording.",
	    .expected = "When the recorded hand presses its button the click lands on the wood pile nearby, even when the "
	                "cursor is over the ground beside it: the hand scoops wood from the pile and later lets it go there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .objects = {{.type = PotInfo::WoodPile_3, .offset = pile, .amount = 400}},
	    .commands = {{.kind = Kind::PlayHandDemo, .delaySeconds = 1.0f, .handDemo = "givewood"}},
	});
}
