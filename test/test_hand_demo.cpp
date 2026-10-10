/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <algorithm>
#include <array>
#include <vector>

#include <HNDFile.h>
#include <gtest/gtest.h>

#include "Hand/HandDemo.h"

using namespace openblack;
using namespace openblack::hand_demo;

namespace
{
/// A record's bytes as the game writes them
std::array<uint8_t, hnd::k_RecordSize> RecordBytes(uint32_t message, float cursorX, float cursorY, uint32_t trigger,
                                                   uint32_t time)
{
	std::array<uint8_t, hnd::k_RecordSize> bytes {};
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(0x00, message);
	put(0x04, 1.5f);
	put(0x34, cursorX);
	put(0x38, cursorY);
	put(0x3C, 1867.5f);
	put(0x40, 54.5f);
	put(0x44, 2539.0f);
	put(0x48, 1914.0f);
	put(0x4C, 29.5f);
	put(0x50, 2509.0f);
	put(0x54, 0x202u);
	put(0x58, 1.25f);
	put(0x5C, trigger);
	put(0x60, time);
	put(0x64, 3.5f);
	put(0x68, int32_t {0x10000});
	put(0x6C, int32_t {-0x20000});
	put(0x70, 7.0f);
	put(0x74, 16u);
	put(0x78, 5u);
	return bytes;
}

/// Records every 10 ms from a time, the marks at the given ones
std::vector<hnd::HNDRecord> Records(size_t count, uint32_t from, std::vector<size_t> triggers = {})
{
	std::vector<hnd::HNDRecord> records(count);
	for (size_t i = 0; i < count; ++i)
	{
		records[i].time = from + static_cast<uint32_t>(i * 10);
		records[i].hints = 7;
		records[i].trigger = std::ranges::find(triggers, i) != triggers.end() ? 1 : 0;
	}
	return records;
}
} // namespace

TEST(HandDemoFile, ReadsItsRecords)
{
	const auto first = RecordBytes(0, 0.25f, 0.75f, 0, 1000);
	const auto second = RecordBytes(3, 0.5f, 0.5f, 1, 1030);
	std::vector<uint8_t> bytes(first.begin(), first.end());
	bytes.insert(bytes.end(), second.begin(), second.end());
	hnd::HNDFile file;
	ASSERT_EQ(file.Open(bytes), hnd::HNDResult::Success);
	ASSERT_EQ(file.records.size(), 2u);
	const auto& move = file.records[0];
	EXPECT_EQ(move.message, hnd::HNDMessage::Move);
	EXPECT_FLOAT_EQ(move.heldPlacement[0], 1.5f);
	EXPECT_FLOAT_EQ(move.cursor[0], 0.25f);
	EXPECT_FLOAT_EQ(move.cursor[1], 0.75f);
	EXPECT_FLOAT_EQ(move.cameraPosition[1], 54.5f);
	EXPECT_FLOAT_EQ(move.cameraFocus[2], 2509.0f);
	EXPECT_EQ(move.hints, 0x202u);
	EXPECT_FLOAT_EQ(move.hintValue, 1.25f);
	EXPECT_EQ(move.trigger, 0u);
	EXPECT_EQ(move.time, 1000u);
	const auto& press = file.records[1];
	EXPECT_EQ(press.message, hnd::HNDMessage::ActionButtonDown);
	EXPECT_EQ(press.trigger, 1u);
	EXPECT_FLOAT_EQ(press.objectDistance, 3.5f);
	EXPECT_EQ(press.objectPosition.x, 0x10000);
	EXPECT_EQ(press.objectPosition.z, -0x20000);
	EXPECT_FLOAT_EQ(press.objectPosition.altitude, 7.0f);
	EXPECT_EQ(press.objectType, 16u);
	EXPECT_EQ(press.objectSubtype, 5u);
}

TEST(HandDemoFile, IsAWholeNumberOfRecords)
{
	std::vector<uint8_t> bytes(hnd::k_RecordSize + 3);
	hnd::HNDFile file;
	EXPECT_EQ(file.Open(bytes), hnd::HNDResult::ErrPartialRecord);
	EXPECT_EQ(file.Open(std::span<const uint8_t> {}), hnd::HNDResult::Success);
	EXPECT_TRUE(file.records.empty());
}

TEST(HandDemo, GameTimeCountsHundredthsOfATurn)
{
	EXPECT_EQ(GameHundredths(12, 0.0f), 1200u);
	EXPECT_EQ(GameHundredths(12, 0.456f), 1245u);
	// Never a whole turn into one
	EXPECT_EQ(GameHundredths(12, 1.0f), 1299u);
}

TEST(HandDemo, TheCursorIsDownThePictureBetweenTheBars)
{
	const std::array cursor {0.5f, 0.25f};
	// No bars: the whole screen
	EXPECT_EQ(CursorPixel(cursor, {1280, 960}, 0.0f), glm::ivec2(640, 240));
	// Bars all the way in: 1280 by 720 between bars of 120
	EXPECT_EQ(CursorPixel(cursor, {1280, 960}, 1.0f), glm::ivec2(640, 300));
	const std::array corner {0.0f, 1.0f};
	EXPECT_EQ(CursorPixel(corner, {1280, 960}, 1.0f), glm::ivec2(0, 840));
}

TEST(HandDemo, TheFirstRecordPlaysAtOnceAndTheRestByTheirTimes)
{
	const auto records = Records(5, 5000);
	std::vector<Played> played;
	Playback playback(records, 100, played);
	ASSERT_EQ(played.size(), 1u);
	EXPECT_EQ(played[0].record, &records[0]);
	bool trigger = false;
	played.clear();
	// 25 hundredths later: the records of 10 and 20
	EXPECT_TRUE(playback.Advance(125, false, trigger, played));
	ASSERT_EQ(played.size(), 2u);
	EXPECT_EQ(played[1].record, &records[2]);
	played.clear();
	// A slow frame catches up, and the last record ends it
	EXPECT_FALSE(playback.Advance(500, false, trigger, played));
	EXPECT_EQ(played.size(), 2u);
	EXPECT_TRUE(playback.Ended());
}

TEST(HandDemo, AMarkHoldsTheRestUntilTheScriptReadsIt)
{
	const auto records = Records(5, 0, {1});
	std::vector<Played> played;
	Playback playback(records, 0, played);
	bool trigger = false;
	played.clear();
	playback.Advance(10, true, trigger, played);
	EXPECT_TRUE(trigger);
	ASSERT_EQ(played.size(), 1u);
	played.clear();
	// Held while the mark stands, however long
	playback.Advance(1000, true, trigger, played);
	EXPECT_TRUE(played.empty());
	// Read: the rest keeps its pace from there
	trigger = false;
	playback.Advance(1000, true, trigger, played);
	ASSERT_EQ(played.size(), 1u);
	EXPECT_EQ(played[0].record, &records[2]);
	played.clear();
	playback.Advance(1009, true, trigger, played);
	EXPECT_TRUE(played.empty());
	playback.Advance(1010, true, trigger, played);
	EXPECT_EQ(played.size(), 1u);
}

TEST(HandDemo, WithoutPauseAMarkIsOnlyNoted)
{
	const auto records = Records(4, 0, {1});
	std::vector<Played> played;
	Playback playback(records, 0, played);
	bool trigger = false;
	played.clear();
	playback.Advance(30, false, trigger, played);
	EXPECT_TRUE(trigger);
	EXPECT_EQ(played.size(), 3u);
}

TEST(HandDemo, NoHintsNearEitherEnd)
{
	const auto records = Records(60, 0);
	std::vector<Played> played;
	Playback playback(records, 0, played);
	EXPECT_EQ(played[0].hints, 0u);
	bool trigger = false;
	played.clear();
	// One go up to the 30th record: counted from the second, the first of this go, all are near the start
	playback.Advance(290, false, trigger, played);
	ASSERT_EQ(played.size(), 29u);
	EXPECT_EQ(played.back().hints, 0u);
	played.clear();
	playback.Advance(300, false, trigger, played);
	ASSERT_EQ(played.size(), 1u);
	EXPECT_EQ(played[0].hints, 7u);
	played.clear();
	// The last twenty show none
	playback.Advance(400, false, trigger, played);
	playback.Advance(410, false, trigger, played);
	EXPECT_EQ(played.back().hints, 0u);
}
