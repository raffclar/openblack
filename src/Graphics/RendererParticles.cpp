/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The particle effects' sprites (PSys)

#include <cmath>
#include <cstring>

#include <array>
#include <span>
#include <string>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

std::pair<entt::id_type, entt::id_type> Renderer::RawTextureIds(const std::string& texture) const
{
	if (const auto it = _rawTextureIds.find(texture); it != _rawTextureIds.end())
	{
		return it->second;
	}
	const std::pair<entt::id_type, entt::id_type> ids {entt::hashed_string(("raw/" + texture).c_str()).value(),
	                                                   entt::hashed_string(("raw/" + texture + "a").c_str()).value()};
	_rawTextureIds.emplace(texture, ids);
	return ids;
}

const bgfx::VertexLayout& Renderer::ParticleQuadLayout()
{
	static const bgfx::VertexLayout s_layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	return s_layout;
}

void Renderer::DrawParticleSprites(std::span<const psys::Effect::DrawAtom> atoms, const Camera& camera, RenderPass viewId) const
{
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* program = _shaderManager->GetShader("WorldQuad");
	const auto cameraFrame = billboard::CameraFrame::From(camera);

	// atoms in the order given: one sprite of a Sorted effect (its own Z object), a run of such sprites next to each
	// other in the queue, or the sprites of a Queued / Immediate effect between its other items. Consecutive atoms with
	// the same material go in one draw call: drawn in the same order with the same states, the same pixels as one call
	// each
	size_t i = 0;
	while (i < atoms.size())
	{
		const auto* creator = atoms[i].creator;
		const auto [texture, alphaTexture] = RawTextureIds(creator->texture);
		std::vector<Vertex> vertices;
		vertices.reserve(6 * (atoms.size() - i));
		size_t j = i;
		for (; j < atoms.size(); ++j)
		{
			const auto& atom = atoms[j];
			const auto* c = atom.creator;
			if (c->texture != creator->texture || c->additive != creator->additive || c->writeDepth != creator->writeDepth)
			{
				break;
			}
			// the whole frame drawn, looped within NumFrames or clamped to its last (frame_anim::ParticleFrameIndex)
			const int frame = frame_anim::ParticleFrameIndex(atom.frame, c->numFrames, c->loopAnim);
			const float alpha = std::clamp(atom.alpha * static_cast<float>(c->scaleAlpha) / 255.0f, 0.0f, 255.0f);
			const uint32_t abgr = (static_cast<uint32_t>(alpha) << 24) | (static_cast<uint32_t>(atom.colour[2]) << 16) |
			                      (static_cast<uint32_t>(atom.colour[1]) << 8) | atom.colour[0];
			// each atom fills its creator's sprite and draws it
			billboard::Sprite sprite;
			sprite.position = atom.position;
			// size = the drawn scale, at least 0.0001; the stretch is the height
			sprite.size = std::max(atom.scale, 1e-4f);
			sprite.height = atom.stretch;
			// ox = OriginX size, oy = OriginY size stretch; the sprite draw subtracts them from the local corners
			sprite.origin = glm::vec2(c->originX * sprite.size, c->originY * sprite.size * sprite.height);
			// the angle is atan2(M[0][2], M[0][0]) of the drawn matrix unless IgnoreRotation; the original's switch for
			// it is always on, so the roll is always on. With IgnoreRotation the angle keeps what it has: (inferred) the
			// 0 the sprite starts with, each creator having its own sprite
			sprite.angle = c->ignoreRotation ? 0.0f : std::atan2(atom.rotation[0][2], atom.rotation[0][0]);
			// CentreAtBase raises the position by height x size x 0.5
			if (c->centreAtBase)
			{
				sprite.position.y += sprite.height * sprite.size * 0.5f;
			}
			// the cell in the flags' low 6 bits; SetHorozontal is flag 0x40, which also takes the origin and the angle
			// (as the yaw)
			sprite.cell = frame_anim::SpriteCell(c->fileOffset + frame);
			sprite.cellsPerRow = static_cast<uint8_t>(c->spritesPerRow);
			sprite.horizontal = c->horizontal;
			const auto quad = billboard::SpriteQuad(sprite, cameraFrame);
			if (!quad.has_value())
			{
				continue;
			}
			for (const int k : billboard::k_SpriteTriangles)
			{
				const auto& p = quad->corners.at(static_cast<size_t>(k));
				const auto& uv = quad->uv.at(static_cast<size_t>(k));
				vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
			}
		}
		i = j;
		if (vertices.empty() || !textures.Contains(texture))
		{
			continue;
		}
		const auto& layout = ParticleQuadLayout();
		const auto count = static_cast<uint32_t>(vertices.size());
		if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, count, layout);
		std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
		program->SetTextureSampler(
		    "s_alpha", 1, textures.Contains(alphaTexture) ? *textures.Handle(alphaTexture) : *textures.Handle(texture));
		bgfx::setVertexBuffer(0, &buffer);
		// the creator's material: modes 13 / 6 (12 / 5 with MaterialUpdateZBuffer), Z test on, two-sided. (inferred) the
		// alpha property is 1: with 0 they would be 8 / 3, the same states
		const auto mode =
		    render_modes::ModeFromProperties(render_modes::Mode::AlphaTexturedAlphaNoZWrite,
		                                     {.additive = creator->additive, .zWrite = creator->writeDepth, .alpha = true});
		bgfx::setState(render_modes::State(mode));
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
	}
}
