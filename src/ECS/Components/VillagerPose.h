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

#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "ECS/VillagerDrawRules.h"

namespace openblack::ecs::components
{

/// How a villager is drawn this frame: the clip its state plays, its place in it in milliseconds, and the bones posed
/// from it. The bones are posed only while the villager is in view, and kept from the last time it was otherwise. With
/// no bones the villager is drawn in the pose its model rests in.
struct VillagerPose
{
	AnimId clip {AnimId::Invalid};
	uint32_t place {0};
	std::vector<glm::mat4> bones;
	/// Where it stood when the last turn began, from where its drawing glides while it walks
	std::optional<glm::vec3> turnStart;
	/// Where it is drawn this frame and the heading it is drawn with (the game's angle), once its drawing has worked
	/// them out; none while it lies in the physics or the hand holds it, when it is drawn where it is
	std::optional<glm::vec3> drawnAt;
	std::optional<float> drawnHeading;
	/// The heading its drawing turns after the way it faces, kept from frame to frame
	std::optional<float> easedHeading;
	/// Drawn in high detail: the heading its body is turned to after that, and its change between clips
	std::optional<float> detailedHeading;
	villager_draw::ClipBlendTrack clipBlend;
};

/// Where a villager is drawn this frame: where its drawing put it, or where it stands
[[nodiscard]] inline glm::vec3 DrawnPosition(const glm::vec3& standsAt, const VillagerPose* pose)
{
	return pose != nullptr && pose->drawnAt.has_value() ? *pose->drawnAt : standsAt;
}

/// The turn a villager is drawn with this frame: its drawn heading's, or its own
[[nodiscard]] inline glm::mat3 DrawnRotation(const glm::mat3& rotation, const VillagerPose* pose)
{
	if (pose == nullptr)
	{
		return rotation;
	}
	if (pose->drawnHeading.has_value())
	{
		return villager_draw::RotationOf(*pose->drawnHeading);
	}
	return rotation;
}

} // namespace openblack::ecs::components
