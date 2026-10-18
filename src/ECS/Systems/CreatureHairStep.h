/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>

#include "Creature/CreatureHair.h"
#include "ECS/Components/CreatureHair.h"

namespace openblack::ecs::systems
{

/// What a hair group takes from its mesh: how many strands and points they have, how they hang and how they look
struct HairGroupShape
{
	size_t strandCount {0};
	uint32_t segmentCount {0};
	creature_hair::Physics physics;
	glm::ivec3 colour {0};
	bool textured {false};
};

/// Gives the shape of the mesh's hair group of an index
using HairGroupShapes = std::function<HairGroupShape(size_t group)>;
/// Gives where the strand of a group is rooted on the posed body, or none where it has no root this frame
using HairRoots = std::function<std::optional<creature_hair::Root>(size_t group, size_t strand)>;

/// Moves the hair of a body for some seconds. The hair takes its groups and strands from the mesh: with other groups
/// than the mesh has, or before it has started, each strand starts again from zero.
inline void StepHair(components::CreatureHair& hair, size_t groupCount, const HairGroupShapes& shapeOf, const HairRoots& rootOf,
                     float seconds)
{
	if (hair.groups.size() != groupCount)
	{
		hair = {};
		hair.groups.resize(groupCount);
	}
	for (size_t g = 0; g < groupCount; ++g)
	{
		const auto shape = shapeOf(g);
		auto& group = hair.groups[g];
		group.colour = shape.colour;
		group.halfWidth = shape.physics.halfWidth;
		group.textured = shape.textured;
		group.strands.resize(shape.strandCount);
		for (size_t s = 0; s < shape.strandCount; ++s)
		{
			const auto root = rootOf(g, s);
			if (!root)
			{
				continue;
			}
			auto& strand = group.strands[s];
			if (!hair.started || strand.positions.size() != shape.segmentCount)
			{
				strand = creature_hair::FromZero(shape.segmentCount);
			}
			creature_hair::Step(strand, *root, shape.physics, seconds);
		}
	}
	hair.started = true;
}

} // namespace openblack::ecs::systems
