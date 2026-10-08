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

#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::systems
{
/// The fish farm side as the fishermen see it, and the inputs of a catch (the season and the tribal power)
class VillagerFishFarmsInterface
{
public:
	virtual ~VillagerFishFarmsInterface() = default;

	/// The town's fish farms, newest first
	[[nodiscard]] virtual std::vector<entt::entity> TownFishFarms(entt::entity town) const = 0;
	[[nodiscard]] virtual int32_t Score(entt::entity farm) const = 0;
	[[nodiscard]] virtual map_coords::MapCoords GetArrivePos(entt::entity farm) const = 0;
	/// A random fishing spot by the farm (two random draws)
	[[nodiscard]] virtual map_coords::MapCoords FishingSpot(entt::entity farm) const = 0;
	[[nodiscard]] virtual uint32_t FishermanCount(entt::entity farm) const = 0;
	[[nodiscard]] virtual bool HasFisherman(entt::entity farm, entt::entity villager) const = 0;
	virtual void AddFisherman(entt::entity farm, entt::entity villager) = 0;
	virtual void RemoveFisherman(entt::entity farm, entt::entity villager) = 0;
	[[nodiscard]] virtual bool IsAvailable(entt::entity farm) const = 0;
	/// The season of the current turn, 0 to 3
	[[nodiscard]] virtual uint32_t Season() const = 0;
	/// The tribal power that scales the villager's catch
	[[nodiscard]] virtual float TribalPower(entt::entity villager) const = 0;
};
} // namespace openblack::ecs::systems
