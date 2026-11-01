/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandShadowCasters.h"

#include <array>
#include <type_traits>

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
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NoLandShadow.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
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

/// The count of the changes to the land's shadow casters, kept with the registry
struct LandShadowCasterChanges
{
	uint64_t version {0};
};

/// Counts a change of a component of a thing that can cast: one with a mesh and a transform. When the registry tells
/// this function, a new component is already there, and a component that goes is still there.
void NoteCasterChange(entt::registry& registry, entt::entity entity)
{
	if (registry.all_of<Mesh, Transform>(entity))
	{
		++registry.ctx().get<LandShadowCasterChanges>().version;
	}
}

/// Every component that the land's shadows depend on: those that LandShadowOf reads, the kinds of things that the
/// land's shadow bake looks for, and the mesh, the transform and the map cells of a thing
template <typename Func>
void ForEachCasterComponent(Func func)
{
	[&func]<typename... Components>(std::type_identity<Components>... /*unused*/) {
		(func.template operator()<Components>(), ...);
	}(std::type_identity<Field> {}, std::type_identity<AnimatedStatic> {}, std::type_identity<Flowers> {},
	  std::type_identity<DeadTree> {}, std::type_identity<Pot> {}, std::type_identity<MagicPile> {},
	  std::type_identity<VillageTotem> {}, std::type_identity<MagicShield> {}, std::type_identity<ShieldDome> {},
	  std::type_identity<SpellSeed> {}, std::type_identity<OneOffSpellSeed> {}, std::type_identity<Vortex> {},
	  std::type_identity<Tree> {}, std::type_identity<BigForest> {}, std::type_identity<Temple> {},
	  std::type_identity<BuildProgress> {}, std::type_identity<Abode> {}, std::type_identity<WorshipAltar> {},
	  std::type_identity<WorshipSite> {}, std::type_identity<SpellDispenser> {}, std::type_identity<Feature> {},
	  std::type_identity<MobileStatic> {}, std::type_identity<TeleportStone> {}, std::type_identity<MobileObject> {},
	  std::type_identity<Ball> {}, std::type_identity<RewardOnLand> {}, std::type_identity<Mesh> {},
	  std::type_identity<Transform> {}, std::type_identity<MapCellResident> {}, std::type_identity<NoLandShadow> {});
}

} // namespace

LandShadow LandShadowOf(const Registry& registry, entt::entity entity)
{
	return LandShadowLookup(registry).Of(entity);
}

namespace
{

/// The components that the shadow of a thing depends on, in the order of the stores of a lookup
template <typename... Components>
struct ShadowComponents
{
	static_assert(sizeof...(Components) == LandShadowLookup::k_StoreCount);

	/// The place of a component in the list
	template <typename Component>
	static constexpr size_t k_Place = [] {
		size_t place = 0;
		bool found = false;
		((found = found || std::is_same_v<Component, Components>, place += found ? 0 : 1), ...);
		return place;
	}();

	static void Find(const entt::registry& registry,
	                 std::array<const entt::sparse_set*, LandShadowLookup::k_StoreCount>& stores)
	{
		size_t place = 0;
		((stores.at(place++) = registry.storage<Components>()), ...);
	}
};

using ShadowStores =
    ShadowComponents<Field, AnimatedStatic, Flowers, DeadTree, Pot, MagicPile, VillageTotem, MagicShield, ShieldDome, SpellSeed,
                     OneOffSpellSeed, Vortex, Tree, BigForest, Temple, BuildProgress, Abode, WorshipAltar, WorshipSite,
                     SpellDispenser, Feature, MobileStatic, TeleportStone, MobileObject, Ball, RewardOnLand, NoLandShadow>;

} // namespace

LandShadowLookup::LandShadowLookup(const Registry& registry)
{
	ShadowStores::Find(registry.Underlying(), _stores);
}

template <typename... Components>
bool LandShadowLookup::AnyOf(entt::entity entity) const
{
	return ([this, entity] {
		const auto* store = _stores.at(ShadowStores::k_Place<Components>);
		return store != nullptr && store->contains(entity);
	}() || ...);
}

template <typename Component>
const Component* LandShadowLookup::TryGet(entt::entity entity) const
{
	if (!AnyOf<Component>(entity))
	{
		return nullptr;
	}
	return &static_cast<const entt::storage_for_t<Component>*>(_stores.at(ShadowStores::k_Place<Component>))->get(entity);
}

bool LandShadowLookup::IsBuilt(entt::entity entity) const
{
	const auto* progress = TryGet<BuildProgress>(entity);
	return progress == nullptr || progress->built >= 1.0f;
}

LandShadow LandShadowLookup::Of(entt::entity entity) const
{
	if (AnyOf<NoLandShadow>(entity))
	{
		return LandShadow::None;
	}
	if (AnyOf<Field, AnimatedStatic, Flowers, DeadTree, Pot, MagicPile, VillageTotem, MagicShield, ShieldDome, SpellSeed,
	          OneOffSpellSeed, Vortex>(entity))
	{
		return LandShadow::None;
	}
	if (AnyOf<Tree, BigForest>(entity))
	{
		return LandShadow::Leaves;
	}
	// The temple casts every one of its triangles, once it is built
	if (AnyOf<Temple>(entity))
	{
		return AnyOf<BuildProgress>(entity) ? LandShadow::None : LandShadow::ShapeBothSides;
	}
	if (const auto* abode = TryGet<Abode>(entity))
	{
		if (abode->type == AbodeNumber::Field)
		{
			return LandShadow::None;
		}
		return abode->type == AbodeNumber::FootballPitch || IsBuilt(entity) ? LandShadow::Shape : LandShadow::None;
	}
	if (AnyOf<WorshipAltar>(entity))
	{
		return IsBuilt(entity) ? LandShadow::Shape : LandShadow::None;
	}
	if (AnyOf<WorshipSite, SpellDispenser, Feature, MobileStatic, TeleportStone, MobileObject, Ball, RewardOnLand>(entity))
	{
		return LandShadow::Shape;
	}
	return LandShadow::None;
}

bool LandShadowChangesInPlace(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<Abode, BuildProgress>(entity);
}

uint64_t LandShadowCastersVersion(Registry& registry)
{
	auto& context = registry.Underlying().ctx();
	if (const auto* changes = context.find<LandShadowCasterChanges>(); changes != nullptr)
	{
		return changes->version;
	}
	// The first time: the count and its signals stay with the registry, also when it is emptied
	context.emplace<LandShadowCasterChanges>();
	ForEachCasterComponent([&registry]<typename Component>() {
		registry.OnConstruct<Component>().template connect<&NoteCasterChange>();
		registry.OnDestroy<Component>().template connect<&NoteCasterChange>();
	});
	return 0;
}

} // namespace openblack::ecs
