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

#include <functional>

#include <entt/entity/entity.hpp>

#include "ECS/PotResource.h"
#include "Enums.h"

// GetResource / RemoveResource / AddResource / IsPoisoned of the original for
// the objects that hold resources: the abodes, the storage pit (StoragePitStore),
// the pots and piles and the villagers.
// `dropper` is the original's interface status: none for villagers and scripts, the hand's for
// the hand (its branch: the desire before and after, the alignment and the town's belief).

namespace openblack::ecs::object_resources
{
/// an abode its own amount, a storage pit the total of its piles (StoragePitStore); a pile of a storage pit the pit's
/// total (part of a structure, the building-site test before it); any other pot its own amount when the resource is
/// its own (PotAmount); a building site's pile its own amount
[[nodiscard]] uint32_t GetResource(entt::entity object, ResourceType type);
/// the resource type of the objects held here: a pot or a pile its info's type (GPotInfo
/// resourceType), a scaffold WOOD; ResourceType::None for any other class (a
/// tree's is answered by ObjectDelivery's ResourceOf)
[[nodiscard]] ResourceType GetResourceType(entt::entity object);
/// a pot's or a pile's amount when the type is the pot's own, else 0; 0 for what is not a pot. The out poison flag is
/// not returned
[[nodiscard]] uint32_t PotAmount(entt::entity pot, ResourceType type);
/// the storage pit (StoragePitStore); an abode DoResourceRemoving (min(amount, what it has)); a pile of a storage pit:
/// the touched pile gives n - min(over, n), over = the pit's amount above its
/// maximum, then the pit's own order, pile 5 -> 1, the rest; a building site's pile the site's RemoveResource;
/// any other pot RemoveFromPotDirect. An abode with a building site
/// and WOOD or ANY: the site's RemoveResource. (pending) the out poison flag is not returned: the callers ask
/// IsPoisoned. Returns what was removed
uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper = {});
/// the storage pit first (it is an abode too): StoragePitStore (with a building site, WOOD or ANY go to the site); an
/// abode (the same building-site redirect) DoResourceAdding (amount added, no cap); a building
/// site's pile the site's AddResource; a pot or a pile
/// pot_resource::PotStructureAddResource. Returns what was taken
uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper = {},
                     bool poisoned = false);
/// whether the object is poisoned, as a villager taking a resource reads it: a storage pit when an available pile
/// (food or wood) is poisoned;
/// an abode never; a pot its own flag
[[nodiscard]] bool IsPoisoned(entt::entity object);
/// a pile resource: true for the wood, food and generic piles, the magic ones and the puzzle grain, false for every
/// other class (pot, storage pit, abode, field ...). By the class: a pot whose GPotInfo potType
/// is PileFood or PileWood (MagicFood and PuzzleGrain are PileFood, MagicWood PileWood; as object::IsPileFood). Not by
/// PileSink: it is assigned at a pile's first SetSize (PotArchetype.cpp) and fields carry it too
[[nodiscard]] bool IsPileResource(entt::entity object);
/// the player a pile belongs to: a pile of a storage pit the pit's, which is its town's owner (the neutral player
/// without a town); any other pile its own (Pot::owner: a magic pile's maker, else the neutral player, as the piles of
/// workshops and building sites). The neutral player for what is not a pot
[[nodiscard]] PlayerNames PlayerOfPile(entt::entity pile);

/// the type is not tested: amount >=
/// what it has -> all of it, and the emptied pot loses its reaction, its poison
/// and its fire; SetSize. A pile without a structure that empties is deleted;
/// a storage pit's or a building site's empty pile stays. Returns what was removed
uint32_t RemoveFromPotDirect(entt::entity pot, uint32_t amount);

/// the adding side of an abode or a storage pit, around `justAdd` (the change itself:
/// the amount for an abode, the piles for a pit). Without an interface or a town
/// only justAdd. With one: the town's desire before and after, the difference x
/// the modifier for the player's last removal of that type, the alignment update
/// of the interface's player, the town's belief with delta x (not the owner ?
/// multiplierForNonOwnerAddingResource : 1) x multiplierForAddingResourceToTown, and the creature copying the player
/// (StoragePitStore::DoCreatureMimicAfterAddingResource; the mimicry itself pending). Returns justAdd's value
uint32_t DoResourceAdding(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                          const std::function<uint32_t()>& justAdd);
/// around `justRemove`: the town's desire
/// before; with an interface and a town the player's last removal is noted, the
/// desire after, and the alignment update (-n, before - after) of the TOWN OWNER (-n the
/// amount asked for). Returns justRemove's value
uint32_t DoResourceRemoving(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                            const std::function<uint32_t()>& justRemove);
} // namespace openblack::ecs::object_resources
