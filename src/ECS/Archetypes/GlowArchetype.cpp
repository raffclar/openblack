/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GlowArchetype.h"

#include <entt/core/hashed_string.hpp>
#include <glm/ext/matrix_float3x3.hpp>

#include "3D/Light.h"
#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

std::array<entt::entity, 2> GlowArchetype::Create(Registry& registry, const LightEmitter& emitter, TempleRoom room)
{
	if (emitter.cone.has_value())
	{
		auto beam = registry.Create();
		registry.Assign<ecs::components::TempleInteriorPart>(beam, room);
		registry.Assign<LightBeam>(beam, *emitter.cone);
	}

	const auto& glow = emitter.glow;
	// Many of the temple's lights only lit its lightmaps: their glows are too small to see
	constexpr float k_VisibleSize = 0.001f;
	if (glow.haloSize < k_VisibleSize)
	{
		return {entt::null, entt::null};
	}

	// As in the game, the glow is drawn additively from data/textures/atmos.raw with its alpha, atmosa.raw
	constexpr auto k_Texture = entt::hashed_string("raw/ATMOS");
	constexpr auto k_Alpha = entt::hashed_string("raw/ATMOSA");
	// The glow is frame 22 of the atmosphere texture's 8 by 8 frames
	const auto uvMin = glm::vec2 {6.0f / 8.0f, 2.0f / 8.0f};
	const auto extent = glm::vec2 {1.0f / 8.0f, 1.0f / 8.0f};
	const auto rotation = glow.orientation.value_or(glm::mat3(1.0f));

	// The light's colour, and a smaller whiter centre drawn over it
	const auto createSprite = [&](const glm::vec4& tint, float size) {
		auto entity = registry.Create();
		registry.Assign<ecs::components::TempleInteriorPart>(entity, room);
		registry.Assign<Sprite>(entity, k_Texture.value(), uvMin, extent, tint, true, !glow.orientation.has_value(),
		                        k_Alpha.value());
		registry.Assign<ecs::components::Transform>(entity, glow.position, rotation, glm::vec3(size));
		return entity;
	};
	return {createSprite(glow.haloColour, glow.haloSize), createSprite(glow.centreColour, glow.centreSize)};
}
