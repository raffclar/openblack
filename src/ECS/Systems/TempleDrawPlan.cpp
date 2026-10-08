/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleDrawPlan.h"

#include <array>

#include <glm/gtx/transform.hpp>

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::TempleInteriorMesh;

temple_draw::Rooms temple_draw::ChooseRooms(const std::function<bool(TempleRoom)>& isRoomDrawn)
{
	constexpr std::array k_Rooms {TempleRoom::Main,  TempleRoom::CreatureCave, TempleRoom::Challenge, TempleRoom::Credits,
	                              TempleRoom::Multi, TempleRoom::Options,      TempleRoom::SaveGame};
	Rooms rooms;
	for (const auto room : k_Rooms)
	{
		if (isRoomDrawn(room))
		{
			rooms.drawn.insert(room);
		}
	}
	rooms.mainRoomDoorsOnly = !rooms.drawn.contains(TempleRoom::Main);
	if (rooms.mainRoomDoorsOnly)
	{
		rooms.drawn.insert(TempleRoom::Main);
	}
	return rooms;
}

temple_draw::Plan temple_draw::PlanParts(std::span<const Part> parts, const Rooms& rooms, TempleRoom currentRoom)
{
	Plan plan;
	plan.drawnParts.reserve(parts.size());
	for (const auto& part : parts)
	{
		if (!rooms.drawn.contains(part.room))
		{
			plan.drawnParts.push_back(false);
			continue;
		}
		auto& mesh = plan.meshes[part.meshId];
		mesh.otherRoom = mesh.otherRoom || part.room != currentRoom;
		mesh.water = mesh.water || part.mesh == TempleInteriorMesh::Water;
		if (part.mesh == TempleInteriorMesh::Pool)
		{
			plan.drawnParts.push_back(false);
			continue;
		}
		mesh.sideRoom = mesh.sideRoom || part.room != TempleRoom::Main;
		if (part.mesh == TempleInteriorMesh::Room && !mesh.room.has_value())
		{
			mesh.room = part.room;
		}
		if (part.room == TempleRoom::Main && rooms.mainRoomDoorsOnly)
		{
			// The game draws just the room's mesh, and of that just the doors
			const bool doors = part.mesh == TempleInteriorMesh::Room;
			if (doors)
			{
				++mesh.instances;
				mesh.doorsOnly = true;
			}
			plan.drawnParts.push_back(doors);
			continue;
		}
		++mesh.instances;
		plan.drawnParts.push_back(true);
		if (part.room == TempleRoom::Main && part.mesh == TempleInteriorMesh::Room)
		{
			mesh.mirrored = true;
		}
		else if (part.room == TempleRoom::Main && part.mesh == TempleInteriorMesh::Floor)
		{
			mesh.reflective = true;
		}
	}
	return plan;
}

void temple_draw::ApplyPlan(const MeshPlan& plan, bool hand, bool allBlended, RenderContext::InstancedDrawDesc& desc)
{
	// The main room's doors seen from another room are left out of its reflection too
	desc.hiddenFromReflection = !plan.mirrored || plan.doorsOnly;
	desc.showsReflection = plan.reflective;
	desc.onlyJoints = plan.doorsOnly;
	// Each side room has its own copy of its door to the main room, which the game never shows: drawn shut, in place,
	// it pokes up above the arch of the doorway. Until how the game keeps it out of sight is known, the side rooms'
	// doors are drawn only while they swing.
	desc.hideShutJoints = plan.sideRoom;
	desc.behindCurrentRoom = plan.otherRoom;
	// The game draws each primitive of the temple's meshes by its material, blended or not, in their order. The meshes
	// of nothing but blended primitives, as the rooms' domes and floors are, go over the rest.
	if (!hand)
	{
		desc.materialBlending = true;
		desc.translucent = allBlended;
	}
	// The creature's room draws its water by the materials' alpha
	if (plan.water)
	{
		desc.translucent = true;
	}
}

glm::mat4 temple_draw::JointModel(const glm::mat4& model, const glm::vec3& pivot, const glm::mat4& joint)
{
	return model * glm::translate(pivot) * joint * glm::translate(-pivot);
}
