/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/EffectsAfterParticles.h"
#include "Graphics/WorldLabelGroup.h"

using namespace openblack::graphics;
using effects_after_particles::k_Order;
using effects_after_particles::Step;

namespace
{

std::ptrdiff_t PlaceOf(Step step)
{
	return std::distance(k_Order.begin(), std::ranges::find(k_Order, step));
}

} // namespace

TEST(EffectsAfterParticles, TheShieldsComeBeforeTheVillageCentres)
{
	EXPECT_EQ(PlaceOf(Step::ShieldDomes), 0);
	EXPECT_EQ(PlaceOf(Step::TownColumns), 1);
}

TEST(EffectsAfterParticles, TheFloatingNumbersComeAfterTheVillageCentresAndBeforeTheRain)
{
	EXPECT_GT(PlaceOf(Step::FloatingNumbers), PlaceOf(Step::TownColumns));
	EXPECT_LT(PlaceOf(Step::FloatingNumbers), PlaceOf(Step::Rain));
}

TEST(EffectsAfterParticles, TheNamesComeAfterTheWeatherAndTheTipBubbleLast)
{
	EXPECT_LT(PlaceOf(Step::Rain), PlaceOf(Step::Snowfall));
	EXPECT_GT(PlaceOf(Step::VillagerNames), PlaceOf(Step::Snowfall));
	EXPECT_EQ(PlaceOf(Step::TipBubble), static_cast<std::ptrdiff_t>(k_Order.size()) - 1);
}

TEST(EffectsAfterParticles, EachGroupOfLabelsComesOneTimeInTheOrderOfTheGroups)
{
	std::vector<WorldLabelGroup> groups;
	for (const auto step : k_Order)
	{
		if (const auto labels = effects_after_particles::LabelsOf(step); labels.has_value())
		{
			groups.push_back(*labels);
		}
	}
	const std::vector expected {WorldLabelGroup::FloatingNumbers, WorldLabelGroup::VillagerNames, WorldLabelGroup::TipBubble};
	EXPECT_EQ(groups, expected);
}

TEST(WorldLabelGroups, TheLandsLabelsComeBeforeTheLabelsAfterTheParticles)
{
	// The creatures add their chat bubbles as the land draws them, and the land then draws the learning words
	EXPECT_LT(WorldLabelGroup::ChatBubbles, WorldLabelGroup::LearningWords);
	EXPECT_LT(WorldLabelGroup::LearningWords, WorldLabelGroup::FloatingNumbers);
	EXPECT_EQ(static_cast<std::size_t>(WorldLabelGroup::TipBubble) + 1, k_WorldLabelGroupCount);
	// The steps after the particles hold none of the land's labels
	for (const auto step : k_Order)
	{
		const auto labels = effects_after_particles::LabelsOf(step);
		EXPECT_NE(labels, std::optional {WorldLabelGroup::ChatBubbles});
		EXPECT_NE(labels, std::optional {WorldLabelGroup::LearningWords});
	}
}

TEST(WorldLabelSubmits, TheLandsLabelsWaitUntilTheScenePassSubmitsThem)
{
	WorldLabelSubmits submits;
	submits.Drawn();
	EXPECT_TRUE(submits.Take(WorldLabelGroup::ChatBubbles));
	EXPECT_TRUE(submits.Take(WorldLabelGroup::LearningWords));
	EXPECT_FALSE(submits.Take(WorldLabelGroup::LearningWords));
}

TEST(EffectsAfterParticles, TheEffectsAreNoGroupOfLabels)
{
	EXPECT_FALSE(effects_after_particles::LabelsOf(Step::ShieldDomes).has_value());
	EXPECT_FALSE(effects_after_particles::LabelsOf(Step::TownColumns).has_value());
	EXPECT_FALSE(effects_after_particles::LabelsOf(Step::Rain).has_value());
	EXPECT_FALSE(effects_after_particles::LabelsOf(Step::Snowfall).has_value());
}

TEST(WorldLabelSubmits, NoGroupWaitsBeforeAFrameDrawsThem)
{
	WorldLabelSubmits submits;
	EXPECT_FALSE(submits.Take(WorldLabelGroup::FloatingNumbers));
	EXPECT_FALSE(submits.Take(WorldLabelGroup::TipBubble));
}

TEST(WorldLabelSubmits, EachGroupSubmitsOneTimeInAFrame)
{
	WorldLabelSubmits submits;
	submits.Drawn();
	EXPECT_TRUE(submits.Take(WorldLabelGroup::VillagerNames));
	EXPECT_FALSE(submits.Take(WorldLabelGroup::VillagerNames));
	// The other groups still wait, for the caller to submit after the scene
	EXPECT_TRUE(submits.Take(WorldLabelGroup::FloatingNumbers));
	EXPECT_TRUE(submits.Take(WorldLabelGroup::TipBubble));
	// The next frame draws them again
	submits.Drawn();
	EXPECT_TRUE(submits.Take(WorldLabelGroup::VillagerNames));
}
