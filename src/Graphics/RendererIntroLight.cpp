/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The intro light (PLAY_JC_SPECIAL 0, ecs/IntroSpecial.h): one Z object of the frame's transparency queue (key
// |head - camera|^2), whose callback draws its sprites one after the other (in the plane of the screen, nothing at or
// before the near plane) in its own material (mode 13, misc0.raw): SRCALPHA / ONE, no Z write, one-sided; ZFUNC
// ALWAYS around them when the light asks.
// Everything comes from the frame's graphics::OverlayFrame (filled before DrawScene): nothing here reads the game.

#include <cstdint>

#include <vector>

#include <entt/core/hashed_string.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawIntroLight(RenderPass viewId, const Camera& camera, const IntroLightOverlay& light) const
{
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/misc0").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/misc0a").value();
	if (light.sprites.empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	// each sprite in its order, four corners and the sprite draw's triangles (0,1,2),(0,2,3):
	// one indexed draw keeps the primitives' order (the state is the same for all)
	const auto frame = billboard::CameraFrame::From(camera);
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	vertices.reserve(light.sprites.size() * 4);
	indices.reserve(light.sprites.size() * billboard::k_SpriteTriangles.size());
	for (const auto& source : light.sprites)
	{
		auto sprite = source;
		sprite.cell = frame_anim::SpriteCell(source.cell); // the cell (0..63) of the 8 x 8 sheet
		const auto quad = billboard::SpriteQuad(sprite, frame);
		if (!quad.has_value())
		{
			continue; // at or before the near plane
		}
		const uint32_t abgr = argb_colour::ToAbgr(sprite.argb);
		const auto first = static_cast<uint16_t>(vertices.size()); // at most 20 x 4
		for (size_t corner = 0; corner < quad->corners.size(); ++corner)
		{
			vertices.push_back({quad->corners.at(corner), quad->uv.at(corner), abgr});
		}
		for (const int corner : billboard::k_SpriteTriangles)
		{
			indices.push_back(static_cast<uint16_t>(first + corner));
		}
	}
	if (vertices.empty())
	{
		return;
	}
	// mode 13: SRCALPHA / ONE, ZWRITE 0; the material culls (CULLMODE 3; the screen quad is front facing, so it
	// shows); ZFUNC 8 while depthAlways, else 4. The shared upload of the world triangles' .raw materials (WorldQuad,
	// s_diffuse / s_alpha)
	render_modes::StateOptions options;
	options.zFunc = light.depthAlways ? render_modes::ZFunc::Always : render_modes::ZFunc::LessEqual;
	world_triangles::SubmitRaw(viewId, vertices, indices, *textures.Handle(k_Texture), *textures.Handle(k_Alpha),
	                           render_modes::materials::k_Misc0Additive, *_shaderManager, options);
}
