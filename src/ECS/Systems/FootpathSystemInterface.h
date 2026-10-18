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
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The footpaths laid in play (components::Footpath, components::FootpathLink, components::FootpathFinder). A town's
/// building that starts working gets a path from the town's storage pit; a town that joins a worship site gets a path
/// from its pit to the site, found over a few turns; a dance away from a temple gets a path from the nearest town's pit.
/// Each path goes round what stands between its ends, by the creatures' route search for a route that no one walks. A
/// thing that the creatures' route rule makes them go round, put on the map or removed from it, sends the paths round it or
/// back on their next turn, and the villagers on the changed paths find their way again.
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
	/// A thing that stays put comes onto the map: when the creatures' route rule makes them go round it, the paths go
	/// round it from their next turn
	virtual void ThingPut(entt::entity thing) = 0;
	/// A thing that stays put leaves the map from where it stood: when the paths went round it, they stop going round it
	/// on their next turn
	virtual void ThingRemoved(entt::entity thing, glm::vec3 position) = 0;
	/// The paths go round the things that came onto the map, and stop going round those that left it, in turn. A land's
	/// things come onto the map before its footpaths load, so no path goes round them.
	virtual void ProcessObstacleChanges() = 0;
	/// Once a turn: the obstacle changes are done; then the paths being found take a few more steps, and those found are
	/// laid
	virtual void ProcessTurn() = 0;

	/// The places of the nodes of a path laid at once between two places, none when no way is found
	[[nodiscard]] virtual std::optional<std::vector<glm::ivec2>> PlanBetween(glm::ivec2 from, glm::ivec2 to) const = 0;
};

} // namespace openblack::ecs::systems
