/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/TempleRoomMeshes.h"

using namespace openblack;

// With the light maps, the game loads the rooms' own meshes and leaves out the low ones
TEST(TempleRoomMeshes, WithLightmapsTheLowMeshesAreLeftOut)
{
	EXPECT_EQ(temple_room_meshes::LoadedName("main_l3d", true, true), "main_l3d");
	EXPECT_EQ(temple_room_meshes::LoadedName("mainlo_l3d", true, false), std::nullopt);
	EXPECT_EQ(temple_room_meshes::LoadedName("challengedome_l3d", true, false), "challengedome_l3d");
}

// Without them, each low mesh takes the place of its room mesh, and a mesh with no low mesh stays
TEST(TempleRoomMeshes, WithoutLightmapsTheLowMeshesTakeThePlace)
{
	EXPECT_EQ(temple_room_meshes::LoadedName("main_l3d", false, true), std::nullopt);
	EXPECT_EQ(temple_room_meshes::LoadedName("mainlo_l3d", false, false), "main_l3d");
	EXPECT_EQ(temple_room_meshes::LoadedName("mainfloorlo_l3d", false, false), "mainfloor_l3d");
	EXPECT_EQ(temple_room_meshes::LoadedName("creaturewaterlo_l3d", false, false), "creaturewater_l3d");
	EXPECT_EQ(temple_room_meshes::LoadedName("challengedome_l3d", false, false), "challengedome_l3d");
	EXPECT_EQ(temple_room_meshes::LoadedName("movement_l3d", false, false), "movement_l3d");
}

// The low mesh's file name of a mesh
TEST(TempleRoomMeshes, LowName)
{
	EXPECT_EQ(temple_room_meshes::LowName("main_l3d"), "mainlo_l3d");
	EXPECT_EQ(temple_room_meshes::LowName("savegamefloor_l3d"), "savegamefloorlo_l3d");
}
