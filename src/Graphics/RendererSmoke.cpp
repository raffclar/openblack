/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The chimney smoke of the houses (ecs/ChimneySmoke.h): one Z-sorter object per chimney, whose callback advances and
// draws its 10 sprites in their own order

#include <algorithm>
#include <exception>
#include <filesystem>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/ChimneySmoke.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Primitive.h"
#include "Renderer.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();

/// The smoke material: mode 6 on smoke.raw with the alpha of smokea.raw. smoke.raw is pure
/// white in the cells the smoke uses (0-15), so the colour is the vertex colour alone and smokea.raw is enough
bool LoadAlpha()
{
	auto& textures = Locator::resources::value().GetTextures();
	if (textures.Contains(k_SmokeAlpha))
	{
		return true;
	}
	try
	{
		const auto& fileSystem = Locator::filesystem::value();
		textures.Load(k_SmokeAlpha, resources::Texture2DLoader::FromDiskTag {},
		              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "smokea.raw"));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Chimney smoke: cannot load Data/Textures/smokea.raw: {}", e.what());
		return false;
	}
	return textures.Contains(k_SmokeAlpha);
}

/// The original's on-screen test of a bounding box, shared (Graphics/RegionOnScreen.h)
using openblack::graphics::region_on_screen::SphereInView;
} // namespace

bool Renderer::LoadChimneySmokeAlpha() const
{
	return LoadAlpha();
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectChimneySmoke(const Camera& camera) const
{
	// the frame's game time in milliseconds (game_clock::FrameGameMs): it stops while the game is paused (as
	// CollectMists). Collected once a frame for the main view (PreDraw)
	const bool paused = !Locator::time::has_value() || game_clock::IsPaused();
	const float milliseconds = paused ? 0.0f : static_cast<float>(game_clock::FrameGameMs());

	std::vector<std::pair<float, uint32_t>> order;
	_frameSmoke.clear();
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Size<ecs::components::ChimneySmoke>() == 0 || !LoadAlpha())
	{
		return order;
	}
	// the landscape draw sets the hand's wind before the objects are drawn
	ecs::chimney_smoke::UpdateHandWind();

	const bool forced = ecs::chimney_smoke::ForcedByTestHook();
	const auto origin = camera.GetOrigin();
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<ecs::components::ChimneySmoke, const ecs::components::Abode, const ecs::components::Mesh,
	              const ecs::components::Transform>(
	    [&](entt::entity entity, ecs::components::ChimneySmoke& smoke, const ecs::components::Abode& abode,
	        const ecs::components::Mesh& mesh, const ecs::components::Transform& transform) {
		    // only when the building was on screen this frame; off screen the smoke does not advance. Stand-in for the
		    // object's screen test: its box's sphere against the view volume
		    if (meshes.Contains(mesh.id))
		    {
			    const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
			    const glm::vec3 centre = transform.position + transform.rotation * (box.Center() * transform.scale);
			    // not region_on_screen::BoxInView: the radius scales by max(transform.scale), not by a model matrix's
			    // column lengths, and the centre is built from the Transform (kept as it was)
			    const float scale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
			    if (!SphereInView(viewProjection, centre, glm::length(box.Size()) * 0.5f * scale))
			    {
				    return;
			    }
		    }
		    // PresentAtHome != 0, or a workshop whose scaffold countdown is not 0 (ecs::workshops)
		    const auto* workshop = registry.TryGet<const ecs::components::Workshop>(entity);
		    const bool lit = abode.presentAtHome != 0 || (workshop != nullptr && workshop->countdown != 0) || forced;
		    if (!ecs::chimney_smoke::UpdateState(smoke, lit))
		    {
			    return;
		    }
		    // the Z-sorter key is |chimney - camera|^2, (x^2 + y^2) + z^2. The callback simulates while it draws, so it
		    // is advanced here, once per frame for the smoke drawn
		    std::vector<ecs::chimney_smoke::DrawnPuff> drawn;
		    ecs::chimney_smoke::Advance(smoke, milliseconds, drawn);
		    order.emplace_back(zsort::Key(smoke.position, origin), static_cast<uint32_t>(_frameSmoke.size()));
		    _frameSmoke.push_back(std::move(drawn));
	    },
	    entt::exclude<ecs::components::Unavailable>);
	return order;
}

void Renderer::DrawChimneySmoke(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const
{
	if (index >= _frameSmoke.size() || _frameSmoke[index].empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& alpha = *textures.Handle(k_SmokeAlpha);
	const auto* program = _shaderManager->GetShader("Sprite");
	const auto frame = billboard::CameraFrame::From(camera);
	for (const auto& puff : _frameSmoke[index])
	{
		// a screen sprite (billboard::Screen): a square of half width = size in the plane of the
		// screen, turned by the angle (x_v = cos x + sin y, y_v = -sin x + cos y, i.e. by -angle), no origin offset;
		// on the GPU through vs_sprite (billboard::ScreenSpriteModel). Nothing when its depth is at or before the near
		// plane
		if (!billboard::InFrontOfNear(puff.position, frame))
		{
			continue;
		}
		const glm::mat4 model = billboard::ScreenSpriteModel(puff.position, glm::vec2(puff.halfWidth), puff.angle);
		// 8 cells per row: the cell's corners v0 (top left) and v2 (bottom right), 1/8 wide
		const auto uv = frame_anim::SpriteCellUv(static_cast<int>(puff.cell), 8);
		const glm::vec4 sampleRect(uv[2] - uv[0], uv[0]);
		// mode 6 (SRCALPHA / INVSRCALPHA, no light, no fog): the sprite shader's normal blend is ONE / INVSRCALPHA with
		// the tint premultiplied by its alpha
		const glm::vec4 colour = argb_colour::ToVec4(puff.argb);
		const glm::vec4 tint(glm::vec3(colour) * colour.a, colour.a);

		bgfx::setTransform(glm::value_ptr(model));
		program->SetUniformValue("u_sampleRect", glm::value_ptr(sampleRect));
		program->SetUniformValue("u_tint", glm::value_ptr(tint));
		program->SetTextureSampler("s_diffuse", 0, alpha);
		_plane->GetVertexBuffer().Bind();
		// the smoke material, mode 6 with the tint premultiplied: depth test, no depth
		// write (the material is two sided: no culling)
		bgfx::setState(render_modes::State(render_modes::materials::k_Smoke, {.writeAlpha = true, .premultiplied = true}));
		bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
	}
}
