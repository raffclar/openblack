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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

enum class MagicTreeType
{
};

struct Tree
{
	TreeInfo type;
	float maxSize;
	uint32_t forestId = 0;
	/// The script's own flag (CREATE_NEW_TREE); the hand sets it to "inside a town" when it replants the tree.
	bool isNonScenic = true;
	/// Still growing. Set at creation when maxSize differs from the size it is created at, cleared once it reaches
	/// maxSize.
	bool growing = false;
	/// Turns left until the next growth step (info growTurns, randomised at creation)
	uint16_t growCounter = 0;
	/// Which of the 16 wind sway slots it uses, round(yAngle x 16 / 2pi) & 15 at creation, so that trees facing the same
	/// way sway together
	uint8_t windSlot = 0;
	/// This frame's bend away from the object carried by the hand, a physics object or a creature, only the drawn
	/// matrix: the angle (0 = not bent) and the horizontal direction from that object to the tree the crown leans
	/// towards
	float bendAngle = 0.0f;
	glm::vec2 bendDirection {0.0f, 1.0f};
	/// bent last frame too: the rubbing sound plays when a bend starts
	bool wasBent = false;
	/// The wood value multiplier: 1.0 for a Tree; a class that overrides it (a magic tree) sets its own
	float woodValueMultiplier = 1.0f;
};

/// A tree that was thrown or dropped where it cannot be replanted: it keeps the tree mesh and the orientation it came
/// to rest with, can be picked up again and gives wood.
struct DeadTree
{
	TreeInfo type;
	/// The wood value multiplier of the tree it was
	float woodValueMultiplier = 1.0f;
};

/// A tree a forester felled (a DeadTree with its own behaviour): it falls with physics away from the forester and has
/// no "wood here" reaction when it lands. The entity also keeps its DeadTree.
struct FelledTree
{
	entt::entity chopper {entt::null};
};

} // namespace openblack::ecs::components
