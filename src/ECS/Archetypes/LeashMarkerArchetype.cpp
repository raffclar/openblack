/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashMarkerArchetype.h"

#include <entt/core/hashed_string.hpp>
#include <glm/vec4.hpp>

#include "3D/CreatureBody.h"
#include "Creature/LeashOrders.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The sheet of markers and footprints, white where its pictures are, and their shapes in the alpha beside it
constexpr auto k_SheetId = entt::hashed_string("raw/misc0");
constexpr auto k_SheetAlphaId = entt::hashed_string("raw/misc0a");
/// Its pictures are eight to a row, eight rows
constexpr float k_CellsPerSide = 8.0f;
/// Both are drawn opaque yellow
constexpr glm::vec4 k_Yellow {1.0f, 1.0f, 0.0f, 1.0f};
} // namespace

std::optional<std::pair<entt::entity, entt::entity>> LeashMarkerArchetype::Create(entt::entity creature, CreatureType species)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_SheetId.value()) || !textures.Contains(k_SheetAlphaId.value()))
	{
		return std::nullopt;
	}
	const auto sheet = textures.Handle(k_SheetId)->GetNativeHandle();
	const auto alpha = textures.Handle(k_SheetAlphaId)->GetNativeHandle();
	const auto make = [&](uint8_t cell) {
		const auto entity = registry.Create();
		const glm::vec2 uv {static_cast<float>(cell % 8) / k_CellsPerSide, static_cast<float>(cell / 8) / k_CellsPerSide};
		registry.Assign<Sprite>(entity, Sprite {.texture = sheet,
		                                        .uvMin = uv,
		                                        .uvExtent = glm::vec2(1.0f / k_CellsPerSide),
		                                        .tint = k_Yellow,
		                                        .additive = false,
		                                        .facesCamera = true,
		                                        .alpha = alpha});
		// Hidden until it is first placed
		registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(0.0f));
		registry.Assign<LeashMarker>(entity, creature);
		return entity;
	};
	const auto ring = make(creature_leash_orders::k_RingCell);
	const auto footprint = make(creature_leash_orders::FootprintCell(creature::InfoRow(species)));
	return std::pair {ring, footprint};
}
