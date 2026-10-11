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

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A housewife's round: at home she makes dinner when one of her household is hungry, and when the home is short of
/// the household's dinner she fetches it from the town's storage pit, gossiping round the pit while it has too little;
/// otherwise she does the housework. Serving dinner keeps her a while, and clearing it away she eats what she carries.
///
/// The game has the whole round but nothing ever starts it: a villager is only ever in these states when put in them
/// from outside, as by a script. The state functions are the living action system's.
namespace openblack::ecs::villager_housewife
{

uint32_t AtHome(components::LivingAction& action);
uint32_t GotoStoragePit(components::LivingAction& action);
uint32_t ArrivesAtStoragePit(components::LivingAction& action);
uint32_t PickupFromStoragePit(components::LivingAction& action);
uint32_t ReturnHomeWithFood(components::LivingAction& action);
uint32_t MakeDinner(components::LivingAction& action);
uint32_t ServesDinner(components::LivingAction& action);
uint32_t ClearsAwayDinner(components::LivingAction& action);
uint32_t DoesHousework(components::LivingAction& action);
uint32_t GossipsAroundStoragePit(components::LivingAction& action);

} // namespace openblack::ecs::villager_housewife
