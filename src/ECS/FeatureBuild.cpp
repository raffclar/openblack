/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FeatureBuild.h"

#include <cstdlib>

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PartialBuild.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::feature_build
{
namespace
{
using namespace components;

void EraseMesh(entt::id_type id)
{
	auto& meshes = Locator::resources::value().GetMeshes();
	if (id != 0 && meshes.Contains(id))
	{
		meshes.Erase(id);
	}
}

/// The model the Feature shows for its percentage
void Redraw(entt::entity entity, Feature& feature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	auto* mesh = registry.TryGet<Mesh>(entity);
	if (feature.intactMesh == 0 && mesh != nullptr)
	{
		feature.intactMesh = mesh->id;
	}
	// drawn as a building: the dry dock while not built (percentage below 1)
	const bool drawBuilding = feature.type == FeatureInfo::ArkDryDock && feature.percentBuilt < 1.0f;
	const float percent = feature.percentBuilt; // min(percent built, 1)
	const auto old = feature.builtMesh;
	feature.builtMesh = 0;
	entt::id_type shown = feature.intactMesh;
	if (drawBuilding)
	{
		shown = 0; // nothing at 0
		feature.builtMesh = physics::PartialBuild::BuildMesh(entity, feature.intactMesh, percent, "feature-built");
		shown = feature.builtMesh;
	}
	if (shown == 0)
	{
		registry.Remove<Mesh>(entity);
	}
	else if (mesh != nullptr)
	{
		mesh->id = shown;
		mesh->submeshId = shown != feature.intactMesh && meshes.Handle(shown)->GetNumSubMeshes() > 1 ? static_cast<int8_t>(-1)
		                                                                                             : static_cast<int8_t>(0);
	}
	else
	{
		registry.Assign<Mesh>(entity, shown, static_cast<int8_t>(0), static_cast<int8_t>(1));
	}
	EraseMesh(old);
	registry.SetDirty();
}
} // namespace

std::optional<float> GetBuiltPercentage(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* feature = registry.TryGet<const Feature>(entity); feature != nullptr)
	{
		return feature->percentBuilt;
	}
	if (registry.AnyOf<Abode>(entity))
	{
		return std::nullopt; // a multi-map fixed object whose percentage openblack does not keep
	}
	return 1.0f; // not a multi-map fixed object
}

bool SetBuiltPercentage(entt::entity entity, float value)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* feature = registry.TryGet<Feature>(entity);
	if (feature == nullptr)
	{
		return false;
	}
	// the value, 0 when negative; >= 1 -> built (1)
	feature->percentBuilt = std::max(value, 0.0f);
	if (feature->percentBuilt >= 1.0f)
	{
		feature->percentBuilt = 1.0f;
	}
	Redraw(entity, *feature);
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "Feature {} built percentage {:.2f}", static_cast<int>(feature->type),
	                    feature->percentBuilt);
	return true;
}

void RunDebugHook()
{
	const char* test = std::getenv("OPENBLACK_TEST_BUILT_PERCENTAGE");
	if (test == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	// TheMissionaries: GArk = CREATE(3 = Feature, 69 = ArkDryDock, (1881.083, 8.1316, 3154.109)), then BUILT_PERCENTAGE
	const glm::vec2 at(1881.083f, 3154.109f);
	const glm::vec3 position(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
	const auto dock = archetypes::FeatureArchetype::Create(position, FeatureInfo::ArkDryDock, 0.0f, 1.0f);
	SetBuiltPercentage(dock, static_cast<float>(std::atof(test)));
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Built percentage test: ArkDryDock at {:.3f}", *GetBuiltPercentage(dock));
}

} // namespace openblack::ecs::feature_build
