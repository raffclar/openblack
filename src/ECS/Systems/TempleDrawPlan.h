/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/Temple.h"
#include "ECS/Systems/RenderingSystemInterface.h"

/// What the temple draws of its rooms' meshes each frame, worked out from the parts alone so that it can be tested
/// without a registry or a renderer (RenderingSystemTemple builds its draw lists from it)
namespace openblack::ecs::systems::temple_draw
{

/// The rooms the temple draws this frame
struct Rooms
{
	std::set<TempleRoom> drawn;
	/// The main room is drawn for its doors alone: from another room, while its doors are shut
	bool mainRoomDoorsOnly {false};
};

/// The game draws the room the player is in and the room the camera is on its way into. From the other rooms the main
/// room, which they lead off, is drawn whole only while one of its doors is open, and otherwise just its doors.
[[nodiscard]] Rooms ChooseRooms(const std::function<bool(TempleRoom)>& isRoomDrawn);

/// One of the temple's parts with a mesh, as the registry holds it
struct Part
{
	entt::id_type meshId;
	TempleRoom room;
	components::TempleInteriorMesh mesh;
};

/// How the temple draws one of its meshes, from the parts that have it
struct MeshPlan
{
	/// The parts of the mesh that are drawn
	uint32_t instances {0};
	/// The main room's own mesh, which the game mirrors through the plane of its origin under its floor
	bool mirrored {false};
	/// The main room's floor, drawn over that reflection, blended by the floor's alpha
	bool reflective {false};
	/// Just its submeshes with joints are drawn: the main room's doors, seen from another room
	bool doorsOnly {false};
	/// Of a room other than the main room
	bool sideRoom {false};
	/// Of a room the player isn't in
	bool otherRoom {false};
	/// The creature's room's water, whose texture slides down it
	bool water {false};
	/// The room whose own mesh it is, which carries the room's scrolls and controls
	std::optional<TempleRoom> room;
};

struct Plan
{
	/// Whether each part, in the order given, is drawn
	std::vector<bool> drawnParts;
	/// Each mesh's plan, in the order of the meshes' ids
	std::map<entt::id_type, MeshPlan> meshes;
};

/// The parts the temple draws and how. The renderer draws the main room's pool itself, twice, so it is never drawn
/// as a part.
[[nodiscard]] Plan PlanParts(std::span<const Part> parts, const Rooms& rooms, TempleRoom currentRoom);

/// The temple's fields of a mesh's draw list from its plan. The hand is drawn as it is everywhere: not by its materials
/// one by one, and never in the main room's reflection. `allBlended`: every primitive of the mesh is blended.
void ApplyPlan(const MeshPlan& plan, bool hand, bool allBlended, RenderContext::InstancedDrawDesc& desc);

/// A submesh with a joint, as a door's leaf, turns about its pivot by its joint's matrix, before the mesh's own matrix
[[nodiscard]] glm::mat4 JointModel(const glm::mat4& model, const glm::vec3& pivot, const glm::mat4& joint);

} // namespace openblack::ecs::systems::temple_draw
