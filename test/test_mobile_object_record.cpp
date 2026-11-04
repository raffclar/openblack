/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The record that a loose object takes its values from, and the row that the land scripts give for it

#include <memory>

#include <gtest/gtest.h>

#include "ECS/MobileObjectRecord.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Tables where the football's record and the table's row of the same kind have different models and weights
std::unique_ptr<InfoConstants> FakeInfo()
{
	auto info = std::make_unique<InfoConstants>();
	info->ball.mobileType = MobileObjectInfo::Ball;
	info->ball.meshId = static_cast<MeshId>(344);
	info->ball.weight = 3.0f;
	auto& row = info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Ball));
	row.mobileType = MobileObjectInfo::Ball;
	row.meshId = static_cast<MeshId>(0);
	row.weight = 7.0f;
	return info;
}
} // namespace

// The row of the football's record makes a loose object of that record, of the loose kind that the record gives
TEST(MobileObjectRecord, TheFootballsRowMakesALooseObjectOfTheFootballsRecord)
{
	const auto info = FakeInfo();
	const auto made = mobile_object_record::FromRow(*info, mobile_object_record::k_FootballRow);
	ASSERT_TRUE(made.has_value());
	EXPECT_TRUE(made->footballRecord);
	EXPECT_EQ(made->type, MobileObjectInfo::Ball);
	EXPECT_EQ(mobile_object_record::Of(*info, *made), &info->ball);
	EXPECT_EQ(mobile_object_record::Of(*info, *made)->weight, 3.0f);
	EXPECT_EQ(mobile_object_record::Row(*made), mobile_object_record::k_FootballRow);
}

// A row of the loose objects' table makes a loose object of that row
TEST(MobileObjectRecord, ARowOfTheTableMakesALooseObjectOfThatRow)
{
	const auto info = FakeInfo();
	const auto row = static_cast<int32_t>(MobileObjectInfo::Ball);
	const auto made = mobile_object_record::FromRow(*info, row);
	ASSERT_TRUE(made.has_value());
	EXPECT_FALSE(made->footballRecord);
	EXPECT_EQ(made->type, MobileObjectInfo::Ball);
	EXPECT_EQ(mobile_object_record::Of(*info, *made)->weight, 7.0f);
	EXPECT_EQ(mobile_object_record::Row(*made), row);
}

// Another row is outside the table, and makes no loose object
TEST(MobileObjectRecord, ARowOutsideTheTableMakesNothing)
{
	const auto info = FakeInfo();
	EXPECT_FALSE(mobile_object_record::FromRow(*info, -1).has_value());
	EXPECT_FALSE(mobile_object_record::FromRow(*info, static_cast<int32_t>(MobileObjectInfo::_COUNT)).has_value());
	EXPECT_EQ(mobile_object_record::Of(*info, MobileObject {.type = MobileObjectInfo::None}), nullptr);
}
