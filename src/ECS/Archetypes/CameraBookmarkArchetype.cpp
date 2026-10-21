/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraBookmarkArchetype.h"

#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity CameraBookmarkArchetype::Create(uint8_t number)
{
	auto& registry = Locator::entitiesRegistry::value();
	constexpr auto k_Sheet = entt::hashed_string("raw/misc0a");
	if (!Locator::resources::value().GetTextures().Contains(k_Sheet.value()))
	{
		throw std::runtime_error("Failed to get Camera Bookmark sprite: misc0a");
	}
	const auto entity = registry.Create();
	// Each marker shows its number in a ring: the fourth row of the sheet holds the numbers from 1, one in each column
	const glm::vec2 extent = glm::vec2 {1.0f / 8.0f, 1.0f / 8.0f};
	const glm::vec4 tint = glm::vec4 {1.0f, 1.0f, 0.0f, 1.0f};
	const float u = static_cast<float>(number) / 8.0f;
	registry.Assign<Sprite>(entity, k_Sheet.value(), glm::vec2 {u, 3.0f / 8.0f}, extent, tint);
	registry.Assign<CameraBookmark>(entity, CameraBookmark {.number = number});
	return entity;
}

std::array<entt::entity, camera_bookmarks::k_KeySlots> CameraBookmarkArchetype::CreateAll()
{
	auto result = std::array<entt::entity, camera_bookmarks::k_KeySlots>();
	for (uint8_t number = 0; number < result.size(); ++number)
	{
		result.at(number) = Create(number);
	}
	return result;
}
