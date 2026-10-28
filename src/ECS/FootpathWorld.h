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
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Footpath.h"

namespace openblack::ecs
{
class Registry;
}

/// What the footpaths need of the rest of the game: the entities, the map's cells, the route search, the rules of the
/// things that the paths go round, the towns' storage pits, the land and the walkers. The game's own world works through
/// the game's systems; tests give a fake.
namespace openblack::ecs::footpath_world
{

using Plan = components::FootpathFinder::Plan;

/// How far the search for a way has come
enum class RouteStatus
{
	Searching,
	Found,
	Failed,
};

/// What the paths go round of a thing at a position
struct ObstacleLook
{
	/// The thing is whole enough to tell
	bool known {false};
	/// Its radius across the ground in metres. None when the creatures' route rule lets them through it.
	std::optional<float> radius;
};

class WorldInterface
{
public:
	virtual ~WorldInterface() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// The game turn
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	/// The things made since the last time go into the map's cells. Each one that stays put goes to the footpaths.
	virtual void SyncMap() = 0;
	/// Where a path to a thing ends, in whole map units
	[[nodiscard]] virtual glm::ivec2 ArrivePlace(entt::entity thing) const = 0;
	/// The two points that a path between two things is planned between: where each is arrived at, moved out of its
	/// building when it is a building
	[[nodiscard]] virtual std::pair<glm::vec2, glm::vec2> PlannedEnds(entt::entity from, entt::entity to) const = 0;
	/// What the paths go round of a thing standing at a position
	[[nodiscard]] virtual ObstacleLook ObstacleOf(entt::entity thing, glm::vec3 position) const = 0;
	/// A search for a way that no one walks between two points, round what stands on the map now. Nothing that holds
	/// either end is gone round.
	[[nodiscard]] virtual Plan StartRoute(glm::vec2 start, glm::vec2 goal) const = 0;
	/// A plan that a load brought back gets again what it does not keep, so that its next step goes round what stands on
	/// the map
	virtual void MakeLive(Plan& plan) const = 0;
	/// Takes the search of the plan one step further
	virtual void StepRoute(Plan& plan) const = 0;
	[[nodiscard]] virtual RouteStatus StatusOfRoute(const Plan& plan) const = 0;
	/// The places of the route from the start to the goal, once the search of the plan found it
	[[nodiscard]] virtual std::optional<std::vector<glm::ivec2>> RouteOf(const Plan& plan) const = 0;
	/// The town a building belongs to, none if there is none
	[[nodiscard]] virtual entt::entity TownOf(entt::entity building) const = 0;
	/// The town's storage pit, built or not
	[[nodiscard]] virtual std::optional<entt::entity> StoragePitOf(entt::entity town) const = 0;
	/// The building stands finished
	[[nodiscard]] virtual bool IsBuilt(entt::entity building) const = 0;
	/// The height of the land at a point
	[[nodiscard]] virtual float LandHeightAt(glm::vec2 point) const = 0;
	/// The altitude of the land at a place in whole map units
	[[nodiscard]] virtual float AltitudeAt(glm::ivec2 place) const = 0;
	/// A walker finds its way again from where it is
	virtual void LookAgain(entt::entity walker) = 0;
	/// A walker walks straight on to a place in whole map units
	virtual void WalkTo(entt::entity walker, glm::ivec2 place) = 0;
};

} // namespace openblack::ecs::footpath_world
