/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedKinds.h"

#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"

using namespace openblack::ecs::components;

bool openblack::ecs::fixed_kinds::IsFixedKind(const Registry& registry, entt::entity thing)
{
	// The buildings and the other things that cover their outline
	if (registry.AnyOf<Abode, Field, StoragePit, SpellDispenser, Temple, TempleExterior, TempleHeart, WorshipSite, WorshipAltar,
	                   VillageTotem, WorshipIcon, TownCentreIcon>(thing))
	{
		return true;
	}
	// The features, the rocks and the other things of the land
	return registry.AnyOf<Feature, AnimatedStatic, Flowers, BigForest, FishFarm, MobileStatic, DeadTree, BuildingPiece,
	                      TeleportStone, Vortex>(thing);
}

bool openblack::ecs::fixed_kinds::IsAlwaysWhole(const Registry& registry, entt::entity thing)
{
	return registry.AnyOf<MobileStatic, DeadTree, BuildingPiece, BigForest, FishFarm, TeleportStone, Vortex>(thing);
}

bool openblack::ecs::fixed_kinds::CellIsFixed(std::span<const entt::entity> fixedInCell,
                                              const std::function<bool(entt::entity)>& isFixedKind)
{
	return !fixedInCell.empty() && isFixedKind(fixedInCell.front());
}
