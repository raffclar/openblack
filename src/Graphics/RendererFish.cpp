/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <array>
#include <limits>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/TempleInteriorInterface.h"
#include "Animals/FishShoal.h"
#include "Camera/Camera.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/FishPuzzleRules.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	uint32_t colour;
};

/// The two triangles of a fish's square
constexpr std::array<size_t, 6> k_Triangles {0, 1, 2, 0, 2, 3};
/// An opaque model with depth, the nearest in front
constexpr uint64_t k_OpaqueState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CW |
                                   BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
} // namespace

void Renderer::DrawFishShoals(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Reflection || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	// The fish pictures of the sprite atlas, and their shapes in the alpha beside it
	static constexpr auto k_TextureId = entt::hashed_string("raw/misc0");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/misc0a");
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_TextureId.value()) || !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}

	std::vector<Vertex> vertices;
	const auto addShoal = [&vertices](const fish_shoal::Shoal& shoal, uint8_t alpha) {
		// White, as opaque as the shoal is at its distance
		const auto colour = (static_cast<uint32_t>(alpha) << 24u) | 0x00FFFFFFu;
		const auto shown = fish_shoal::ShownCount(shoal.fullness);
		for (uint32_t i = 0; i < shown; ++i)
		{
			const auto& fish = shoal.fish.at(i);
			const auto corners = fish_shoal::Corners(fish);
			const auto uvs = fish_shoal::AtlasCorners(fish.frame);
			for (const auto corner : k_Triangles)
			{
				// Drawn into what lies under the sea, which is seen through a mirror in the sea's level: mirrored
				// back, each corner falls where it is seen from above
				const auto& at = corners.at(corner);
				vertices.push_back({{at.x, -at.y, at.z}, uvs.at(corner), colour});
			}
		}
	};
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const ecs::components::FishFarm>([&addShoal](const ecs::components::FishFarm& farm) {
		if (farm.shoal.has_value() && farm.shownAlpha.has_value())
		{
			addShoal(*farm.shoal, *farm.shownAlpha);
		}
	});
	// The shoals of the fish puzzle swim in the turns wherever the camera is, but show only as near as a farm's shoal.
	// After the fish are caught, they do not show.
	const auto eye = desc.camera->GetOrigin();
	registry.Each<const ecs::components::PuzzleGame>([&addShoal, eye](const ecs::components::PuzzleGame& game) {
		if (!game.fish.has_value() || game.fish->net.caught)
		{
			return;
		}
		for (const auto& shoal : game.fish->shoals)
		{
			const auto offset = eye - shoal.centre;
			if (const auto alpha = fish_shoal::AlphaAt(glm::dot(offset, offset)))
			{
				addShoal(shoal, *alpha);
			}
		}
	});
	if (vertices.empty())
	{
		// A warm-up draws one triangle of no size, so that the first shoal in sight makes no pipeline
		if (!_warmingUp)
		{
			return;
		}
		vertices.resize(k_Triangles.size() / 2);
	}

	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
	bgfx::setVertexBuffer(0, &buffer);
	// Unlit, blended over what lies under the sea, both sides, for the sea to be blended over them in turn
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
	program->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawFishPuzzleFloats(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !desc.drawWater ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(ecs::fish_puzzle::k_FloatMeshId.value()))
	{
		return;
	}
	const auto& mesh = *meshes.Handle(ecs::fish_puzzle::k_FloatMeshId.value());
	const auto* program = _shaderManager->GetShader("Object");
	// The draw only reads the registry, so that it adds no store while the side threads read the stores
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const ecs::components::PuzzleGame>([this, &mesh, program](const ecs::components::PuzzleGame& game) {
		    if (!game.fish.has_value())
		    {
			    return;
		    }
		    // Only what of each float is under the sea's level is drawn, beneath the sea's surface, which the sea is then
		    // blended over. The float keeps its own colour.
		    for (const auto& point : ecs::fish_puzzle::FloatPoints(game.fish->net))
		    {
			    const auto model = glm::translate(glm::mat4(1.0f), point);
			    L3DMeshSubmitDesc submitDesc = {};
			    submitDesc.viewId = RenderPass::UnderSea;
			    submitDesc.program = program;
			    submitDesc.state = k_OpaqueState;
			    submitDesc.modelMatrices = &model;
			    submitDesc.matrixCount = 1;
			    submitDesc.useMaterialBlending = true;
			    submitDesc.useMaterialCulling = true;
			    submitDesc.cutAbove = 0.0f;
			    DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
		    }
	    });
}
