/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ScriptHeaders/NativeStubCalls.h"

using openblack::script::NativeStubCalls;

TEST(NativeStubCalls, OnlyTheFirstCallOfANativeIsReported)
{
	NativeStubCalls stubs;
	EXPECT_TRUE(stubs.Record(22));
	EXPECT_FALSE(stubs.Record(22));
	EXPECT_FALSE(stubs.Record(22));
	EXPECT_TRUE(stubs.Record(3));
	EXPECT_EQ(stubs.Calls(22), 3u);
	EXPECT_EQ(stubs.Calls(3), 1u);
	EXPECT_EQ(stubs.Calls(4), 0u);
	EXPECT_EQ(stubs.Total(), 4u);
}

TEST(NativeStubCalls, EachUnwrittenCaseOfANativeIsReportedApart)
{
	NativeStubCalls stubs;
	EXPECT_TRUE(stubs.Record(21, 5));
	EXPECT_TRUE(stubs.Record(21, 7));
	EXPECT_FALSE(stubs.Record(21, 5));
	EXPECT_TRUE(stubs.Record(21));
	// A native's calls are summed over its cases, without counting a neighbouring native's
	stubs.Record(20);
	stubs.Record(22, 1);
	EXPECT_EQ(stubs.Calls(21), 4u);
}

TEST(NativeStubCalls, ListsTheMostCalledFirstThenByNative)
{
	NativeStubCalls stubs;
	stubs.Record(9);
	stubs.Record(4);
	stubs.Record(7, 2);
	stubs.Record(7, 2);
	const auto entries = stubs.Entries();
	ASSERT_EQ(entries.size(), 3u);
	EXPECT_EQ(entries[0].native, 7u);
	EXPECT_EQ(entries[0].detail, 2);
	EXPECT_EQ(entries[0].calls, 2u);
	EXPECT_EQ(entries[1].native, 4u);
	EXPECT_FALSE(entries[1].detail.has_value());
	EXPECT_EQ(entries[2].native, 9u);

	stubs.Clear();
	EXPECT_TRUE(stubs.Entries().empty());
	EXPECT_TRUE(stubs.Record(9));
}
