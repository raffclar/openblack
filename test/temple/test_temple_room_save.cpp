/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <utility>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "Temple/TempleRoomSave.h"

using namespace openblack;

namespace
{
const temple::RoomSave k_Rooms {.roomsOpened = true, .shown = {true, false, true, true, false}};
} // namespace

TEST(TempleRoomSave, TheRoomsKeepTheBytesOfTheOldSave)
{
	const entt::meta_ctx context;
	ecs::reflection::BinaryWriter rooms(context);
	temple::SaveRooms(rooms, k_Rooms);
	ecs::reflection::BinaryWriter old(context);
	ecs::reflection::Save(old, k_Rooms.roomsOpened);
	for (const bool each : k_Rooms.shown)
	{
		ecs::reflection::Save(old, each);
	}
	EXPECT_EQ(rooms.Bytes(), old.Bytes());
}

TEST(TempleRoomSave, AllTheStateOfTheRoomsIsAPresentationValue)
{
	const entt::meta_ctx context;
	ecs::reflection::BinaryWriter out(context);
	// The held dialogue comes first and stays a synced value
	ecs::reflection::Save(out, false);
	const auto start = out.Size();
	temple::SaveRooms(out, k_Rooms);
	EXPECT_EQ(out.PresentationRanges(), (std::vector<std::pair<size_t, size_t>> {{start, out.Size()}}));
}
