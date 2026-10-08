/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GroundMarks.h"

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/AllMeshes.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
struct Mark
{
	entt::entity entity {entt::null};
	int32_t life {0}; ///< ms
};

/// What this module keeps between calls (Locator::worldEffects)
struct GroundMarksState
{
	/// the newest first
	std::vector<Mark> marks {};
	/// (approximate) openblack's frame delta is a float: the fraction waits for the next frame so the sum stays in
	/// whole ms like the original's integer frame time (taken off as is, with no carry)
	float carry {0.0f};
};

GroundMarksState& GroundMarksData()
{
	return openblack::Locator::worldEffects::value().Get<GroundMarksState>();
}
} // namespace

entt::entity ecs::ground_marks::Create(const glm::vec3& position, const glm::mat3& rotation, float scale)
{
	auto& state = GroundMarksData();
	// the dust of disappear_smoke mode 1, made whether the mark's
	// object could be drawn or not (both makers: the explosion's crater and the uprooted tree's)
	disappear_smoke::Create(position, 1, 1.0f, 0xFFFFFFFFu);
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto mesh = resources::HashIdentifier(MeshId::TreeRootsPile);
	if (!meshes.Contains(mesh))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, glm::vec3(scale));
	registry.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	// a morphable object whose deltas the melting takes once
	registry.Assign<MorphWithTerrain>(entity, land_morph::Melting::Snapshot);
	state.marks.insert(state.marks.begin(), Mark {entity, k_LifeMs});
	return entity;
}

entt::entity ecs::ground_marks::CreateExplosionMark(const glm::vec3& position, float angle)
{
	const auto rotation = affine::AngleY(angle);
	return Create(position, rotation, k_ExplosionScale);
}

void ecs::ground_marks::Clear()
{
	auto& state = GroundMarksData();
	// the marks are deleted; the entities go with the registry's reset
	state.marks.clear();
	state.carry = 0.0f;
}

void ecs::ground_marks::Update(float gameMilliseconds)
{
	auto& state = GroundMarksData();
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	state.carry += std::max(gameMilliseconds, 0.0f);
	const auto step = static_cast<int32_t>(state.carry);
	state.carry -= static_cast<float>(step);
	bool dirty = false;
	for (auto& mark : state.marks)
	{
		if (!registry.Valid(mark.entity))
		{
			mark.entity = entt::null;
			continue;
		}
		if (mark.life <= k_FadeMs)
		{
			// the colour's alpha byte = life x 0.255, truncated: 255 at 1000 ms
			const auto alpha = static_cast<int32_t>(static_cast<float>(mark.life) * k_FadeAlphaPerMs) & 0xFF;
			registry.AssignOrReplace<Alpha>(mark.entity, static_cast<float>(alpha) / 255.0f);
			dirty = true;
		}
		mark.life -= step;
		if (mark.life <= 0)
		{
			registry.Destroy(mark.entity);
			mark.entity = entt::null;
			dirty = true;
		}
	}
	std::erase_if(state.marks, [](const Mark& mark) { return mark.entity == entt::null; });
	if (dirty)
	{
		registry.SetDirty();
	}
}
