/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "ECS/Components/Footpath.h"
#include "ECS/FootpathPlanning.h"
#include "ECS/FootpathWorld.h"
#include "ECS/Systems/FootpathSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FootpathSystem final: public FootpathSystemInterface
{
public:
	/// The footpaths of the game's own world
	FootpathSystem();
	/// The footpaths of a world given to them, as a test's fake
	explicit FootpathSystem(std::unique_ptr<footpath_world::WorldInterface> world);
	~FootpathSystem() override;

	void BuildingWorking(entt::entity building) override;
	void TownJoinedSite(entt::entity site, entt::entity town) override;
	void DanceMade(entt::entity dance, entt::entity town) override;
	void DanceMoved(entt::entity dance) override;
	void ThingPut(entt::entity thing) override;
	void ThingRemoved(entt::entity thing, glm::vec3 position, bool filed) override;
	void ProcessObstacleChanges() override;
	void CatchUpWithMap() override;
	void ProcessTurn() override;
	[[nodiscard]] std::optional<std::vector<glm::ivec2>> PlanBetween(glm::ivec2 from, glm::ivec2 to) const override;

private:
	/// A path sent round a thing put on the map, or back from one removed from it. Whether its nodes changed. The caller
	/// gives the square that holds all the path's nodes, which a thing put far from it cannot touch.
	bool Reroute(entt::entity footpath, const components::FootpathObstacleChange& change,
	             const std::optional<std::pair<glm::ivec2, glm::ivec2>>& nodeSquare);
	/// The path takes the planned nodes, each on the land
	void SetNodes(components::Footpath& footpath, std::span<const footpath_planning::PlanNode> nodes) const;
	/// A new path with two nodes, the first where the first thing is arrived at
	entt::entity MakeFootpath(glm::ivec2 first, glm::ivec2 last);
	/// The path's nodes between its two ends are laid along the route
	void LayRoute(entt::entity footpath, std::span<const glm::ivec2> route, bool settle);
	/// A thing keeps a path among those that lead from it, the newest first
	void AddFootpath(entt::entity thing, entt::entity footpath);
	/// The town's storage pit, while it stands built
	[[nodiscard]] std::optional<entt::entity> StandingStoragePit(entt::entity town) const;

	std::unique_ptr<footpath_world::WorldInterface> _world;
};

} // namespace openblack::ecs::systems
