/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <limits>

#include <bgfx/bgfx.h>
#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "Creature/TempleLeashes.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// Blended and alpha tested as the collar's material says, writing depth, the backs of its faces culled
constexpr uint64_t k_CollarState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
constexpr float k_White = 255.0f;
} // namespace

void Renderer::DrawTempleLeashes(const DrawSceneDesc& desc) const
{
	using ecs::components::LeashPost;
	using ecs::components::Transform;
	if (desc.camera == nullptr)
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!meshes.Contains(LeashPost::k_MeshId) || !textures.Contains(LeashPost::k_TextureId))
	{
		return;
	}
	const auto& mesh = *meshes.Handle(LeashPost::k_MeshId);
	const auto skin = textures.Handle(LeashPost::k_TextureId)->GetNativeHandle();
	const auto eye = desc.camera->GetOrigin();
	const bool reflection = desc.viewId == RenderPass::Reflection;
	const auto translucent = TranslucentView(desc.viewId);
	const auto* program = _shaderManager->GetShader("Object");

	// Each collar is drawn among the things that blend, by its distance from the camera, straight after its glow, which
	// stands at the same place: the collar shows whole over the glow. Lit in white by the sun alone: no light or colour
	// of the land reaches it, and no shadow falls on it.
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const LeashPost, const Transform>([&](const LeashPost& post, const Transform& transform) {
		if (transform.scale == glm::vec3(0.0f))
		{
			return;
		}
		const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = translucent;
		submitDesc.program = program;
		submitDesc.state = k_CollarState;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.mirrored = reflection;
		submitDesc.useMaterialBlending = true;
		submitDesc.useMaterialCulling = true;
		// Its band of the leash texture, slid round it
		submitDesc.skinTexture = &skin;
		submitDesc.uvOffset = glm::vec2(post.look.scroll, temple_leashes::Band(post.type));
		submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = glm::vec3(k_White), .alpha = 1.0f};
		submitDesc.landColourAdded = false;
		submitDesc.groundShadows = false;
		submitDesc.sortDepth = std::max(zsort::Depth(transform.position, eye), 1u) - 1;
		DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	});
}
