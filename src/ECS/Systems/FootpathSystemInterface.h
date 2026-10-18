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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{

/// The footpaths laid in play (components::Footpath, components::FootpathLink, components::FootpathFinder). A town's
/// building that starts working gets a path from the town's storage pit; a town that joins a worship site gets a path
/// from its pit to the site, found over a few turns; a dance away from a temple gets a path from the nearest town's pit.
/// Each path goes round what stands between its ends, by the creatures' route search for a route that no one walks.
class FootpathSystemInterface
{
public:
	virtual ~FootpathSystemInterface() = default;

	/// A building of a town is finished, is made whole again or is made already standing in play: it gets a path from
	/// its town's storage pit to its door, unless it is the pit or the town has no pit standing
	virtual void BuildingWorking(entt::entity building) = 0;
	/// A town joins a worship site: unless one of the site's paths already comes from the pit, a path from the town's
	/// storage pit to the site's gate is found over the next turns
	virtual void TownJoinedSite(entt::entity site, entt::entity town) = 0;
	/// A dance is made: one that is not a worship site's or a temple's gets a path from the storage pit of its town, or
	/// of the town nearest it
	virtual void DanceMade(entt::entity dance, entt::entity town) = 0;
	/// Once a turn: the paths being found take a few more steps, and those found are laid
	virtual void ProcessTurn() = 0;

	/// The places of the nodes of a path laid at once between two places, none when no way is found
	[[nodiscard]] virtual std::optional<std::vector<glm::ivec2>> PlanBetween(glm::ivec2 from, glm::ivec2 to) const = 0;
};

} // namespace openblack::ecs::systems
