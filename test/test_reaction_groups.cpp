/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/LivingReaction.h"
#include "ECS/ReactionGroups.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The members of the group of a reaction as a walk of all living things counts them
uint32_t Walked(const Registry& registry, uint32_t reaction)
{
	uint32_t members = 0;
	registry.Each<const LivingReaction>([&members, reaction](entt::entity /*unused*/, const LivingReaction& state) {
		if (state.reaction == reaction && state.left != reaction)
		{
			++members;
		}
	});
	return members;
}

entt::entity Reacting(Registry& registry, uint32_t reaction)
{
	const auto living = registry.Create();
	registry.Assign<LivingReaction>(living).reaction = reaction;
	reaction_groups::Changed(registry, living);
	return living;
}

/// A reader of the count in the signal of a reaction component that goes, as a death reads the group of its reaction
struct CountInSignal
{
	const Registry* registry {nullptr};
	uint32_t reaction {0};
	std::vector<uint32_t> members;
	std::vector<uint32_t> walked;
	void Gone(entt::registry& /*unused*/, entt::entity /*unused*/)
	{
		members.push_back(reaction_groups::MembersOf(*registry, reaction));
		walked.push_back(Walked(*registry, reaction));
	}
};

} // namespace

TEST(ReactionGroups, TheCountsFollowJoinsLeavesAndStops)
{
	Registry registry;
	std::vector<entt::entity> living;
	for (uint32_t i = 0; i < 6; ++i)
	{
		living.push_back(Reacting(registry, i < 4 ? 7 : 9));
	}
	EXPECT_EQ(reaction_groups::MembersOf(registry, 7), 4u);
	EXPECT_EQ(reaction_groups::MembersOf(registry, 9), 2u);

	// One leaves the group but keeps the reaction, one takes up the other reaction, one stops
	registry.Get<LivingReaction>(living[0]).left = 7;
	reaction_groups::Changed(registry, living[0]);
	registry.Get<LivingReaction>(living[1]).reaction = 9;
	reaction_groups::Changed(registry, living[1]);
	registry.Get<LivingReaction>(living[4]).reaction = 0;
	reaction_groups::Changed(registry, living[4]);
	EXPECT_EQ(reaction_groups::MembersOf(registry, 7), Walked(registry, 7));
	EXPECT_EQ(reaction_groups::MembersOf(registry, 9), Walked(registry, 9));
	EXPECT_EQ(reaction_groups::MembersOf(registry, 7), 2u);

	// One that left a group and then stops counts in the group of no reaction, as the walk counts it
	registry.Get<LivingReaction>(living[0]).reaction = 0;
	reaction_groups::Changed(registry, living[0]);
	EXPECT_EQ(reaction_groups::MembersOf(registry, 0), Walked(registry, 0));
	EXPECT_TRUE(reaction_groups::CountsMatchWalk(registry));

	// A direct write to the stores makes the counts come again from a walk
	registry.Get<LivingReaction>(living[2]).reaction = 9;
	registry.StoresWritten();
	EXPECT_EQ(reaction_groups::MembersOf(registry, 9), Walked(registry, 9));
	EXPECT_TRUE(reaction_groups::CountsMatchWalk(registry));
}

TEST(ReactionGroups, ALivingThingThatGoesCountsUntilItsSignalEnds)
{
	Registry registry;
	std::vector<entt::entity> living;
	for (uint32_t i = 0; i < 4; ++i)
	{
		living.push_back(Reacting(registry, 3));
	}
	CountInSignal reader {.registry = &registry, .reaction = 3};
	registry.Underlying().on_destroy<LivingReaction>().connect<&CountInSignal::Gone>(reader);
	for (const auto entity : living)
	{
		registry.Destroy(entity);
	}
	EXPECT_EQ(reader.members, reader.walked);
	EXPECT_EQ(reader.members, (std::vector<uint32_t> {4, 3, 2, 1}));
	EXPECT_EQ(reaction_groups::MembersOf(registry, 3), 0u);
	EXPECT_TRUE(reaction_groups::CountsMatchWalk(registry));
}

TEST(ReactionGroups, TheCountsComeAgainAfterAReset)
{
	Registry registry;
	Reacting(registry, 5);
	EXPECT_EQ(reaction_groups::MembersOf(registry, 5), 1u);
	registry.Reset();
	EXPECT_EQ(reaction_groups::MembersOf(registry, 5), 0u);
	Reacting(registry, 5);
	Reacting(registry, 5);
	EXPECT_EQ(reaction_groups::MembersOf(registry, 5), 2u);
	EXPECT_TRUE(reaction_groups::CountsMatchWalk(registry));
}
