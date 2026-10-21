/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VegetationSystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TreeRustle.h"
#include "Graphics/MeshDetail.h"
#include "Locator.h"
#include "Nature/TreeBend.h"
#include "Nature/TreeSway.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
// The sways speed up and slow down at random every two seconds
constexpr auto k_SpeedChangeTime =
    std::chrono::duration<float, std::milli>(static_cast<float>(openblack::nature::tree_sway::k_SpeedChangeMilliseconds));
// How much further a field's crop leans than a tree
constexpr float k_FieldLean = 1.75f;

// A bent tree leans away from the bend point by up to this many radians
constexpr float k_MaxBend = 0.47123894f;
} // namespace

void VegetationSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	_speedTime += gameTime;
	const bool changeSpeeds = _speedTime > k_SpeedChangeTime;
	auto& rng = Locator::rng::value();
	for (size_t i = 0; i < k_SwayCount; ++i)
	{
		if (changeSpeeds)
		{
			_speeds.at(i) = rng.NextValue(nature::tree_sway::k_MinSpeed, nature::tree_sway::k_MaxSpeed);
		}
		_phases.at(i) = nature::tree_sway::StepPhase(_phases.at(i), _speeds.at(i), static_cast<uint32_t>(gameTime.count()));
		_leans.at(i) = nature::tree_sway::Lean(_phases.at(i));
	}
	if (changeSpeeds)
	{
		_speedTime = decltype(_speedTime)::zero();
	}
}

void VegetationSystem::Mark(const nature::tree_bend::Source& source, std::optional<uint8_t> slot)
{
	const auto written = _bendTable.Write(source, slot);
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto cells = nature::tree_bend::CellsMarkedBy(source.position);
	for (int x = cells.first.x; x <= cells.last.x; ++x)
	{
		for (int z = cells.first.y; z <= cells.last.y; ++z)
		{
			if (x < 0 || z < 0 || x >= MapInterface::k_GridSize.x || z >= MapInterface::k_GridSize.y)
			{
				continue;
			}
			for (const auto entity : map.GetFixedInGridCell(MapInterface::CellId(x, z)))
			{
				if (auto* tree = registry.TryGet<Tree>(entity))
				{
					tree->bendSlot = written;
				}
			}
		}
	}
}

void VegetationSystem::UpdateBendPoints(bool handShown)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	// A model bends the trees as far as its scaled size reaches, from where it is drawn
	const auto modelSource = [&registry, &meshes](entt::entity entity,
	                                              const glm::vec3& position) -> std::optional<nature::tree_bend::Source> {
		const auto* transform = registry.TryGet<const Transform>(entity);
		const auto* mesh = registry.TryGet<const Mesh>(entity);
		if (transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id))
		{
			return std::nullopt;
		}
		return nature::tree_bend::HandSource(nature::tree_bend::HeldModel {
		    .position = position,
		    .scale = transform->scale.y,
		    .halfDiagonal = openblack::graphics::mesh_detail::HalfDiagonal(meshes.Handle(mesh->id)->GetBoundingBox().Size()),
		});
	};

	// 1. The thing that the hand holds. The empty hand, or the hand put away, writes nothing: its slot keeps the last
	// point
	if (handShown && Locator::handGrabSystem::has_value())
	{
		if (const auto held = Locator::handGrabSystem::value().GetHeld(); held.has_value() && registry.Valid(*held))
		{
			if (const auto* transform = registry.TryGet<const Transform>(*held))
			{
				if (const auto source = modelSource(*held, transform->position))
				{
					Mark(*source, nature::tree_bend::k_HandSlot);
				}
			}
		}
	}
	// 2. Each body that moves in the physics, in the order of the list of bodies, from where the frame draws it
	if (Locator::dynamicsSystem::has_value())
	{
		for (const auto body : Locator::dynamicsSystem::value().DrawnBodies())
		{
			if (!registry.Valid(body))
			{
				continue;
			}
			const auto* pose = registry.TryGet<const PhysicsDrawPose>(body);
			const auto* transform = registry.TryGet<const Transform>(body);
			if (transform == nullptr)
			{
				continue;
			}
			if (const auto source = modelSource(body, pose != nullptr ? pose->origin : transform->position))
			{
				Mark(*source, std::nullopt);
			}
		}
	}
	// 3. The primary creature of the local player, as far as its body reaches across the ground
	if (Locator::playerSystem::has_value())
	{
		auto& players = Locator::playerSystem::value();
		if (const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer());
		    creature.has_value() && registry.Valid(*creature))
		{
			if (const auto* transform = registry.TryGet<const Transform>(*creature))
			{
				Mark({.position = transform->position, .radius = object_measures::TwoDRadius(registry, *creature)},
				     nature::tree_bend::k_CreatureSlot);
			}
		}
	}

	// Each marked tree reads the point in its slot. It loses its mark when the point is above it or out of its reach
	_bends.clear();
	std::unordered_map<entt::id_type, float> meshHeights;
	registry.Each<Tree, const Transform, const Mesh>([&](entt::entity entity, Tree& tree, const Transform& transform,
	                                                     const Mesh& mesh) {
		if (tree.bendSlot == nature::tree_bend::k_NoSlot)
		{
			return;
		}
		const auto [found, inserted] = meshHeights.try_emplace(mesh.id, 0.0f);
		if (inserted && meshes.Contains(mesh.id))
		{
			found->second = meshes.Handle(mesh.id)->GetBoundingBox().Size().y;
		}
		const auto bend =
		    nature::tree_bend::ReadBend(_bendTable.At(tree.bendSlot), transform.position, transform.scale.y * found->second);
		if (!bend.has_value())
		{
			tree.bendSlot = nature::tree_bend::k_NoSlot;
			return;
		}
		_bends.emplace(entity, *bend);
	});
}

void VegetationSystem::Rustle(std::chrono::duration<float, std::milli> gameTime)
{
	const auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	auto& audio = Locator::audio::value();
	auto& rng = Locator::rng::value();
	const auto camera = Locator::camera::value().GetOrigin();
	const auto idleChance = TreeRustle::IdleChance(gameTime);
	const auto bank = std::string(TreeRustle::k_Bank);
	// The temple hides the land, so the game draws no tree there and no tree rustles. Elsewhere a rustle plays only as
	// its sample allows, as the scripts' sound effects do.
	const auto conditions = audio::CurrentSoundEffectConditions();
	if (conditions.insideTemple)
	{
		return;
	}

	// The trees share a few meshes, whose heights are looked up once each
	std::unordered_map<entt::id_type, float> meshHeights;
	const auto meshHeight = [&meshes, &meshHeights](entt::id_type id) {
		const auto [found, inserted] = meshHeights.try_emplace(id, 0.0f);
		if (inserted && meshes.Contains(id))
		{
			found->second = meshes.Handle(id)->GetBoundingBox().Size().y;
		}
		return found->second;
	};

	registry.Each<const Tree, const Transform, const Mesh, const Swayable>([&](entt::entity entity, const Tree& /*unused*/,
	                                                                           const Transform& transform, const Mesh& mesh,
	                                                                           const Swayable& /*unused*/) {
		// Only a bent tree or a tree near the camera can rustle, and only for those does its height matter: the others
		// are passed over before it is looked up
		const auto bend = _bends.find(entity);
		if (bend == _bends.end() && !TreeRustle::IsNearCamera(transform.position, camera))
		{
			return;
		}
		if (bend != _bends.end())
		{
			// The bend's sound measures its distance from the camera with the squares in another order than the idle's
			audio.PlayAnimEffect(bank, TreeRustle::BendKeys(bend->second.amount).ToArray(), entity, transform.position,
			                     conditions, openblack::audio::AnimEffectMeasure::AcrossFirst);
		}
		const auto height = transform.scale.y * meshHeight(mesh.id);
		if (idleChance > 1 && TreeRustle::IsAmongTree(transform.position, height, camera) &&
		    rng.NextValue<uint32_t>(0, idleChance - 1) == 1)
		{
			// The idle rustle stays with its tree. The rustle of a bend stays where it started.
			const auto idle =
			    audio.PlayAnimEffect(bank, TreeRustle::IdleKeys().ToArray(), entity, transform.position, conditions);
			audio.SetEmitterFollows(idle.emitter, audio::Follows::Owner);
		}
	});
}

glm::mat4 VegetationSystem::GetTreeMatrix(entt::entity tree, const glm::mat4& model, const glm::vec3& position, float scale,
                                          uint8_t swaySlot) const
{
	if (const auto bend = _bends.find(tree); bend != _bends.end())
	{
		// Tip the tree's top away from the bend point, about its base. A point exactly at the trunk tips it nowhere
		const auto axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), bend->second.direction);
		if (glm::dot(axis, axis) <= 0.0f)
		{
			return model;
		}
		const auto tilt = glm::rotate(glm::mat4(1.0f), k_MaxBend * bend->second.amount, axis);
		return glm::translate(glm::mat4(1.0f), position) * tilt * glm::translate(glm::mat4(1.0f), -position) * model;
	}

	// Lean the tree with its sway: the higher a point of the tree, the further it moves
	auto swaying = model;
	swaying[1].z = scale * _leans.at(swaySlot % k_SwayCount);
	return swaying;
}

// The renderer asks for this matrix only for a ripe crop: an unripe crop does not sway
glm::mat4 VegetationSystem::GetFieldMatrix(const glm::mat4& model, float scale, uint8_t swaySlot) const
{
	auto swaying = model;
	swaying[1].x = 0.0f;
	swaying[1].z = scale * _leans.at(swaySlot % k_SwayCount) * k_FieldLean;
	return swaying;
}
