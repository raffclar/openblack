/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Magic/Impressiveness.h"
#include "Magic/VillagerReactionRules.h"

using namespace openblack::magic;
using namespace openblack::magic::villager_reaction;

namespace
{
/// A watcher who might become the mate of the one held: every check passes
constexpr VillagerInHandWatch k_Mate {.heldIsVillager = true,
                                      .heldInHand = true,
                                      .watcherSexuallyActive = true,
                                      .otherSex = true,
                                      .samePlayer = true,
                                      .watcherScripted = false};
} // namespace

TEST(VillagerPickup, AMateToBeHeedsTheOneHeldByTheBreedersPriority)
{
	EXPECT_EQ(VillagerInHandPriority(k_Mate, 120), 120);
	// The priority is read as a byte
	EXPECT_EQ(VillagerInHandPriority(k_Mate, 0x1A5), 0xA5);
}

TEST(VillagerPickup, AnyFailedCheckLeavesTheWatcherBe)
{
	auto watch = k_Mate;
	watch.heldIsVillager = false;
	EXPECT_EQ(VillagerInHandPriority(watch, 120), 0);
	watch = k_Mate;
	watch.heldInHand = false;
	EXPECT_EQ(VillagerInHandPriority(watch, 120), 0);
	watch = k_Mate;
	watch.watcherSexuallyActive = false;
	EXPECT_EQ(VillagerInHandPriority(watch, 120), 0);
	watch = k_Mate;
	watch.otherSex = false;
	EXPECT_EQ(VillagerInHandPriority(watch, 120), 0);
	watch = k_Mate;
	watch.samePlayer = false;
	EXPECT_EQ(VillagerInHandPriority(watch, 120), 0);
	watch = k_Mate;
	watch.watcherScripted = true;
	EXPECT_EQ(VillagerInHandPriority(watch, 120), 0);
}

TEST(VillagerPickup, AMateToBeWaitsOnlyWhileTheOneHeldIsWithinReach)
{
	EXPECT_TRUE(KeepsWaitingForMate(0.0f, 20.0f));
	EXPECT_TRUE(KeepsWaitingForMate(19.99f, 20.0f));
	EXPECT_FALSE(KeepsWaitingForMate(20.0f, 20.0f));
	EXPECT_FALSE(KeepsWaitingForMate(35.0f, 20.0f));
}

TEST(VillagerPickup, AThrownVillagerImpressesByItsOwnImpressivenessWeighedByDistance)
{
	// A watcher right by it takes all of its impressiveness, one at the edge of the reaction's reach a little of it
	const ImpressionInputs near {.landBalance = 1.0f,
	                             .impressiveValue = 0.5f,
	                             .reactionMultiplier = 1.0f,
	                             .distance = 0.0f,
	                             .maxDistance = 25.0f,
	                             .power = 1.0f,
	                             .boredom = 1.0f};
	auto far = near;
	far.distance = 25.0f;
	EXPECT_FLOAT_EQ(ImpressiveValue(near), 0.5f);
	EXPECT_LT(ImpressiveValue(far), ImpressiveValue(near));
	EXPECT_GT(ImpressiveValue(far), 0.0f);
	// A thing with no impressiveness impresses no one, however near
	auto dull = near;
	dull.impressiveValue = 0.0f;
	EXPECT_FLOAT_EQ(ImpressiveValue(dull), 0.0f);
}
