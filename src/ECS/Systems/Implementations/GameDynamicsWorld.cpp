/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameDynamicsWorld.h"

#include <string>
#include <string_view>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/WaterRings.h"
#include "Audio/GameSoundEffects.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/SnowDust.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

namespace
{
/// The bank the collision sounds are picked from
constexpr std::string_view k_CollisionBank = "editor.sad";
} // namespace

Registry& GameDynamicsWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

const Registry& GameDynamicsWorld::Entities() const
{
	return Locator::entitiesRegistry::value();
}

const LandIslandInterface* GameDynamicsWorld::Land() const
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}

const InfoConstants* GameDynamicsWorld::Info() const
{
	return Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
}

const GObjectInfo* GameDynamicsWorld::InfoOf(entt::entity object) const
{
	return world_objects::InfoOf(object);
}

float GameDynamicsWorld::LifeOf(entt::entity object) const
{
	return world_objects::LifeOf(object);
}

float GameDynamicsWorld::HeightOf(entt::entity object) const
{
	return world_objects::SizeOf(object).height;
}

void GameDynamicsWorld::Remove(entt::entity object)
{
	world_objects::Remove(object);
}

std::optional<dynamics::ModelSize> GameDynamicsWorld::SizeOfModel(entt::id_type mesh) const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == 0 || !meshes.Contains(mesh))
	{
		return std::nullopt;
	}
	const auto& loaded = *meshes.Handle(mesh);
	return dynamics::ModelSize {.size = loaded.GetBoundingBox().Size(), .boned = loaded.IsBoned()};
}

std::optional<dynamics::Model> GameDynamicsWorld::ModelOf(entt::id_type mesh) const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == 0 || !meshes.Contains(mesh))
	{
		return std::nullopt;
	}
	const auto& loaded = *meshes.Handle(mesh);
	dynamics::Model model;
	model.size = loaded.GetBoundingBox().Size();
	model.boned = loaded.IsBoned();
	const auto& subMeshes = loaded.GetSubMeshes();
	model.indices.reserve(subMeshes.size());
	for (const auto& subMesh : subMeshes)
	{
		// A boned model's vertices count as the file holds them, unposed, as the game takes them
		const auto& geometry = subMesh->GetBodyGeometry();
		model.indices.emplace_back(geometry.indices.begin(), geometry.indices.end());
		model.parts.push_back({.positions = geometry.positions,
		                       .indices = model.indices.back(),
		                       .isPhysics = subMesh->IsPhysics(),
		                       .nearestDetail = (subMesh->GetFlags().lodMask & 1u) != 0});
		model.bones.emplace_back(geometry.bones);
	}
	return model;
}

physics::Material GameDynamicsWorld::MaterialOf(physics::MaterialRow row) const
{
	if (!Locator::resources::has_value())
	{
		return {};
	}
	const auto& materials = Locator::resources::value().GetPhysicsMaterials();
	return materials.Contains(physics::k_MaterialsId.value()) ? (*materials.Handle(physics::k_MaterialsId.value()))[row]
	                                                          : physics::Material {};
}

bool GameDynamicsWorld::HasMap() const
{
	return Locator::entitiesMap::has_value();
}

std::vector<entt::entity> GameDynamicsWorld::FixedThenMobileInCell(glm::ivec2 cell) const
{
	const auto& map = Locator::entitiesMap::value();
	const MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
	std::vector<entt::entity> found(map.GetFixedInGridCell(id).begin(), map.GetFixedInGridCell(id).end());
	const auto& mobile = map.GetMobileInGridCell(id);
	found.insert(found.end(), mobile.begin(), mobile.end());
	return found;
}

std::vector<entt::entity> GameDynamicsWorld::AllInCell(glm::ivec2 cell) const
{
	return Locator::entitiesMap::value().GetAllInCell(cell);
}

void GameDynamicsWorld::Refile(entt::entity object)
{
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(object);
	}
}

std::optional<glm::vec3> GameDynamicsWorld::CameraOrigin() const
{
	return Locator::camera::has_value() ? std::optional(Locator::camera::value().GetOrigin()) : std::nullopt;
}

GameRandomInterface* GameDynamicsWorld::Random()
{
	return Locator::gameRandom::has_value() ? &Locator::gameRandom::value() : nullptr;
}

void GameDynamicsWorld::PlaySound(audio::SoundId sound)
{
	if (Locator::audio::has_value())
	{
		audio::PlayGameSoundEffect(static_cast<entt::id_type>(sound), std::nullopt);
	}
}

std::optional<audio::AnimEffectPlay> GameDynamicsWorld::PlayCollisionSound(std::span<const int32_t> keys, entt::entity owner,
                                                                           glm::vec3 position)
{
	if (!Locator::audio::has_value())
	{
		return std::nullopt;
	}
	return Locator::audio::value().PlayAnimEffect(std::string(k_CollisionBank), keys, owner, position);
}

void GameDynamicsWorld::MoveSound(entt::entity emitter, glm::vec3 position)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().SetEmitterPosition(emitter, position);
	}
}

void GameDynamicsWorld::AddWaterRing(const water_rings::Ring& ring)
{
	if (Locator::waterRingSystem::has_value())
	{
		Locator::waterRingSystem::value().Add(ring);
	}
}

void GameDynamicsWorld::ScareFish(glm::vec3 point)
{
	if (Locator::fishFarmSystem::has_value())
	{
		Locator::fishFarmSystem::value().Scare(point);
	}
}

int32_t GameDynamicsWorld::SnowAt(glm::vec3 point) const
{
	return snow_dust::SnowAt(point);
}

void GameDynamicsWorld::StartedMoving(entt::entity object)
{
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().StartedMoving(object, false);
	}
}

bool GameDynamicsWorld::IsOnFire(entt::entity object) const
{
	return Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object);
}

void GameDynamicsWorld::CreateReaction(const ReactionSystemInterface::Source& source)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Create(source);
	}
}

void GameDynamicsWorld::RemoveReactions(entt::entity initiator, Reaction type)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().RemoveFrom(initiator, type);
	}
}

void GameDynamicsWorld::UntieLeashesTiedTo(entt::entity object)
{
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	Entities().Each<const components::Creature>([&leashes, object](entt::entity creature, const components::Creature&) {
		if (leashes.TiedTo(creature) == object)
		{
			leashes.UntieToHand(creature);
		}
	});
}

bool GameDynamicsWorld::IsComputerPlayer(PlayerNames player) const
{
	return Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(player);
}

void GameDynamicsWorld::FitDeadTreeObstacle(entt::entity deadTree)
{
	archetypes::DeadTreeArchetype::FitObstacle(deadTree);
}
