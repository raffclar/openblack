/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PotArchetype.h"

#include <algorithm>

#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// a food pile's texture V offset, 0.25 (1 - clamp(offset / H + 1, 0, 1)), that keeps the grain still in the world
/// while the pile sinks (a selection, not an animation)
float PileFoodV(const PileSink& sink)
{
	return 0.25f * (1.0f - std::clamp(sink.offset.value / std::max(sink.height, 1e-3f) + 1.0f, 0.0f, 1.0f));
}
} // namespace

entt::entity PotArchetype::Create(const glm::vec3& position, float yAngleRadians, PotInfo type, int32_t amount, bool allowEmpty)
{
	if (static_cast<int32_t>(type) < 0 || static_cast<int32_t>(type) >= static_cast<int32_t>(PotInfo::_COUNT))
	{
		return entt::null;
	}
	if (amount < 0 || (amount == 0 && !allowEmpty))
	{
		return entt::null;
	}

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(type));

	// magic food and magic wood start at scale 0.3 / 0.7; every other pot is created at 1.
	const float scale = type == PotInfo::MagicFood ? 0.3f : (type == PotInfo::MagicWood ? 0.7f : 1.0f);
	// the world matrix with no x or z turn: RotationYXZ(a, 0, 0) is AngleY(a) bit for bit
	registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	registry.Assign<Pot>(entity, static_cast<uint16_t>(amount), static_cast<uint16_t>(info.maxAmountInPot), type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	if (info.potType == PotType::PileFood)
	{
		registry.Assign<MorphWithTerrain>(entity);
	}
	// a new pile starts fully buried (offset = -height)
	// and SetSize raises it out of the ground over 1 s.
	SetSize(entity, info.potType != PotType::Pot);
	// a pot or pile (type 21, counted as fixed) goes at the tail of its cell's fixed list at once, so adding a resource
	// at a position sees it
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}

void PotArchetype::SetSize(entt::entity entity, bool animate)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pot = registry.TryGet<const Pot>(entity);
	if (pot == nullptr || pot->type == PotInfo::_COUNT || pot->type == PotInfo::HandWood || pot->type == PotInfo::HandFood)
	{
		return; // hand pots keep scale 1 and follow the hand
	}
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	auto& transform = registry.Get<Transform>(entity);
	if (info.potType == PotType::Pot)
	{
		// a pot's scale follows its amount (the only reader of scaleEvery).
		const float every = static_cast<float>(std::max(1u, info.scaleEvery));
		transform.scale = glm::vec3(std::min(5.0f, static_cast<float>(pot->amount) / every + 0.25f));
		registry.SetDirty();
		return;
	}

	// a pile: target offset = (the proportion raised - 1) * its height. The proportion raised is the wood or food
	// pile's own (an empty pile is 0: fully buried and not drawn); no mesh, no height
	const float proportion = ecs::object::GetProportionRaised(entity);
	const float height = ecs::object::GetHeight(entity);
	auto* sink = registry.TryGet<PileSink>(entity);
	if (sink == nullptr)
	{
		// the pile stands at the land's height + offset.
		const float ground =
		    Locator::terrainSystem::has_value()
		        ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
		        : transform.position.y;
		sink = &registry.Assign<PileSink>(entity, ground);
		sink->offset.SetPosition(-height);
		// only the storage pit food pile (info 2) and magic food (info 10) scroll their grain.
		if (pot->type == PotInfo::StoragePitFoodPile || pot->type == PotInfo::MagicFood)
		{
			registry.Assign<UvScroll>(entity);
		}
	}
	sink->height = height;
	// the original's matrix is the Zoomer's for T = 1 s: end position = target, end velocity and acceleration = 0.
	const float target = (proportion - 1.0f) * height;
	if (animate)
	{
		sink->offset.SetDestinationWithSpeedAndTime(target, 0.0f, 1.0f);
	}
	else
	{
		sink->offset.SetPosition(target);
	}
	// Food piles keep MorphWithTerrain: the height-map shader raises each vertex by the land under it minus the land
	// under the origin, so the sink in the origin's y is kept.
	transform.position.y = sink->baseY + sink->offset.value;
	if (auto* scroll = registry.TryGet<UvScroll>(entity); scroll != nullptr)
	{
		scroll->v = PileFoodV(*sink);
	}
	registry.SetDirty();
}

void PotArchetype::UpdateSizes(float seconds)
{
	// the piles' draw: t += frame time; past the 1 s duration the offset is the target.
	auto& registry = Locator::entitiesRegistry::value();
	bool dirty = false;
	registry.Each<PileSink, Transform>([&](entt::entity entity, PileSink& sink, Transform& transform) {
		if (!sink.offset.IsMoving())
		{
			return;
		}
		sink.offset.Update(seconds);
		transform.position.y = sink.baseY + sink.offset.value;
		// 0.25 * (1 - clamp(offset / H + 1, 0, 1)), the mesh's texture V offset.
		if (auto* scroll = registry.TryGet<UvScroll>(entity); scroll != nullptr)
		{
			scroll->v = PileFoodV(sink);
		}
		dirty = true;
	});
	if (dirty)
	{
		registry.SetDirty();
	}
}
