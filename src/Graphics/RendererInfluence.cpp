/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The influence border and the hand's ripples on it (ECS/Influence/InfluenceCircles.cpp keeps both lists).
// Once a frame in the world view: every circle of every player as a curtain 40 high in burn.raw / burna.raw, its
// middle row lit in the owner's colour, scrolled, in its own material (render mode 6, two-sided, tiled), as world
// triangles at once (no Z object), after everything else drawn at once and before the Z-sorter drain at the frame's
// end (docs/bw1-notes/original-frame.md, rows 23 / 24a). The ripples: one Z object each, whose callback draws its 7
// smoke.raw sprites in the plane of the border.

#include <cstdint>

#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "ECS/Influence/Influence.h"
#include "GameClock.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// Data\Textures\burn.raw: ARGB4444 with the alpha of burna.raw (argb4444::k_AlphaFlagStems); the loader keeps the two
/// files as two textures
constexpr entt::id_type k_Burn = entt::hashed_string("raw/burn").value();
constexpr entt::id_type k_BurnAlpha = entt::hashed_string("raw/burna").value();
/// smoke.raw and its alpha, the texture of the smoke material the ripples use
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
/// every ripple sprite uses cell 63
constexpr int k_RippleCell = 63;
} // namespace

void Renderer::UpdateInfluenceCurtain(const Camera& camera) const
{
	_preInfluenceScroll.reset();
	// the camera gate and the middle row's alpha from the camera's height
	const auto alpha = influence::CurtainAlpha(camera.GetOrigin().y);
	if (!alpha.has_value())
	{
		return;
	}
	// the material is made on the first draw, render_modes::materials::k_InfluenceCircle.
	// the scroll clock and the offset; it advances with or without circles
	_preInfluenceScroll = frame_anim::InfluenceScroll(_influenceScrollMs, game_clock::FrameGameMs());
	// the middle rows' alpha, for the players whose border is shown
	influence::SetCurtainAlpha(*alpha);
}

void Renderer::DrawInfluenceCircles(RenderPass viewId) const
{
	// the camera gate, the scroll and the middle rows' alpha of this frame: UpdateInfluenceCurtain (PreDraw)
	if (!_preInfluenceScroll.has_value())
	{
		return;
	}
	const auto offset = *_preInfluenceScroll;
	// the plain world-to-clipping matrix, world space (vs_blob's u_viewProj)
	const auto circles = influence::Circles();
	if (circles.empty() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Burn) || !textures.Contains(k_BurnAlpha))
	{
		return;
	}
	const auto& diffuse = *textures.Handle(k_Burn);
	const auto& alphaTexture = *textures.Handle(k_BurnAlpha);
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	for (const auto& circle : circles)
	{
		// world triangles (3N + 3 vertices, 4N triangles): every circle is drawn, the invisible ones (alpha 0, border
		// not shown yet) too. No fog, no haze, no specular; the material's culling, none; the UV offset added by the
		// triangle draw (the material's bit 0x10 is clear)
		const auto& curtain = circle.curtain;
		vertices.clear();
		indices.clear();
		for (size_t k = 0; k < curtain.positions.size(); ++k)
		{
			vertices.push_back({curtain.positions[k], frame_anim::OffsetUv(curtain.uvs[k], offset, false),
			                    world_triangles::ToAbgr(curtain.colours[k])});
		}
		for (const auto index : curtain.indices)
		{
			indices.push_back(static_cast<uint16_t>(index)); // at most 3 x 250 + 2
		}
		world_triangles::SubmitRaw(viewId, vertices, indices, diffuse, alphaTexture, render_modes::materials::k_InfluenceCircle,
		                           *_shaderManager);
	}
	// the offset is only this draw's
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectInfluenceRipples(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	const auto ripples = influence::Ripples();
	const auto eye = camera.GetOrigin();
	order.reserve(ripples.size());
	for (size_t i = 0; i < ripples.size(); ++i)
	{
		// a Z object keyed by |ripple point - camera|^2 summed (x^2 + y^2) + z^2
		order.emplace_back(zsort::Key(ripples[i].point, eye, zsort::SumOrder::XYZ), static_cast<uint32_t>(i));
	}
	return order;
}

void Renderer::DrawInfluenceRipple(RenderPass viewId, uint32_t index, uint32_t frameGameMs) const
{
	const auto ripples = influence::Ripples();
	if (index >= ripples.size() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	// the sprites grow and fade (influence::DrawRipple); M = the ripple's matrix x world_to_clip, here the
	// world matrix (vs_blob's u_viewProj does the rest)
	const auto matrix = ripples[index].matrix;
	const auto sprites = influence::DrawRipple(index, frameGameMs);
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	vertices.reserve(sprites.size() * 4);
	indices.reserve(sprites.size() * billboard::k_SpriteTriangles.size());
	for (const auto& drawn : sprites)
	{
		// the quad in the matrix's XZ plane turned by the sprite's angle
		billboard::Sprite sprite {
		    .size = drawn.size,
		    .angle = drawn.angle,
		    .argb = drawn.argb,
		    .cell = frame_anim::SpriteCell(k_RippleCell),
		};
		const auto quad = billboard::PlaneOfMatrix(sprite, matrix);
		const auto first = static_cast<uint16_t>(vertices.size());
		for (size_t c = 0; c < quad.corners.size(); ++c)
		{
			vertices.push_back({quad.corners.at(c), quad.uv.at(c), world_triangles::ToAbgr(sprite.argb)});
		}
		for (const int corner : billboard::k_SpriteTriangles)
		{
			indices.push_back(static_cast<uint16_t>(first + corner));
		}
	}
	// the smoke material: mode 6, two-sided
	world_triangles::SubmitRaw(viewId, vertices, indices, *textures.Handle(k_Smoke), *textures.Handle(k_SmokeAlpha),
	                           render_modes::materials::k_Smoke, *_shaderManager);
}
