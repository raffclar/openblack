/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

/// Where a creature stands at the end of its turn. A creature's Transform moves once a turn, through here; between turns
/// only its drawn pose moves (components::CreatureDrawPose).
namespace openblack::ecs::creature_pose
{

/// Where a creature's body is drawn this frame
struct DrawnPlacement
{
	glm::vec3 position;
	glm::mat3 rotation;
};

/// The creature's Transform takes the turn's position, through the map cells (map_cells::MoveMapObject, which links it
/// to its new cell when it changes), and is turned to face along the heading (creature_locomotion::HeadingOf)
void CommitTurnPose(entt::entity creature, glm::vec3 position, float heading);

/// The heading a body's rotation faces: it is turned about y, and its mesh looks back along +z
[[nodiscard]] float ReadHeading(const glm::mat3& rotation);

/// The heading of a walk along a route the follower moved on from one point to another (creature_locomotion::HeadingOf
/// of the way it went), or `previous` when it hardly moved
[[nodiscard]] float HeadingFromFollower(glm::vec2 before, glm::vec2 after, float previous);

/// Between turns: its drawn pose (components::CreatureDrawPose), while that pose belongs to the turn that put its
/// Transform where it is. Nothing otherwise: not moved by its locomotion yet, put somewhere else by something other than
/// its turn, or drawn by the hand or the physics. Only reads the registry, so it makes no storage. ecs::DrawnModel draws
/// the body there
[[nodiscard]] std::optional<DrawnPlacement> BetweenTurns(const Registry& registry, entt::entity creature);

/// The position and rotation ecs::DrawnModel draws the body at, from the same sources in the same order: the hand's
/// pose, the physics' pose, BetweenTurns, else its Transform. For the parts that need the body's rotation on its own
/// (where the eyes look ahead, the turn of what it holds); the matrix is ecs::DrawnBodyModel's. Needs a Transform
[[nodiscard]] DrawnPlacement DrawnPlacementOf(const Registry& registry, entt::entity creature);

/// The scale the body is drawn at, as ecs::DrawnModel draws it: smaller in its temple's pen
/// (components::CreatureDrawPose::scale), else its Transform's. Needs a Transform
[[nodiscard]] glm::vec3 DrawnScale(const Registry& registry, entt::entity creature);

/// How much of its own size the body is drawn at (DrawnScale over its Transform's scale, along x): 1 but in its pen.
/// For what is sized apart from the body's matrix (the eyes, the hair's strands), so that they shrink with it
[[nodiscard]] float DrawnSizeShare(const Registry& registry, entt::entity creature);

} // namespace openblack::ecs::creature_pose
