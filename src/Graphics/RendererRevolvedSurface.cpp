/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The surfaces of revolution of the particle effects (ZR_SurfRevol: the teleport pool, the dispensers' discs): the
// original sets the scrolled UV offset and draws the mesh in render mode 6: colour = texture x diffuse + specular,
// alpha = texture alpha x diffuse alpha. Here two passes with the WorldQuad program: the textured one blended, then the
// specular added with a white texture and the same alpha (alpha x specular is what the specular adds under that
// blend). Unlit (UseLighting is not ported). Particles/Rules/SurfRevol.h.
//
// A surface is drawn by the indexed world triangle draw, the one with a specular per vertex: the primitive has as many
// speculars as colours, as ZR_SurfRevol sizes both to NumU x NumV. That draw always goes through the world-to-clipping
// matrix, the material's culling (its two-sided bit) and the material through the current table. So the surfaces stay
// here and do not go through graphics::world_triangles (WorldTriangles.h): that one takes an L3D primitive's material
// (its per-vertex specular is the FragMesh's), while a surface has its own mode 6 material with the .raw texture and
// its alpha file.
//
// The surface draw never reads the effect's draw path: a surface has no Z object of its own on any path. A Sorted
// effect's (the teleport pool; the dispensers' discs) is drawn at once when the effect is drawn, in the main view
// after the models (with the spells, before the drain); a Queued one inside its effect's single Z object, at its place
// in the effect's items; an Immediate one inside the hand's. Renderer.cpp picks the place; this draws one surface.

#include <cstring>

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Particles/Rules/SurfRevol.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The specular pass's white pixel
constexpr std::array<uint8_t, 4> k_WhitePixel {255, 255, 255, 255};
} // namespace

const Texture2D& Renderer::RevolvedSurfaceWhiteTexture() const
{
	if (!_revolvedSurfaceWhite)
	{
		_revolvedSurfaceWhite = std::make_unique<Texture2D>("surfrevol_white");
		_revolvedSurfaceWhite->Create(1, 1, 1, TextureFormat::RGBA8, Wrapping::Repeat, Filter::Linear,
		                              bgfx::copy(k_WhitePixel.data(), static_cast<uint32_t>(k_WhitePixel.size())));
	}
	return *_revolvedSurfaceWhite;
}

void Renderer::DrawParticleSurface(RenderPass viewId, const psys::surf_revol::Surface& drawn) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto surfaces = std::span(&drawn, 1);
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	for (const auto& surface : surfaces)
	{
		const entt::id_type texture = entt::hashed_string(("raw/" + surface.texture).c_str()).value();
		if (!textures.Contains(texture))
		{
			continue;
		}
		// the alpha file is <name>a.raw (S_TileLandscapeA.raw has a capital A)
		auto alphaTexture = entt::hashed_string(("raw/" + surface.texture + "a").c_str()).value();
		if (!textures.Contains(alphaTexture))
		{
			alphaTexture = entt::hashed_string(("raw/" + surface.texture + "A").c_str()).value();
		}
		const auto& alpha = textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture);
		const auto vertexCount = static_cast<uint32_t>(surface.vertices.size());
		const auto indexCount = static_cast<uint32_t>(surface.indices.size());
		for (int pass = 0; pass < 2; ++pass)
		{
			if (bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
			    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
			{
				return;
			}
			bgfx::TransientVertexBuffer vertices;
			bgfx::TransientIndexBuffer indices;
			bgfx::allocTransientVertexBuffer(&vertices, vertexCount, layout);
			bgfx::allocTransientIndexBuffer(&indices, indexCount);
			auto* out = reinterpret_cast<Vertex*>(vertices.data);
			for (const auto& v : surface.vertices)
			{
				*out++ = {v.position.x, v.position.y, v.position.z, v.uv.x, v.uv.y, pass == 0 ? v.abgr : v.specular};
			}
			std::memcpy(indices.data, surface.indices.data(), surface.indices.size() * sizeof(uint16_t));
			if (pass == 0)
			{
				program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture), 0);
			}
			else
			{
				program->SetTextureSampler("s_diffuse", 0, RevolvedSurfaceWhiteTexture(), 0);
			}
			program->SetTextureSampler("s_alpha", 1, alpha, 0);
			bgfx::setVertexBuffer(0, &vertices);
			bgfx::setIndexBuffer(&indices);
			// the ZR_SurfRevol material: 13 / 6, 12 / 5 with MaterialUpdateZBuffer, Z test on; the specular goes on top
			// additively (mode 13, (inferred)). The alpha table the draw puts with an alpha != 0xFF leaves 5, 6, 12 and
			// 13 as they are (render_modes::k_GlobalAlphaModes), so the mode is the material's
			const auto mode = pass == 1 ? render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite
			                            : render_modes::ModeFromProperties(
			                                  render_modes::Mode::AlphaTexturedAlphaNoZWrite,
			                                  {.additive = surface.additive, .zWrite = surface.writeDepth, .alpha = true});
			// CULLMODE ((~flags) & 1) * 2 + 1 with the material's two-sided bit (MaterialSetDoubleSided), so a one-sided
			// surface (the teleport pool's SF_TeleportVortex)
			// culls D3DCULL_CCW as the models do (render_modes::CullFor; the vertices are in the world, no mirror)
			bgfx::setState(render_modes::State(mode, {.cull = render_modes::CullFor(surface.doubleSided, false)}));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}
