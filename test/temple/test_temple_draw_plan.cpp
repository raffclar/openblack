/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <set>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/TempleDrawPlan.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::TempleInteriorMesh;

namespace
{
constexpr entt::id_type k_MainRoom = 1;
constexpr entt::id_type k_MainFloor = 2;
constexpr entt::id_type k_MainPool = 3;
constexpr entt::id_type k_CaveRoom = 4;
constexpr entt::id_type k_CaveWater = 5;
constexpr entt::id_type k_CaveFloor = 6;

/// The parts of the main room and the creature's room, as the temple makes them
std::vector<temple_draw::Part> Parts()
{
	return {
	    {.meshId = k_MainRoom, .room = TempleRoom::Main, .mesh = TempleInteriorMesh::Room},
	    {.meshId = k_MainFloor, .room = TempleRoom::Main, .mesh = TempleInteriorMesh::Floor},
	    {.meshId = k_MainPool, .room = TempleRoom::Main, .mesh = TempleInteriorMesh::Pool},
	    {.meshId = k_CaveRoom, .room = TempleRoom::CreatureCave, .mesh = TempleInteriorMesh::Room},
	    {.meshId = k_CaveWater, .room = TempleRoom::CreatureCave, .mesh = TempleInteriorMesh::Water},
	    {.meshId = k_CaveFloor, .room = TempleRoom::CreatureCave, .mesh = TempleInteriorMesh::Floor},
	};
}
} // namespace

TEST(TempleDrawPlan, ChoosesTheDrawnRoomsAndTheMainRoomsDoors)
{
	// In the main room
	auto rooms = temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::Main; });
	EXPECT_EQ(rooms.drawn, std::set<TempleRoom> {TempleRoom::Main});
	EXPECT_FALSE(rooms.mainRoomDoorsOnly);
	// In the creature's room with the main room's doors shut: the main room for its doors alone
	rooms = temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::CreatureCave; });
	EXPECT_EQ(rooms.drawn, (std::set<TempleRoom> {TempleRoom::Main, TempleRoom::CreatureCave}));
	EXPECT_TRUE(rooms.mainRoomDoorsOnly);
	// On the way from the main room into the creature's room, both whole
	rooms =
	    temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::Main || room == TempleRoom::CreatureCave; });
	EXPECT_EQ(rooms.drawn, (std::set<TempleRoom> {TempleRoom::Main, TempleRoom::CreatureCave}));
	EXPECT_FALSE(rooms.mainRoomDoorsOnly);
}

TEST(TempleDrawPlan, MirrorsTheMainRoomInItsFloorAndLeavesThePoolToTheRenderer)
{
	const auto parts = Parts();
	const auto rooms = temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::Main; });
	const auto plan = temple_draw::PlanParts(parts, rooms, TempleRoom::Main);
	EXPECT_EQ(plan.drawnParts, (std::vector<bool> {true, true, false, false, false, false}));
	EXPECT_TRUE(plan.meshes.at(k_MainRoom).mirrored);
	EXPECT_FALSE(plan.meshes.at(k_MainRoom).reflective);
	EXPECT_EQ(plan.meshes.at(k_MainRoom).room, TempleRoom::Main);
	EXPECT_TRUE(plan.meshes.at(k_MainFloor).reflective);
	EXPECT_FALSE(plan.meshes.at(k_MainFloor).mirrored);
	EXPECT_EQ(plan.meshes.at(k_MainPool).instances, 0u);
	// The rooms not drawn have no plan
	EXPECT_FALSE(plan.meshes.contains(k_CaveRoom));
}

TEST(TempleDrawPlan, DrawsTheMainRoomsDoorsAloneFromASideRoom)
{
	const auto parts = Parts();
	const auto rooms = temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::CreatureCave; });
	const auto plan = temple_draw::PlanParts(parts, rooms, TempleRoom::CreatureCave);
	// The main room's own mesh for its doors, not its floor or pool; the whole creature's room
	EXPECT_EQ(plan.drawnParts, (std::vector<bool> {true, false, false, true, true, true}));
	const auto& doors = plan.meshes.at(k_MainRoom);
	EXPECT_TRUE(doors.doorsOnly);
	EXPECT_FALSE(doors.mirrored);
	EXPECT_TRUE(doors.otherRoom);
	EXPECT_FALSE(doors.sideRoom);
	const auto& cave = plan.meshes.at(k_CaveRoom);
	EXPECT_TRUE(cave.sideRoom);
	EXPECT_FALSE(cave.otherRoom);
	EXPECT_EQ(cave.room, TempleRoom::CreatureCave);
	EXPECT_TRUE(plan.meshes.at(k_CaveWater).water);
	// Only the room's own mesh carries its scrolls and controls
	EXPECT_FALSE(plan.meshes.at(k_CaveFloor).room.has_value());
}

TEST(TempleDrawPlan, GivesTheDrawListsTheirTempleFields)
{
	const auto parts = Parts();
	const auto rooms = temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::CreatureCave; });
	const auto plan = temple_draw::PlanParts(parts, rooms, TempleRoom::CreatureCave);

	RenderContext::InstancedDrawDesc doors(0, 1, false);
	temple_draw::ApplyPlan(plan.meshes.at(k_MainRoom), false, false, doors);
	EXPECT_TRUE(doors.onlyJoints);
	EXPECT_TRUE(doors.hiddenFromReflection);
	EXPECT_TRUE(doors.behindCurrentRoom);
	EXPECT_FALSE(doors.hideShutJoints);
	EXPECT_TRUE(doors.materialBlending);
	EXPECT_FALSE(doors.translucent);

	RenderContext::InstancedDrawDesc water(1, 1, false);
	temple_draw::ApplyPlan(plan.meshes.at(k_CaveWater), false, false, water);
	EXPECT_TRUE(water.translucent);
	EXPECT_TRUE(water.hideShutJoints);
	EXPECT_FALSE(water.behindCurrentRoom);

	// The main room seen from inside it: in its reflection, and its floor showing the reflection
	const auto inMain = temple_draw::PlanParts(
	    parts, temple_draw::ChooseRooms([](TempleRoom room) { return room == TempleRoom::Main; }), TempleRoom::Main);
	RenderContext::InstancedDrawDesc mainRoom(0, 1, false);
	temple_draw::ApplyPlan(inMain.meshes.at(k_MainRoom), false, false, mainRoom);
	EXPECT_FALSE(mainRoom.hiddenFromReflection);
	RenderContext::InstancedDrawDesc floor(1, 1, false);
	temple_draw::ApplyPlan(inMain.meshes.at(k_MainFloor), false, true, floor);
	EXPECT_TRUE(floor.showsReflection);
	EXPECT_TRUE(floor.hiddenFromReflection);
	EXPECT_TRUE(floor.translucent);

	// The hand is drawn as it is on the land, and never in the reflection
	RenderContext::InstancedDrawDesc hand(2, 1, false);
	temple_draw::ApplyPlan(temple_draw::MeshPlan {.instances = 1}, true, false, hand);
	EXPECT_FALSE(hand.materialBlending);
	EXPECT_TRUE(hand.hiddenFromReflection);
}

TEST(TempleDrawPlan, TheWorldsDrawListsKeepTheTemplesFieldsAtRest)
{
	// What the world's rendering system makes: the temple's fields as they are made, which its draws never read
	const RenderContext::InstancedDrawDesc desc(12, 3, true);
	EXPECT_EQ(desc.offset, 12u);
	EXPECT_EQ(desc.count, 3u);
	EXPECT_TRUE(desc.morphWithTerrain);
	EXPECT_FALSE(desc.hiddenFromReflection);
	EXPECT_FALSE(desc.showsReflection);
	EXPECT_FALSE(desc.onlyJoints);
	EXPECT_FALSE(desc.hideShutJoints);
	EXPECT_FALSE(desc.behindCurrentRoom);
	EXPECT_FALSE(desc.materialBlending);
	EXPECT_FALSE(desc.translucent);
	EXPECT_EQ(desc.uvOffset, glm::vec2(0.0f));
	EXPECT_TRUE(desc.subMeshTextures.empty());
	EXPECT_TRUE(desc.subMeshGlows.empty());
	EXPECT_TRUE(desc.hiddenSubMeshes.empty());
}

TEST(TempleDrawPlan, TurnsAJointAboutItsPivot)
{
	// A door's leaf turned a quarter about a hinge at (2, 0, 0), in a room placed at (100, 0, 0)
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 0.0f, 0.0f));
	const auto joint = glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto turned = temple_draw::JointModel(model, glm::vec3(2.0f, 0.0f, 0.0f), joint);
	// The hinge stays where it is, and a point a unit along the leaf turns a quarter about it
	const auto hinge = turned * glm::vec4(2.0f, 0.0f, 0.0f, 1.0f);
	EXPECT_NEAR(hinge.x, 102.0f, 1e-5f);
	EXPECT_NEAR(hinge.z, 0.0f, 1e-5f);
	const auto leaf = turned * glm::vec4(3.0f, 0.0f, 0.0f, 1.0f);
	EXPECT_NEAR(leaf.x, 102.0f, 1e-5f);
	EXPECT_NEAR(std::abs(leaf.z), 1.0f, 1e-5f);
	// A shut door's joint is the identity, which leaves the leaf as the room has it
	EXPECT_EQ(temple_draw::JointModel(model, glm::vec3(2.0f, 0.0f, 0.0f), glm::mat4(1.0f)), model);
}
