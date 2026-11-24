/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawnTemple.h"

#include <glm/gtx/transform.hpp>

#include "ECS/Components/LightBeam.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
[[nodiscard]] glm::mat4 PlaceOf(const ecs::components::Transform& transform)
{
	return glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
}
} // namespace

void DrawnTemple::CopyRoomParts(const ecs::Registry& rooms)
{
	using namespace ecs::components;
	ClearRoomParts();
	rooms.Each<const MistDome, const Transform, const TempleInteriorPart>(
	    [this](const MistDome& mist, const Transform& transform, const TempleInteriorPart& part) {
		    if (mist.dome != nullptr && IsRoomDrawn(part.room))
		    {
			    mists.push_back({.dome = mist.dome,
			                     .uvOffset = mist.uvOffset,
			                     .colour = mist.colour,
			                     .position = transform.position,
			                     .scale = transform.scale});
		    }
	    });
	rooms.Each<const LightBeam, const TempleInteriorPart>([this](const LightBeam& beam, const TempleInteriorPart& part) {
		cones.push_back({.cone = beam.cone, .room = part.room});
	});
	rooms.Each<const Mesh, const Transform, const TempleInteriorPart>(
	    [this](const Mesh& mesh, const Transform& transform, const TempleInteriorPart& part) {
		    if (part.mesh == TempleInteriorMesh::Room)
		    {
			    roomModels.push_back({.mesh = mesh.id, .room = part.room, .model = PlaceOf(transform)});
		    }
	    });
	// The last part of the main room's model, in the order of the parts with a place, gives the place of the exit glow
	rooms.Each<const TempleInteriorPart, const Transform>([this](const TempleInteriorPart& part, const Transform& transform) {
		if (part.room == TempleRoom::Main && part.mesh == TempleInteriorMesh::Room)
		{
			mainRoomModel = PlaceOf(transform);
		}
	});
}

void DrawnTemple::ClearRoomParts()
{
	mists.clear();
	cones.clear();
	roomModels.clear();
	mainRoomModel = glm::mat4(1.0f);
}
