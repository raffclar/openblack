/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// What FallingSpell (Magic/Objects/FallingSpell.h) draws over the film fall.bik in mode 2: the sparks (sprites with
// the smoke material, drawn by the Z-sorter's flush at the end of the frame) and the light bursts (a finish frame
// callback, world triangles in the additive atmosphere material). Both are anchored to points on the screen, so they
// are drawn here in screen pixels in the ScreenOverlay view, after the film and before the bars and the fade
// (Renderer::DrawFinishFrameOverlays).

#include <cstring>

#include <array>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "Camera/Camera.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Magic/Objects/FallingSpell.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();

struct Vertex
{
	float x, y, z, u, v;
	uint32_t abgr;
};
} // namespace

void Renderer::DrawFallingSpellOverlay(const Camera* camera) const
{
	const auto& spell = magic::falling_spell::Get();
	if (!spell.IsActive() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const int width = _resolution.x;
	const int height = _resolution.y;
	// (inferred) the near plane of mode 2 is the one the last camera left: openblack's camera's (the draw's
	// copy, DrawSceneDesc::camera)
	const float nearZ = camera != nullptr ? billboard::CameraFrame::From(*camera).nearZ : 1.0f;
	const auto toClipX = [width](float px) { return 2.0f * px / static_cast<float>(width) - 1.0f; };
	const auto toClipY = [height](float py) { return 1.0f - 2.0f * py / static_cast<float>(height); };
	const auto& textures = Locator::resources::value().GetTextures();
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto submit = [&](const std::vector<Vertex>& vertices, entt::hashed_string::hash_type texture,
	                        entt::hashed_string::hash_type alpha, const render_modes::Material& material) {
		const auto count = static_cast<uint32_t>(vertices.size());
		if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, layout) < count || !textures.Contains(texture) ||
		    !textures.Contains(alpha))
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		const auto* program = _shaderManager->GetShader("WorldQuad");
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(alpha));
		bgfx::setVertexBuffer(0, &buffer);
		// the Z: the end of frame's Z reset quad (z = 1 everywhere) of the frame before, and nothing with Z
		// drawn in mode 2 before these (the film is ZFUNC ALWAYS without Z), so LESSEQUAL always passes: ALWAYS here;
		// neither mode writes Z (6 and 13, the "NoZWrite" variants)
		bgfx::setState(render_modes::State(material, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
		bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
	};

	// the sparks, far to near, each one Z object; one buffer keeps that order: their two triangles {0, 1, 2},
	// {0, 2, 3} one after the other
	std::vector<Vertex> sparks;
	for (const auto& quad : spell.SparkQuads(width, height, nearZ))
	{
		for (const int i : billboard::k_SpriteTriangles)
		{
			const auto& v = quad.at(static_cast<size_t>(i));
			sparks.push_back({toClipX(v.pixel.x), toClipY(v.pixel.y), 0.5f, v.uv.x, v.uv.y, argb_colour::ToAbgr(v.argb)});
		}
	}
	// the smoke material (smoke.raw, mode 6, two-sided; the sprite's flag 0x80 selects it)
	submit(sparks, k_Smoke, k_SmokeAlpha, render_modes::materials::k_Smoke);

	// the four bursts in their order, 64 world triangles each
	std::vector<Vertex> bursts;
	const auto indices = magic::falling_spell::BurstIndices();
	for (const auto& fan : spell.Bursts())
	{
		for (const int i : indices)
		{
			const auto& v = fan.vertices.at(static_cast<size_t>(i));
			bursts.push_back({toClipX(v.pixel.x), toClipY(v.pixel.y), 0.5f, v.uv.x, v.uv.y, argb_colour::ToAbgr(v.argb)});
		}
	}
	// the additive atmosphere material: atmos.raw, mode 13 (SRCALPHA / ONE), two-sided
	submit(bursts, k_Atmos, k_AtmosAlpha, render_modes::materials::k_AtmosAdditive);
}
