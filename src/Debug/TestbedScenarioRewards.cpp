/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the reward chests the challenges give: a row of the common kinds dropping from the sky in
// front of the camera, to be tapped open one by one, and a single chest standing close up for watching its lid lift

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The row of chests runs east to west in front of the camera, this far apart
constexpr float k_Spacing = 6.0f;
constexpr float k_RowNorth = 20.0f;

constexpr glm::vec2 RowSpot(int index)
{
	return {(static_cast<float>(index) - 2.5f) * k_Spacing, k_RowNorth};
}
} // namespace

void testbed_scenarios::AddRewardScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "story.reward_chests",
	    .name = "Reward chests of each common kind",
	    .facet = Facet::Miracles,
	    .description = "Six reward chests for the player drop from the sky in a row in front of the camera, a second "
	                   "apart, as a challenge's script drops them: large food, large wood, a toy ball, a weeping stone, "
	                   "the heal seed and the heal power-up on its own. Tap each with the Action button to open it.",
	    .expected = "Each falls 150 m in 3 seconds turning half a round a second, thumps down with its landing sound and a "
	                "puff of sandy dust. A tap plays the tap sound; on the land it then plays the two opening sounds, the "
	                "closed chest gives way to the open one and its lid lifts once. Food and wood are poured at the chest "
	                "onto a new pile; the ball and the weeping stone appear where the chest stands; the heal seed floats "
	                "a one-shot globe 2 m above the chest when the player can't cast heal, and its power-up's gesture "
	                "shows on the land beside the chest when they can; the power-up chest shows only its gesture. A "
	                "second tap on an opened chest does nothing. Each opened chest goes 200 game turns later.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 22.0f, -14.0f}, .look = {0.0f, 0.0f, k_RowNorth}},
	    .rewards = {{.type = RewardObjectInfo::LargeFood, .offset = RowSpot(0), .delaySeconds = 1.0f},
	                {.type = RewardObjectInfo::LargeWood, .offset = RowSpot(1), .delaySeconds = 2.0f},
	                {.type = RewardObjectInfo::RewardObjectToyBall, .offset = RowSpot(2), .delaySeconds = 3.0f},
	                {.type = RewardObjectInfo::WeepingStone, .offset = RowSpot(3), .delaySeconds = 4.0f},
	                {.type = RewardObjectInfo::SpellSeedHeal, .offset = RowSpot(4), .delaySeconds = 5.0f},
	                {.type = RewardObjectInfo::HealPu1, .offset = RowSpot(5), .delaySeconds = 6.0f}},
	});

	all.push_back({
	    .id = "story.reward_chest_opens",
	    .name = "A reward chest opening close up",
	    .facet = Facet::Miracles,
	    .description = "A toy ball reward chest stands on the land close in front of the camera, as a challenge's script "
	                   "puts one down without dropping it. Tap it with the Action button.",
	    .expected = "The tap plays the tap sound and the two opening sounds; the closed chest gives way to the open one, "
	                "whose lid lifts once and stays open, and the toy ball appears where it stands. The opened chest goes "
	                "200 game turns later.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -7.0f}, .look = {0.0f, 0.5f, 0.0f}},
	    .rewards = {{.type = RewardObjectInfo::RewardObjectToyBall, .offset = {0.0f, 0.0f}, .fromSky = false}},
	});
}
