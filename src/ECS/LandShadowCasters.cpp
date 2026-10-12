/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandShadowCasters.h"

#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

using namespace components;

namespace
{

/// Whether a building's building has ended
bool IsBuilt(const Registry& registry, entt::entity entity)
{
	const auto* progress = registry.TryGet<const BuildProgress>(entity);
	return progress == nullptr || progress->built >= 1.0f;
}

} // namespace

LandShadow LandShadowOf(const Registry& registry, entt::entity entity)
{
	if (registry.AnyOf<Field, AnimatedStatic, Flowers, DeadTree, Pot, MagicPile, VillageTotem, MagicShield, ShieldDome,
	                   SpellSeed, OneOffSpellSeed, Vortex>(entity))
	{
		return LandShadow::None;
	}
	if (registry.AnyOf<Tree, BigForest>(entity))
	{
		return LandShadow::Leaves;
	}
	// The temple casts every one of its triangles, once it is built
	if (registry.AllOf<Temple>(entity))
	{
		return registry.AllOf<BuildProgress>(entity) ? LandShadow::None : LandShadow::ShapeBothSides;
	}
	if (const auto* abode = registry.TryGet<const Abode>(entity))
	{
		if (abode->type == AbodeNumber::Field)
		{
			return LandShadow::None;
		}
		return abode->type == AbodeNumber::FootballPitch || IsBuilt(registry, entity) ? LandShadow::Shape : LandShadow::None;
	}
	if (registry.AllOf<WorshipAltar>(entity))
	{
		return IsBuilt(registry, entity) ? LandShadow::Shape : LandShadow::None;
	}
	if (registry.AnyOf<WorshipSite, SpellDispenser, Feature, MobileStatic, TeleportStone, MobileObject, Ball, RewardOnLand>(
	        entity))
	{
		return LandShadow::Shape;
	}
	return LandShadow::None;
}

} // namespace openblack::ecs
