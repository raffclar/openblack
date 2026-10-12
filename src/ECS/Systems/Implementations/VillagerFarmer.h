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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A farmer walks to one of its town's fields that wants farming. While the field still wants sowing it walks to a
/// random spot in it and sows it, one sowing a spot, until the field is fully sown. Once the crop has grown and is
/// ripe it digs it up at random spots instead, taking its fill and carrying it to the store whenever it has less room
/// left than a load of crop. The state functions are the living action system's.
namespace openblack::ecs::villager_farmer
{

/// The best of a town's fields for the villager, and how good it is; none when no field wants farming
struct FieldChoice
{
	entt::entity field {entt::null};
	float score {0.0f};
};
[[nodiscard]] std::optional<FieldChoice> FindBestField(components::LivingAction& action);

/// The villager walks to a field to farm it, or to its town's best field when none is given. 1 when it went.
uint32_t VillagerBecomesFarmer(components::LivingAction& action, std::optional<entt::entity> field);
/// The villager walks to a field that wants sowing or harvesting, with its first spot there picked. 1 when it went.
uint32_t SetFarmerGotoField(components::LivingAction& action, entt::entity field);

uint32_t FarmerArrivesAtFarm(components::LivingAction& action);
uint32_t FarmerPlantsCrop(components::LivingAction& action);
uint32_t FarmerDigsUpCrop(components::LivingAction& action);
/// Taking up farming from another kind of state it joins the field's farmers, and leaving for another kind it leaves
/// them
bool EnterFarming(components::LivingAction& action, VillagerStates previous, VillagerStates next);
bool ExitFarming(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_farmer
