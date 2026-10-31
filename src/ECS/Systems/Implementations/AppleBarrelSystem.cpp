/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AppleBarrelSystem.h"

#include <algorithm>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Animals/AnimalAnimation.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/AppleBarrelRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
const entt::id_type k_BarrelModel = resources::HashIdentifier(MeshId::Z_AppleBarrel);
const entt::id_type k_WormModel = resources::HashIdentifier(MeshId::AnimalWorm);
const entt::id_type k_WormClip = resources::HashIdentifier(AnimId::AWorm);

/// The worm's model posed at its place in its clip; its resting pose when the clip doesn't fit it
void PoseWorm(const L3DAnim& clip, const graphics::L3DMesh& model, uint32_t place, std::vector<glm::mat4>& bones)
{
	const auto& frames = clip.GetFrames();
	const auto& parents = model.GetBoneParents();
	const animals::ClipTiming timing {
	    .playTime = clip.GetPlayTime(), .frameCount = frames.size(), .looping = clip.IsLooping(), .playedByTime = true};
	const auto span = animals::SpanAt(timing, place);
	if (!model.IsBoned() || span.from >= frames.size() || span.to >= frames.size() ||
	    frames[span.from].bones.size() < parents.size() || frames[span.to].bones.size() < parents.size())
	{
		bones.clear();
		return;
	}
	bones.resize(parents.size());
	animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, bones);
}
} // namespace

std::optional<entt::entity> AppleBarrelSystem::CarryingStore() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	// The land draws the barrel for the player at this computer
	const auto player =
	    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;

	// The pits are measured from the player's temple
	std::optional<glm::vec2> temple;
	registry.Each<const Temple, const Transform>([&](entt::entity, const Temple& found, const Transform& transform) {
		if (!temple.has_value() && found.owner == player)
		{
			temple = glm::xz(transform.position);
		}
	});

	// The storage pit of each of the player's towns (its first), the towns in the order they were gained
	struct TownStore
	{
		uint32_t gained;
		uint32_t townId;
		std::optional<entt::entity> store;
		glm::vec2 place {0.0f};
	};
	std::vector<TownStore> towns;
	registry.Each<const Town>([&](entt::entity, const Town& town) {
		if (town.owner == player)
		{
			towns.push_back({.gained = town.gained, .townId = town.id, .store = std::nullopt});
		}
	});
	if (towns.empty())
	{
		return std::nullopt;
	}
	registry.Each<const Abode, const StoragePit, const Transform>(
	    [&](entt::entity entity, const Abode& abode, const StoragePit&, const Transform& transform) {
		    const auto town = std::ranges::find(towns, abode.townId, &TownStore::townId);
		    if (town != towns.end() && !town->store.has_value())
		    {
			    town->store = entity;
			    town->place = glm::xz(transform.position);
		    }
	    });
	std::ranges::sort(towns, {}, &TownStore::gained);
	std::vector<entt::entity> stores;
	std::vector<glm::vec2> places;
	for (const auto& town : towns)
	{
		if (town.store.has_value())
		{
			stores.push_back(*town.store);
			places.push_back(town.place);
		}
	}
	const auto nearest = apple_barrel::NearestStore(places, apple_barrel::MeasuredFrom(temple));
	return nearest.has_value() ? std::optional(stores[*nearest]) : std::nullopt;
}

void AppleBarrelSystem::Remove()
{
	if (!Locator::drawnRegistry::has_value())
	{
		_barrel = entt::null;
		return;
	}
	auto& drawn = Locator::drawnRegistry::value();
	if (_barrel != entt::null && drawn.Valid(_barrel) && drawn.AllOf<AppleBarrel>(_barrel))
	{
		if (const auto worm = drawn.Get<const AppleBarrel>(_barrel).worm; worm != entt::null && drawn.Valid(worm))
		{
			drawn.Destroy(worm);
		}
		drawn.Destroy(_barrel);
		drawn.SetDirty();
	}
	_barrel = entt::null;
}

void AppleBarrelSystem::Update(std::chrono::milliseconds frameGameTime)
{
	if (!Locator::resources::has_value() || !Locator::drawnRegistry::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::terrainSystem::value().IsLoaded())
	{
		return;
	}
	const auto elapsed = static_cast<uint32_t>(frameGameTime.count());

	// The barrel and its worm are things of the drawn registry: only the frames make, place and remove them
	auto& drawn = Locator::drawnRegistry::value();
	if (_barrel != entt::null && (!drawn.Valid(_barrel) || !drawn.AllOf<AppleBarrel>(_barrel)))
	{
		_barrel = entt::null;
	}
	// The barrel goes while no pit carries it
	const auto store = CarryingStore();
	if (!store.has_value())
	{
		Remove();
		return;
	}

	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& animations = Locator::resources::value().GetAnimations();
	if (!meshes.Contains(k_BarrelModel) || !meshes.Contains(k_WormModel))
	{
		return;
	}

	// The barrel stands at the pit's edge on the land, unturned and at its own size
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& storeTransform = registry.Get<const Transform>(*store);
	const auto ground = apple_barrel::BarrelSpot(glm::xz(storeTransform.position), storeTransform.rotation,
	                                             object_measures::TwoDRadius(registry, *store));
	const glm::vec3 barrelSpot {ground.x, Locator::terrainSystem::value().GetHeightAt(ground), ground.y};
	const auto barrelModel = meshes.Handle(k_BarrelModel);
	const auto wormSpot = apple_barrel::WormSpot(barrelSpot, barrelModel->GetChimneyPos().value_or(glm::vec3(0.0f)));

	// The barrel and its worm are made when a pit first carries them
	if (_barrel == entt::null)
	{
		_barrel = drawn.Create();
		drawn.Assign<Transform>(_barrel, barrelSpot, glm::mat3(1.0f), glm::vec3(1.0f));
		drawn.Assign<Mesh>(_barrel, k_BarrelModel, static_cast<int8_t>(0), static_cast<int8_t>(0));
		const auto worm = drawn.Create();
		drawn.Assign<Transform>(worm, wormSpot, glm::mat3(1.0f), glm::vec3(1.0f));
		drawn.Assign<Mesh>(worm, k_WormModel, static_cast<int8_t>(0), static_cast<int8_t>(0));
		drawn.Assign<AppleBarrelWorm>(worm, _barrel);
		// The worm is very small. Thus it stays in view to a larger distance than its size alone gives.
		drawn.Assign<DrawImportance>(worm, apple_barrel::k_WormImportance);
		drawn.Assign<AnimatedStaticPose>(worm);
		drawn.Assign<AppleBarrel>(_barrel, *store, worm);
	}
	auto& barrel = drawn.Get<AppleBarrel>(_barrel);
	barrel.store = *store;
	drawn.Get<Transform>(_barrel).position = barrelSpot;
	drawn.Get<Transform>(barrel.worm).position = wormSpot;

	// The worm wriggles on by the game's clock
	auto& worm = drawn.Get<AppleBarrelWorm>(barrel.worm);
	auto& pose = drawn.Get<AnimatedStaticPose>(barrel.worm);
	if (animations.Contains(k_WormClip))
	{
		const auto clip = animations.Handle(k_WormClip);
		_wormPlace = apple_barrel::StepWorm(_wormPlace, elapsed, clip->GetPlayTime(), clip->IsLooping());
		worm.place = _wormPlace;
		pose.place = worm.place;
		PoseWorm(*clip, *meshes.Handle(k_WormModel), worm.place, pose.bones);
	}
	drawn.SetDirty();
}

void AppleBarrelSystem::Reset()
{
	// The drawn registry goes with the land
	_barrel = entt::null;
}

void AppleBarrelSystem::FindAfterLoad()
{
	// The drawn registry goes with the land. A world saved when the barrel was a thing of the land can still have the
	// barrel and its worm: they go, and the frames make them again.
	_barrel = entt::null;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> old;
	registry.Each<const AppleBarrel>([&old](entt::entity entity, const AppleBarrel&) { old.push_back(entity); });
	registry.Each<const AppleBarrelWorm>([&old](entt::entity entity, const AppleBarrelWorm&) { old.push_back(entity); });
	for (const auto entity : old)
	{
		registry.Destroy(entity);
	}
	if (!old.empty())
	{
		registry.SetDirty();
	}
}
