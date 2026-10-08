/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The missionaries' boat (MissionaryBoat, ecs/MissionaryBoat.h): its reflection (drawn under water in 0xFF303070) and the
// sprites of the boat's frame: the flat smoke of its wake and the DisappearSmoke puffs, all with the smoke material
// (smoke.raw / smokea.raw, render mode 6). Each sprite is one Z object of the frame's single transparency queue
// (Graphics/ZSort.h).

#include <cmath>
#include <cstring>

#include <array>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Mesh.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Texture = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_Alpha = entt::hashed_string("raw/smokea").value();
} // namespace

void Renderer::DrawBoatReflection(RenderPass viewId) const
{
	const auto entity = ecs::missionary_boat::GetReflectedHull();
	if (entity == entt::null || !Locator::rendereringSystem::has_value())
	{
		return;
	}
	// DrawUnderWater: mirrored in y = 0 (the reflection camera here), what had y < 0 clipped away, unlit in the object
	// colour 0xFF303070 (vs_object mode 2, sea_pass::UnderWater). (inferred) the specular is left as it was (only the
	// colour is written): the hull's last draw's specular is not kept in openblack, 0. The hull (missionary_boat,
	// MissionaryBoat.cpp) never gets MorphWithTerrain, so its instance's morphWithTerrain is false and DrawUnderWater
	// draws it with ObjectInstanced, as before
	DrawUnderWater(viewId, entity, sea_pass::UnderWater(ecs::missionary_boat::k_ReflectionColour, 0u));
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectBoatSprites(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	_frameBoatSprites.clear();
	const auto& wake = ecs::missionary_boat::GetWake();
	const auto& clouds = ecs::disappear_smoke::Get();
	if (wake.empty() && clouds.empty())
	{
		return order;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return order;
	}
	const auto eye = camera.GetOrigin();
	// key = |sprite position - camera|^2, (x^2 + y^2) + z^2; the sprite is queued whether or not the draw then culls it
	// at the near plane
	const auto add = [this, &order, &eye](const glm::vec3& position, std::optional<billboard::Quad> quad, uint32_t argb) {
		order.emplace_back(zsort::Key(position, eye), static_cast<uint32_t>(_frameBoatSprites.size()));
		_frameBoatSprites.push_back({std::move(quad), argb});
	};
	// the wake: flag 0x40, the quad in the sprite's XZ turned about Y (billboard::Horizontal)
	for (const auto& wakeSprite : wake)
	{
		billboard::Sprite sprite {
		    .position = wakeSprite.position,
		    .size = wakeSprite.half,
		    .height = wakeSprite.aspect,
		    .angle = wakeSprite.angle,
		    .argb = wakeSprite.argb,
		    .cell = frame_anim::SpriteCell(wakeSprite.cell),
		    .horizontal = true,
		};
		add(sprite.position, billboard::Horizontal(sprite), sprite.argb);
	}
	// the puffs: mode A, in the plane of the screen and turned on it (view x -> (cos, -sin), view y -> (sin, cos);
	// billboard::SpriteQuad), nothing at or before the near plane
	const auto frame = billboard::CameraFrame::From(camera);
	for (const auto& cloud : clouds)
	{
		if (cloud.life <= 0.0f)
		{
			continue;
		}
		for (const auto& puff : cloud.puffs)
		{
			billboard::Sprite sprite {
			    .position = puff.position,
			    .size = puff.half,
			    .angle = puff.angle,
			    .argb = puff.argb,
			    .cell = frame_anim::SpriteCell(puff.cell),
			};
			add(sprite.position, billboard::SpriteQuad(sprite, frame), sprite.argb);
		}
	}
	return order;
}

void Renderer::DrawBoatSprite(RenderPass viewId, uint32_t index) const
{
	if (index >= _frameBoatSprites.size() || !_frameBoatSprites[index].quad.has_value())
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& [quad, argb] = _frameBoatSprites[index];
	// the cell of the 8 x 8 sheet is in the quad's UVs, the colour is the vertex diffuse
	const uint32_t abgr = argb_colour::ToAbgr(argb);
	std::array<Vertex, billboard::k_SpriteTriangles.size()> vertices {};
	for (size_t k = 0; k < vertices.size(); ++k)
	{
		const auto i = static_cast<size_t>(billboard::k_SpriteTriangles.at(k));
		const auto& p = quad->corners.at(i);
		const auto& uv = quad->uv.at(i);
		vertices.at(k) = {p.x, p.y, p.z, uv.x, uv.y, abgr};
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
	const auto& textures = Locator::resources::value().GetTextures();
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// the smoke material (the boat's and the smoke puffs'), mode 6: SRCALPHA / INVSRCALPHA, no Z write, both faces
	bgfx::setState(render_modes::State(render_modes::materials::k_Smoke));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}
