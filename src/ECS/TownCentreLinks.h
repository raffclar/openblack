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

#include <unordered_map>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::town_centre_links
{

/// The totem of each village centre and the town of each town number, found in one walk of each store. A walk for one
/// centre gives the same totem and the same town.
class Links
{
public:
	/// Finds the links of all the centres:
	/// - the totem of a centre is the first totem with a transform on it, in the order of a walk of these totems;
	/// - the town of a number is the last town with an impression of that number, in the order of a walk of these towns.
	/// The walk goes through the smaller store of the two, as the walk of a view does. A tie keeps the first store.
	[[nodiscard]] static Links Of(const Registry& registry);

	/// The totem on the centre, null when no totem with a transform stands on it
	[[nodiscard]] entt::entity TotemOf(entt::entity centre) const;
	/// The town with an impression that has this number, null when there is none
	[[nodiscard]] entt::entity TownOf(uint32_t townId) const;

private:
	std::unordered_map<entt::entity, entt::entity> _totems;
	std::unordered_map<uint32_t, entt::entity> _towns;
};

} // namespace openblack::ecs::town_centre_links
