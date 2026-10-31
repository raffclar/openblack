/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ObjectMeasures.h"

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "Animals/FishFarmRules.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/ThingModel.h"
#include "ECS/WorshipSites.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
namespace cast_moves = openblack::creature_cast_moves;

/// How far a species' bones reach from its middle, in the first frame of its first animation on its base model
std::optional<float> MeasureBoneReach(CreatureType species)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto meshId = creature::GetIdFromType(species, creature::CreatureBody::Appearance::Base);
	const auto rigId = creature::GetRigId(species);
	if (!meshes.Contains(meshId) || !rigs.Contains(rigId))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(meshId);
	const auto* animation = rigs.Handle(rigId)->GetAnimation(creature::CreatureRig::Mesh::Base, 0);
	if (animation == nullptr)
	{
		return std::nullopt;
	}
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(mesh->GetBoneParents(), mesh->GetBoneMatrices());
	return cast_moves::BoneReach(*animation, skeleton);
}

/// The bone reach of a species. The first request measures it and keeps it in the context of the registry. If the
/// resources have no model or animation of the species yet, the reach is 0, and the next request measures again.
float BoneReachOf(const ecs::Registry& registry, CreatureType species)
{
	const auto row = static_cast<size_t>(species);
	auto& kept = registry.Context().creatureBoneReach;
	if (row >= kept.size())
	{
		return MeasureBoneReach(species).value_or(0.0f);
	}
	if (!kept.at(row).has_value())
	{
		kept.at(row) = MeasureBoneReach(species);
	}
	return kept.at(row).value_or(0.0f);
}

/// A model's half width, height and length, times the thing's scale
std::optional<glm::vec3> HalfExtentsOf(const ecs::Registry& registry, entt::entity entity)
{
	// A temple's heart has the model of its temple, as the temple is scaled
	if (const auto* heart = registry.TryGet<const TempleHeart>(entity))
	{
		return registry.Valid(heart->temple) ? HalfExtentsOf(registry, heart->temple) : std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	// A one-shot globe has no mesh of its own to draw, but it measures as the model of its dome. An altar measures as
	// its model before the model shows.
	const auto model = ecs::thing_model::ModelOf(registry, entity);
	const auto id = model.has_value()                         ? model
	                : registry.AllOf<OneOffSpellSeed>(entity) ? std::optional(OneOffSpellSeed::k_MeshId.value())
	                                                          : std::nullopt;
	if (!id.has_value() || transform == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(*id))
	{
		return std::nullopt;
	}
	const auto& box = meshes.Handle(*id)->GetBoundingBox();
	return (box.maxima - box.minima) * 0.5f * transform->scale.x;
}
} // namespace

float object_measures::TwoDRadius(const Registry& registry, entt::entity entity)
{
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		return cast_moves::CreatureRadius(transform != nullptr ? transform->scale.x : 0.0f,
		                                  BoneReachOf(registry, creature->species));
	}
	if (registry.AllOf<Field>(entity))
	{
		return cast_moves::k_FieldRadius;
	}
	if (registry.AllOf<FishFarm>(entity))
	{
		return fish_farm::k_Radius;
	}
	// A teleport stone has the same radius whatever its model and its size
	if (registry.AllOf<TeleportStone>(entity))
	{
		return creature_obstacles::k_TeleportStoneWidth;
	}
	const auto half = HalfExtentsOf(registry, MeasuredPart(registry, entity));
	return half.has_value() ? std::max(half->x, half->z) : 0.0f;
}

entt::entity object_measures::MeasuredPart(const Registry& registry, entt::entity entity)
{
	// A town centre's totem is as wide and as high as the icon on its plinth. The icon has the scale of the totem.
	if (const auto* totem = registry.TryGet<const VillageTotem>(entity); totem != nullptr && registry.Valid(totem->icon))
	{
		return totem->icon;
	}
	return entity;
}

float object_measures::DistanceFromObject(const Registry& registry, entt::entity entity, entt::entity other)
{
	const auto& transform = registry.Get<const Transform>(entity);
	const auto otherPlace = glm::xz(registry.Get<const Transform>(other).position);
	if (registry.AllOf<WorshipSite>(entity))
	{
		const auto centre = worship_site::CentreOnLand(transform.position, transform.rotation, transform.scale.x);
		return worship_site::EdgeGap(centre, otherPlace, TwoDRadius(registry, other));
	}
	return gutils::GetDistanceInMetres(glm::xz(transform.position), otherPlace) -
	       (TwoDRadius(registry, other) + TwoDRadius(registry, entity));
}

float object_measures::NavRadius(const Registry& registry, entt::entity creature)
{
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	return walk != nullptr ? walk->radius : TwoDRadius(registry, creature);
}

float object_measures::Height(const Registry& registry, entt::entity entity)
{
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		return cast_moves::k_HeightOfSizeOne * ShownSize(*creature);
	}
	const auto half = HalfExtentsOf(registry, MeasuredPart(registry, entity));
	return half.has_value() ? 2.0f * half->y : 0.0f;
}

float object_measures::RoutePlanRadius(const Registry& registry, entt::entity entity, entt::entity creature)
{
	return cast_moves::RoutePlanRadius(TwoDRadius(registry, entity), Height(registry, entity), registry.AllOf<Tree>(entity),
	                                   Height(registry, creature), NavRadius(registry, creature));
}

bool object_measures::WalkedUpToItself(const Registry& registry, entt::entity entity)
{
	return registry.AnyOf<Abode, Field, Feature, MobileStatic, Tree>(entity);
}

std::optional<glm::vec3> object_measures::PositionOf(const Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}
