/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <gtest/gtest.h>

#include "ECS/VillagerNeeds.h"

using namespace openblack::ecs::villager_needs;

TEST(VillagerNeeds, FoodDesireRisesAsTheCubeOfHowEmptyItIs)
{
	EXPECT_FLOAT_EQ(DesireForFood(1.0f), 0.0f);
	EXPECT_FLOAT_EQ(DesireForFood(1.3f), 0.0f);
	EXPECT_FLOAT_EQ(DesireForFood(0.5f), 0.875f);
	EXPECT_FLOAT_EQ(DesireForFood(0.0f), 1.0f);
}

TEST(VillagerNeeds, RestIsWantedFullyAtTheLifeItGoesHomeHurt)
{
	EXPECT_FLOAT_EQ(LifeDesireFromLife(1.0f, 0.3f), 0.0f);
	EXPECT_FLOAT_EQ(LifeDesireFromLife(0.3f, 0.3f), 1.0f);
	EXPECT_FLOAT_EQ(LifeDesireFromLife(0.1f, 0.3f), 1.0f);
	// Half way between: three quarters wanted
	EXPECT_FLOAT_EQ(LifeDesireFromLife(0.65f, 0.3f), 0.75f);
}

TEST(VillagerNeeds, TheTriggerIsTheGreaterNeedAndHalfTheLesser)
{
	TriggerInputs in {.hungry = true, .foodDesire = 0.4f, .lifeDesire = 0.5f, .ownDesireThreshold = 0.3f};
	// Rest counts past 0.3: 0.2; food 0.4; 0.4 + 0.1
	EXPECT_FLOAT_EQ(OwnDesiresTrigger(in), 0.5f);
	// Food counts only when hungry
	in.hungry = false;
	EXPECT_FLOAT_EQ(OwnDesiresTrigger(in), 0.2f);
	// No more than 1
	in.hungry = true;
	in.foodDesire = 1.0f;
	in.lifeDesire = 1.3f;
	EXPECT_FLOAT_EQ(OwnDesiresTrigger(in), 1.0f);
	// A knock on its home leaves it free of its needs
	in.woken = true;
	EXPECT_FLOAT_EQ(OwnDesiresTrigger(in), 0.0f);
}

TEST(VillagerNeeds, AChildIsAlwaysPressedALittle)
{
	const TriggerInputs in {.child = true};
	EXPECT_FLOAT_EQ(OwnDesiresTrigger(in), 0.11f);
	const TriggerInputs needy {.hungry = true, .child = true, .foodDesire = 0.5f};
	EXPECT_FLOAT_EQ(OwnDesiresTrigger(needy), 0.5f);
}

TEST(VillagerNeeds, TheStrongerOwnNeedIsSeenToFirst)
{
	std::string tried;
	const auto sleep = [&tried](bool goes) {
		return [&tried, goes] {
			tried += "S";
			return goes;
		};
	};
	const auto eat = [&tried](bool goes) {
		return [&tried, goes] {
			tried += "E";
			return goes;
		};
	};
	EXPECT_TRUE(SatisfyOwnDesire(0.9f, 0.5f, 0.3f, sleep(true), eat(true)));
	EXPECT_EQ(tried, "E");
	tried.clear();
	EXPECT_TRUE(SatisfyOwnDesire(0.9f, 0.5f, 0.3f, sleep(true), eat(false)));
	EXPECT_EQ(tried, "ES");
	tried.clear();
	EXPECT_FALSE(SatisfyOwnDesire(0.5f, 0.9f, 0.3f, sleep(false), eat(false)));
	EXPECT_EQ(tried, "SE");
	tried.clear();
	// Neither past the threshold
	EXPECT_FALSE(SatisfyOwnDesire(0.2f, 0.1f, 0.3f, sleep(true), eat(true)));
	EXPECT_EQ(tried, "");
}

TEST(VillagerNeeds, TheLessLifeTheLikelierAPause)
{
	EXPECT_TRUE(PausesForASecond(0.005f, 1.0f, false, 0.01f));
	EXPECT_FALSE(PausesForASecond(0.02f, 1.0f, false, 0.01f));
	// At no life half the draws pause
	EXPECT_TRUE(PausesForASecond(0.5f, 0.0f, false, 0.01f));
	EXPECT_FALSE(PausesForASecond(0.52f, 0.0f, false, 0.01f));
	// Poison halves the life it feels
	EXPECT_TRUE(PausesForASecond(0.06f, 1.0f, true, 0.01f));
}
