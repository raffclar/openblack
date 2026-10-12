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

// A button's record gives the hand back the throw it was ready to make: its velocity is the first three numbers
TEST(HandDemo, ARecordCarriesTheThrowTheHandWasReadyToMake)
{
	hnd::HNDRecord record;
	record.heldPlacement = {73.9941f, 5.2013f, -52.1485f, 0.0f, 0.0f, 0.0f, 1966.6f, 17.5f, 2364.8f, 0.3f, 1.1f, -0.1f};
	EXPECT_EQ(hand_demo::RecordedThrowVelocity(record), glm::vec3(73.9941f, 5.2013f, -52.1485f));
}

TEST(HandDemoPlayback, AButtonIsHeldFromItsPressToItsRelease)
{
	HeldButtons held;
	held = AfterRecord(held, hnd::HNDMessage::MoveButtonDown);
	EXPECT_TRUE(held.move);
	EXPECT_FALSE(held.action);
	held = AfterRecord(held, hnd::HNDMessage::Move);
	EXPECT_TRUE(held.move);
	held = AfterRecord(held, hnd::HNDMessage::ActionButtonDown);
	EXPECT_TRUE(held.move);
	EXPECT_TRUE(held.action);
	held = AfterRecord(held, hnd::HNDMessage::MoveButtonUp);
	EXPECT_FALSE(held.move);
	EXPECT_TRUE(held.action);
	held = AfterRecord(held, hnd::HNDMessage::ActionButtonUp);
	EXPECT_FALSE(held.action);
}

namespace
{
/// A recorded press on the storage pit, as the wood lesson's recording has one: type 16 (a store), subtype 5, a little
/// over 1830 by 2642 metres
hnd::HNDRecord PressOnStore()
{
	hnd::HNDRecord record;
	record.message = hnd::HNDMessage::ActionButtonDown;
	record.objectType = static_cast<uint32_t>(script::ObjectType::Store);
	record.objectSubtype = 5;
	record.objectPosition = {.x = map_coords::ToFixed(1830.0f), .z = map_coords::ToFixed(2642.0f), .altitude = 0.0f};
	record.objectDistance = 40.0f;
	return record;
}

map_coords::MapCoords At(float x, float z)
{
	return map_coords::FromMetres({x, z});
}

ecs::script_find::Kind Store(uint32_t subtype)
{
	return {.type = script::ObjectType::Store, .subtype = subtype};
}
} // namespace

TEST(HandDemo, OnlyTheActionButtonAndTheUnusedMessageRetarget)
{
	EXPECT_TRUE(ClickCanRetarget(hnd::HNDMessage::ActionButtonDown));
	EXPECT_TRUE(ClickCanRetarget(hnd::HNDMessage::ActionButtonUp));
	EXPECT_TRUE(ClickCanRetarget(hnd::HNDMessage::Other));
	EXPECT_FALSE(ClickCanRetarget(hnd::HNDMessage::Move));
	EXPECT_FALSE(ClickCanRetarget(hnd::HNDMessage::MoveButtonDown));
	EXPECT_FALSE(ClickCanRetarget(hnd::HNDMessage::MoveButtonUp));
	EXPECT_FALSE(ClickCanRetarget(hnd::HNDMessage::DoubleClick));
}

TEST(HandDemo, AClickRecordedOnNothingLandsOnNothing)
{
	hnd::HNDRecord record;
	record.message = hnd::HNDMessage::ActionButtonDown;
	record.objectSubtype = hnd::k_NoObject;
	const ThingUnderHand under {.kind = Store(5), .at = At(1830.0f, 2642.0f)};
	EXPECT_EQ(TargetOfClick(record, under), ClickTarget::NoThing);
	EXPECT_EQ(TargetOfClick(record, std::nullopt), ClickTarget::NoThing);
}

TEST(HandDemo, AClickStaysOnTheRecordedKindUnderTheHandNearTheRecordedPlace)
{
	const auto record = PressOnStore();
	EXPECT_EQ(TargetOfClick(record, ThingUnderHand {.kind = Store(5), .at = At(1832.0f, 2642.0f)}), ClickTarget::UnderHand);
}

TEST(HandDemo, AClickLooksForTheRecordedKindWhenTheHandIsOverSomethingElse)
{
	const auto record = PressOnStore();
	EXPECT_EQ(TargetOfClick(record, std::nullopt), ClickTarget::NearestOfKind);
	// Another subtype, another type, or the right kind 3 metres or more away
	EXPECT_EQ(TargetOfClick(record, ThingUnderHand {.kind = Store(2), .at = At(1830.0f, 2642.0f)}), ClickTarget::NearestOfKind);
	EXPECT_EQ(TargetOfClick(record, ThingUnderHand {.kind = {.type = script::ObjectType::Rock, .subtype = 5},
	                                                .at = At(1830.0f, 2642.0f)}),
	          ClickTarget::NearestOfKind);
	EXPECT_EQ(TargetOfClick(record, ThingUnderHand {.kind = Store(5), .at = At(1833.1f, 2642.0f)}), ClickTarget::NearestOfKind);
	// The thing under the hand is compared exactly: a recorded "any" subtype is no match for it
	auto any = record;
	any.objectSubtype = ecs::script_find::k_AnySubtype;
	EXPECT_EQ(TargetOfClick(any, ThingUnderHand {.kind = Store(5), .at = At(1830.0f, 2642.0f)}), ClickTarget::NearestOfKind);
}

TEST(HandDemo, AThingWithNoSubtypeUnderTheHandMatchesARecordedNone)
{
	auto record = PressOnStore();
	record.objectSubtype = ecs::script_find::k_NoSubtype;
	const ThingUnderHand under {.kind = {.type = script::ObjectType::Store}, .at = At(1830.0f, 2642.0f)};
	EXPECT_EQ(TargetOfClick(record, under), ClickTarget::UnderHand);
}

TEST(HandDemo, TheClickMovesToAThingOfTheKindWithinThreeMetres)
{
	const auto record = PressOnStore();
	EXPECT_TRUE(TakesRecordedClick(record, Store(5), At(1832.0f, 2642.0f)));
	EXPECT_TRUE(TakesRecordedClick(record, Store(5), At(1832.9f, 2642.0f)));
	EXPECT_FALSE(TakesRecordedClick(record, Store(5), At(1833.5f, 2642.0f)));
	EXPECT_FALSE(TakesRecordedClick(record, Store(2), At(1830.0f, 2642.0f)));
	// A thing with no subtype is never one the search asks for, unless any will do
	EXPECT_FALSE(TakesRecordedClick(record, {.type = script::ObjectType::Store}, At(1830.0f, 2642.0f)));
	auto any = record;
	any.objectSubtype = ecs::script_find::k_AnySubtype;
	EXPECT_TRUE(TakesRecordedClick(any, Store(2), At(1830.0f, 2642.0f)));
	EXPECT_TRUE(TakesRecordedClick(any, {.type = script::ObjectType::Store}, At(1830.0f, 2642.0f)));
}
