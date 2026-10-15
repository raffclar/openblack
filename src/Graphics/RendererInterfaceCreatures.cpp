/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <span>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/ext/matrix_transform.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::graphics
{

namespace
{
/// The instances of the creatures that the interface shows, one for each player
constexpr uint16_t k_MostShown = 4;
} // namespace

void Renderer::ShowInterfaceCreatures(interface_creatures::Request request) noexcept
{
	_interfaceCreatures = std::move(request);
}

void Renderer::DrawInterfaceCreatures(const DrawSceneDesc& desc, const InterfaceCreatureBody& drawBody) const
{
	// Each request shows for one frame of the scene only
	const auto request = std::move(*_interfaceCreatures);
	_interfaceCreatures.reset();
	if (request.creatures.empty() || desc.camera == nullptr || request.viewSize.x <= 0.0f || request.viewSize.y <= 0.0f)
	{
		return;
	}
	if (!_interfaceCreatureInstances.has_value())
	{
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		_interfaceCreatureInstances = fromBgfx(bgfx::createDynamicVertexBuffer(k_MostShown, layout));
	}

	// One camera goes round all the creatures, over the interface's space, with the depth cleared before them
	const auto view = interface_creatures::View(request.turn);
	const auto projection = interface_creatures::SpaceProjection(desc.camera->GetProjectionMatrix());
	const auto viewId = static_cast<bgfx::ViewId>(RenderPass::InterfaceCreatures);
	bgfx::setViewClear(viewId, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
	bgfx::setViewRect(viewId, static_cast<uint16_t>(request.viewOrigin.x), static_cast<uint16_t>(request.viewOrigin.y),
	                  static_cast<uint16_t>(request.viewSize.x), static_cast<uint16_t>(request.viewSize.y));
	bgfx::setViewTransform(viewId, &view, &projection);
	bgfx::touch(viewId);

	const auto& meshes = Locator::resources::value().GetMeshes();
	std::vector<ecs::systems::RenderContext::ObjectInstance> instances;
	struct Body
	{
		entt::id_type mesh;
		std::span<const glm::mat4> bones;
		std::optional<L3DMeshSubmitDesc::MorphTargets> targets;
		std::optional<ecs::components::CreatureEyes> eyes;
	};
	std::vector<Body> bodies;
	for (const auto& shown : request.creatures)
	{
		if (instances.size() == k_MostShown || !desc.entities.Valid(shown.creature))
		{
			continue;
		}
		const auto* mesh = desc.entities.TryGet<const ecs::components::Mesh>(shown.creature);
		const auto* creature = desc.entities.TryGet<const ecs::components::Creature>(shown.creature);
		const auto* transform = desc.entities.TryGet<const ecs::components::Transform>(shown.creature);
		if (mesh == nullptr || creature == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
		{
			continue;
		}
		const auto body = meshes.Handle(mesh->id);
		const auto* animation = desc.entities.TryGet<const ecs::components::CreatureAnimation>(shown.creature);
		const bool posed = animation != nullptr && animation->boneMatrices.size() == body->GetBoneMatrices().size();
		const auto bones =
		    posed ? std::span<const glm::mat4>(animation->boneMatrices) : std::span<const glm::mat4>(body->GetBoneMatrices());
		if (bones.empty())
		{
			continue;
		}
		// The creature goes to the point under its place on the screen, at its depth, its bones' middle and its feet there
		// The interface's space is the game's default screen, which the whole interface takes
		const auto depth = interface_creatures::Depth(ecs::components::ShownSize(*creature), shown.size, 1.0f);
		const auto point =
		    interface_creatures::PointFromScreen(view, projection, interface_creatures::k_Space, shown.anchor, depth);
		const auto model = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
		const auto move =
		    glm::translate(glm::mat4(1.0f), interface_creatures::MoveToPoint(point, model, bones, transform->position.y));
		instances.push_back({.model = move * model});

		Body drawn {.mesh = mesh->id, .bones = bones, .targets = std::nullopt, .eyes = std::nullopt};
		if (const auto* morph = desc.entities.TryGet<const ecs::components::CreatureMorph>(shown.creature); morph != nullptr)
		{
			const auto ids = creature_morph::MeshesOf(creature->species, morph->drawn,
			                                          [&meshes](entt::id_type id) { return meshes.Contains(id); });
			drawn.targets = L3DMeshSubmitDesc::MorphTargets {
			    .meshes = {&*meshes.Handle(ids.evilGood), &*meshes.Handle(ids.thinFat), &*meshes.Handle(ids.weakStrong)},
			    .weights = glm::abs(glm::vec3(morph->drawn.evilGood, morph->drawn.thinFat, morph->drawn.weakStrong)),
			    .skins = {},
			    .blendSeams = Locator::config::value().blendCreatureSeams,
			};
			if (const auto skins = _creatureSkins.find(shown.creature); skins != _creatureSkins.end())
			{
				drawn.targets->skins = skins->second.drawn;
			}
		}
		// Its eyes move with it
		if (const auto* eyes = desc.entities.TryGet<const ecs::components::CreatureEyes>(shown.creature); eyes != nullptr)
		{
			drawn.eyes = *eyes;
			for (auto& eye : drawn.eyes->drawn)
			{
				if (eye.eyeball.has_value())
				{
					eye.eyeball = move * *eye.eyeball;
				}
				if (eye.eyelid.has_value())
				{
					eye.eyelid = move * *eye.eyelid;
				}
			}
		}
		bodies.push_back(std::move(drawn));
	}
	if (instances.empty())
	{
		return;
	}
	bgfx::update(toBgfx(*_interfaceCreatureInstances), 0,
	             bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(instances.front()))));
	for (size_t i = 0; i < bodies.size(); ++i)
	{
		const auto& body = bodies.at(i);
		drawBody(body.mesh, static_cast<uint32_t>(i), body.bones, body.targets.has_value() ? &*body.targets : nullptr,
		         body.eyes.has_value() ? &*body.eyes : nullptr);
	}
}

} // namespace openblack::graphics
