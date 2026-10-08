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
/// The field side as the farmers see it: the town's fields, what a field needs and its farmer list
class VillagerFieldsInterface
{
public:
	virtual ~VillagerFieldsInterface() = default;

	/// The town's fields, newest first
	[[nodiscard]] virtual std::vector<entt::entity> TownFields(entt::entity town) const = 0;
	[[nodiscard]] virtual float GetDesireToBeFarmed(entt::entity field) const = 0;
	/// 1 to sow, 2 to harvest, else 0
	[[nodiscard]] virtual int GetFieldActivity(entt::entity field) const = 0;
	[[nodiscard]] virtual map_coords::MapCoords GetArrivePos(entt::entity field) const = 0;
	/// A random work point on the field (two random draws)
	[[nodiscard]] virtual map_coords::MapCoords RandomFarmPoint(entt::entity field) const = 0;
	/// False when the crop is not ripe; else a random work point in out
	[[nodiscard]] virtual bool RipeFarmPoint(entt::entity field, map_coords::MapCoords& out) const = 0;
	virtual bool PlantCrop(entt::entity field) = 0;
	[[nodiscard]] virtual bool IsStillSowing(entt::entity field) const = 0;
	/// Takes food from the field and returns how much was taken
	[[nodiscard]] virtual int32_t RemoveFood(entt::entity field, float amount) = 0;
	virtual void AddFarmer(entt::entity field, entt::entity villager) = 0;
	virtual void RemoveFarmer(entt::entity field, entt::entity villager) = 0;
	/// Whether the thing is still one of the game's fields
	[[nodiscard]] virtual bool IsField(entt::entity thing) const = 0;
};
} // namespace openblack::ecs::systems
