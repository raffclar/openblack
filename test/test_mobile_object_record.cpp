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
#include "ECS/Registry.h"
#include "Editor/EditorEntities.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::editor;

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

// The level editor copies a loose object by its row, so that a copy of one with the football's record has that record
TEST(MobileObjectRecord, TheEditorCopiesALooseObjectByItsRow)
{
	Registry registry;
	const auto football = registry.Create();
	registry.Assign<MobileObject>(football, MobileObject {.type = MobileObjectInfo::Ball, .footballRecord = true});
	const auto ball = registry.Create();
	registry.Assign<MobileObject>(ball, MobileObject {.type = MobileObjectInfo::Ball});

	const auto copy = ItemOf(registry, football);
	ASSERT_TRUE(copy.has_value());
	EXPECT_EQ(copy->kind, PlaceKind::MobileObject);
	EXPECT_EQ(copy->type, mobile_object_record::k_FootballRow);
	const auto plain = ItemOf(registry, ball);
	ASSERT_TRUE(plain.has_value());
	EXPECT_EQ(plain->type, static_cast<int32_t>(MobileObjectInfo::Ball));

	// The row of the copy makes a loose object of the football's record again
	const auto info = FakeInfo();
	const auto made = mobile_object_record::FromRow(*info, copy->type);
	ASSERT_TRUE(made.has_value());
	EXPECT_TRUE(made->footballRecord);
	EXPECT_EQ(mobile_object_record::Of(*info, *made), &info->ball);
}

// The kind of the row's record decides what the game makes: a crop or a creed, and the eyes are a creed. The row of the
// poo makes poo whatever its record says.
TEST(MobileObjectRecord, TheKindOfTheRecordDecidesWhatTheGameMakes)
{
	using mobile_object_record::Class;
	using mobile_object_record::ClassOf;
	EXPECT_EQ(ClassOf(MobileObjectInfo::Crop, MobileObjectInfo::Crop), Class::Crop);
	EXPECT_EQ(ClassOf(MobileObjectInfo::Creed, MobileObjectInfo::Creed), Class::Creed);
	EXPECT_EQ(ClassOf(MobileObjectInfo::Eyes, MobileObjectInfo::Creed), Class::Creed);
	EXPECT_EQ(ClassOf(MobileObjectInfo::LumpOfPoo, MobileObjectInfo::LumpOfPoo), Class::Poo);
	EXPECT_EQ(ClassOf(MobileObjectInfo::LumpOfPoo, MobileObjectInfo::Creed), Class::Poo);
	EXPECT_EQ(ClassOf(MobileObjectInfo::Ball, MobileObjectInfo::Ball), Class::Plain);
	EXPECT_EQ(ClassOf(MobileObjectInfo::OldScaffold, MobileObjectInfo::Champi), Class::Plain);
}

// A loose object takes the kind of its record from the tables. One with the football's record is never poo.
TEST(MobileObjectRecord, ALooseObjectTakesTheKindOfItsRecordFromTheTables)
{
	auto info = FakeInfo();
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Eyes)).mobileType = MobileObjectInfo::Creed;
	info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Crop)).mobileType = MobileObjectInfo::Crop;
	info->ball.mobileType = MobileObjectInfo::LumpOfPoo;
	using mobile_object_record::Class;
	EXPECT_EQ(mobile_object_record::ClassOf(*info, MobileObject {.type = MobileObjectInfo::Eyes}), Class::Creed);
	EXPECT_EQ(mobile_object_record::ClassOf(*info, MobileObject {.type = MobileObjectInfo::Crop}), Class::Crop);
	EXPECT_EQ(mobile_object_record::ClassOf(*info, MobileObject {.type = MobileObjectInfo::Ball}), Class::Plain);
	EXPECT_EQ(mobile_object_record::ClassOf(*info, MobileObject {.type = MobileObjectInfo::LumpOfPoo, .footballRecord = true}),
	          Class::Plain);
	EXPECT_EQ(mobile_object_record::ClassOf(*info, MobileObject {.type = MobileObjectInfo::_COUNT}), Class::Plain);
}
