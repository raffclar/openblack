/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::archetypes
{
/// A town centre's totem: the tribe's plinth at the centre's totem point, with the centre's turn and scale, and an icon
/// standing on it
class VillageTotemArchetype
{
public:
	/// Made for a town centre standing built in a town; none when the centre's model has no totem point
	static entt::entity Create(entt::entity townCentre, const GAbodeInfo& info);
};
} // namespace openblack::ecs::archetypes
