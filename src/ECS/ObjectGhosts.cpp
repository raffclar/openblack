/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectGhosts.h"

#include <algorithm>

#include <glm/mat4x4.hpp>

#include "3D/FrameAnim.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "Graphics/RenderModes.h"
#include "Locator.h"

namespace openblack::ecs::object_ghosts
{
namespace
{
struct Ghost
{
	entt::id_type meshId;
	glm::mat4 model;
	float remainingMs;
};

/// What this module keeps between calls (Locator::worldEffects)
struct ObjectGhostsState
{
	std::vector<Ghost> ghosts {};
};

ObjectGhostsState& ObjectGhostsData()
{
	return openblack::Locator::worldEffects::value().Get<ObjectGhostsState>();
}
} // namespace

void Add(entt::entity object)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::worldEffects::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto* mesh = registry.TryGet<const components::Mesh>(object);
	const auto* transform = registry.TryGet<const components::Transform>(object);
	if (mesh == nullptr || transform == nullptr)
	{
		return;
	}
	// where it is drawn now: the hand's pose while the hand still draws it, else its own place
	auto model = affine::Model(*transform);
	if (const auto* pose = registry.TryGet<const components::HandDrawPose>(object); pose != nullptr)
	{
		model = affine::Model(pose->position, pose->rotation, transform->scale);
	}
	ObjectGhostsData().ghosts.push_back({mesh->id, model, graphics::frame_anim::k_GhostMs});
}

void Update(float frameMs)
{
	if (!Locator::worldEffects::has_value())
	{
		return;
	}
	auto& ghosts = ObjectGhostsData().ghosts;
	for (auto& ghost : ghosts)
	{
		const auto step = graphics::frame_anim::GhostStep(ghost.remainingMs, frameMs);
		ghost.remainingMs = step.expired ? -1.0f : step.remainingMs;
	}
	std::erase_if(ghosts, [](const Ghost& ghost) { return ghost.remainingMs < 0.0f; });
}

size_t Count()
{
	return Locator::worldEffects::has_value() ? ObjectGhostsData().ghosts.size() : 0;
}

std::vector<psys::mesh_atoms::Instance> Instances()
{
	std::vector<psys::mesh_atoms::Instance> result;
	if (!Locator::worldEffects::has_value())
	{
		return result;
	}
	for (const auto& ghost : ObjectGhostsData().ghosts)
	{
		const auto frame = graphics::frame_anim::GoolooFrame(ghost.remainingMs);
		psys::mesh_atoms::Instance instance {
		    .meshId = ghost.meshId,
		    .model = ghost.model,
		    .alpha = 1.0f,
		    .uv = frame.uv,
		    .translucent = false,
		    .additive = false,
		    .colour = {0xFF, 0xFF, 0xFF},
		    .landscapeColour = true,
		};
		// (inferred) the material byte the frame gives is the alpha reference of the copy's materials: not drawn so
		// here, the copy keeps its materials' own
		result.push_back(instance);
		// then again at no offset, in render mode 10
		instance.uv = glm::vec2(0.0f);
		instance.mode = graphics::render_modes::Mode::AlphaTexturedAlphaAdditiveChroma;
		result.push_back(instance);
	}
	return result;
}

void Clear()
{
	if (Locator::worldEffects::has_value())
	{
		ObjectGhostsData().ghosts.clear();
	}
}
} // namespace openblack::ecs::object_ghosts
