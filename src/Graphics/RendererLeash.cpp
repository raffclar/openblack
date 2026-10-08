/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The creatures' leashes. Each worn rope (Creature/LeashRope.h) is a ribbon facing the camera with a faint strip on the
// land beneath it, built by Graphics/LeashDraw.h and drawn as world triangles at once in leash.raw / leasha.raw: the
// shadow first, then the rope, which writes depth. On a land without a worn leash there is nothing to draw.

#include <cstdint>

#include <utility>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Registry.h"
#include "Graphics/Haze.h"
#include "Graphics/LeashDraw.h"
#include "Graphics/WorldTriangles.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawLeashes(RenderPass viewId, const Camera& camera) const
{
	// the scripts hide the leashes (SET_DRAW_LEASH); none in the temple
	const bool shown = help::script_control::GetCameraControl().drawLeash != 0;
	if (!shown || InTemple() || !Locator::entitiesRegistry::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::resources::has_value())
	{
		return;
	}
	// through the const registry: a land without a creature gains no storage
	const auto ropes = leash_draw::Ropes(std::as_const(Locator::entitiesRegistry::value()), shown);
	if (ropes.empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	using ecs::components::CreatureLeash;
	if (!textures.Contains(CreatureLeash::k_TextureId) || !textures.Contains(CreatureLeash::k_AlphaTextureId))
	{
		return;
	}
	const auto& diffuse = *textures.Handle(CreatureLeash::k_TextureId);
	const auto& alpha = *textures.Handle(CreatureLeash::k_AlphaTextureId);
	const auto& island = Locator::terrainSystem::value();
	const leash_rope::GroundHeight ground = [&island](glm::vec2 point) { return island.GetHeightAt(point); };
	// each corner in the land's light under it, hazed (inferred: the original's leash light is not read yet)
	const bool lit = IsLandLit();
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const leash_draw::LightAt light = [this, lit, &view](const glm::vec3& point) {
		if (!lit)
		{
			return 0xFFFFFFFFu;
		}
		auto colour = land_light::At(land_light::CurrentTable(), glm::vec2(point.x, point.z)).diffuse;
		[[maybe_unused]] const auto specular = haze::ApplyObject(_haze, haze::Depth(view, point), 0, &colour);
		return colour;
	};
	const auto indices = leash_rope::RibbonIndices();
	for (const auto* rope : ropes)
	{
		const auto drawn = leash_draw::Build(*rope, camera.GetOrigin(), ground, light);
		world_triangles::SubmitRaw(viewId, drawn.shadow, indices, diffuse, alpha, leash_draw::k_ShadowMaterial,
		                           *_shaderManager);
		world_triangles::SubmitRaw(viewId, drawn.rope, indices, diffuse, alpha, leash_draw::k_RopeMaterial, *_shaderManager);
	}
}
