/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The points of an object's model that glints sparkle on: those of a miracle's seed shown in a globe or over an icon,
// every point of every part of its model in order, placed as the seed was last drawn

#define LOCATOR_IMPLEMENTATIONS

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleVisuals.h"
#include "ParticleRuleSystem.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A model of a seed and where it was last drawn, for the glints of an effect that acts on the seed
struct SeedTarget
{
	const graphics::L3DMesh* model {nullptr};
	std::optional<glm::mat4> placement;
};

/// The model with the loaded models, if it is loaded
const graphics::L3DMesh* LoadedModel(entt::id_type id)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(id) ? &*meshes.Handle(id) : nullptr;
}

const OneOffSpellSeed* GlobeOf(entt::entity object)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::entitiesRegistry::value().Valid(object))
	{
		return nullptr;
	}
	return Locator::entitiesRegistry::value().TryGet<const OneOffSpellSeed>(object);
}

/// The seed that an object shows: the seed in a globe, or the seed over an icon that has the effect of its holder
SeedTarget SeedTargetOf(entt::entity object)
{
	if (const auto* globe = GlobeOf(object); globe != nullptr)
	{
		if (!Locator::infoConstants::has_value())
		{
			return {};
		}
		const auto& seed = magic::GetSpellSeedInfo(Locator::infoConstants::value(), globe->seedType);
		return {.model = LoadedModel(resources::HashIdentifier(seed.mesh)), .placement = globe->seedPlacement};
	}
	if (Locator::spellIconSystem::has_value())
	{
		if (const auto target = Locator::spellIconSystem::value().FindHolderTarget(object); target.has_value())
		{
			return {.model = LoadedModel(target->model), .placement = target->placement};
		}
	}
	return {};
}
} // namespace

uint32_t GameParticleWorld::TargetPointCount(entt::entity object) const
{
	const auto target = SeedTargetOf(object);
	if (target.model == nullptr)
	{
		return 0;
	}
	size_t count = 0;
	for (const auto& part : target.model->GetSubMeshes())
	{
		count += part->GetSurfacePoints().size();
	}
	return static_cast<uint32_t>(count);
}

std::optional<glm::vec3> GameParticleWorld::TargetPoint(entt::entity object, uint32_t index) const
{
	const auto target = SeedTargetOf(object);
	if (target.model == nullptr || !target.placement.has_value())
	{
		return std::nullopt;
	}
	size_t remaining = index;
	for (const auto& part : target.model->GetSubMeshes())
	{
		const auto& points = part->GetSurfacePoints();
		if (remaining < points.size())
		{
			return glm::vec3(*target.placement * glm::vec4(points[remaining].position, 1.0f));
		}
		remaining -= points.size();
	}
	return std::nullopt;
}

float GameParticleWorld::TargetScale(entt::entity object) const
{
	const auto* globe = GlobeOf(object);
	const auto* transform = globe != nullptr ? Locator::entitiesRegistry::value().TryGet<const Transform>(object) : nullptr;
	// The seed is drawn at six tenths of the globe's scale. The seed over an icon has its full size.
	return transform != nullptr ? transform->scale.x * magic::visuals::k_GlobeSeedScale : 1.0f;
}
