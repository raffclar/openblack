/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Temple/TempleEntrancePoint.h"
#include "Temple/TempleRoomLock.h"

using openblack::TempleRoom;
using openblack::temple::RoomLock;
using openblack::temple::TowardsEntrance;

TEST(TempleRoomLock, FreeUntilLocked)
{
	RoomLock lock;
	EXPECT_FALSE(lock.Leaves(TempleRoom::Main));
	lock.Left();
	EXPECT_FALSE(lock.TakeEnded());
}

TEST(TempleRoomLock, AnyOtherRoomLeavesTheLockedOne)
{
	RoomLock lock;
	lock.Lock(TempleRoom::SaveGame);
	EXPECT_FALSE(lock.Leaves(TempleRoom::SaveGame));
	EXPECT_TRUE(lock.Leaves(TempleRoom::Main));
	EXPECT_TRUE(lock.Leaves(TempleRoom::CreatureCave));
}

TEST(TempleRoomLock, LeavingTheTempleEndsTheGameOnce)
{
	RoomLock lock;
	lock.Lock(TempleRoom::SaveGame);
	lock.Left();
	EXPECT_FALSE(lock.GetRoom().has_value());
	EXPECT_TRUE(lock.TakeEnded());
	EXPECT_FALSE(lock.TakeEnded());
}

TEST(TempleRoomLock, ANewLandFreesThePlayerWithoutEndingTheGame)
{
	RoomLock lock;
	lock.Lock(TempleRoom::SaveGame);
	lock.Clear();
	lock.Left();
	EXPECT_FALSE(lock.TakeEnded());
}

TEST(TempleEntrancePoint, OutTowardsTheEntranceAndUp)
{
	// The entrance 3 east and 4 up from the temple: 90 along that way, then 40 higher
	const auto point = TowardsEntrance({100.0f, 10.0f, 200.0f}, {103.0f, 14.0f, 200.0f}, 90.0f, 40.0f);
	EXPECT_FLOAT_EQ(point.x, 100.0f + 54.0f);
	EXPECT_FLOAT_EQ(point.y, 10.0f + 72.0f + 40.0f);
	EXPECT_FLOAT_EQ(point.z, 200.0f);
}

TEST(TempleEntrancePoint, AnEntranceAtTheTempleLeavesThePointOverIt)
{
	const auto point = TowardsEntrance({100.0f, 10.0f, 200.0f}, {100.0f, 10.0f, 200.0f}, 90.0f, 40.0f);
	EXPECT_FLOAT_EQ(point.x, 100.0f);
	EXPECT_FLOAT_EQ(point.y, 50.0f);
	EXPECT_FLOAT_EQ(point.z, 200.0f);
}
