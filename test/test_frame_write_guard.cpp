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

#include <gtest/gtest.h>

#include "Debug/FrameWriteGuard.h"
#include "GameFrameOrder.h"

using namespace openblack;

namespace
{

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
