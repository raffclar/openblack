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

#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/ChimneySmoke.h"

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs::components
{
struct Transform;
}

/// The chimney smoke of the houses (wiki docs/bw1-notes/rendering-objects.md, "Chimney smoke"). The renderer
/// calls UpdateHandWind once per frame, then, for every Abode on screen, UpdateState (the
/// rules of the abode's draw) and Advance (the Z-sorter callback, which simulates while it draws).
namespace openblack::ecs::chimney_smoke
{

/// One visible puff of this frame, as the sprite draw gets it
struct DrawnPuff
{
	glm::vec3 position;
	float halfWidth; ///< the sprite's size, world units at the puff's depth
	float angle;     ///< in the screen plane: x_v = cos x + sin y, y_v = -sin x + cos y
	uint32_t cell;   ///< 0..15, 8 cells per row of smoke.raw
	uint32_t argb;   ///< alpha << 24 | the smoke's RGB
};

/// The chimney in world space: the mesh's chimney point through the object's 3 x 3
/// matrix (rotation and scale) plus its position (with the foundation altitude)
glm::vec3 ChimneyWorldPosition(const glm::vec3& meshPoint, const components::Transform& transform);

/// gives the abode its smoke when its mesh has a chimney (flag 0x400);
/// grey 0x808080 for a workshop, white otherwise. Does nothing for a mesh without one.
void Attach(entt::entity abode, const graphics::L3DMesh& mesh, const components::Transform& transform, bool workshop);

/// a new smoke at a chimney (10 hidden puffs, ages i x 90, random angles and spins)
components::ChimneySmoke Create(const glm::vec3& chimney, uint32_t rgb);

/// once per frame, before the objects are drawn: the hand position, wind and
/// speed that Advance reads, from the hand and its velocity (advanced here
/// once per game turn)
void UpdateHandWind();

/// for an abode on screen this frame: lit = someone is at home, or a workshop making a
/// scaffold. Returns whether the smoke goes to the Z-sorter (state != 3).
bool UpdateState(components::ChimneySmoke& smoke, bool lit);

/// advances the 10 puffs by the frame's milliseconds (0 while paused) and appends the visible ones,
/// in their order 0..9; a dying smoke that drew nothing dies (state 3)
void Advance(components::ChimneySmoke& smoke, float milliseconds, std::vector<DrawnPuff>& drawn);

/// OPENBLACK_TEST_CHIMNEY=all: every chimney smokes (for screenshots while no villager ever goes home)
bool ForcedByTestHook();

} // namespace openblack::ecs::chimney_smoke
