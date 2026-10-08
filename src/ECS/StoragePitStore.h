/*******************************************************************************
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

#include <entt/fwd.hpp>

#include "ECS/PotResource.h"
#include "Enums.h"

namespace openblack::ecs
{
/// What a storage pit reports to its player's creature to mimic, once it has taken a resource from an interface. The
/// values are the original's deed numbers
enum class StoragePitDeed : uint8_t
{
	PutFoodInStoragePit = 2,
	PutWoodInStoragePit = 4,
	PutWoodInBuildingSite = 7,
	StealFoodFromFarm = 43,
	StealFoodFromStoragePit = 44,
	StealWoodFromStoragePit = 45,
};

/// A storage pit as one store: its five wood piles and its food pile hold the store's resources, and the Abode totals
/// (the original's mirror of them) follow them.
class StoragePitStore
{
public:
	/// With a building site (the pit not built yet) WOOD and ANY go to the site's AddResource. Only FOOD and WOOD:
	/// food to the food pile, wood to Wood Pile 1..5 while n != 0, each pile's AddToPotDirect (the pile sound with the n
	/// still asked for, the cap at maxAmountInPot when next < 19, the poison, SetSize). The pulse: the store held none of
	/// it before and something went in -> the town's pulse. Then DoResourceAdding (the mirror follows the piles here).
	/// Returns the amount stored. `dropper` is the hand's status, default none. (approximate) CREATE_ABODE's initial
	/// fill (AbodeArchetype) goes through here too: one pile sound per pile reached and the pulse; how the original
	/// fills a new pit is (pending)
	static uint32_t AddResource(entt::entity store, ResourceType type, uint32_t amount,
	                            const pot_resource::Dropper& dropper = {}, bool poisoned = false);
	/// Only FOOD (the food pile) and WOOD (pile 5 -> 1), each pile's JustRemoveResource
	/// (object_resources::RemoveFromPotDirect: an emptied pile loses its poison, its reaction and its fire, and stays);
	/// something removed -> DoResourceRemoving (the town's CallDesireFunction with the store's total before the
	/// removal). Returns the amount removed.
	static uint32_t RemoveResource(entt::entity store, ResourceType type, uint32_t amount,
	                               const pot_resource::Dropper& dropper = {});
	/// After the pit took a resource from an interface: the deed its creature would mimic, or none for anything but a
	/// storage pit. Wood on an unbuilt pit is building-site wood. Otherwise the deed depends on whether the player who
	/// owned what the interface last picked up (Dropper::sourceOwner) is the dropper's own, none never being; food that
	/// is not draws GameRand(2) to tell a farm from a pit, even when the player has no creature. As in the original it is asked
	/// twice for one deposit into a pit with a town: at the end of DoResourceAdding, and again by the deposit (an object given,
	/// when something was taken; a resource put down at a point, whatever was taken). (pending) The creature's mimicry itself;
	/// for now the deed is returned
	static std::optional<StoragePitDeed> DoCreatureMimicAfterAddingResource(entt::entity store, ResourceType type,
	                                                                        const pot_resource::Dropper& dropper);
	/// WOOD total - 5 x the Wood Pile 1 pot's maxAmountInPot, any other type total - the Storage Pit Food Pile's
	/// maxAmountInPot; signed
	[[nodiscard]] static int32_t AmountOverMaximum(entt::entity store, ResourceType type);
	/// The Abode totals set again from the piles, after a pile changed outside Add / Remove
	static void SyncTotals(entt::entity store);
	/// A store pile reports the store's total.
	[[nodiscard]] static uint32_t GetResource(entt::entity store, ResourceType type);
	/// The storage pit owning a pile, or entt::null.
	[[nodiscard]] static entt::entity OwnerOf(entt::entity pile);
	StoragePitStore() = delete;
};
} // namespace openblack::ecs
