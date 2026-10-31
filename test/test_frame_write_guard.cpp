/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Debug/FrameWriteGuard.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "GameFrameOrder.h"

using namespace openblack;

namespace
{

/// The saved state of a fake system: a synced count, a presentation pace and a synced flag
std::vector<uint8_t> FakeSave(uint32_t count, uint32_t pace, uint8_t flag,
                              std::vector<frame_write_guard::ByteRange>& presentation)
{
	const entt::meta_ctx context;
	ecs::reflection::BinaryWriter out(context);
	ecs::reflection::Save(out, count);
	ecs::reflection::SavePresentation(out, pace);
	ecs::reflection::Save(out, flag);
	presentation = out.PresentationRanges();
	return out.Bytes();
}

frame_write_guard::Sample SampleOf(std::vector<std::pair<std::string, uint64_t>> parts, uint64_t draws = 0)
{
	return {.hash = {.hash = 0, .parts = std::move(parts)}, .syncedDraws = draws};
}

} // namespace

TEST(FrameWriteGuard, SameSamplesChangeNothing)
{
	const auto sample = SampleOf({{"entities", 1}, {"Animal", 2}}, 5);
	EXPECT_TRUE(frame_write_guard::ChangedParts(sample, sample).empty());
}

TEST(FrameWriteGuard, APartWithAnotherHashChanged)
{
	const auto before = SampleOf({{"entities", 1}, {"Animal", 2}, {"Transform", 3}});
	const auto after = SampleOf({{"entities", 1}, {"Animal", 9}, {"Transform", 3}});
	EXPECT_EQ(frame_write_guard::ChangedParts(before, after), std::vector<std::string> {"Animal"});
}

TEST(FrameWriteGuard, AddedAndRemovedPartsChanged)
{
	const auto before = SampleOf({{"entities", 1}, {"Vortex", 4}});
	const auto after = SampleOf({{"entities", 1}, {"Animal", 2}});
	EXPECT_EQ(frame_write_guard::ChangedParts(before, after), (std::vector<std::string> {"Animal", "Vortex"}));
}

TEST(FrameWriteGuard, SyncedDrawsChanged)
{
	const auto before = SampleOf({{"entities", 1}}, 10);
	const auto after = SampleOf({{"entities", 1}}, 11);
	EXPECT_EQ(frame_write_guard::ChangedParts(before, after),
	          std::vector<std::string> {std::string(frame_write_guard::k_SyncedDrawsPart)});
}

TEST(FrameWriteGuard, EachStepHasItsName)
{
	EXPECT_EQ(game_frame::Name(game_frame::Step::FrameClock), "FrameClock");
	EXPECT_EQ(game_frame::Name(game_frame::Step::FrameEffects), "FrameEffects");
	EXPECT_EQ(game_frame::Name(game_frame::Step::ProfileAlignment), "ProfileAlignment");
	EXPECT_EQ(game_frame::Name(game_frame::Step::_Count), "unknown");
}

TEST(FrameWriteGuard, TheChangedEntityIsTheFirstRowThatDiffers)
{
	auto before = SampleOf({{"Transform.position", 1}});
	auto after = SampleOf({{"Transform.position", 2}});
	before.rows["Transform.position"] = {{7, 10}, {8, 20}};
	after.rows["Transform.position"] = {{7, 10}, {8, 21}};
	EXPECT_EQ(frame_write_guard::ChangedEntity(before, after, "Transform.position"), std::optional<uint32_t>(8));
	EXPECT_FALSE(frame_write_guard::ChangedEntity(before, before, "Transform.position").has_value());
	EXPECT_FALSE(frame_write_guard::ChangedEntity(before, after, "Animal.move").has_value());
}

TEST(FrameWriteGuard, TheChangedValuesOfASystemAreFoundByTheirNumber)
{
	auto before = SampleOf({{"system fire", 1}});
	auto after = SampleOf({{"system fire", 2}});
	before.systemValues["system fire"] = {10, 20, 30};
	after.systemValues["system fire"] = {10, 21, 30, 40};
	EXPECT_EQ(frame_write_guard::ChangedValues(before, after, "system fire"), (std::vector<size_t> {1, 3}));
	EXPECT_TRUE(frame_write_guard::ChangedValues(before, before, "system fire").empty());
	EXPECT_TRUE(frame_write_guard::ChangedValues(before, after, "system snow").empty());
}

TEST(FrameWriteGuard, AStepThatLoadsALandIsNotAWriteOfTheFrame)
{
	EXPECT_TRUE(frame_write_guard::LooksAtStep(3, 3));
	EXPECT_FALSE(frame_write_guard::LooksAtStep(3, 4));
	EXPECT_FALSE(frame_write_guard::LooksAtStep(0, 1));
}

TEST(FrameWriteGuard, TheWriterKeepsTheBytesOfEachPresentationValue)
{
	std::vector<frame_write_guard::ByteRange> presentation;
	const auto bytes = FakeSave(1, 5, 2, presentation);
	ASSERT_EQ(bytes.size(), 9u);
	EXPECT_EQ(presentation, (std::vector<frame_write_guard::ByteRange> {{4u, 8u}}));
	// A presentation value in a presentation value is one range
	const entt::meta_ctx context;
	ecs::reflection::BinaryWriter out(context);
	out.BeginPresentation();
	ecs::reflection::Save(out, uint32_t {1});
	ecs::reflection::SavePresentation(out, uint32_t {2});
	out.EndPresentation();
	EXPECT_EQ(out.PresentationRanges(), (std::vector<frame_write_guard::ByteRange> {{0u, 8u}}));
}

TEST(FrameWriteGuard, TheWriterKeepsThePresentationValuesOfTheBytesThatItAppends)
{
	const entt::meta_ctx context;
	ecs::reflection::BinaryWriter part(context);
	ecs::reflection::Save(part, uint32_t {1});
	ecs::reflection::SavePresentation(part, uint32_t {2});
	ecs::reflection::BinaryWriter out(context);
	ecs::reflection::SavePresentation(out, uint8_t {3});
	ecs::reflection::Save(out, uint8_t {4});
	out.Append(part);
	EXPECT_EQ(out.Bytes().size(), 10u);
	EXPECT_EQ(out.PresentationRanges(), (std::vector<frame_write_guard::ByteRange> {{0u, 1u}, {6u, 10u}}));
	// Inside a presentation value, all the appended bytes are in that value
	ecs::reflection::BinaryWriter inside(context);
	inside.BeginPresentation();
	inside.Append(part);
	inside.EndPresentation();
	EXPECT_EQ(inside.PresentationRanges(), (std::vector<frame_write_guard::ByteRange> {{0u, 8u}}));
}

TEST(FrameWriteGuard, TheSyncedHashLeavesOutThePresentationValues)
{
	std::vector<frame_write_guard::ByteRange> first;
	std::vector<frame_write_guard::ByteRange> second;
	std::vector<frame_write_guard::ByteRange> third;
	const auto before = FakeSave(1, 5, 2, first);
	const auto paced = FakeSave(1, 9, 2, second);
	const auto counted = FakeSave(3, 5, 2, third);
	ASSERT_NE(before, paced);
	EXPECT_EQ(frame_write_guard::SyncedHash(before, 0, before.size(), first),
	          frame_write_guard::SyncedHash(paced, 0, paced.size(), second));
	EXPECT_NE(frame_write_guard::SyncedHash(before, 0, before.size(), first),
	          frame_write_guard::SyncedHash(counted, 0, counted.size(), third));
	// The hash of the presentation value alone is the same for each pace, and the hash of a synced value is not
	EXPECT_EQ(frame_write_guard::SyncedHash(before, 4, 8, first), frame_write_guard::SyncedHash(paced, 4, 8, second));
	EXPECT_NE(frame_write_guard::SyncedHash(before, 0, 4, first), frame_write_guard::SyncedHash(counted, 0, 4, third));
	// With no presentation value, the hash looks at each byte
	EXPECT_NE(frame_write_guard::SyncedHash(before, 0, before.size(), {}),
	          frame_write_guard::SyncedHash(paced, 0, paced.size(), {}));
}
