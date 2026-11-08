/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The sleep of a small number of microseconds that keeps its time correct over many calls

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "Common/SleepDebt.h"

using namespace openblack;
using namespace std::chrono_literals;

TEST(SleepDebt, SleepsOnlyWhenTheDebtIsMoreThanAMillisecond)
{
	SleepDebt debt;
	std::vector<std::chrono::milliseconds> asked;
	const auto exact = [&asked](std::chrono::milliseconds time) {
		asked.push_back(time);
		return std::chrono::microseconds(time);
	};
	debt.Sleep(500us, exact);
	debt.Sleep(500us, exact);
	// A debt of exactly one millisecond does not sleep
	EXPECT_TRUE(asked.empty());
	EXPECT_EQ(debt.Debt(), 1000);
	debt.Sleep(500us, exact);
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_EQ(asked[0], 1ms);
	// The half millisecond that was not slept stays, and the sleep was shorter than the debt
	EXPECT_EQ(debt.Debt(), 500);
	EXPECT_EQ(debt.Oversleep(), -500);
	// The next call has a debt of 1000 plus the 500 that the last sleep did not sleep
	debt.Sleep(500us, exact);
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[1], 1ms);
	EXPECT_EQ(debt.Debt(), 500);
}

TEST(SleepDebt, AnOversleepIsTakenFromTheNextCalls)
{
	SleepDebt debt;
	// The system sleeps 15 milliseconds for each sleep
	std::chrono::microseconds total {0};
	const auto slow = [&total](std::chrono::milliseconds) {
		total += 15ms;
		return std::chrono::microseconds(15ms);
	};
	for (int i = 0; i < 1000; ++i)
	{
		debt.Sleep(500us, slow);
	}
	// 1000 calls of 500 microseconds sleep approximately half a second in all, not 1000 sleeps of 15 milliseconds. The
	// difference is less than two of the long sleeps
	EXPECT_GE(total, 500ms);
	EXPECT_LE(total, 530ms);
}
