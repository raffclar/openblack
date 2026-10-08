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
#include <vector>

#include "RoutePlanner/ObstacleGrid.h"

namespace openblack::ecs::systems
{
/// The route planner's shared state: the square grid's callbacks every holder uses, the search's obstacle hook, and
/// the footpaths' pool of reusable holders (route_planner::ObstacleGrid and the footpaths go through it)
class RoutePlanStateSystemInterface
{
public:
	/// InstallCallbacks's callbacks (set at every land load) and SetObstacleHook's hook and context (set for one
	/// follower update)
	struct Callbacks
	{
		route_planner::FillSquareFn fillSquare {nullptr};
		route_planner::SpecialObjectsFn specialObjects {nullptr};
		route_planner::ObstacleHook obstacleHook {nullptr};
		void* obstacleContext {nullptr};
	};
	/// Every holder ever made, and the ones not leased now (last in, first out); the pool never shrinks
	struct HolderPool
	{
		std::vector<std::unique_ptr<route_planner::ObstacleGrid>> all;
		std::vector<route_planner::ObstacleGrid*> free;
	};

	virtual ~RoutePlanStateSystemInterface() = default;

	[[nodiscard]] virtual Callbacks& GetCallbacks() = 0;
	[[nodiscard]] virtual HolderPool& GetHolderPool() = 0;
};
} // namespace openblack::ecs::systems
