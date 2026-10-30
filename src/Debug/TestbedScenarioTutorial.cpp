/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the tutorial's lessons: the help texts of a lesson stacking up in the dialogue box, and a
// hand demonstration's click landing on the thing of its kind nearest where it was recorded, a wood pile or a totem

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

/// Where the worship lesson's recording takes hold of the Norse town's totem, in metres on the map. The town centre
/// stands there, as its totem point is over its middle.
constexpr glm::vec2 k_RecordedTotem {3233.32f, 3398.76f};
/// The player's temple stands well clear of the town centre, and the town has one villager, so that it has a worship site
constexpr glm::vec2 k_TotemTemple {0.0f, -60.0f};
constexpr glm::vec2 k_TotemVillager {10.0f, -10.0f};
/// The town believes in the player, so that it stays theirs
constexpr float k_TotemTownBelief = 1000.0f;

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

	const glm::vec2 centre = k_RecordedTotem - flat_land::k_MapMiddle;
	all.push_back({
	    .id = "tutorial.demo_click_totem",
	    .name = "A hand demonstration's click finding the town's totem",
	    .facet = Facet::Tutorial,
	    .description = "The player's Norse town centre stands where the worship lesson's recording takes hold of the "
	                   "totem, with the player's temple and the town's worship site built and no one at worship. The "
	                   "worship lesson's totem hand demonstration plays. The camera follows the recording.",
	    .expected = "When the recorded hand presses its button the click lands on the totem, even when the cursor is "
	                "over the ground beside it: the hand takes hold of the totem and raises it, and when the hand lets "
	                "go the totem eases up to the share it was raised to.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = centre},
	                {.type = VillagerInfo::NorseHousewifeFemale, .offset = centre + k_TotemVillager, .joinTown = true}},
	    .temples = {{.offset = centre + k_TotemTemple}},
	    .commands = {{.kind = Kind::PlayHandDemo, .delaySeconds = 1.0f, .handDemo = "totem"}},
	    .townBeliefInPlayer = k_TotemTownBelief,
	    .worshipShare = 0.0f,
	});
}
