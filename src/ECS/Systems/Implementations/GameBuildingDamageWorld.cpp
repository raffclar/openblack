/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameBuildingDamageWorld.h"

#include <exception>
#include <limits>

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Registry.h"
#include "ECS/SnowDust.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

namespace
{
/// The bank the knocks and crashes of buildings play from
constexpr std::string_view k_BuildingSoundBank = "editor.sad";
/// The game's crush, which a breaking blow applies
constexpr size_t k_CrushPreset = 3;

const graphics::L3DMesh* MeshOf(entt::id_type id)
{
	if (!Locator::resources::has_value() || id == 0)
	{
		return nullptr;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(id) ? &*meshes.Handle(id) : nullptr;
}
} // namespace

Registry& GameBuildingDamageWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

const Registry& GameBuildingDamageWorld::Entities() const
{
	return Locator::entitiesRegistry::value();
}

float GameBuildingDamageWorld::LandHeight(glm::vec2 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

std::vector<building_world::ModelPart> GameBuildingDamageWorld::ModelParts(entt::id_type mesh) const
{
	std::vector<building_world::ModelPart> parts;
	const auto* model = MeshOf(mesh);
	if (model == nullptr)
	{
		return parts;
	}
	const auto& subMeshes = model->GetSubMeshes();
	for (uint32_t s = 0; s < subMeshes.size(); ++s)
	{
		const auto& subMesh = *subMeshes[s];
		const auto flags = subMesh.GetFlags();
		// Only the nearest level of detail, and only what is drawn
		if ((flags.lodMask & 1U) == 0 || flags.status != 0)
		{
			continue;
		}
		const auto& surface = subMesh.GetSurface();
		const auto& primitives = subMesh.GetPrimitives();
		for (uint32_t p = 0; p < primitives.size(); ++p)
		{
			building_world::ModelPart part {.material = (s << 16U) | p};
			const auto& source = primitives[p];
			for (uint32_t i = 0; i + 2 < source.indicesCount; i += 3)
			{
				std::array<physics::damage::Corner, 3> corners;
				bool inside = true;
				for (uint32_t c = 0; c < 3; ++c)
				{
					const auto index = source.indicesOffset + i + c;
					if (index >= surface.indices.size() || surface.indices[index] >= surface.positions.size())
					{
						inside = false;
						break;
					}
					const auto vertex = surface.indices[index];
					corners.at(c) = {.position = surface.positions[vertex], .uv = surface.uvs.at(vertex)};
				}
				if (inside)
				{
					part.triangles.push_back(corners);
				}
			}
			parts.push_back(std::move(part));
		}
	}
	return parts;
}

entt::id_type GameBuildingDamageWorld::MakeModel(const std::string& name, entt::id_type source,
                                                 std::span<const building_world::DrawnPart> parts)
{
	const auto* model = MeshOf(source);
	if (model == nullptr)
	{
		return 0;
	}
	using MadePrimitive = graphics::L3DSubMesh::MadePrimitive;
	// Parts are gathered into sub meshes of no more vertices than a 16-bit index reaches
	std::vector<std::vector<MadePrimitive>> subMeshes(1);
	size_t vertices = 0;
	const auto& sourceSubMeshes = model->GetSubMeshes();
	for (const auto& part : parts)
	{
		const auto s = part.material >> 16U;
		const auto p = part.material & 0xFFFFU;
		if (s >= sourceSubMeshes.size() || p >= sourceSubMeshes[s]->GetPrimitives().size() ||
		    part.vertices.size() > std::numeric_limits<uint16_t>::max())
		{
			continue;
		}
		MadePrimitive made {.source = sourceSubMeshes[s].get(), .sourcePrimitive = p, .indices = part.indices};
		made.vertices.reserve(part.vertices.size());
		for (const auto& vertex : part.vertices)
		{
			made.vertices.push_back({.position = vertex.position, .uv = vertex.uv, .normal = vertex.normal});
		}
		if (vertices + made.vertices.size() > std::numeric_limits<uint16_t>::max())
		{
			subMeshes.emplace_back();
			vertices = 0;
		}
		vertices += made.vertices.size();
		subMeshes.back().push_back(std::move(made));
	}
	std::erase_if(subMeshes, [](const auto& primitives) { return primitives.empty(); });
	if (subMeshes.empty())
	{
		return 0;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = entt::hashed_string(name.c_str()).value();
	if (meshes.Contains(id))
	{
		meshes.Erase(id);
	}
	try
	{
		meshes.Load(id, resources::L3DLoader::FromMadeTag {}, name, *model, std::span(subMeshes));
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't make the model {}: {}", name, error.what());
		return 0;
	}
	return id;
}

void GameBuildingDamageWorld::EraseModel(entt::id_type mesh)
{
	if (mesh != 0 && Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(mesh))
	{
		Locator::resources::value().GetMeshes().Erase(mesh);
	}
}

DynamicsSystemInterface* GameBuildingDamageWorld::Dynamics()
{
	return Locator::dynamicsSystem::has_value() ? &Locator::dynamicsSystem::value() : nullptr;
}

GameRandomInterface* GameBuildingDamageWorld::Random()
{
	return Locator::gameRandom::has_value() ? &Locator::gameRandom::value() : nullptr;
}

int32_t GameBuildingDamageWorld::SnowAt(glm::vec3 point) const
{
	return snow_dust::SnowAt(point);
}

uint32_t GameBuildingDamageWorld::DustTint(uint32_t argb, int32_t snow) const
{
	return snow_dust::Tint(argb, snow);
}

void GameBuildingDamageWorld::PlaySound(std::span<const int32_t> keys, entt::entity building, glm::vec3 position)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().PlayAnimEffect(std::string(k_BuildingSoundBank), keys, building, position);
	}
}

float GameBuildingDamageWorld::LifeOf(entt::entity object) const
{
	return world_objects::LifeOf(object);
}

const GObjectInfo* GameBuildingDamageWorld::InfoOf(entt::entity object) const
{
	return world_objects::InfoOf(object);
}

std::optional<PlayerNames> GameBuildingDamageWorld::PlayerOf(entt::entity object) const
{
	return world_objects::PlayerOf(object);
}

void GameBuildingDamageWorld::Remove(entt::entity object)
{
	world_objects::Remove(object);
}

std::optional<magic::EffectValues> GameBuildingDamageWorld::Crush() const
{
	if (!Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	return magic::EffectValues::From(Locator::infoConstants::value().effect.at(k_CrushPreset));
}

void GameBuildingDamageWorld::ApplyEffect(entt::entity object, const magic::EffectValues& values,
                                          const magic::EffectSource& source)
{
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ApplyEffectToObject(object, values, source);
	}
}

void GameBuildingDamageWorld::PlayerDid(size_t deed, glm::vec3 point, entt::entity object, PlayerNames player)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().PlayerDid(deed, point, object, player);
	}
}
