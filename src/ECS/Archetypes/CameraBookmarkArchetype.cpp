/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraBookmarkArchetype.h"

#include "3D/FrameAnim.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
constexpr int k_FirstCell = 24;
} // namespace

std::array<entt::entity, 8> CameraBookmarkArchetype::CreateAll()
{
	auto& registry = Locator::entitiesRegistry::value();
	auto texture = Locator::resources::value().GetTextures().Handle(entt::hashed_string("raw/misc0a").value());
	if (!texture)
	{
		throw std::runtime_error("Failed to get Camera Bookmark sprite: misc0a");
	}

	auto result = std::array<entt::entity, 8>();

	registry.Create(result.begin(), result.end());
	const glm::vec2 extent = glm::vec2 {1.0f / 8.0f, 1.0f / 8.0f};
	const glm::vec4 tint = glm::vec4 {1.0f, 1.0f, 0.0f, 1.0f};

	// TODO (#749) use std::views::enumerate
	for (uint8_t i = 0; auto entity : result)
	{
		// (openblack) cells 24..31 of misc0a (row 3), openblack's own choice: no original address
		registry.Assign<Sprite>(entity, texture->GetNativeHandle(), graphics::frame_anim::SpriteCellUv(k_FirstCell + i)[0],
		                        extent, tint);
		++i;
		registry.Assign<CameraBookmark>(entity, i, 0.0f);
	}

	return result;
}
